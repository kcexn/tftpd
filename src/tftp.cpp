/* Copyright (C) 2025 Kevin Exton (kevin.exton@pm.me)
 *
 * tftp-server is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * tftp-server is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with tftp-server.  If not, see <https://www.gnu.org/licenses/>.
 */
/**
 * @file tftp.cpp
 * @brief This file defines the TFTP application logic.
 */
#include "tftp/tftp.hpp"
#include "tftp/filesystem.hpp"

namespace tftp {
/** @brief Inserts characters from buf to buffer while accomodating netascii. */
static inline auto insert_data(std::vector<char> &buffer,
                               std::span<const char> buf,
                               std::uint8_t mode) -> void
{
  using enum messages::mode_t;

  const auto *end = buf.data() + buf.size();
  for (const auto *get = buf.data(); get != end; ++get)
  {
    if (mode == NETASCII)
    {
      if (*get == '\0')
        continue;

      if (*get == '\n')
      {
        if (buffer.size() > sizeof(messages::data) && buffer.back() == '\0')
        {
          buffer.pop_back();
        }
        else
        {
          buffer.push_back('\r');
        }
      }
    }

    buffer.push_back(*get);

    if (mode == NETASCII && *get == '\r')
    {
      buffer.push_back('\0');
    }
  }
}

/** @brief Prepares the next block to send. */
static inline auto send_next(iterator_t siter) -> std::uint16_t
{
  using enum messages::opcode_t;

  auto &[key, session] = *siter;
  auto &state = session.state;
  auto &buffer = state.buffer;

  // Prepare the next chunk for writing.
  state.block_num += 1; // block_num wraps back to 0.

  // Size the buffer for a complete TFTP DATA message and enough
  // extra to handle netascii processing.
  buffer.reserve(messages::DATAMSG_MAXLEN + messages::DATALEN);
  if (buffer.size() < sizeof(messages::data))
    buffer.resize(sizeof(messages::data));

  auto put = buffer.begin();

  // Set the message headers.
  auto *msg = reinterpret_cast<messages::data *>(std::addressof(*put));
  msg->opc = htons(DATA);
  msg->block_num = htons(state.block_num);

  put += sizeof(*msg);

  if (buffer.size() > messages::DATAMSG_MAXLEN)
    put = std::copy(put + messages::DATALEN, buffer.end(), put);

  buffer.erase(put, buffer.end());

  // Read the next file chunk into the RRQ buffer.
  auto buf = std::array<char, messages::DATALEN>();
  auto len = state.file->readsome(buf.data(), buf.size());
  if (state.file->fail()) [[unlikely]]
    return messages::ACCESS_VIOLATION; // GCOVR_EXCL_LINE

  insert_data(buffer, std::span(buf.data(), len), state.mode);
  return 0;
}

#ifndef TFTP_SERVER_STATIC_TEST
auto handle_request(messages::request req, iterator_t siter) -> std::uint16_t
{
  using enum messages::opcode_t;
  // Invalid request message.
  if (req.opc == DATA || req.opc == ACK || req.mode == 0)
    return messages::ILLEGAL_OPERATION;

  // Mail mode is not allowed in RRQ's.
  if (req.opc == RRQ && req.mode == messages::MAIL)
    return messages::ILLEGAL_OPERATION;

  auto &[key, session] = *siter;
  auto &state = session.state;

  state.opc = req.opc;
  state.target = req.filename;
  state.mode = req.mode;

  if (req.opc == WRQ && req.mode == messages::MAIL)
  {
    state.target =
        filesystem::mail_directory() / state.target /
        std::format("{:%Y%m%d_%H%M%S}", std::chrono::system_clock::now());
  }

  auto openmode = (req.opc == WRQ)
                      ? std::ios::out | std::ios::binary | std::ios::trunc
                      : std::ios::in | std::ios::binary;

  auto err = std::error_code();
  state.file = filesystem::tmpfile_from(state.target, openmode, state.tmp, err);
  if (!state.file)
  {
    if (err == std::errc::no_such_file_or_directory)
    {
      if (req.opc == WRQ && req.mode == messages::MAIL)
        return messages::NO_SUCH_USER;

      return messages::FILE_NOT_FOUND;
    }

    return messages::ACCESS_VIOLATION;
  }

  if (req.opc == RRQ)
    return send_next(siter);

  return 0;
}

/**
 * @brief Processes an ack message.
 * @param ack The TFTP ack to process.
 * @param siter An iterator pointing to the session.
 * @returns 0 if successful, a non-zero TFTP error otherwise.
 */
auto handle_ack(messages::ack ack, iterator_t siter) -> std::uint16_t
{
  using enum messages::opcode_t;

  auto &[key, session] = *siter;
  auto &state = session.state;

  if (state.opc != RRQ)
    return messages::UNKNOWN_TID;

  if (state.buffer.size() >= messages::DATAMSG_MAXLEN &&
      ntohs(ack.block_num) == state.block_num)
  {
    return send_next(siter);
  }

  state.file->close();
  return 0;
}

auto handle_data(const messages::data *data, std::size_t len,
                 iterator_t siter) -> std::uint16_t
{
  using enum messages::opcode_t;

  auto &[key, session] = *siter;
  auto &opc = session.state.opc;
  auto &block_num = session.state.block_num;
  auto &target = session.state.target;
  auto &tmp = session.state.tmp;

  if (opc != WRQ)
    return messages::UNKNOWN_TID;

  // Re-ACK duplicate packets.
  if (ntohs(data->block_num) != block_num + 1)
    return 0;

  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  const auto *payload = reinterpret_cast<const char *>(data) + sizeof(*data);
  len -= sizeof(*data);
  block_num += 1;

  // Write the data to the file.
  auto &file = session.state.file;
  file->write(payload, static_cast<std::streamsize>(len));
  if (file->fail())
    return messages::DISK_FULL; // GCOVR_EXCL_LINE

  // File writing is complete.
  if (len < messages::DATALEN)
  {
    file->close();
    auto err = std::error_code();
    std::filesystem::rename(tmp, target, err);
    if (err != std::errc{}) [[unlikely]]
      return messages::ACCESS_VIOLATION; // GCOVR_EXCL_LINE
  }

  return 0;
}
#endif // TFTP_SERVER_STATIC_TEST
} // namespace tftp

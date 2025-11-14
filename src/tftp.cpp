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
/**
 * @brief Inserts data into a buffer, handling NETASCII conversion.
 * @details This function appends data from a source span (`buf`) to a
 * destination vector (`buffer`). If the transfer mode is `NETASCII`, it
 * performs line-ending conversions as specified by the TFTP protocol.
 *
 *          NETASCII Conversion Rules:
 *          - Bare carriage returns (`\r`) are converted to `\r\0`.
 *          - Bare line feeds (`\n`) are converted to `\r\n`.
 *          - A `\r\n` sequence in the input is handled correctly to produce a
 *            `\r\n` sequence in the output buffer by observing that a `\n`
 *            is preceded by a `\r\0` from a previous conversion.
 *          - Bare null bytes (`\0`) in the input are skipped to avoid ambiguity
 *            with the `\r\0` sequence.
 *
 * @param[in,out] buffer The destination buffer to which data will be inserted.
 * @param[in]     buf    A span of characters representing the source data.
 * @param[in]     mode   The TFTP transfer mode (e.g., `NETASCII` or `OCTET`).
 */
static inline auto insert_data(std::vector<char> &buffer,
                               std::span<const char> buf,
                               std::uint8_t mode) -> void
{
  using enum messages::mode_t;

  if (mode != NETASCII)
  {
    buffer.insert(buffer.end(), buf.begin(), buf.end());
    return;
  }

  // NETASCII processing.
  for (const auto chr : buf)
  {
    // Skip bare \0 bytes so as to not confuse \r\0 handling.
    if (chr == '\0')
      continue;

    if (chr == '\n')
    {
      if (buffer.size() > sizeof(messages::data) && buffer.back() == '\0')
      {
        // If the previous byte is \0 then it must be from a \r\0 sequence.
        buffer.pop_back();
      }
      else
      {
        // Otherwise this is a bare \n.
        buffer.push_back('\r');
      }
    }

    buffer.push_back(chr);

    // Bare \r must be followed by a \0.
    if (chr == '\r')
    {
      buffer.push_back('\0');
    }
  }
}

/**
 * @brief Prepares the next data block to be sent for a file transfer session.
 * @details This function constructs the next data packet to be sent to the
 * client.
 *
 * The session buffer is reused for each packet. If a previous operation (like
 * NETASCII conversion) resulted in more data than could fit in a single packet,
 * that "overflow" data is present at the end of the buffer. This function moves
 * that overflow data to the beginning of the buffer to be sent in the current
 * packet. The buffer layout is conceptualized as:
 * `[header][512 bytes data][overflow]`.
 *
 * To prevent reallocations, the buffer capacity is reserved to hold a full data
 * packet plus space for NETASCII expansion. The buffer is resized to at least
 * hold the DATA packet header.
 *
 * New file data is read into a temporary buffer. This data is then processed
 * for NETASCII conversion (if required for the session's mode) and appended to
 * the session buffer after the header and any overflow data.
 *
 * @param siter An iterator pointing to the current session in the sessions map.
 * @return std::uint16_t Returns 0 on success. If there is a file read error, it
 * returns `messages::ACCESS_VIOLATION`.
 */
static inline auto send_next(iterator_t siter) -> std::uint16_t
{
  using enum messages::opcode_t;

  auto &[key, session] = *siter;
  auto &state = session.state;
  auto &buffer = state.buffer;

  state.block_num += 1; // block_num wraps on overflow.

  buffer.reserve(messages::DATAMSG_MAXLEN + messages::DATALEN);
  if (buffer.size() < sizeof(messages::data))
    buffer.resize(sizeof(messages::data));

  auto data_start = buffer.begin() + sizeof(messages::data);

  if (buffer.size() > messages::DATAMSG_MAXLEN)
  {
    auto overflow_start = data_start + messages::DATALEN;
    data_start = std::copy(overflow_start, buffer.end(), data_start);
  }

  data_start = buffer.erase(data_start, buffer.end());

  auto *msg = reinterpret_cast<messages::data *>(buffer.data());
  msg->opc = htons(DATA);
  msg->block_num = htons(state.block_num);

  auto read_size =
      static_cast<std::streamsize>(messages::DATAMSG_MAXLEN - buffer.size());
  auto read_buf = std::array<char, messages::DATALEN>();
  state.file->read(read_buf.data(), read_size);
  if (state.file->bad()) [[unlikely]]
    return messages::ACCESS_VIOLATION; // GCOVR_EXCL_LINE

  insert_data(buffer, std::span(read_buf.data(), state.file->gcount()),
              state.mode);
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

  if (ntohs(ack.block_num) == state.block_num)
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

  // Wraps block_num around
  auto next_block = static_cast<std::uint16_t>(block_num + 1);
  if (ntohs(data->block_num) != next_block)
    return 0;

  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  const auto *payload = reinterpret_cast<const char *>(data) + sizeof(*data);
  len -= sizeof(*data);
  block_num = next_block;

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

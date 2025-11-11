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
 * @file tftp_server.cpp
 * @brief This file defines the TFTP server.
 */
#include "tftp/tftp_server.hpp"
#include "tftp/filesystem.hpp"
#include "tftp/protocol/tftp_protocol.hpp"

#include <net/timers/timers.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cassert>
#include <filesystem>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
namespace tftp {
/** @brief Additional buffer length for <PORT>,[],: and null.  */
static constexpr auto ADDR_BUFLEN = 9UL;
/** @brief Socket address type. */
template <typename T> using socket_address = ::io::socket::socket_address<T>;

/** @brief Milliseconds type. */
using milliseconds = std::chrono::milliseconds;

/** @brief Bounds checked implementation of strlen. */
static inline auto strnlen(const char *str,
                           std::size_t maxlen) noexcept -> std::size_t
{
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  const auto *found = std::find(str, str + maxlen, '\0');
  return found - str;
}

/** @brief Converts the socket address to a string inside buf. */
static inline auto
to_str(std::span<char> buf,
       socket_address<sockaddr_in6> addr) noexcept -> std::string_view
{
  assert(buf.size() >= INET6_ADDRSTRLEN + ADDR_BUFLEN &&
         "Buffer must be large enough to print an IPv6 address and a port "
         "number.");

  using namespace io::socket;
  using std::to_chars;

  std::memset(buf.data(), 0, buf.size());
  unsigned short port = 0;
  std::size_t len = 0;

  if (addr->sin6_family == AF_INET)
  {
    const auto *addr_v4 =
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        reinterpret_cast<sockaddr_in *>(std::ranges::data(addr));
    inet_ntop(addr_v4->sin_family, &addr_v4->sin_addr, buf.data(), buf.size());
    port = ntohs(addr_v4->sin_port);
    len = strnlen(buf.data(), buf.size());
  }
  else
  {
    buf[0] = '[';
    inet_ntop(addr->sin6_family, &addr->sin6_addr, buf.data() + 1,
              buf.size() - 1);
    port = ntohs(addr->sin6_port);
    len = strnlen(buf.data(), buf.size());
    buf[len++] = ']';
  }

  buf[len++] = ':';
  to_chars(buf.data() + len, buf.data() + buf.size(), port);

  return {buf.data()};
}

/** @brief Converts valid C strings to string views. */
static inline auto to_view(const char *str,
                           std::size_t maxlen) -> std::string_view
{
  const auto len = strnlen(str, maxlen);
  if (len == maxlen)
    return {};

  return str;
}

/** @brief Converts a string_view to a TFTP mode. */
static inline auto to_mode(std::string_view mode) -> messages::mode_t
{
  using enum messages::mode_t;
  constexpr auto BUFSIZE = sizeof("netascii");

  auto buf = std::array<char, BUFSIZE>{};
  const auto len = std::min(mode.size(), buf.size());
  std::transform(mode.begin(), mode.begin() + len, buf.begin(),
                 [](unsigned char chr) { return std::tolower(chr); });

  if (std::strncmp(buf.data(), "netascii", buf.size()) == 0)
    return NETASCII;

  if (std::strncmp(buf.data(), "octet", buf.size()) == 0)
    return OCTET;

  if (std::strncmp(buf.data(), "mail", buf.size()) == 0)
    return MAIL;

  return {};
}

/**
 * @brief Parses a TFTP request and returns a session::state_t if valid.
 * @param msg A span providing a view into the message.
 * @returns std::nullopt if the msg is invalid, a session::state_t otherwise.
 */
static inline auto parse_request(std::span<const std::byte> msg)
    -> std::optional<messages::request>
{
  using enum messages::error_t;
  using enum messages::opcode_t;

  auto req = messages::request{};
  const auto *buf = msg.data();
  const auto *end = msg.data() + msg.size();

  const auto *opcode = reinterpret_cast<const messages::opcode_t *>(buf);
  req.opc = static_cast<messages::opcode_t>(ntohs(*opcode));
  if (req.opc != RRQ && req.opc != WRQ)
    return std::nullopt;

  buf += sizeof(messages::opcode_t);

  auto filepath = to_view(reinterpret_cast<const char *>(buf), end - buf);
  if (filepath.empty())
    return std::nullopt;

  req.filename = filepath.data();
  buf += filepath.size() + 1;

  auto mode = to_view(reinterpret_cast<const char *>(buf), end - buf);
  if (mode.empty())
    return std::nullopt;

  req.mode = to_mode(mode);
  if (req.mode == 0)
    return std::nullopt;

  return req;
}

static inline auto
clamped_exp_weighted_average(milliseconds curr,
                             milliseconds prev) -> milliseconds
{
  auto avg = prev * 3 / 4 + curr / 4;
  avg = std::min(avg, session::TIMEOUT_MAX);
  avg = std::max(avg, session::TIMEOUT_MIN);
  return avg;
}

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

#ifndef TFTP_SERVER_STATIC_TEST
auto server::error(async_context &ctx, const socket_dialog &socket,
                   iterator siter, std::uint16_t error) -> void
{
  using namespace stdexec;
  using enum messages::error_t;
  using namespace std::filesystem;

  std::error_code err;
  auto &[key, session] = *siter;

  auto msg = socket_message{.address = {key}};
  switch (error)
  {
    case NOT_DEFINED:
      msg.buffers = errors::not_implemented();
      break;

    case ACCESS_VIOLATION:
      msg.buffers = errors::access_violation();
      break;

    case FILE_NOT_FOUND:
      msg.buffers = errors::file_not_found();
      break;

    // Integration tests for a disk_full condition are a huge pain.
    case DISK_FULL:                      // GCOVR_EXCL_LINE
      msg.buffers = errors::disk_full(); // GCOVR_EXCL_LINE
      break;                             // GCOVR_EXCL_LINE

    case NO_SUCH_USER:
      msg.buffers = errors::no_such_user();
      break;

    case UNKNOWN_TID:
      msg.buffers = errors::unknown_tid();
      break;

    case ILLEGAL_OPERATION:
      msg.buffers = errors::illegal_operation();
      break;

    case TIMED_OUT:
      msg.buffers = errors::timed_out();
      [[fallthrough]];

    default:
      break;
  }

  if (msg.buffers)
  {
    sender auto sendmsg = io::sendmsg(socket, msg, 0) |
                          then([](auto &&len) {}) |
                          upon_error([](auto &&error) {});
    ctx.scope.spawn(std::move(sendmsg));
  }

  cleanup(ctx, socket, siter);
}

auto server::ack(async_context &ctx, const socket_dialog &socket,
                 const std::shared_ptr<read_context> &rctx,
                 std::span<const std::byte> msg, iterator siter) -> void
{
  using enum messages::opcode_t;
  using enum messages::error_t;
  auto addrbuf = std::array<char, INET6_ADDRSTRLEN + ADDR_BUFLEN>{};

  auto &[key, session] = *siter;
  auto addrstr = to_str(addrbuf, key);

  auto &state = session.state;
  if (state.opc == RRQ)
  {
    if (state.buffer.size() < messages::DATAMSG_MAXLEN)
    {
      spdlog::info("RRQ for {} served to {}.", state.target.c_str(), addrstr);
      return cleanup(ctx, socket, siter);
    }

    const auto *ack = reinterpret_cast<const messages::ack *>(msg.data());
    if (ntohs(ack->block_num) == state.block_num)
      send_next(ctx, socket, siter);

    return reader(ctx, socket, rctx);
  }

  spdlog::error("Ack from {} with unknown TID.", addrstr);
  error(ctx, socket, siter, UNKNOWN_TID);
}

auto server::rrq(async_context &ctx, const socket_dialog &socket,
                 const std::shared_ptr<read_context> &rctx,
                 std::span<const std::byte> buf, iterator siter) -> void
{
  using enum messages::opcode_t;
  using enum messages::error_t;
  using namespace detail;

  auto addrbuf = std::array<char, INET6_ADDRSTRLEN + ADDR_BUFLEN>{};

  auto &[key, session] = *siter;
  auto addrstr = to_str(addrbuf, key);
  auto &state = session.state;

  // Out-of-the-blue packet, already a session running on this socket.
  if (state.opc != 0)
    return reader(ctx, socket, rctx);

  spdlog::info("New RRQ from {}.", addrstr);

  auto request = parse_request(buf);
  if (!request)
  {
    spdlog::error("Invalid RRQ from {}.", addrstr);
    return error(ctx, socket, siter, NOT_DEFINED);
  }

  state.opc = request->opc;
  state.target = request->filename;
  state.mode = request->mode;

  assert(state.opc == RRQ && "Operation MUST be a read request.");
  if (state.mode == messages::MAIL)
  {
    spdlog::error("Invalid RRQ from {}. Mail mode is not allowed.", addrstr);
    return error(ctx, socket, siter, ILLEGAL_OPERATION);
  }

  auto err = std::error_code();
  state.file = filesystem::tmpfile_from(
      state.target, std::ios::in | std::ios::binary, state.tmp, err);
  if (!state.file)
  {
    spdlog::error("Copying {} failed with error: {}", state.target.c_str(),
                  err.message());
    if (err == std::errc::no_such_file_or_directory)
      return error(ctx, socket, siter, FILE_NOT_FOUND);

    return error(ctx, socket, siter, ACCESS_VIOLATION);
  }

  state.socket = static_cast<session::socket_type>(*socket.socket);

  send_next(ctx, socket, siter);
  reader(ctx, socket, rctx);
}

auto server::send(async_context &ctx, const socket_dialog &socket,
                  iterator siter) -> void
{
  using namespace stdexec;
  auto &[key, session] = *siter;

  auto &buffer = session.state.buffer;
  auto span = std::span(buffer.data(),
                        std::min(buffer.size(), messages::DATAMSG_MAXLEN));

  sender auto sendmsg =
      io::sendmsg(socket, socket_message{.address = {key}, .buffers = span},
                  0) |
      then([](auto &&) {}) | upon_error([](auto &&) {});

  ctx.scope.spawn(std::move(sendmsg));
}

// NOLINTBEGIN(cppcoreguidelines-pro-*)
auto server::send_next(async_context &ctx, const socket_dialog &socket,
                       iterator siter) -> void
{
  using enum messages::opcode_t;
  using enum messages::mode_t;
  using enum messages::error_t;
  using namespace std::chrono;

  auto &[key, session] = *siter;
  auto &timer = session.state.timer;
  auto &mode = session.state.mode;
  auto &block_num = session.state.block_num;
  auto &buffer = session.state.buffer;
  auto &file = session.state.file;
  auto &[start_time, avg_rtt] = session.state.statistics;

  // Reset the timer and prepare the next block.
  timer = ctx.timers.remove(timer);
  block_num += 1;

  // Size the buffer for a complete TFTP DATA message and enough
  // extra to handle netascii processing.
  buffer.reserve(messages::DATAMSG_MAXLEN + messages::DATALEN);
  if (buffer.size() < sizeof(messages::data))
    buffer.resize(sizeof(messages::data));

  auto put = buffer.begin();

  // Set the message headers.
  auto *msg = reinterpret_cast<messages::data *>(std::addressof(*put));
  msg->opc = htons(DATA);
  msg->block_num = htons(block_num);

  put += sizeof(*msg);

  if (buffer.size() > messages::DATAMSG_MAXLEN)
    put = std::copy(put + messages::DATALEN, buffer.end(), put);

  buffer.erase(put, buffer.end());

  // Read the next file chunk into the RRQ buffer.
  auto buf = std::array<char, messages::DATALEN>();
  auto len = file->readsome(buf.data(), buf.size());
  if (file->fail()) [[unlikely]]
    return error(ctx, socket, siter, ACCESS_VIOLATION); // GCOVR_EXCL_LINE

  insert_data(buffer, std::span(buf.data(), len), mode);

  send(ctx, socket, siter);

  // Update statistics and set a new timer.
  auto now = session::clock::now();
  avg_rtt = clamped_exp_weighted_average(
      duration_cast<milliseconds>(now - start_time), avg_rtt);

  start_time = now;

  timer = ctx.timers.add(
      2 * avg_rtt,
      [&, siter, retries = 0](auto timer_id) mutable {
        constexpr auto MAX_RETRIES = 5;
        if (retries++ >= MAX_RETRIES)
          return error(ctx, socket, siter, TIMED_OUT);

        send(ctx, socket, siter);
      },
      2 * avg_rtt);
}
// NOLINTEND(cppcoreguidelines-pro-*)

auto server::wrq(async_context &ctx, const socket_dialog &socket,
                 const std::shared_ptr<read_context> &rctx,
                 std::span<const std::byte> buf, iterator siter) -> void
{
  using enum messages::opcode_t;
  using enum messages::error_t;
  using namespace detail;
  auto addrbuf = std::array<char, INET6_ADDRSTRLEN + ADDR_BUFLEN>{};

  auto &[key, session] = *siter;
  auto addrstr = to_str(addrbuf, key);
  auto &state = session.state;

  // Out-of-the-blue packet, already a session running on this socket.
  if (state.opc != 0)
    return reader(ctx, socket, rctx);

  spdlog::info("New WRQ from {}.", addrstr);

  auto request = parse_request(buf);
  if (!request)
  {
    spdlog::error("Invalid WRQ from {}.", addrstr);
    return error(ctx, socket, siter, NOT_DEFINED);
  }

  state.opc = request->opc;
  state.target = request->filename;
  state.mode = request->mode;

  assert(state.opc == WRQ && "Operation MUST be a write request.");
  if (state.mode == messages::MAIL)
  {
    state.target =
        filesystem::mail_directory() / state.target /
        std::format("{:%Y%m%d_%H%M%S}", std::chrono::system_clock::now());
  }

  auto err = std::error_code();
  state.file = filesystem::tmpfile_from(
      state.target, std::ios::out | std::ios::binary | std::ios::trunc,
      state.tmp, err);
  if (!state.file)
  {
    spdlog::error(
        "WRQ error from {}. Unable to open temporary file with error: {}",
        addrstr, err.message());
    if (state.mode == messages::MAIL &&
        err == std::errc::no_such_file_or_directory)
      return error(ctx, socket, siter, NO_SUCH_USER);

    return error(ctx, socket, siter, ACCESS_VIOLATION);
  }

  state.socket = static_cast<session::socket_type>(*socket.socket);

  get_next(ctx, socket, siter);
  reader(ctx, socket, rctx);
}

auto server::data(async_context &ctx, const socket_dialog &socket,
                  const std::shared_ptr<read_context> &rctx,
                  std::span<const std::byte> buf, iterator siter) -> void
{
  using enum messages::opcode_t;
  using enum messages::error_t;
  auto addrbuf = std::array<char, INET6_ADDRSTRLEN + ADDR_BUFLEN>{};

  auto &[key, session] = *siter;
  auto addrstr = to_str(addrbuf, key);
  auto &opc = session.state.opc;
  auto &block_num = session.state.block_num;
  auto &target = session.state.target;
  auto &tmp = session.state.tmp;

  if (opc == WRQ)
  {
    const auto *msg = reinterpret_cast<const messages::data *>(buf.data());

    // Re-ACK duplicate packets.
    if (ntohs(msg->block_num) == block_num)
      ack(ctx, socket, siter);

    if (ntohs(msg->block_num) != block_num + 1)
      return reader(ctx, socket, rctx);

    const char *data = reinterpret_cast<const char *>(msg) + sizeof(*msg);
    block_num++;

    // Write the data to the file.
    auto &file = session.state.file;
    file->write(data, static_cast<std::streamsize>(buf.size()));
    if (file->fail())
    {                                                   // GCOVR_EXCL_LINE
      spdlog::error("Invalid DATA from {}, {} failed.", // GCOVR_EXCL_LINE
                    addrstr,                            // GCOVR_EXCL_LINE
                    tmp.c_str());                       // GCOVR_EXCL_LINE
      return error(ctx, socket, siter, DISK_FULL);      // GCOVR_EXCL_LINE
    }

    // File writing is complete.
    if (buf.size() < messages::DATAMSG_MAXLEN)
    {
      file->close();
      std::error_code err;
      std::filesystem::rename(tmp, target, err);
      if (err != std::errc{}) [[unlikely]]
      {
        spdlog::error("WRQ failed for {} with error: {}.",  // GCOVR_EXCL_LINE
                      addrstr,                              // GCOVR_EXCL_LINE
                      err.message());                       // GCOVR_EXCL_LINE
        return error(ctx, socket, siter, ACCESS_VIOLATION); // GCOVR_EXCL_LINE
      }
    }

    get_next(ctx, socket, siter);
    return reader(ctx, socket, rctx);
  }

  spdlog::error("Data from {} with unknown TID.", addrstr);
  error(ctx, socket, siter, UNKNOWN_TID);
}

/** @brief Acks the current block of data to the client.. */
auto server::ack(async_context &ctx, const socket_dialog &socket,
                 iterator siter) -> void
{
  using enum messages::opcode_t;
  using namespace stdexec;
  using socket_message = io::socket::socket_message<sockaddr_in6>;

  auto &[key, session] = *siter;
  auto &buffer = session.state.buffer;
  auto &block_num = session.state.block_num;

  buffer.resize(sizeof(messages::ack));

  auto *ack = reinterpret_cast<messages::ack *>(buffer.data());
  ack->opc = htons(ACK);
  ack->block_num = htons(block_num);

  sender auto sendmsg =
      io::sendmsg(socket, socket_message{.address = {key}, .buffers = buffer},
                  0) |
      then([](auto &&) {}) | upon_error([](auto &&) {});

  ctx.scope.spawn(std::move(sendmsg));
}

/** @brief Prepares to receive the next block of a file from the client. */
auto server::get_next(async_context &ctx, const socket_dialog &socket,
                      iterator siter) -> void
{
  using enum messages::error_t;
  using namespace std::chrono;

  auto &[key, session] = *siter;
  auto &timer = session.state.timer;
  auto &[start_time, avg_rtt] = session.state.statistics;

  timer = ctx.timers.remove(timer);

  ack(ctx, socket, siter);

  // Update statistics.
  auto now = session::clock::now();
  avg_rtt = clamped_exp_weighted_average(
      duration_cast<milliseconds>(now - start_time), avg_rtt);
  start_time = now;

  // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
  timer = ctx.timers.add(
      5 * avg_rtt,
      [&, siter](auto) mutable { return error(ctx, socket, siter, TIMED_OUT); },
      5 * avg_rtt);
  // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
}

auto server::cleanup(async_context &ctx, const socket_dialog &socket,
                     iterator siter) -> void
{
  std::error_code err;
  auto &[key, session] = *siter;
  auto &timer = session.state.timer;
  auto &file = session.state.file;
  auto &tmp = session.state.tmp;

  // Delete any associated timers.
  timer = ctx.timers.remove(timer);

  // Close the file if it is open.
  file.reset();

  // Delete any temporary files.
  if (!tmp.empty() && !remove(tmp, err)) [[unlikely]]
  {
    spdlog::warn(                                            // GCOVR_EXCL_LINE
        "Failed to delete temporary file {} with error: {}", // GCOVR_EXCL_LINE
        tmp.c_str(), err.message());                         // GCOVR_EXCL_LINE
  }

  // Shutdown the read-side of the socket.
  // This removes the socket from the underlying event-loop if
  // we have reached here due to a timeout.
  io::shutdown(socket, SHUT_RD);

  // Cleanup the rest of the session.
  sessions_.erase(siter);
}

auto server::service(async_context &ctx, const socket_dialog &socket,
                     const std::shared_ptr<read_context> &rctx,
                     std::span<const std::byte> buf, iterator siter) -> void
{
  using enum messages::opcode_t;
  using enum messages::error_t;

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  const auto *opcode = reinterpret_cast<const messages::opcode_t *>(buf.data());
  switch (ntohs(*opcode))
  {
    case RRQ:
      return rrq(ctx, socket, rctx, buf, siter);

    case ACK:
      return ack(ctx, socket, rctx, buf, siter);

    case WRQ:
      return wrq(ctx, socket, rctx, buf, siter);

    case DATA:
      return data(ctx, socket, rctx, buf, siter);

    default:
      return error(ctx, socket, siter, ILLEGAL_OPERATION);
  }
}

auto server::operator()(async_context &ctx, const socket_dialog &socket,
                        const std::shared_ptr<read_context> &rctx,
                        std::span<const std::byte> buf) -> void
{
  using enum messages::opcode_t;
  using enum messages::error_t;
  using namespace io::socket;
  if (!rctx)
    return;

  auto address = *rctx->msg.address;
  if (address->sin6_family == AF_INET)
  {
    address = socket_address(
        reinterpret_cast<sockaddr_in *>(std::ranges::data(address)));
  }

  auto [siter, last] = sessions_.equal_range(address);
  for (; siter != last; ++siter)
  {
    auto &[key, session] = *siter;
    if (session.state.socket == socket)
      return service(ctx, socket, rctx, buf, siter);
  }

  siter = sessions_.emplace(address, session());
  service(ctx, ctx.poller.emplace(address->sin6_family, SOCK_DGRAM, 0),
          std::make_shared<read_context>(), buf, siter);
  reader(ctx, socket, rctx);
}
#endif // TFTP_SERVER_STATIC_TEST
} // namespace tftp

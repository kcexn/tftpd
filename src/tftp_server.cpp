/* Copyright (C) 2025 Kevin Exton (kevin.exton@pm.me)
 *
 * Echo is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Echo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Echo.  If not, see <https://www.gnu.org/licenses/>.
 */
/**
 * @file tftp_server.cpp
 * @brief This file defines the TFTP server.
 */
#include "tftp/tftp_server.hpp"
#include "tftp/detail/endianness.hpp"
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

/** @brief Converts the socket address to a string in the buffer provided by
 * buf. */
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

/** @brief Converts valid C strings to spans. */
static inline auto to_view(const char *str,
                           std::size_t maxlen) -> std::string_view
{
  const auto len = strnlen(str, maxlen);
  if (len == maxlen)
    return {};

  return str;
}

/** @brief Converts a span to a TFTP mode. */
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
static inline auto
parse_request(std::span<const std::byte> msg) -> std::optional<session::state_t>
{
  using enum messages::error_t;
  using enum messages::opcode_t;
  using detail::ntohs;

  auto state = session::state_t();
  const auto *buf = msg.data();
  const auto *end = msg.data() + msg.size();

  const auto *opcode = reinterpret_cast<const messages::opcode_t *>(buf);
  state.opc = static_cast<messages::opcode_t>(ntohs(*opcode));
  if (state.opc != RRQ && state.opc != WRQ)
    return std::nullopt;

  buf += sizeof(messages::opcode_t);

  auto filepath = to_view(reinterpret_cast<const char *>(buf), end - buf);
  if (filepath.empty())
    return std::nullopt;

  state.target = filepath;
  buf += filepath.size() + 1;

  auto mode = to_view(reinterpret_cast<const char *>(buf), end - buf);
  if (mode.empty())
    return std::nullopt;

  state.mode = to_mode(mode);
  if (state.mode == 0)
    return std::nullopt;

  return state;
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
static constexpr auto CLAMP_MIN_DEFAULT = milliseconds(5);
static constexpr auto CLAMP_MAX_DEFAULT = milliseconds(500);
static inline auto clamped_exp_weighted_average(
    milliseconds curr, milliseconds prev, milliseconds min = CLAMP_MIN_DEFAULT,
    milliseconds max = CLAMP_MAX_DEFAULT) -> milliseconds
{
  auto avg = prev * 3 / 4 + curr / 4;
  avg = std::min(avg, max);
  avg = std::max(avg, min);
  return avg;
}
// NOLINTEND(bugprone-easily-swappable-parameters)

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

  if (error != UNKNOWN_TID)
    cleanup(ctx, socket, siter);
}

auto server::ack(async_context &ctx, const socket_dialog &socket,
                 const std::shared_ptr<read_context> &rctx,
                 std::span<const std::byte> msg, iterator siter) -> void
{
  using enum messages::opcode_t;
  using enum messages::error_t;
  auto addrbuf = std::array<char, INET6_ADDRSTRLEN + ADDR_BUFLEN>{};

  if (socket == server_socket_)
  {
    error(ctx, socket, siter, UNKNOWN_TID);
    return reader(ctx, socket, rctx);
  }

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

  error(ctx, socket, siter, ILLEGAL_OPERATION);
}

auto server::rrq(async_context &ctx, const socket_dialog &socket,
                 const std::shared_ptr<read_context> &rctx,
                 std::span<const std::byte> buf, iterator siter) -> void
{
  using enum messages::opcode_t;
  using enum messages::error_t;
  auto addrbuf = std::array<char, INET6_ADDRSTRLEN + ADDR_BUFLEN>{};

  auto &[key, session] = *siter;
  auto addrstr = to_str(addrbuf, key);

  if (session.state.opc != 0)
  {
    spdlog::debug("Duplicate RRQ from {}.", addrstr);
    return reader(ctx, socket, rctx);
  }

  spdlog::info("New RRQ from {}.", addrstr);

  auto state_ = parse_request(buf);
  if (!state_)
  {
    spdlog::error("Invalid RRQ from {}.", addrstr);
    return error(ctx, socket, siter, NOT_DEFINED);
  }

  auto &state = session.state = *state_;
  assert(state.opc == RRQ && "Operation MUST be a read request.");
  if (state.mode == messages::MAIL)
  {
    spdlog::error("Invalid RRQ from {}. Mail mode is not allowed.", addrstr);
    return error(ctx, socket, siter, ILLEGAL_OPERATION);
  }

  // Copy to a temporary file.
  auto tmp = (std::filesystem::temp_directory_path() / "tftp.")
                 .concat(std::format("{:05d}", count_++));

  auto err = std::error_code();
  if (!std::filesystem::copy_file(state.target, tmp, err))
  {
    spdlog::error("Copying {} to {} failed with error: {}",
                  state.target.c_str(), tmp.c_str(), err.message());
    if (err == std::errc::no_such_file_or_directory)
      return error(ctx, socket, siter, FILE_NOT_FOUND);

    return error(ctx, socket, siter, ACCESS_VIOLATION);
  }

  // Open the temporary file for sending.
  state.tmp = tmp;
  state.file = std::make_shared<std::fstream>(state.tmp,
                                              std::ios::in | std::ios::binary);
  if (!state.file->is_open()) [[unlikely]]
  {
    spdlog::error("Unable to open the temporary file: {}", // GCOVR_EXCL_LINE
                  state.tmp.c_str());                      // GCOVR_EXCL_LINE
    return error(ctx, socket, siter, ACCESS_VIOLATION);    // GCOVR_EXCL_LINE
  }

  // Start sending the file.
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

  // Size the buffer for a complete TFTP DATA message.
  buffer.reserve(messages::DATAMSG_MAXLEN);
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

  // This isn't thread-safe.
  timer = ctx.timers.add(
      2 * avg_rtt,
      [&, siter, retries = 0](auto) mutable {
        constexpr auto MAX_RETRIES = 5;
        if (retries++ >= MAX_RETRIES)
          return error(ctx, socket, siter, TIMED_OUT);

        send(ctx, socket, siter);
      },
      2 * avg_rtt);
}
// NOLINTEND(cppcoreguidelines-pro-*)

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

auto server::initialize(const socket_handle &socket) noexcept -> std::error_code
{
  server_socket_ = static_cast<socket_type>(socket);
  return {};
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

  auto [siter, emplaced] = sessions_.try_emplace(address);
  if (emplaced)
  {
    service(ctx, ctx.poller.emplace(address->sin6_family, SOCK_DGRAM, 0),
            std::make_shared<read_context>(), buf, siter);
    reader(ctx, socket, rctx);
  }
  else
  {
    service(ctx, socket, rctx, buf, siter);
  }
}
#endif // TFTP_SERVER_STATIC_TEST
} // namespace tftp

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
#include "tftp/protocol/tftp_protocol.hpp"

#include <net/timers/timers.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <filesystem>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
namespace tftp {
/** @brief Additional buffer length for <PORT>,[],: and null.  */
static constexpr auto ADDR_BUFLEN = 9UL;
/** @brief The maximum buffer length for TFTP requests. */
static constexpr auto TFTP_RQ_BUFLEN = 512UL;
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
static inline auto to_mode(std::string_view mode) -> mode_enum
{
  using enum mode_enum;
  constexpr auto MAXLEN = sizeof("netascii");
  auto tmp = std::array<char, MAXLEN>{};
  auto len = std::min(mode.size(), tmp.size());
  // NOLINTNEXTLINE(bugprone-narrowing-conversions)
  std::transform(mode.begin(), mode.begin() + len, tmp.begin(),
                 // NOLINTNEXTLINE(readability-identifier-length)
                 [](unsigned char ch) { return std::tolower(ch); });

  if (std::strncmp(tmp.data(), "netascii", tmp.size()) == 0)
    return NETASCII;

  if (std::strncmp(tmp.data(), "octet", tmp.size()) == 0)
    return OCTET;

  if (std::strncmp(tmp.data(), "mail", tmp.size()) == 0)
    return MAIL;

  return {};
}

/** @brief Parses a TFTP RRQ and returns a session_state if valid. */
static inline auto
parse_rrq(std::span<const std::byte> buf) -> std::optional<session_state>
{
  using enum opcode_enum;

  auto state = session_state();
  state.op = RRQ;

  auto len = buf.size() - sizeof(state.op);
  auto filepath = to_view(
      // NOLINTNEXTLINE(cppcoreguidelines-*)
      reinterpret_cast<const char *>(buf.data() + sizeof(state.op)), len);
  if (filepath.empty())
    return std::nullopt;

  state.target = filepath;

  auto mode =
      to_view(filepath.data() + filepath.size() + 1, len - filepath.size() - 1);
  if (mode.empty())
    return std::nullopt;

  state.mode = to_mode(mode);
  if (state.mode == mode_enum{})
    return std::nullopt;

  return state;
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
static constexpr auto CLAMP_MIN_DEFAULT = milliseconds(20);
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

#ifndef TFTP_SERVER_STATIC_TEST
auto server::error(async_context &ctx, const socket_dialog &socket,
                   iterator siter, error_enum error) -> void
{
  using namespace stdexec;
  using enum error_enum;
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
      break;

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
                 std::span<const std::byte> buf, iterator siter) -> void
{
  using enum opcode_enum;
  using enum error_enum;
  auto addrbuf = std::array<char, INET6_ADDRSTRLEN + ADDR_BUFLEN>{};
  auto addrstr = to_str(addrbuf, *rctx->msg.address);

  auto &[key, session] = *siter;

  auto &state = session.state;
  if (state.op == RRQ)
  {
    if (state.buffer.size() < sizeof(tftp_data_msg) + TFTP_RQ_BUFLEN)
    {
      spdlog::info("RRQ for {} served to {}.", state.target.c_str(), addrstr);
      return cleanup(ctx, socket, siter);
    }

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto *ack = reinterpret_cast<const tftp_ack_msg *>(buf.data());
    if (ack->block_num == state.block_num)
      send_next(ctx, socket, siter);

    return reader(ctx, socket, rctx);
  }

  return error(ctx, socket, siter, UNKNOWN_TID);
}

auto server::rrq(async_context &ctx, const socket_dialog &socket,
                 const std::shared_ptr<read_context> &rctx,
                 std::span<const std::byte> buf, iterator siter) -> void
{
  using enum error_enum;
  auto addrbuf = std::array<char, INET6_ADDRSTRLEN + ADDR_BUFLEN>{};

  auto &[key, session] = *siter;
  auto addrstr = to_str(addrbuf, key);

  if (session.state.op != opcode_enum{})
  {
    spdlog::debug("Duplicate RRQ from {}.", addrstr); // GCOVR_EXCL_LINE
    return reader(ctx, socket, rctx);
  }

  spdlog::info("New RRQ from {}.", addrstr);

  auto state_ = parse_rrq(buf);
  if (!state_)
  {
    spdlog::error("Invalid RRQ from {}.", addrstr);
    return error(ctx, socket, siter, NOT_DEFINED);
  }

  auto &state = session.state = *state_;

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
  state.file = std::make_shared<std::fstream>(state.tmp, std::ios::in);
  if (!state.file->is_open()) [[unlikely]]
  {
    spdlog::error("Unable to open the temporary file: {}", // GCOVR_EXCL_LINE
                  state.tmp.c_str());                      // GCOVR_EXCL_LINE
    return error(ctx, socket, siter, ACCESS_VIOLATION);    // GCOVR_EXCL_LINE
  }

  // Start sending the file.
  send_next(ctx, socket, siter);
}

auto server::send(async_context &ctx, const socket_dialog &socket,
                  iterator siter) -> void
{
  using namespace stdexec;
  auto &[key, session] = *siter;
  auto &buffer = session.state.buffer;

  sender auto sendmsg =
      io::sendmsg(socket, socket_message{.address = {key}, .buffers = buffer},
                  0) |
      then([](auto &&) {}) | upon_error([](auto &&) {});

  ctx.scope.spawn(std::move(sendmsg));
}

// NOLINTBEGIN(cppcoreguidelines-pro-*)
auto server::send_next(async_context &ctx, const socket_dialog &socket,
                       iterator siter) -> void
{
  using enum opcode_enum;
  using enum error_enum;
  using namespace std::chrono;
  using clock_type = session_state::clock_type;

  auto &[key, session] = *siter;
  auto &timer = session.state.timer;
  auto &block_num = session.state.block_num;
  auto &buffer = session.state.buffer;
  auto &file = session.state.file;
  auto &[start_time, avg_rtt] = session.state.statistics;

  // Reset the timer and prepare the next block.
  ctx.timers.remove(timer);
  block_num += 1;

  // Size the buffer for a complete TFTP DATA message.
  buffer.resize(sizeof(tftp_data_msg) + TFTP_RQ_BUFLEN);

  // Set the message headers.
  auto *msg = reinterpret_cast<tftp_data_msg *>(buffer.data());
  msg->opcode = DATA;
  msg->block_num = block_num;

  // Read the next file chunk into the RRQ buffer.
  auto len = file->readsome(buffer.data() + sizeof(*msg), TFTP_RQ_BUFLEN);
  if (file->fail()) [[unlikely]]
    return error(ctx, socket, siter, ACCESS_VIOLATION); // GCOVR_EXCL_LINE

  // Size the buffer down if we are EOF.
  buffer.resize(sizeof(*msg) + len);

  send(ctx, socket, siter);

  // Update statistics and set a new timer.
  auto now = clock_type::now();
  avg_rtt = clamped_exp_weighted_average(
      duration_cast<milliseconds>(now - start_time), avg_rtt);

  start_time = now;
  timer = ctx.timers.add(
      milliseconds(2 * avg_rtt),
      [&, socket, siter, retries = 0](net::timers::timer_id tid) mutable {
        constexpr auto MAX_RETRIES = 5;
        if (retries++ >= MAX_RETRIES)
          return error(ctx, socket, siter, TIMED_OUT);

        send(ctx, socket, siter);
      },
      milliseconds(2 * avg_rtt));
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
  ctx.timers.remove(timer);

  // Close the file if it is open.
  file.reset();

  // Delete any temporary files.
  if (!tmp.empty() && !remove(tmp, err)) [[unlikely]]
  {
    spdlog::warn(                                            // GCOVR_EXCL_LINE
        "Failed to delete temporary file {} with error: {}", // GCOVR_EXCL_LINE
        tmp.c_str(), err.message());                         // GCOVR_EXCL_LINE
  }

  // Cleanup the rest of the session.
  sessions_.erase(siter);
}

auto server::service(async_context &ctx, const socket_dialog &socket,
                     const std::shared_ptr<read_context> &rctx,
                     std::span<const std::byte> buf, iterator siter) -> void
{
  using enum opcode_enum;
  using enum error_enum;

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  const auto *msg = reinterpret_cast<const tftp_msg *>(buf.data());
  switch (msg->opcode)
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
  using namespace io::socket;
  if (!rctx)
    return;

  auto [siter, emplaced] = sessions_.try_emplace(*rctx->msg.address);
  auto &[key, session] = *siter;

  if (emplaced)
  {
    auto address = key;
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

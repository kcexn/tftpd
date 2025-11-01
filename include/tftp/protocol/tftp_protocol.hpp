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
 * @file tftp_protocol.hpp
 * @brief This file declares utilities for the TFTP protocol.
 */
#pragma once
#ifndef TFTP_PROTOCOL_HPP
#define TFTP_PROTOCOL_HPP
#include <array>
#include <bit>
#include <cassert>
#include <cstdint>
/** @brief TFTP related utilities. */
namespace tftp {
/** @brief The TFTP opcodes. */
// NOLINTNEXTLINE(performance-enum-size)
enum struct opcode_enum : std::uint16_t { RRQ = 1, WRQ, DATA, ACK, ERROR };
/** @brief The TFTP protocol modes. */
enum struct mode_enum : std::uint8_t { NETASCII = 1, OCTET, MAIL };
/** @brief The TFTP error codes. */
// NOLINTNEXTLINE(performance-enum-size)
enum struct error_enum : std::uint16_t {
  NOT_DEFINED = 0,
  FILE_NOT_FOUND,
  ACCESS_VIOLATION,
  DISK_FULL,
  ILLEGAL_OPERATION,
  UNKNOWN_TID,
  FILE_ALREADY_EXISTS,
  NO_SUCH_USER
};

/** @brief A generic tftp message type. */
struct tftp_msg {
  opcode_enum opcode = {};
};

/** @brief A tftp error message. */
struct tftp_error_msg {
  opcode_enum opcode = {};
  error_enum error = {};
};

/** @brief A tftp data message. */
struct tftp_data_msg {
  opcode_enum opcode = {};
  std::uint16_t block_num = 0;
};

/** @brief A tftp ack message. */
struct tftp_ack_msg {
  opcode_enum opcode = {};
  std::uint16_t block_num = 0;
};

/** @brief Error messages. */
struct errors {
  /** @brief Constructs a tftp message from an error number and a string. */
  // NOLINTBEGIN
  template <std::size_t N>
  static constexpr auto errmsg(const error_enum error,
                               const char (&str)[N]) noexcept -> decltype(auto)
  {
    using enum opcode_enum;

    constexpr auto bufsize = sizeof(tftp_error_msg) + N;
    const auto msg = tftp_error_msg{.opcode = ERROR, .error = error};
    const auto msg_bytes =
        std::bit_cast<std::array<char, sizeof(tftp_error_msg)>>(msg);

    std::array<char, bufsize> buf{};

    // Copy header bytes
    for (std::size_t i = 0; i < sizeof(tftp_error_msg); ++i)
    {
      buf[i] = msg_bytes[i];
    }

    // Copy error message string (including null terminator)
    for (std::size_t i = 0; i < N; ++i)
    {
      buf[sizeof(tftp_error_msg) + i] = str[i];
    }

    return buf;
  }
  // NOLINTEND

  static auto not_implemented() noexcept -> decltype(auto)
  {
    using enum error_enum;
    static const auto msg = errmsg(NOT_DEFINED, "Not Implemented.");
    return static_cast<const decltype(msg) &>(msg);
  }

  static auto access_violation() noexcept -> decltype(auto)
  {
    using enum error_enum;
    static const auto msg = errmsg(ACCESS_VIOLATION, "Access violation.");
    return static_cast<const decltype(msg) &>(msg);
  }

  static auto file_not_found() noexcept -> decltype(auto)
  {
    using enum error_enum;
    static const auto msg = errmsg(FILE_NOT_FOUND, "File not found.");
    return static_cast<const decltype(msg) &>(msg);
  }

  static auto disk_full() noexcept -> decltype(auto)
  {
    using enum error_enum;
    static const auto msg = errmsg(DISK_FULL, "No disk space available.");
    return static_cast<const decltype(msg) &>(msg);
  }

  static auto unknown_tid() noexcept -> decltype(auto)
  {
    using enum error_enum;
    static const auto msg = errmsg(UNKNOWN_TID, "Unknown TID.");
    return static_cast<const decltype(msg) &>(msg);
  }
};

} // namespace tftp
#endif // TFTP_PROTOCOL_HPP

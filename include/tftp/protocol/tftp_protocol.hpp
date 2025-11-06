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
#include "tftp/detail/endian.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
/** @brief TFTP related utilities. */
namespace tftp {
// NOLINTBEGIN(performance-enum-size)
/** @brief A struct to contain TFTP message marshalling logic. */
struct messages {
  /** @brief Protocol defined operations. */
  enum opcode_t : std::uint16_t { RRQ = 1, WRQ, DATA, ACK, ERROR };
  /** @brief Protocol defined modes. */
  enum mode_t : std::uint8_t { NETASCII = 1, OCTET, MAIL };
  /** @brief Protocol errors. */
  enum error_t : std::uint16_t {
    NOT_DEFINED = 0,
    FILE_NOT_FOUND,
    ACCESS_VIOLATION,
    DISK_FULL,
    ILLEGAL_OPERATION,
    UNKNOWN_TID,
    FILE_ALREADY_EXISTS,
    NO_SUCH_USER,
    // Errors below this point are all ALIASES to NOT_DEFINED.
    TIMED_OUT
  };

  /** @brief Read and write request messages. */
  struct request {
    uint16_t opc;
    const char *filename;
    uint16_t mode;
  };

  /** @brief error message. */
  struct error {
    uint16_t opc;
    uint16_t error;
  };

  /** @brief Data/ack message. */
  struct data {
    uint16_t opc;
    uint16_t block_num;
  };
  /** @brief Ack message. */
  using ack = data;

  /** @brief The maximum data size of a message. */
  static constexpr auto DATALEN = 512UL;
  /** @brief The maximum frame size for a data message. */
  static constexpr auto DATAMSG_MAXLEN = sizeof(data) + DATALEN;
};
// NOLINTEND(performance-enum-size)

/** @brief Error messages. */
struct errors {
  // NOLINTBEGIN
  /**
   * @brief Constructs a tftp message from an error number and a string.
   * @tparam N The length of the string (including the null byte).
   * @param error The error code.
   * @param str The error message.
   * @returns A byte array containing the error message with all fields in
   * network byte order.
   */
  template <std::size_t N>
  static constexpr auto msg(const messages::error_t error,
                            const char (&str)[N]) noexcept
  {
    using enum messages::opcode_t;
    using detail::htons_;

    constexpr auto bufsize = sizeof(messages::error) + N;
    const auto msg =
        messages::error{.opc = htons_(ERROR), .error = htons_(error)};
    const auto bytes = std::bit_cast<std::array<char, sizeof(msg)>>(msg);

    auto buf = std::array<char, bufsize>();
    auto it = buf.begin();
    for (auto ch : bytes)
    {
      *it++ = ch;
    }
    for (auto ch : str)
    {
      *it++ = ch;
    }

    return buf;
  }
  // NOLINTEND

  static auto not_implemented() noexcept -> decltype(auto)
  {
    using enum messages::error_t;
    static constexpr auto buf = msg(NOT_DEFINED, "Not implemented.");
    return static_cast<const decltype(buf) &>(buf);
  }

  static auto timed_out() noexcept -> decltype(auto)
  {
    using enum messages::error_t;
    static constexpr auto buf = msg(NOT_DEFINED, "Timed Out");
    return static_cast<const decltype(buf) &>(buf);
  }

  static auto access_violation() noexcept -> decltype(auto)
  {
    using enum messages::error_t;
    static constexpr auto buf = msg(ACCESS_VIOLATION, "Access violation.");
    return static_cast<const decltype(buf) &>(buf);
  }

  static auto file_not_found() noexcept -> decltype(auto)
  {
    using enum messages::error_t;
    static constexpr auto buf = msg(FILE_NOT_FOUND, "File not found.");
    return static_cast<const decltype(buf) &>(buf);
  }

  static auto disk_full() noexcept -> decltype(auto)
  {
    using enum messages::error_t;
    static constexpr auto buf = msg(DISK_FULL, "No space available.");
    return static_cast<const decltype(buf) &>(buf);
  }

  static auto unknown_tid() noexcept -> decltype(auto)
  {
    using enum messages::error_t;
    static constexpr auto buf = msg(UNKNOWN_TID, "Unknown TID.");
    return static_cast<const decltype(buf) &>(buf);
  }

  static auto illegal_operation() noexcept -> decltype(auto)
  {
    using enum messages::error_t;
    static constexpr auto buf = msg(ILLEGAL_OPERATION, "Illegal operation.");
    return static_cast<const decltype(buf) &>(buf);
  }
};

} // namespace tftp
#endif // TFTP_PROTOCOL_HPP

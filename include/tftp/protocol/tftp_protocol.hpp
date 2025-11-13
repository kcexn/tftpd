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
#include <string_view>
/** @brief TFTP related utilities. */
namespace tftp {
// NOLINTBEGIN(performance-enum-size)
/** @brief A struct to contain TFTP message marshalling logic and protocol
 * definitions. */
struct messages {
  /**
   * @brief Protocol defined operations (opcodes).
   * These are the valid TFTP operation codes as defined in RFC 1350.
   */
  enum opcode_t : std::uint16_t { RRQ = 1, WRQ, DATA, ACK, ERROR };

  /**
   * @brief Protocol defined transfer modes.
   * These are the supported TFTP transfer modes as defined in RFC 1350.
   */
  enum mode_t : std::uint8_t { NETASCII = 1, OCTET, MAIL };

  /**
   * @brief Protocol defined error codes.
   * These are the standard TFTP error codes as defined in RFC 1350.
   */
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

  /**
   * @brief Read and write request message structure.
   * Represents the structure of RRQ and WRQ packets.
   */
  struct request {
    /** @brief Operation Code (RRQ or WRQ). */
    uint16_t opc;
    /** @brief Transfer mode (NETASCII, OCTET, or MAIL). */
    uint16_t mode;
    /** @brief Null-terminated filename. */
    const char *filename;
  };

  /**
   * @brief Error message structure.
   * Represents the structure of ERROR packets.
   */
  struct error {
    /** @brief Operation code (ERROR) */
    uint16_t opc;
    /** @brief Error code from error_t enum. */
    uint16_t error;
  };

  /**
   * @brief Data message structure.
   * Represents the structure of DATA packets.
   */
  struct data {
    /** @brief Operation code (DATA). */
    uint16_t opc;
    /** @brief Block number (starts at 1). */
    uint16_t block_num;
  };

  /**
   * @brief Acknowledgment message structure.
   * ACK packets have the same structure as DATA packets (opcode + block
   * number).
   */
  using ack = data;

  /** @brief The maximum data payload size in bytes (512 bytes per RFC 1350). */
  static constexpr auto DATALEN = 512UL;
  /** @brief The maximum total size of a DATA message (header + payload). */
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
  static constexpr auto msg(const std::uint16_t error,
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

  /**
   * @brief Converts a TFTP error to a string.
   * @param error The TFTP error.
   * @returns A string_view containing the relevant error message.
   */
  static constexpr auto errstr(std::uint16_t error) noexcept -> std::string_view
  {
    using enum messages::error_t;
    switch (error)
    {
      case ACCESS_VIOLATION:
        return "Access violation.";

      case FILE_NOT_FOUND:
        return "File not found.";

      case DISK_FULL:
        return "Disk full.";

      case NO_SUCH_USER:
        return "No such user.";

      case FILE_ALREADY_EXISTS:
        return "File already exists.";

      case UNKNOWN_TID:
        return "Unknown TID.";

      case ILLEGAL_OPERATION:
        return "Illegal operation.";

      case TIMED_OUT:
        return "Timed out.";

      default:
        return "Not defined.";
    }
  }

  /**
   * @brief Creates a "Not implemented" error packet.
   *
   * Returns a pre-formatted TFTP error packet with error code NOT_DEFINED
   * and the message "Not implemented." This is used for features or operations
   * that are not yet implemented in the TFTP server.
   *
   * @return Const reference to a static buffer containing the error packet.
   */
  static auto not_implemented() noexcept -> decltype(auto)
  {
    using enum messages::error_t;
    static constexpr auto buf = msg(NOT_DEFINED, "Not implemented.");
    return static_cast<const decltype(buf) &>(buf);
  }

  /**
   * @brief Creates a "Timed Out" error packet.
   *
   * Returns a pre-formatted TFTP error packet with error code NOT_DEFINED
   * and the message "Timed Out". This is used when a connection or operation
   * times out.
   *
   * @return Const reference to a static buffer containing the error packet.
   */
  static auto timed_out() noexcept -> decltype(auto)
  {
    using enum messages::error_t;
    static constexpr auto buf = msg(NOT_DEFINED, "Timed Out");
    return static_cast<const decltype(buf) &>(buf);
  }

  /**
   * @brief Creates an "Access violation" error packet.
   *
   * Returns a pre-formatted TFTP error packet with error code ACCESS_VIOLATION
   * and the message "Access violation." This is used when a client attempts
   * to access a file or perform an operation they don't have permission for.
   *
   * @return Const reference to a static buffer containing the error packet.
   */
  static auto access_violation() noexcept -> decltype(auto)
  {
    using enum messages::error_t;
    static constexpr auto buf = msg(ACCESS_VIOLATION, "Access violation.");
    return static_cast<const decltype(buf) &>(buf);
  }

  /**
   * @brief Creates a "File not found" error packet.
   *
   * Returns a pre-formatted TFTP error packet with error code FILE_NOT_FOUND
   * and the message "File not found." This is used when a requested file
   * does not exist on the server.
   *
   * @return Const reference to a static buffer containing the error packet.
   */
  static auto file_not_found() noexcept -> decltype(auto)
  {
    using enum messages::error_t;
    static constexpr auto buf = msg(FILE_NOT_FOUND, "File not found.");
    return static_cast<const decltype(buf) &>(buf);
  }

  /**
   * @brief Creates a "Disk full" error packet.
   *
   * Returns a pre-formatted TFTP error packet with error code DISK_FULL
   * and the message "No space available." This is used when the server's
   * disk is full or an allocation limit has been exceeded.
   *
   * @return Const reference to a static buffer containing the error packet.
   */
  static auto disk_full() noexcept -> decltype(auto) // GCOVR_EXCL_LINE
  {
    using enum messages::error_t;
    static constexpr auto buf = msg(DISK_FULL, "No space available.");
    return static_cast<const decltype(buf) &>(buf); // GCOVR_EXCL_LINE
  }

  /**
   * @brief Creates an "Unknown TID" error packet.
   *
   * Returns a pre-formatted TFTP error packet with error code UNKNOWN_TID
   * and the message "Unknown TID." This is used when a packet is received
   * from an unknown transfer ID (port number).
   *
   * @return Const reference to a static buffer containing the error packet.
   */
  static auto unknown_tid() noexcept -> decltype(auto)
  {
    using enum messages::error_t;
    static constexpr auto buf = msg(UNKNOWN_TID, "Unknown TID.");
    return static_cast<const decltype(buf) &>(buf);
  }

  /**
   * @brief Creates a "No such user" error packet.
   *
   * Returns a pre-formatted TFTP error packet with error code NO_SUCH_USER
   * and the message "No such user." This is used in MAIL mode when the
   * specified user does not exist.
   *
   * @return Const reference to a static buffer containing the error packet.
   */
  static auto no_such_user() noexcept -> decltype(auto)
  {
    using enum messages::error_t;
    static constexpr auto buf = msg(NO_SUCH_USER, "No such user.");
    return static_cast<const decltype(buf) &>(buf);
  }

  /**
   * @brief Creates an "Illegal operation" error packet.
   *
   * Returns a pre-formatted TFTP error packet with error code ILLEGAL_OPERATION
   * and the message "Illegal operation." This is used when a client attempts
   * an invalid TFTP operation.
   *
   * @return Const reference to a static buffer containing the error packet.
   */
  static auto illegal_operation() noexcept -> decltype(auto)
  {
    using enum messages::error_t;
    static constexpr auto buf = msg(ILLEGAL_OPERATION, "Illegal operation.");
    return static_cast<const decltype(buf) &>(buf);
  }
};

} // namespace tftp
#endif // TFTP_PROTOCOL_HPP

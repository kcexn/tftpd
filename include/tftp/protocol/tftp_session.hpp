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
 * @file tftp_session.hpp
 * @brief This file declares a TFTP session handle.
 */
#pragma once
#ifndef TFTP_SESSION_HPP
#define TFTP_SESSION_HPP
#include "tftp_protocol.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>
/** @brief TFTP related utilities. */
namespace tftp {

/** @brief TFTP protocol state. */
struct session_state {
  /** @brief The requested filepath. */
  std::filesystem::path target;
  /** @brief The temporary filepath. */
  std::filesystem::path tmp;
  /** @brief A write buffer. */
  std::vector<char> buffer;
  /** @brief The fstream associated with the operation. */
  std::shared_ptr<std::fstream> file;
  /** @brief The current protocol block number. */
  std::uint16_t block_num = 0;
  /** @brief The file operation. */
  opcode_enum op{};
  /** @brief The operating mode. */
  mode_enum mode{};
};

/** @brief A TFTP session consists of the protocol state and a buffer. */
struct session {
  /**
   * @brief UDP frame storage type.
   * @details This is used for copying data from
   * socket reads that set MSG_TRUNC flag. The
   * maximum size of UDP frame is 64KiB.
   */
  using frame_storage = std::vector<std::byte>;
  /** @brief Maximum UDP framesize. */
  static constexpr auto UDP_MAX_LEN = 64 * 1024UL;

  /** @brief The session state. */
  session_state state;
  /** @brief The session storage. */
  frame_storage buffer;
};

} // namespace tftp
#endif // TFTP_PROTOCOL_HPP

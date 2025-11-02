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

#include <cpptime.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>
/** @brief TFTP related utilities. */
namespace tftp {

/** @brief TFTP protocol state. */
struct session_state {
  /** @brief Clock type. */
  using clock_type = std::chrono::steady_clock;

  /** @brief The requested filepath. */
  std::filesystem::path target;
  /** @brief The temporary filepath. */
  std::filesystem::path tmp;
  /** @brief A write buffer. */
  std::vector<char> buffer;
  /** @brief The fstream associated with the operation. */
  std::shared_ptr<std::fstream> file;
  /** @brief RTT statistics. */
  struct {
    /** @brief Used to mark the start time of an interval. */
    clock_type::time_point start_time;
    /** @brief The aggregate avg round trip time. */
    std::chrono::milliseconds avg_rtt;
  } statistics;
  /** @brief A timer id associated to the TFTP session. */
  CppTime::timer_id timer;
  /** @brief The current protocol block number. */
  std::uint16_t block_num = 0;
  /** @brief The file operation. */
  opcode_enum op{};
  /** @brief The operating mode. */
  mode_enum mode{};
};

/** @brief A TFTP session consists of the protocol state and a buffer. */
struct session {
  /** @brief The session state. */
  session_state state;
};

} // namespace tftp
#endif // TFTP_PROTOCOL_HPP

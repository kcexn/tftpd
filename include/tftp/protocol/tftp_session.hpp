/* Copyright (C) 2025 Kevin Exton (kevin.exton@pm.me)
 *
 * tftpd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * tftpd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with tftpd.  If not, see <https://www.gnu.org/licenses/>.
 */
/**
 * @file tftp_session.hpp
 * @brief This file declares a TFTP session handle.
 */
#pragma once
#ifndef TFTP_SESSION_HPP
#define TFTP_SESSION_HPP
#include <net/timers/timers.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>
/** @brief TFTP related utilities. */
namespace tftp {

/** @brief A TFTP session holds all of the session state. */
struct session {
  /** @brief The session clock. */
  using clock = std::chrono::steady_clock;
  /** @brief The session timestamp. */
  using timestamp = clock::time_point;
  /** @brief the session duration. */
  using duration = std::chrono::milliseconds;
  /** @brief The session timer. */
  using timer_id = net::timers::timer_id;
  /** @brief The socket address type. */
  template <typename T> using socket_address = io::socket::socket_address<T>;
  /** @brief The invalid timer value. */
  static constexpr auto INVALID_TIMER = net::timers::INVALID_TIMER;
  /** @brief The native socket type. */
  using socket_type = io::socket::native_socket_type;
  /** @brief The invalid socket constant. */
  static constexpr auto INVALID_SOCKET = io::socket::INVALID_SOCKET;
  /** @brief Timeout min value. */
  static constexpr auto TIMEOUT_MIN = std::chrono::milliseconds(2);
  /** @brief Timeout max value. */
  static constexpr auto TIMEOUT_MAX = std::chrono::milliseconds(200);

  /** @brief The session state. */
  struct state_t {
    /** @brief The requested filepath. */
    std::filesystem::path target;
    /** @brief The temporary filepath. */
    std::filesystem::path tmp;
    /** @brief A write buffer. */
    std::vector<char> buffer;
    /** @brief The fstream associated with the operation. */
    std::shared_ptr<std::fstream> file;
    /** @brief RTT statistics aggregate type. */
    struct statistics_t {
      /** @brief Used to mark the start time of an interval. */
      timestamp start_time{clock::now() - TIMEOUT_MAX / 2};
      /** @brief The aggregate avg round trip time. */
      duration avg_rtt{TIMEOUT_MAX};
    };
    /** @brief RTT statistics. */
    statistics_t statistics;
    /** @brief A timer id associated to the TFTP session. */
    timer_id timer{INVALID_TIMER};
    /** @brief The local socket that the session is keyed on. */
    socket_type socket{INVALID_SOCKET};
    /** @brief The current protocol block number. */
    std::uint16_t block_num = 0;
    /** @brief The file operation. */
    std::uint16_t opc = 0;
    /** @brief The operating mode. */
    std::uint8_t mode = 0;
  };

  /** @brief The session state. */
  state_t state;
};

} // namespace tftp
#endif // TFTP_PROTOCOL_HPP

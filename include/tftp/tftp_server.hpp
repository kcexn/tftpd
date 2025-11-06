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
 * @file tftp_server.hpp
 * @brief This file declares the TFTP server.
 */
#pragma once
#ifndef TFTP_SERVER_HPP
#define TFTP_SERVER_HPP
#include "protocol/tftp_session.hpp"

#include <net/service/async_udp_service.hpp>

#include <map>
/** @namespace For top-level tftp services. */
namespace tftp {
/** @brief UDP Read Buffer Size, 4KiB. */
static constexpr auto BUFSIZE = 4 * 1024UL;
/** @brief The service type to use. */
template <typename UDPStreamHandler>
using udp_base = net::service::async_udp_service<UDPStreamHandler, BUFSIZE>;

/** @brief A TFTP server. */
class server : public udp_base<server> {
public:
  /** @brief The base class. */
  using Base = udp_base<server>;
  /** @brief The socket message type. */
  using socket_message = io::socket::socket_message<sockaddr_in6>;
  /** @brief Sessions map. */
  using sessions_t = std::multimap<socket_address<sockaddr_in6>, session>;
  /** @brief Map iterator. */
  using iterator = sessions_t::iterator;

  /**
   * @brief Constructs a TFTP server on the socket address.
   * @tparam T The type of the socket_address.
   * @param address The local IP address to bind to.
   */
  template <typename T>
  explicit server(socket_address<T> address) noexcept : Base(address)
  {}

  /**
   * @brief Services the incoming socket_message.
   * @param ctx The asynchronous context of the message.
   * @param socket The socket that the message was read from.
   * @param rctx The read context that manages the read buffer lifetime.
   * @param buf The data buffer read off the socket.
   * @param siter An iterator pointing to the demultiplexed session.
   */
  auto service(async_context &ctx, const socket_dialog &socket,
               const std::shared_ptr<read_context> &rctx,
               std::span<const std::byte> buf, iterator siter) -> void;
  /**
   * @brief Receives the bytes emitted by the service_base reader.
   * @param ctx The asynchronous context of the message.
   * @param socket The socket that the message was read from.
   * @param rctx The read context that manages the read buffer lifetime.
   * @param buf The bytes that were read from the socket.
   */
  auto operator()(async_context &ctx, const socket_dialog &socket,
                  const std::shared_ptr<read_context> &rctx,
                  std::span<const std::byte> buf) -> void;

private:
  /** @brief An atomic counter for constructing temporary files. */
  std::atomic<std::uint16_t> count_;
  /** @brief The TFTP sessions. */
  sessions_t sessions_;

  // Application Logic.
  /** @brief Sends an error notice to client and closes the connection. */
  auto error(async_context &ctx, const socket_dialog &socket, iterator siter,
             std::uint16_t error) -> void;

  /** @brief Services a read request. */
  auto rrq(async_context &ctx, const socket_dialog &socket,
           const std::shared_ptr<read_context> &rctx,
           std::span<const std::byte> buf, iterator siter) -> void;

  /** @brief Services an ack. */
  auto ack(async_context &ctx, const socket_dialog &socket,
           const std::shared_ptr<read_context> &rctx,
           std::span<const std::byte> msg, iterator siter) -> void;

  /** @brief Services a write request. */
  auto wrq(async_context &ctx, const socket_dialog &socket,
           const std::shared_ptr<read_context> &rctx,
           std::span<const std::byte> buf, iterator siter) -> void;

  /** @brief Services a data packet. */
  auto data(async_context &ctx, const socket_dialog &socket,
            const std::shared_ptr<read_context> &rctx,
            std::span<const std::byte> buf, iterator siter) -> void;

  /** @brief Cleans-up the session from the server. */
  auto cleanup(async_context &ctx, const socket_dialog &socket,
               iterator siter) -> void;

  /** @brief Sends the current block of data to the client.. */
  static auto send(async_context &ctx, const socket_dialog &socket,
                   iterator siter) -> void;

  /** @brief Prepares and sends the next block of a file to the client. */
  auto send_next(async_context &ctx, const socket_dialog &socket,
                 iterator siter) -> void;

  /** @brief Acks the current block of data to the client. */
  static auto ack(async_context &ctx, const socket_dialog &socket,
                  iterator siter) -> void;

  /** @brief Prepares to receive the next block of a file from the client. */
  auto get_next(async_context &ctx, const socket_dialog &socket,
                iterator siter) -> void;
};
} // namespace tftp
#endif // TFTP_SERVER_HPP

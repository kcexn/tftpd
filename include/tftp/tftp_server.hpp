/* Copyright (C) 2025 Kevin Exton (kevin.exton@pm.me)
 *
 * tftpd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * tftpd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with tftpd.  If not, see <https://www.gnu.org/licenses/>.
 */
/**
 * @file tftp_server.hpp
 * @brief This file declares the TFTP server.
 */
#pragma once
#ifndef TFTP_SERVER_HPP
#define TFTP_SERVER_HPP
#include "tftp.hpp"

#include <net/cppnet.hpp>
/** @namespace For top-level tftp services. */
namespace tftp {
/** @brief TFTP max buffer allocation.. */
static constexpr auto BUFSIZE = 516UL;
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

  /**
   * @brief Constructs a TFTP server on the socket address.
   * @tparam T The type of the socket_address.
   * @param address The local IP address to bind to.
   */
  template <typename T>
  explicit server(socket_address<T> address) noexcept : Base(address)
  {}

  /**
   * @brief Dispatches the TFTP message to the right handler.
   * @param ctx The asynchronous context of the message.
   * @param socket The socket that the message was read from.
   * @param rctx The read context that manages the read buffer lifetime.
   * @param buf The data buffer read off the socket.
   * @param siter An iterator pointing to the demultiplexed session.
   */
  auto tftp_route(async_context &ctx, const socket_dialog &socket,
                  const std::shared_ptr<read_context> &rctx,
                  std::span<const std::byte> buf, iterator_t siter) -> void;
  /**
   * @brief Services the TFTP messages emitted by the server.
   * @param ctx The asynchronous context of the message.
   * @param socket The socket that the message was read from.
   * @param rctx The read context that manages the read buffer lifetime.
   * @param buf The bytes that were read from the socket.
   */
  auto service(async_context &ctx, const socket_dialog &socket,
               const std::shared_ptr<read_context> &rctx,
               std::span<const std::byte> buf) -> void;

private:
  /** @brief The TFTP sessions. */
  sessions_t sessions_;

  // Application Logic.
  /**
   * @brief Sends an error notice to client and closes the connection.
   * @param ctx The asynchronous context of the message.
   * @param socket The socket that the message was read from.
   * @param siter An iterator pointing to the session.
   * @param error The TFTP error code to send.
   */
  auto error(async_context &ctx, const socket_dialog &socket, iterator_t siter,
             std::uint16_t error) -> void;

  /**
   * @brief Services a read request.
   * @param ctx The asynchronous context of the message.
   * @param socket The socket that the message was read from.
   * @param rctx The read context that manages the read buffer lifetime.
   * @param buf The data buffer containing the RRQ packet.
   * @param siter An iterator pointing to the session.
   */
  auto rrq(async_context &ctx, const socket_dialog &socket,
           const std::shared_ptr<read_context> &rctx,
           std::span<const std::byte> buf, iterator_t siter) -> void;

  /**
   * @brief Services an ack.
   * @param ctx The asynchronous context of the message.
   * @param socket The socket that the message was read from.
   * @param rctx The read context that manages the read buffer lifetime.
   * @param msg The data buffer containing the ACK packet.
   * @param siter An iterator pointing to the session.
   */
  auto ack(async_context &ctx, const socket_dialog &socket,
           const std::shared_ptr<read_context> &rctx,
           std::span<const std::byte> msg, iterator_t siter) -> void;

  /**
   * @brief Services a write request.
   * @param ctx The asynchronous context of the message.
   * @param socket The socket that the message was read from.
   * @param rctx The read context that manages the read buffer lifetime.
   * @param buf The data buffer containing the WRQ packet.
   * @param siter An iterator pointing to the session.
   */
  auto wrq(async_context &ctx, const socket_dialog &socket,
           const std::shared_ptr<read_context> &rctx,
           std::span<const std::byte> buf, iterator_t siter) -> void;

  /**
   * @brief Services a data packet.
   * @param ctx The asynchronous context of the message.
   * @param socket The socket that the message was read from.
   * @param rctx The read context that manages the read buffer lifetime.
   * @param buf The data buffer containing the DATA packet.
   * @param siter An iterator pointing to the session.
   */
  auto data(async_context &ctx, const socket_dialog &socket,
            const std::shared_ptr<read_context> &rctx,
            std::span<const std::byte> buf, iterator_t siter) -> void;

  /**
   * @brief Cleans-up the session from the server.
   * @param ctx The asynchronous context of the message.
   * @param socket The socket that the message was read from.
   * @param siter An iterator pointing to the session to clean up.
   */
  auto cleanup(async_context &ctx, const socket_dialog &socket,
               iterator_t siter) -> void;

  /**
   * @brief Sends the current block of data to the client.
   * @param ctx The asynchronous context of the message.
   * @param socket The socket to send data on.
   * @param siter An iterator pointing to the session.
   */
  static auto send_data(async_context &ctx, const socket_dialog &socket,
                        iterator_t siter) -> void;

  /**
   * @brief Acks the current block of data to the client.
   * @param ctx The asynchronous context of the message.
   * @param socket The socket to send the ACK on.
   * @param siter An iterator pointing to the session.
   */
  static auto send_ack(async_context &ctx, const socket_dialog &socket,
                       iterator_t siter) -> void;
};
} // namespace tftp
#endif // TFTP_SERVER_HPP

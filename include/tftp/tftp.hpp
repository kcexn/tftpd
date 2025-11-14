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
 * @file tftp.hpp
 * @brief This file declares TFTP application logic.
 */
#pragma once
#ifndef TFTP_HPP
#define TFTP_HPP
#include "protocol/tftp_protocol.hpp"
#include "protocol/tftp_session.hpp"

#include <map>
/** @namespace For top-level tftp services. */
namespace tftp {
/** @brief The TFTP sessions container. */
using sessions_t =
    std::multimap<io::socket::socket_address<sockaddr_in6>, session>;
/** @brief The TFTP sessions iterator. */
using iterator_t = sessions_t::iterator;

/**
 * @brief Processes a request.
 * @param req The TFTP request to process.
 * @param siter An iterator pointing to the session.
 * @returns 0 if successful, a non-zero TFTP error otherwise.
 */
auto handle_request(messages::request req, iterator_t siter) -> std::uint16_t;

/**
 * @brief Processes an ack message.
 * @param ack The TFTP ack to process.
 * @param siter An iterator pointing to the session.
 * @returns 0 if successful, a non-zero TFTP error otherwise.
 */
auto handle_ack(messages::ack ack, iterator_t siter) -> std::uint16_t;

/**
 * @brief Processes a data message.
 * @param data A pointer to the beginning of the TFTP data frame.
 * @param len The length of the data frame including the TFTP header.
 * @param siter An iterator pointing to the session.
 * @returns 0 if successful, a non-zero TFTP error otherwise.
 */
auto handle_data(const messages::data *data, std::size_t len,
                 iterator_t siter) -> std::uint16_t;
} // namespace tftp
#endif // TFTP_HPP

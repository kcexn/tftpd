/* Copyright (C) 2025 Kevin Exton (kevin.exton@pm.me)
 *
 * tftp-server is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * tftp-server is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with tftp-server.  If not, see <https://www.gnu.org/licenses/>.
 */
/**
 * @file endianness.hpp
 * @brief This file defines constexpr host to network byte-order conversions.
 */
#pragma once
#ifndef TFTP_ENDIANNESS_HPP
#define TFTP_ENDIANNESS_HPP
#include <bit>
#include <cstdint>
/** @brief Defines internal tftp implementation details. */
namespace tftp::detail {
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
/**
 * @brief Converts a 16-bit unsigned integer from host to network byte order.
 * @param hostshort The 16-bit unsigned integer in host byte order.
 * @returns The 16-bit unsigned integer in network byte order.
 */
constexpr auto htons_(const std::uint16_t hostshort) noexcept -> std::uint16_t
{
  if constexpr (std::endian::native == std::endian::little)
  {
    return static_cast<std::uint16_t>((hostshort << 8) | (hostshort >> 8));
  }
  return hostshort;
}

/**
 * @brief Converts a 32-bit unsigned integer from host to network byte order.
 * @param hostlong The 32-bit unsigned integer in host byte order.
 * @returns The 32-bit unsigned integer in network byte order.
 */
constexpr auto htonl_(const std::uint32_t hostlong) noexcept -> std::uint32_t
{
  if constexpr (std::endian::native == std::endian::little)
  {
    return (hostlong << 24) | ((hostlong << 8) & 0x00FF0000) |
           ((hostlong >> 8) & 0x0000FF00) | (hostlong >> 24);
  }
  return hostlong;
}

/**
 * @brief Converts a 64-bit unsigned integer from host to network byte order.
 * @param hostlonglong The 64-bit unsigned integer in host byte order.
 * @returns The 64-bit unsigned integer in network byte order.
 */
constexpr auto
htonll_(const std::uint64_t hostlonglong) noexcept -> std::uint64_t
{
  if constexpr (std::endian::native == std::endian::little)
  {
    return (hostlonglong << 56) | ((hostlonglong << 40) & 0x00FF000000000000) |
           ((hostlonglong << 24) & 0x0000FF0000000000) |
           ((hostlonglong << 8) & 0x000000FF00000000) |
           ((hostlonglong >> 8) & 0x00000000FF000000) |
           ((hostlonglong >> 24) & 0x0000000000FF0000) |
           ((hostlonglong >> 40) & 0x000000000000FF00) | (hostlonglong >> 56);
  }
  return hostlonglong;
}

/**
 * @brief Converts a 16-bit unsigned integer from network to host byte order.
 * @param netshort The 16-bit unsigned integer in network byte order.
 * @returns The 16-bit unsigned integer in host byte order.
 */
constexpr auto ntohs_(const std::uint16_t netshort) noexcept -> std::uint16_t
{
  if constexpr (std::endian::native == std::endian::little)
  {
    return static_cast<std::uint16_t>((netshort >> 8) | (netshort << 8));
  }
  return netshort;
}

/**
 * @brief Converts a 32-bit unsigned integer from network to host byte order.
 * @param netlong The 32-bit unsigned integer in network byte order.
 * @returns The 32-bit unsigned integer in host byte order.
 */
constexpr auto ntohl_(const std::uint32_t netlong) noexcept -> std::uint32_t
{
  if constexpr (std::endian::native == std::endian::little)
  {
    return (netlong >> 24) | ((netlong >> 8) & 0x0000FF00) |
           ((netlong << 8) & 0x00FF0000) | (netlong << 24);
  }
  return netlong;
}

/**
 * @brief Converts a 64-bit unsigned integer from network to host byte order.
 * @param netlonglong The 64-bit unsigned integer in network byte order.
 * @returns The 64-bit unsigned integer in host byte order.
 */
constexpr auto
ntohll_(const std::uint64_t netlonglong) noexcept -> std::uint64_t
{
  if constexpr (std::endian::native == std::endian::little)
  {
    return (netlonglong << 56) | ((netlonglong << 40) & 0x00FF000000000000) |
           ((netlonglong << 24) & 0x0000FF0000000000) |
           ((netlonglong << 8) & 0x000000FF00000000) |
           ((netlonglong >> 8) & 0x00000000FF000000) |
           ((netlonglong >> 24) & 0x0000000000FF0000) |
           ((netlonglong >> 40) & 0x000000000000FF00) | (netlonglong >> 56);
  }
  return netlonglong;
}
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
} // namespace tftp::detail
#endif // TFTP_ENDIANNESS_HPP

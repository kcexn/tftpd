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
 * @file endian.hpp
 * @brief This file defines constexpr host to network byte-order conversions.
 */
#pragma once
#ifndef TFTP_ENDIAN_HPP
#define TFTP_ENDIAN_HPP
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

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
} // namespace tftp::detail
#endif // TFTP_ENDIANNESS_HPP

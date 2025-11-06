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
#include "tftp/detail/endianness.hpp"

#include <gtest/gtest.h>

#include <cstdint>

using namespace tftp::detail;

struct EndiannessTest16
    : public ::testing::TestWithParam<std::pair<std::uint16_t, std::uint16_t>> {
};
struct EndiannessTest32
    : public ::testing::TestWithParam<std::pair<std::uint32_t, std::uint32_t>> {
};
struct EndiannessTest64
    : public ::testing::TestWithParam<std::pair<std::uint64_t, std::uint64_t>> {
};

TEST_P(EndiannessTest16, HostToNetworkAndBack)
{
  const auto [value, swapped] = GetParam();

  if constexpr (std::endian::native == std::endian::little)
  {
    EXPECT_EQ(htons_(value), swapped);
    EXPECT_EQ(ntohs_(swapped), value);
  }
  else
  {
    EXPECT_EQ(htons_(value), value);
    EXPECT_EQ(ntohs_(value), value);
  }
}

TEST_P(EndiannessTest32, HostToNetworkAndBack)
{
  const auto [value, swapped] = GetParam();

  if constexpr (std::endian::native == std::endian::little)
  {
    EXPECT_EQ(htonl_(value), swapped);
    EXPECT_EQ(ntohl_(swapped), value);
  }
  else
  {
    EXPECT_EQ(htonl_(value), value);
    EXPECT_EQ(ntohl_(value), value);
  }
}

TEST_P(EndiannessTest64, HostToNetworkAndBack)
{
  const auto [value, swapped] = GetParam();

  if constexpr (std::endian::native == std::endian::little)
  {
    EXPECT_EQ(htonll_(value), swapped);
    EXPECT_EQ(ntohll_(swapped), value);
  }
  else
  {
    EXPECT_EQ(htonll_(value), value);
    EXPECT_EQ(ntohll_(value), value);
  }
}

INSTANTIATE_TEST_SUITE_P(EndiannessTestCases16, EndiannessTest16,
                         ::testing::Values(std::make_pair(0x1234, 0x3412)));
INSTANTIATE_TEST_SUITE_P(EndiannessTestCases32, EndiannessTest32,
                         ::testing::Values(std::make_pair(0x12345678,
                                                          0x78563412)));
INSTANTIATE_TEST_SUITE_P(EndiannessTestCases64, EndiannessTest64,
                         ::testing::Values(std::make_pair(0x123456789ABCDEF0,
                                                          0xF0DEBC9A78563412)));

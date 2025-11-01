/* Copyright (C) 2025 Kevin Exton (kevin.exton@pm.me)
 *
 * TFTP is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * TFTP is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with TFTP.  If not, see <https://www.gnu.org/licenses/>.
 */

// NOLINTBEGIN
#ifndef TFTP_SERVER_STATIC_TEST
#define TFTP_SERVER_STATIC_TEST
#include "../src/tftp_server.cpp"

#include <gtest/gtest.h>

#include <array>

using namespace tftp;

TEST(TftpServerStaticTests, TestToStr)
{
  auto buf = std::array<char, INET6_ADDRSTRLEN + ADDR_BUFLEN>{};
  auto addr_v6 = socket_address<sockaddr_in6>{};
  addr_v6->sin6_family = AF_INET6;
  addr_v6->sin6_addr = in6addr_loopback;
  addr_v6->sin6_port = htons(8080);

  const auto *addr = reinterpret_cast<sockaddr *>(std::ranges::data(addr_v6));

  auto addrstr = to_str(buf, addr);
  EXPECT_EQ(addrstr, "[::1]:8080");

  auto addr_v4 = socket_address<sockaddr_in>{};
  addr_v4->sin_family = AF_INET;
  addr_v4->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr_v4->sin_port = htons(8080);

  addr = reinterpret_cast<sockaddr *>(std::ranges::data(addr_v4));

  ASSERT_EQ(addr->sa_family, AF_INET);

  addrstr = to_str(buf, addr);
  EXPECT_EQ(addrstr, "127.0.0.1:8080");
}

class TftpServerStaticModeTest
    : public ::testing::TestWithParam<std::pair<std::string_view, mode_enum>> {
};

TEST_P(TftpServerStaticModeTest, TestToMode)
{
  auto [str, mode] = GetParam();
  EXPECT_EQ(to_mode(str), mode);
}

INSTANTIATE_TEST_SUITE_P(
    TftpServerStaticTests, TftpServerStaticModeTest,
    ::testing::Values(std::make_pair("netascii", mode_enum::NETASCII),
                      std::make_pair("mail", mode_enum::MAIL),
                      std::make_pair("octet", mode_enum::OCTET),
                      std::make_pair("unknown", mode_enum{})));

TEST(TftpServerStaticTests, TestParseRRQ)
{
  using enum opcode_enum;

  auto rrq = std::vector<char>(sizeof(tftp_msg));
  auto *opcode = reinterpret_cast<opcode_enum *>(rrq.data());
  *opcode = RRQ;

  auto path = std::string("test.txt");
  rrq.insert(rrq.end(), path.begin(), path.end());

  ASSERT_FALSE(parse_rrq(
      std::span{reinterpret_cast<std::byte *>(rrq.data()), rrq.size()}));

  rrq.push_back('\0');
  auto mode = std::string("netascii");
  rrq.insert(rrq.end(), mode.begin(), mode.end());

  ASSERT_FALSE(parse_rrq(
      std::span{reinterpret_cast<std::byte *>(rrq.data()), rrq.size()}));

  rrq.back() = '\0';
  ASSERT_FALSE(parse_rrq(
      std::span{reinterpret_cast<std::byte *>(rrq.data()), rrq.size()}));

  rrq.back() = 'i';
  rrq.push_back('\0');
  ASSERT_TRUE(parse_rrq(
      std::span{reinterpret_cast<std::byte *>(rrq.data()), rrq.size()}));
}

#undef TFTP_SERVER_STATIC_TEST
#endif
// NOLINTEND

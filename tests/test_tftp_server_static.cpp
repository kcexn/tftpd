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
using enum messages::mode_t;
using enum messages::opcode_t;

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
    : public ::testing::TestWithParam<
          std::pair<std::string_view, std::uint8_t>> {};

TEST_P(TftpServerStaticModeTest, TestToMode)
{
  auto [str, mode] = GetParam();
  ASSERT_EQ(to_mode(str), mode);
}

INSTANTIATE_TEST_SUITE_P(
    TftpServerStaticTests, TftpServerStaticModeTest,
    ::testing::Values(std::make_pair("netascii", messages::mode_t::NETASCII),
                      std::make_pair("mail", messages::mode_t::MAIL),
                      std::make_pair("octet", messages::mode_t::OCTET),
                      std::make_pair("unknown", messages::mode_t{0})));

TEST(TftpServerStaticTests, TestParseRRQ)
{
  auto request = std::vector<char>();

  auto opc = htons(ACK);

  request.resize(sizeof(opc));
  std::memcpy(request.data(), &opc, sizeof(opc));

  ASSERT_FALSE(parse_request(std::span{
      reinterpret_cast<std::byte *>(request.data()), request.size()}));

  opc = htons(RRQ);
  std::memcpy(request.data(), &opc, sizeof(opc));

  auto path = std::string("test.txt");
  request.insert(request.end(), path.begin(), path.end());

  ASSERT_FALSE(parse_request(std::span{
      reinterpret_cast<std::byte *>(request.data()), request.size()}));

  request.push_back('\0');
  auto mode = std::string("netascii");
  request.insert(request.end(), mode.begin(), mode.end());

  ASSERT_FALSE(parse_request(std::span{
      reinterpret_cast<std::byte *>(request.data()), request.size()}));

  request.back() = '\0';
  ASSERT_FALSE(parse_request(std::span{
      reinterpret_cast<std::byte *>(request.data()), request.size()}));

  request.back() = 'i';
  request.push_back('\0');
  ASSERT_TRUE(parse_request(std::span{
      reinterpret_cast<std::byte *>(request.data()), request.size()}));
}

#undef TFTP_SERVER_STATIC_TEST
#endif
// NOLINTEND

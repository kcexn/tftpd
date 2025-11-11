/* Copyright (C) 2025 Kevin Exton (kevin.exton@pm.me)
 *
 * Cloudbus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cloudbus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Cloudbus.  If not, see <https://www.gnu.org/licenses/>.
 */
// NOLINTBEGIN
#pragma once
#ifndef TFTP_TEST_SERVER_FIXTURE_HPP
#define TFTP_TEST_SERVER_FIXTURE_HPP
#include "tftp/protocol/tftp_protocol.hpp"
#include "tftp/tftp_server.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <format>

#include <arpa/inet.h>
#include <netinet/in.h>

using namespace tftp;

static inline auto test_counter = std::atomic<std::uint16_t>();
class TftpServerTests : public ::testing::Test {
protected:
  using tftp_server = net::service::context_thread<server>;

  auto SetUp() noexcept -> void override
  {
    using enum messages::opcode_t;
    using enum net::service::async_context::context_states;

    addr_v4->sin_family = AF_INET;
    addr_v4->sin_port = htons(8080);

    test_file = (std::filesystem::temp_directory_path() / "test.")
                    .concat(std::format("{:05d}", test_counter++));
    remove(test_file);

    rrq_octet.resize(sizeof(messages::opcode_t));
    auto opc = htons(RRQ);
    std::memcpy(rrq_octet.data(), &opc, sizeof(opc));

    std::ranges::copy(std::string_view(test_file.c_str()),
                      std::back_inserter(rrq_octet));
    rrq_octet.push_back('\0');

    rrq_netascii = rrq_octet;
    rrq_mail = rrq_octet;

    std::ranges::copy("octet", std::back_inserter(rrq_octet));

    std::ranges::copy("netascii", std::back_inserter(rrq_netascii));

    std::ranges::copy("mail", std::back_inserter(rrq_mail));

    wrq_octet = rrq_octet;
    opc = htons(WRQ);
    std::memcpy(wrq_octet.data(), &opc, sizeof(opc));

    wrq_mail = rrq_mail;
    std::memcpy(wrq_mail.data(), &opc, sizeof(opc));

    wrq_no_permission.resize(sizeof(std::uint16_t));
    std::memcpy(wrq_no_permission.data(), &opc, sizeof(messages::opcode_t));

    std::ranges::copy("/root/tftp.no-permission",
                      std::back_inserter(wrq_no_permission));
    std::ranges::copy("octet", std::back_inserter(wrq_no_permission));

    ack.resize(sizeof(messages::ack));
    auto *ackmsg = reinterpret_cast<messages::ack *>(ack.data());
    ackmsg->opc = htons(ACK);
    ackmsg->block_num = htons(0);

    server_ = std::make_unique<tftp_server>();
    server_->start(addr_v4);
    server_->state.wait(PENDING);
    ASSERT_EQ(server_->state, STARTED);
  }

  auto TearDown() noexcept -> void override
  {
    using enum net::service::async_context::context_states;

    server_->signal(server_->terminate);
    server_->state.wait(STARTED);
    ASSERT_EQ(server_->state, STOPPED);
    server_.reset();

    std::filesystem::remove(test_file);
  }

  std::mutex mtx;
  std::condition_variable cvar;
  io::socket::socket_address<sockaddr_in> addr_v4;
  std::unique_ptr<tftp_server> server_;
  std::filesystem::path test_file;

  std::vector<char> rrq_octet;
  std::vector<char> rrq_netascii;
  std::vector<char> rrq_mail;
  std::vector<char> wrq_octet;
  std::vector<char> wrq_no_permission;
  std::vector<char> wrq_mail;
  std::vector<char> ack;
};

class TftpServerRRQOctetTests
    : public TftpServerTests,
      public ::testing::WithParamInterface<std::size_t> {};

class TftpServerRRQNetAsciiTests : public TftpServerTests,
                                   public ::testing::WithParamInterface<
                                       std::pair<std::string, std::string>> {};
#endif // TFTP_TEST_SERVER_FIXTURE_HPP
// NOLINTEND

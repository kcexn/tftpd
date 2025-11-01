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
#include "tftp/tftp_server.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <format>

#include <arpa/inet.h>
#include <netinet/in.h>
using namespace io::socket;
using namespace net::service;
using namespace tftp;

static std::atomic<std::uint16_t> test_counter;
class TftpServerTest : public ::testing::Test {
protected:
  using tftp_server = context_thread<server>;

  auto SetUp() noexcept -> void
  {
    using enum opcode_enum;

    addr_v4->sin_family = AF_INET;
    addr_v4->sin_port = htons(8080);

    test_file = (std::filesystem::temp_directory_path() / "test.")
                    .concat(std::format("{:05d}", test_counter++));

    rrq.resize(sizeof(opcode_enum));
    auto *rrqmsg = reinterpret_cast<tftp_msg *>(rrq.data());
    rrqmsg->opcode = RRQ;
    std::ranges::copy(std::string_view(test_file.c_str()),
                      std::back_inserter(rrq));
    rrq.push_back('\0');
    std::ranges::copy(std::string_view("netascii"), std::back_inserter(rrq));
    rrq.push_back('\0');

    ack.resize(sizeof(tftp_ack_msg));
    auto *ackmsg = reinterpret_cast<tftp_ack_msg *>(ack.data());
    ackmsg->opcode = ACK;
    ackmsg->block_num = 0;

    server_ = std::make_unique<tftp_server>();
    server_->start(mtx, cvar, addr_v4);
    {
      auto lock = std::unique_lock{mtx};
      cvar.wait(lock, [&] { return server_->interrupt || server_->stopped; });
    }
    ASSERT_FALSE(server_->stopped.load());
  }

  auto TearDown() noexcept -> void
  {
    server_->signal(server_->terminate);
    {
      auto lock = std::unique_lock{mtx};
      cvar.wait(lock, [&] { return server_->stopped.load(); });
    }
    ASSERT_TRUE(server_->stopped.load());
    server_.reset();
  }

  std::mutex mtx;
  std::condition_variable cvar;
  socket_address<sockaddr_in> addr_v4;
  std::unique_ptr<tftp_server> server_;
  std::filesystem::path test_file;

  std::vector<char> rrq;
  std::vector<char> ack;
};

TEST_F(TftpServerTest, TestFileNotFound)
{
  using namespace io::socket;

  auto sock = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");
  auto len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = rrq}, 0);
  ASSERT_EQ(len, rrq.size());

  auto recvbuf = std::vector<char>(516);
  auto sockmsg = socket_message{.address = {socket_address<sockaddr_in6>()},
                                .buffers = recvbuf};
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_EQ(std::memcmp(recvbuf.data(), errors::file_not_found().data(), len),
            0);
}

TEST_F(TftpServerTest, TestInvalidRRQ)
{
  using namespace io::socket;

  rrq.resize(rrq.size() - 3);

  auto sock = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");
  auto len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = rrq}, 0);
  ASSERT_EQ(len, rrq.size());

  auto recvbuf = std::vector<char>(516);
  auto sockmsg = socket_message{.address = {socket_address<sockaddr_in6>()},
                                .buffers = recvbuf};
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_EQ(std::memcmp(recvbuf.data(), errors::not_implemented().data(), len),
            0);
}

TEST_F(TftpServerTest, TestRRQNotPermitted)
{
  using namespace io::socket;
  using namespace std::filesystem;

  auto f = std::ofstream(test_file);
  f.close();
  permissions(test_file,
              perms::owner_read | perms::group_read | perms::others_read,
              perm_options::remove);

  auto sock = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");
  auto len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = rrq}, 0);
  ASSERT_EQ(len, rrq.size());

  auto recvbuf = std::vector<char>(516);
  auto sockmsg = socket_message{.address = {socket_address<sockaddr_in6>()},
                                .buffers = recvbuf};
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_EQ(std::memcmp(recvbuf.data(), errors::access_violation().data(), len),
            0);

  ASSERT_TRUE(remove(test_file));
}

TEST_F(TftpServerTest, TestInvalidAck)
{
  using namespace io::socket;

  auto sock = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");
  auto len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = ack}, 0);
  ASSERT_EQ(len, ack.size());

  auto recvbuf = std::vector<char>(516);
  auto sockmsg = socket_message{.address = {socket_address<sockaddr_in6>()},
                                .buffers = recvbuf};
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_EQ(std::memcmp(recvbuf.data(), errors::unknown_tid().data(), len), 0);
}

class TftpServerRRQTests : public ::testing::TestWithParam<std::size_t> {
protected:
  using tftp_server = context_thread<server>;

  auto SetUp() noexcept -> void
  {
    using enum opcode_enum;

    addr_v4->sin_family = AF_INET;
    addr_v4->sin_port = htons(8080);

    test_file = (std::filesystem::temp_directory_path() / "test.")
                    .concat(std::format("{:05d}", test_counter++));

    rrq.resize(sizeof(opcode_enum));
    auto *rrqmsg = reinterpret_cast<tftp_msg *>(rrq.data());
    rrqmsg->opcode = RRQ;
    std::ranges::copy(std::string_view(test_file.c_str()),
                      std::back_inserter(rrq));
    rrq.push_back('\0');
    std::ranges::copy(std::string_view("netascii"), std::back_inserter(rrq));
    rrq.push_back('\0');

    ack.resize(sizeof(tftp_ack_msg));
    auto *ackmsg = reinterpret_cast<tftp_ack_msg *>(ack.data());
    ackmsg->opcode = ACK;
    ackmsg->block_num = 0;

    server_ = std::make_unique<tftp_server>();
    server_->start(mtx, cvar, addr_v4);
    {
      auto lock = std::unique_lock{mtx};
      cvar.wait(lock, [&] { return server_->interrupt || server_->stopped; });
    }
    ASSERT_FALSE(server_->stopped.load());
  }

  auto TearDown() noexcept -> void
  {
    server_->signal(server_->terminate);
    {
      auto lock = std::unique_lock{mtx};
      cvar.wait(lock, [&] { return server_->stopped.load(); });
    }
    ASSERT_TRUE(server_->stopped.load());
    server_.reset();
  }

  std::mutex mtx;
  std::condition_variable cvar;
  socket_address<sockaddr_in> addr_v4;
  std::unique_ptr<tftp_server> server_;
  std::filesystem::path test_file;

  std::vector<char> rrq;
  std::vector<char> ack;
};

TEST_P(TftpServerRRQTests, TestRRQ)
{
  using namespace io::socket;
  using namespace std::filesystem;

  std::vector<char> test_data(GetParam());

  {
    auto inf = std::ifstream("/dev/random");
    auto outf = std::ofstream(test_file);
    inf.read(test_data.data(), test_data.size());
    outf.write(test_data.data(), test_data.size());
  }

  auto sock = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");
  auto len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = rrq}, 0);
  ASSERT_EQ(len, rrq.size());

  for (std::size_t i = 0; i == 0 || len == 516; ++i)
  {
    using namespace io;
    auto recvbuf = std::vector<char>(516);
    auto sockmsg = socket_message{.address = {socket_address<sockaddr_in6>()},
                                  .buffers = recvbuf};
    len = recvmsg(sock, sockmsg, 0);
    ASSERT_EQ(
        std::memcmp(recvbuf.data() + 4, test_data.data() + i * 512, len - 4),
        0);

    auto *datamsg = reinterpret_cast<tftp_data_msg *>(recvbuf.data());
    auto *ackmsg = reinterpret_cast<tftp_ack_msg *>(ack.data());
    ackmsg->block_num = datamsg->block_num;

    sendmsg(sock, socket_message{.address = {addr_v4}, .buffers = ack}, 0);
  }

  remove(test_file);
}

INSTANTIATE_TEST_SUITE_P(TftpServerParamTests, TftpServerRRQTests,
                         ::testing::Values(511, 512, 513, 1023, 1024, 1025));

// NOLINTEND

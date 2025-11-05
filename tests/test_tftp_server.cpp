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
#include "test_server_fixture.hpp"

using namespace io::socket;
using namespace net::service;

TEST_F(TftpServerTests, TestFileNotFound)
{
  using namespace io::socket;

  auto sock = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");
  auto len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = rrq_octet}, 0);
  ASSERT_EQ(len, rrq_octet.size());

  auto recvbuf = std::vector<char>(516);
  auto sockmsg = socket_message{.address = {socket_address<sockaddr_in6>()},
                                .buffers = recvbuf};
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_EQ(std::memcmp(recvbuf.data(), errors::file_not_found().data(), len),
            0);
}

TEST_F(TftpServerTests, TestInvalidRRQ)
{
  using namespace io::socket;

  rrq_octet.resize(rrq_octet.size() - 3);

  auto sock = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");
  auto len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = rrq_octet}, 0);
  ASSERT_EQ(len, rrq_octet.size());

  auto recvbuf = std::vector<char>(516);
  auto sockmsg = socket_message{.address = {socket_address<sockaddr_in6>()},
                                .buffers = recvbuf};
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_EQ(std::memcmp(recvbuf.data(), errors::not_implemented().data(), len),
            0);
}

TEST_F(TftpServerTests, TestRRQNotPermitted)
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
      sock, socket_message{.address = {addr_v4}, .buffers = rrq_octet}, 0);
  ASSERT_EQ(len, rrq_octet.size());

  auto recvbuf = std::vector<char>(516);
  auto sockmsg = socket_message{.address = {socket_address<sockaddr_in6>()},
                                .buffers = recvbuf};
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_EQ(std::memcmp(recvbuf.data(), errors::access_violation().data(), len),
            0);

  ASSERT_TRUE(remove(test_file));
}

TEST_F(TftpServerTests, TestInvalidAck)
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

TEST_F(TftpServerTests, TestRRQTimeout)
{
  using namespace io::socket;
  using namespace io;
  using namespace std::chrono;
  using clock_type = steady_clock;

  std::vector<char> test_data(5 * 512);

  {
    auto inf = std::ifstream("/dev/random");
    auto outf = std::ofstream(test_file);
    inf.read(test_data.data(), test_data.size());
    outf.write(test_data.data(), test_data.size());
  }

  auto sock = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");
  auto len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = rrq_octet}, 0);
  ASSERT_EQ(len, rrq_octet.size());

  auto recvbuf = std::vector<char>(516);
  auto sockmsg = socket_message<sockaddr_in>{
      .address = {socket_address<sockaddr_in>()}, .buffers = recvbuf};

  // ACK five times to prime statistics.
  for (int i = 0; i < 5; ++i)
  {
    len = recvmsg(sock, sockmsg, 0);
    ASSERT_EQ(
        std::memcmp(recvbuf.data() + 4, test_data.data() + i * 512, len - 4),
        0);

    auto *datamsg = reinterpret_cast<messages::data *>(recvbuf.data());
    auto *ackmsg = reinterpret_cast<messages::ack *>(ack.data());
    ackmsg->block_num = datamsg->block_num;

    sendmsg(sock, socket_message{.address = sockmsg.address, .buffers = ack},
            0);
  }

  auto start = clock_type::now();

  // Timeout after 6 attempts (1 + 5 retries).
  for (int i = 0; i < 6; i++)
  {
    len = recvmsg(sock, sockmsg, 0);
  }

  len = recvmsg(sock, sockmsg, 0);
  ASSERT_EQ(std::memcmp(recvbuf.data(), errors::timed_out().data(), len), 0);

  auto timeout = duration_cast<milliseconds>(clock_type::now() - start);
  EXPECT_GE(timeout, 240ms);
  EXPECT_LE(timeout, 1500ms);

  remove(test_file);
}

TEST_F(TftpServerTests, TestIllegalOp)
{
  using namespace io::socket;
  using namespace io;

  auto sock = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");

  ack.resize(16 * 1024);
  auto *msg = reinterpret_cast<messages::ack *>(ack.data());
  msg->opc = htons(0);

  auto len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = ack}, 0);
  ASSERT_EQ(len, ack.size());

  auto recvbuf = std::vector<char>(516);
  auto sockmsg = socket_message{.address = {socket_address<sockaddr_in6>()},
                                .buffers = recvbuf};
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_EQ(
      std::memcmp(recvbuf.data(), errors::illegal_operation().data(), len), 0);
}

TEST_F(TftpServerTests, TestMailRRQ)
{
  using namespace io::socket;
  using namespace io;

  auto sock = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");

  auto len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = rrq_mail}, 0);
  ASSERT_EQ(len, rrq_mail.size());

  auto recvbuf = std::vector<char>(516);
  auto sockmsg = socket_message{.address = {socket_address<sockaddr_in6>()},
                                .buffers = recvbuf};
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_EQ(
      std::memcmp(recvbuf.data(), errors::illegal_operation().data(), len), 0);
}

TEST_F(TftpServerTests, TestDuplicateRRQ)
{
  using namespace io::socket;
  using namespace std::filesystem;

  std::vector<char> test_data(511);

  {
    auto inf = std::ifstream("/dev/random");
    auto outf = std::ofstream(test_file);
    inf.read(test_data.data(), test_data.size());
    outf.write(test_data.data(), test_data.size());
  }

  auto sock = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");
  auto len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = rrq_octet}, 0);
  ASSERT_EQ(len, rrq_octet.size());

  len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = rrq_octet}, 0);
  ASSERT_EQ(len, rrq_octet.size());

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

    auto *datamsg = reinterpret_cast<messages::data *>(recvbuf.data());
    auto *ackmsg = reinterpret_cast<messages::ack *>(ack.data());
    ackmsg->block_num = datamsg->block_num;

    sendmsg(sock, socket_message{.address = sockmsg.address, .buffers = ack},
            0);
  }

  remove(test_file);
}

TEST_P(TftpServerRRQOctetTests, TestRRQ)
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
      sock, socket_message{.address = {addr_v4}, .buffers = rrq_octet}, 0);
  ASSERT_EQ(len, rrq_octet.size());

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

    auto *datamsg = reinterpret_cast<messages::data *>(recvbuf.data());
    auto *ackmsg = reinterpret_cast<messages::ack *>(ack.data());
    ackmsg->block_num = datamsg->block_num;

    sendmsg(sock, socket_message{.address = sockmsg.address, .buffers = ack},
            0);
  }

  remove(test_file);
}

TEST_P(TftpServerRRQNetAsciiTests, TestNetAsciiRRQ)
{
  using namespace io::socket;
  using namespace std::filesystem;

  auto [netascii_str, linux_str] = GetParam();

  {
    auto outf = std::ofstream(test_file);
    for (auto it = linux_str.begin(); it != linux_str.end(); ++it)
    {
      // Randomly insert null bytes.
      if (std::rand() < RAND_MAX / 4)
        it = linux_str.insert(it, '\0');
    }
    outf.write(linux_str.data(), linux_str.size());
  }

  auto sock = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");
  auto len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = rrq_netascii}, 0);
  ASSERT_EQ(len, rrq_netascii.size());

  for (std::size_t i = 0; i == 0 || len == 516; ++i)
  {
    using namespace io;
    auto recvbuf = std::vector<char>(516);
    auto sockmsg = socket_message{.address = {socket_address<sockaddr_in6>()},
                                  .buffers = recvbuf};
    len = recvmsg(sock, sockmsg, 0);

    auto received_str = std::string(recvbuf.begin() + sizeof(messages::data),
                                    recvbuf.begin() + len);
    auto begin = netascii_str.begin() + i * messages::DATALEN;
    auto end = begin + len - sizeof(messages::data);
    auto expected_str = std::string(begin, end);

    ASSERT_EQ(received_str, expected_str);

    auto *datamsg = reinterpret_cast<messages::data *>(recvbuf.data());
    auto *ackmsg = reinterpret_cast<messages::ack *>(ack.data());
    ackmsg->block_num = datamsg->block_num;

    sendmsg(sock, socket_message{.address = sockmsg.address, .buffers = ack},
            0);
  }

  remove(test_file);
}

INSTANTIATE_TEST_SUITE_P(TftpRRQTests, TftpServerRRQOctetTests,
                         ::testing::Values(511, 512, 513, 1023, 1024, 1025));

auto netascii_lines(std::size_t n) -> std::string
{
  auto lines = std::string();
  for (std::size_t i = 0; i < n; ++i)
  {
    lines += "\r\n";
  }
  return lines;
}
INSTANTIATE_TEST_SUITE_P(
    TftpRRQTests, TftpServerRRQNetAsciiTests,
    ::testing::Values(std::make_pair("Hello, world!\r\n", "Hello, world!\n"),
                      std::make_pair("Hello, world!\r\n", "Hello, world!\r\n"),
                      std::make_pair(netascii_lines(512),
                                     std::string(512, '\n'))));

// NOLINTEND

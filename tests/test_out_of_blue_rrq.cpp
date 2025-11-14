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

/**
 * Test suite for out-of-the-blue RRQ packet handling covering lines 302-303 in
 * tftp_server.cpp
 *
 * These tests verify both branches of the condition:
 *   if (state.opc != 0)
 *     return reader(ctx, socket, rctx);
 *
 * This condition detects when an RRQ is received for a session that already has
 * an active operation in progress (state.opc is set to RRQ or WRQ).
 *
 * Branch 1: state.opc != 0 is TRUE  -> Ignore duplicate RRQ (out-of-the-blue)
 * Branch 2: state.opc == 0 is TRUE  -> Process new RRQ (fresh session)
 */

// Branch 1: state.opc != 0 is TRUE
// Scenario: Client sends duplicate RRQ while transfer is in progress
// Expected: Server ignores the duplicate RRQ and continues with existing
// transfer
TEST_F(TftpServerTests, TestOutOfBlueRRQ_DuplicateRRQDuringTransfer)
{
  using enum messages::opcode_t;
  using namespace std::filesystem;

  // Create a file that requires multiple blocks (> 512 bytes)
  std::vector<char> test_data(1024);
  {
    auto inf = std::ifstream("/dev/random");
    auto outf = std::ofstream(test_file);
    inf.read(test_data.data(), test_data.size());
    outf.write(test_data.data(), test_data.size());
  }

  auto sock = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");

  // Send initial RRQ (establishes session with state.opc = RRQ)
  auto len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = rrq_octet}, 0);
  ASSERT_EQ(len, rrq_octet.size());

  // Receive DATA block 1
  auto recvbuf = std::vector<char>(516);
  auto sockmsg = socket_message{.address = {socket_address<sockaddr_in6>()},
                                .buffers = recvbuf};
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_EQ(len, 516); // Full block

  auto *data = reinterpret_cast<messages::data *>(recvbuf.data());
  ASSERT_EQ(ntohs(data->opc), DATA);
  ASSERT_EQ(ntohs(data->block_num), 1);

  // At this point, state.opc != 0 (session is active with RRQ)
  // Send a duplicate RRQ from the same client (out-of-the-blue packet)
  len = io::sendmsg(
      sock, socket_message{.address = sockmsg.address, .buffers = rrq_octet},
      0);
  ASSERT_EQ(len, rrq_octet.size());

  // The duplicate RRQ should be ignored (server calls reader() and continues)
  // We should NOT receive a new DATA block 1 or any error
  // Instead, we need to ACK block 1 to continue the original transfer

  // Send ACK for block 1
  std::vector<char> ack_packet(4);
  auto opc = htons(ACK);
  std::memcpy(ack_packet.data(), &opc, sizeof(opc));
  auto block_num = htons(1);
  std::memcpy(ack_packet.data() + 2, &block_num, sizeof(block_num));

  len = io::sendmsg(
      sock, socket_message{.address = sockmsg.address, .buffers = ack_packet},
      0);
  ASSERT_EQ(len, ack_packet.size());

  // Should receive DATA block 2 (proving the original session continued)
  sockmsg.buffers = recvbuf;
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_EQ(len, 516);

  data = reinterpret_cast<messages::data *>(recvbuf.data());
  ASSERT_EQ(ntohs(data->opc), DATA);
  ASSERT_EQ(ntohs(data->block_num), 2);

  remove(test_file);
}

// Branch 1: state.opc != 0 is TRUE
// Scenario: Client sends multiple duplicate RRQs rapidly
// Expected: All duplicates are ignored, original transfer continues
TEST_F(TftpServerTests, TestOutOfBlueRRQ_MultipleDuplicateRRQs)
{
  using enum messages::opcode_t;
  using namespace std::filesystem;

  // Create a file for transfer
  std::vector<char> test_data(600);
  {
    auto inf = std::ifstream("/dev/random");
    auto outf = std::ofstream(test_file);
    inf.read(test_data.data(), test_data.size());
    outf.write(test_data.data(), test_data.size());
  }

  auto sock = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");

  // Send initial RRQ
  auto len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = rrq_octet}, 0);
  ASSERT_EQ(len, rrq_octet.size());

  // Receive DATA block 1
  auto recvbuf = std::vector<char>(516);
  auto sockmsg = socket_message{.address = {socket_address<sockaddr_in6>()},
                                .buffers = recvbuf};
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_GT(len, 0);

  // Send multiple duplicate RRQs (all should be ignored)
  for (int i = 0; i < 3; ++i)
  {
    len = io::sendmsg(
        sock, socket_message{.address = sockmsg.address, .buffers = rrq_octet},
        0);
    ASSERT_EQ(len, rrq_octet.size());
  }

  // ACK block 1 to continue transfer
  std::vector<char> ack_packet(4);
  auto opc = htons(ACK);
  std::memcpy(ack_packet.data(), &opc, sizeof(opc));
  auto block_num = htons(1);
  std::memcpy(ack_packet.data() + 2, &block_num, sizeof(block_num));

  len = io::sendmsg(
      sock, socket_message{.address = sockmsg.address, .buffers = ack_packet},
      0);
  ASSERT_EQ(len, ack_packet.size());

  // Should still receive block 2 (transfer continues normally)
  sockmsg.buffers = recvbuf;
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_GT(len, 0);

  auto *data = reinterpret_cast<messages::data *>(recvbuf.data());
  ASSERT_EQ(ntohs(data->opc), DATA);
  ASSERT_EQ(ntohs(data->block_num), 2);

  remove(test_file);
}

// Branch 2: state.opc == 0 is TRUE
// Scenario: Fresh RRQ for a new transfer (no existing session)
// Expected: Server processes the RRQ and starts transfer
TEST_F(TftpServerTests, TestOutOfBlueRRQ_FreshRRQ_NewSession)
{
  using enum messages::opcode_t;
  using namespace std::filesystem;

  // Create a file to read
  std::vector<char> test_data(200);
  {
    auto inf = std::ifstream("/dev/random");
    auto outf = std::ofstream(test_file);
    inf.read(test_data.data(), test_data.size());
    outf.write(test_data.data(), test_data.size());
  }

  auto sock = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");

  // Send RRQ (state.opc is initially 0, so this is a fresh session)
  auto len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = rrq_octet}, 0);
  ASSERT_EQ(len, rrq_octet.size());

  // Should receive DATA block 1 (proving RRQ was processed)
  auto recvbuf = std::vector<char>(516);
  auto sockmsg = socket_message{.address = {socket_address<sockaddr_in6>()},
                                .buffers = recvbuf};
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_GT(len, 0);

  auto *data = reinterpret_cast<messages::data *>(recvbuf.data());
  ASSERT_EQ(ntohs(data->opc), DATA);
  ASSERT_EQ(ntohs(data->block_num), 1);

  remove(test_file);
}

// Branch 2: state.opc == 0 is TRUE
// Scenario: RRQ after a previous transfer has completed
// Expected: Server processes the new RRQ (state.opc was reset to 0 after
// cleanup)
TEST_F(TftpServerTests, TestOutOfBlueRRQ_FreshRRQ_AfterCompletedTransfer)
{
  using enum messages::opcode_t;
  using namespace std::filesystem;

  // Create a small file (< 512 bytes for single block transfer)
  std::vector<char> test_data1(100);
  {
    auto inf = std::ifstream("/dev/random");
    auto outf = std::ofstream(test_file);
    inf.read(test_data1.data(), test_data1.size());
    outf.write(test_data1.data(), test_data1.size());
  }

  auto sock = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");

  // First transfer: Send RRQ
  auto len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = rrq_octet}, 0);
  ASSERT_EQ(len, rrq_octet.size());

  // Receive DATA block 1 (only block)
  auto recvbuf = std::vector<char>(516);
  auto sockmsg = socket_message{.address = {socket_address<sockaddr_in6>()},
                                .buffers = recvbuf};
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_GT(len, 0);
  ASSERT_LT(len, 516); // Last block is < 512 bytes

  // ACK block 1 to complete transfer
  std::vector<char> ack_packet(4);
  auto opc = htons(ACK);
  std::memcpy(ack_packet.data(), &opc, sizeof(opc));
  auto block_num = htons(1);
  std::memcpy(ack_packet.data() + 2, &block_num, sizeof(block_num));

  len = io::sendmsg(
      sock, socket_message{.address = sockmsg.address, .buffers = ack_packet},
      0);
  ASSERT_EQ(len, ack_packet.size());

  // Wait a bit for session cleanup
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Create a new file for second transfer
  auto test_file2 = (std::filesystem::temp_directory_path() / "test2.")
                        .concat(std::format("{:05d}", test_counter++));
  std::vector<char> test_data2(150);
  {
    auto inf = std::ifstream("/dev/random");
    auto outf = std::ofstream(test_file2);
    inf.read(test_data2.data(), test_data2.size());
    outf.write(test_data2.data(), test_data2.size());
  }

  // Build new RRQ for second file
  std::vector<char> rrq_octet2;
  rrq_octet2.resize(sizeof(messages::opcode_t));
  auto rrq_opc = htons(RRQ);
  std::memcpy(rrq_octet2.data(), &rrq_opc, sizeof(rrq_opc));
  std::ranges::copy(std::string_view(test_file2.c_str()),
                    std::back_inserter(rrq_octet2));
  rrq_octet2.push_back('\0');
  std::ranges::copy("octet", std::back_inserter(rrq_octet2));

  // Second transfer: Send new RRQ (state.opc should be 0 after cleanup)
  auto sock2 = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  len = io::sendmsg(
      sock2, socket_message{.address = {addr_v4}, .buffers = rrq_octet2}, 0);
  ASSERT_EQ(len, rrq_octet2.size());

  // Should receive DATA block 1 for new transfer
  auto sockmsg2 = socket_message{.address = {socket_address<sockaddr_in6>()},
                                 .buffers = recvbuf};
  len = io::recvmsg(sock2, sockmsg2, 0);
  ASSERT_GT(len, 0);

  auto *data = reinterpret_cast<messages::data *>(recvbuf.data());
  ASSERT_EQ(ntohs(data->opc), DATA);
  ASSERT_EQ(ntohs(data->block_num), 1);

  remove(test_file);
  remove(test_file2);
}

// Branch 2: state.opc == 0 is TRUE
// Scenario: Multiple sequential RRQs from different clients (different sockets)
// Expected: Each RRQ creates a new session and is processed independently
TEST_F(TftpServerTests, TestOutOfBlueRRQ_FreshRRQ_MultipleClients)
{
  using enum messages::opcode_t;
  using namespace std::filesystem;

  // Create files for multiple clients
  auto test_file1 = test_file;
  auto test_file2 = (std::filesystem::temp_directory_path() / "test2.")
                        .concat(std::format("{:05d}", test_counter++));

  std::vector<char> test_data(150);
  {
    auto inf = std::ifstream("/dev/random");

    auto outf1 = std::ofstream(test_file1);
    inf.read(test_data.data(), test_data.size());
    outf1.write(test_data.data(), test_data.size());
    outf1.close();

    auto outf2 = std::ofstream(test_file2);
    inf.read(test_data.data(), test_data.size());
    outf2.write(test_data.data(), test_data.size());
  }

  // Client 1
  auto sock1 = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");

  auto len = io::sendmsg(
      sock1, socket_message{.address = {addr_v4}, .buffers = rrq_octet}, 0);
  ASSERT_EQ(len, rrq_octet.size());

  auto recvbuf1 = std::vector<char>(516);
  auto sockmsg1 = socket_message{.address = {socket_address<sockaddr_in6>()},
                                 .buffers = recvbuf1};
  len = io::recvmsg(sock1, sockmsg1, 0);
  ASSERT_GT(len, 0);

  auto *data1 = reinterpret_cast<messages::data *>(recvbuf1.data());
  ASSERT_EQ(ntohs(data1->opc), DATA);
  ASSERT_EQ(ntohs(data1->block_num), 1);

  // Client 2 (different socket, so different session with state.opc = 0)
  auto sock2 = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);

  // Build RRQ for second file
  std::vector<char> rrq_octet2;
  rrq_octet2.resize(sizeof(messages::opcode_t));
  auto rrq_opc = htons(RRQ);
  std::memcpy(rrq_octet2.data(), &rrq_opc, sizeof(rrq_opc));
  std::ranges::copy(std::string_view(test_file2.c_str()),
                    std::back_inserter(rrq_octet2));
  rrq_octet2.push_back('\0');
  std::ranges::copy("octet", std::back_inserter(rrq_octet2));

  len = io::sendmsg(
      sock2, socket_message{.address = {addr_v4}, .buffers = rrq_octet2}, 0);
  ASSERT_EQ(len, rrq_octet2.size());

  auto recvbuf2 = std::vector<char>(516);
  auto sockmsg2 = socket_message{.address = {socket_address<sockaddr_in6>()},
                                 .buffers = recvbuf2};
  len = io::recvmsg(sock2, sockmsg2, 0);
  ASSERT_GT(len, 0);

  auto *data2 = reinterpret_cast<messages::data *>(recvbuf2.data());
  ASSERT_EQ(ntohs(data2->opc), DATA);
  ASSERT_EQ(ntohs(data2->block_num), 1);

  remove(test_file1);
  remove(test_file2);
}

// NOLINTEND

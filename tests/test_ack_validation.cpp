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

// NOLINTBEGIN
#include "test_server_fixture.hpp"

using namespace io::socket;
using namespace net::service;

/**
 * Test suite for ACK packet validation covering lines 248-249 in
 * tftp_server.cpp These tests verify all branches of the condition: if
 * (msg.size() < sizeof(messages::ack)) return error(ctx, socket, siter,
 * ILLEGAL_OPERATION);
 *
 * Note: messages::ack has the same structure as messages::data
 * sizeof(messages::ack) = 4 bytes (2 bytes opcode + 2 bytes block_num)
 */

// Branch 1: msg.size() < sizeof(messages::ack) is TRUE
// Expected: ILLEGAL_OPERATION error
TEST_F(TftpdTests, TestAckPacketTooSmall_3Bytes)
{
  using enum messages::opcode_t;
  using namespace std::filesystem;

  // Create a file to read
  std::vector<char> test_data(100);
  {
    auto inf = std::ifstream("/dev/random");
    auto outf = std::ofstream(test_file);
    inf.read(test_data.data(), test_data.size());
    outf.write(test_data.data(), test_data.size());
  }

  auto sock = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");

  // Send RRQ
  auto len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = rrq_octet}, 0);
  ASSERT_EQ(len, rrq_octet.size());

  // Receive DATA block 1
  auto recvbuf = std::vector<char>(516);
  auto sockmsg = socket_message{.address = {socket_address<sockaddr_in6>()},
                                .buffers = recvbuf};
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_GT(len, 0);

  // Send an ACK that is only 3 bytes (too small)
  // sizeof(messages::ack) = 4 bytes, so 3 bytes should trigger error
  std::vector<char> invalid_ack(3);
  auto opc = htons(ACK);
  std::memcpy(invalid_ack.data(), &opc, sizeof(opc));
  invalid_ack[2] = 0x01; // Incomplete block number

  len = io::sendmsg(
      sock, socket_message{.address = sockmsg.address, .buffers = invalid_ack},
      0);
  ASSERT_EQ(len, invalid_ack.size());

  // Should receive ILLEGAL_OPERATION error
  sockmsg.buffers = recvbuf;
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_EQ(
      std::memcmp(recvbuf.data(), errors::illegal_operation().data(), len), 0);

  remove(test_file);
}

// Branch 1: msg.size() < sizeof(messages::ack) is TRUE (2 bytes)
// Expected: ILLEGAL_OPERATION error
TEST_F(TftpdTests, TestAckPacketTooSmall_2Bytes)
{
  using enum messages::opcode_t;
  using namespace std::filesystem;

  // Create a file to read
  std::vector<char> test_data(100);
  {
    auto inf = std::ifstream("/dev/random");
    auto outf = std::ofstream(test_file);
    inf.read(test_data.data(), test_data.size());
    outf.write(test_data.data(), test_data.size());
  }

  auto sock = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");

  // Send RRQ
  auto len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = rrq_octet}, 0);
  ASSERT_EQ(len, rrq_octet.size());

  // Receive DATA block 1
  auto recvbuf = std::vector<char>(516);
  auto sockmsg = socket_message{.address = {socket_address<sockaddr_in6>()},
                                .buffers = recvbuf};
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_GT(len, 0);

  // Send an ACK that is only 2 bytes (opcode only, no block number)
  std::vector<char> invalid_ack(2);
  auto opc = htons(ACK);
  std::memcpy(invalid_ack.data(), &opc, sizeof(opc));

  len = io::sendmsg(
      sock, socket_message{.address = sockmsg.address, .buffers = invalid_ack},
      0);
  ASSERT_EQ(len, invalid_ack.size());

  // Should receive ILLEGAL_OPERATION error
  sockmsg.buffers = recvbuf;
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_EQ(
      std::memcmp(recvbuf.data(), errors::illegal_operation().data(), len), 0);

  remove(test_file);
}

// Branch 1: msg.size() < sizeof(messages::ack) is TRUE (1 byte)
// Expected: ILLEGAL_OPERATION error
TEST_F(TftpdTests, TestAckPacketTooSmall_1Byte)
{
  using enum messages::opcode_t;
  using namespace std::filesystem;

  // Create a file to read
  std::vector<char> test_data(100);
  {
    auto inf = std::ifstream("/dev/random");
    auto outf = std::ofstream(test_file);
    inf.read(test_data.data(), test_data.size());
    outf.write(test_data.data(), test_data.size());
  }

  auto sock = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");

  // Send RRQ
  auto len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = rrq_octet}, 0);
  ASSERT_EQ(len, rrq_octet.size());

  // Receive DATA block 1
  auto recvbuf = std::vector<char>(516);
  auto sockmsg = socket_message{.address = {socket_address<sockaddr_in6>()},
                                .buffers = recvbuf};
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_GT(len, 0);

  // Send an ACK that is only 1 byte (partial opcode)
  std::vector<char> invalid_ack(1);
  invalid_ack[0] = 0x04; // Part of ACK opcode

  len = io::sendmsg(
      sock, socket_message{.address = sockmsg.address, .buffers = invalid_ack},
      0);
  ASSERT_EQ(len, invalid_ack.size());

  // Should receive ILLEGAL_OPERATION error
  sockmsg.buffers = recvbuf;
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_EQ(
      std::memcmp(recvbuf.data(), errors::illegal_operation().data(), len), 0);

  remove(test_file);
}

// Branch 1: msg.size() < sizeof(messages::ack) is TRUE (0 bytes)
// Expected: ILLEGAL_OPERATION error
TEST_F(TftpdTests, TestAckPacketEmpty)
{
  using enum messages::opcode_t;
  using namespace std::filesystem;

  // Create a file to read
  std::vector<char> test_data(100);
  {
    auto inf = std::ifstream("/dev/random");
    auto outf = std::ofstream(test_file);
    inf.read(test_data.data(), test_data.size());
    outf.write(test_data.data(), test_data.size());
  }

  auto sock = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");

  // Send RRQ
  auto len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = rrq_octet}, 0);
  ASSERT_EQ(len, rrq_octet.size());

  // Receive DATA block 1
  auto recvbuf = std::vector<char>(516);
  auto sockmsg = socket_message{.address = {socket_address<sockaddr_in6>()},
                                .buffers = recvbuf};
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_GT(len, 0);

  // Send an empty ACK (0 bytes)
  std::vector<char> empty_ack(0);

  len = io::sendmsg(
      sock, socket_message{.address = sockmsg.address, .buffers = empty_ack},
      0);

  // Note: sendmsg with empty buffer may or may not send
  // If it does, server should respond with error
  if (len == 0)
  {
    sockmsg.buffers = recvbuf;
    len = io::recvmsg(sock, sockmsg, 0);
    if (len > 0)
    {
      // If we got a response, it should be an error
      ASSERT_EQ(
          std::memcmp(recvbuf.data(), errors::illegal_operation().data(), len),
          0);
    }
  }

  remove(test_file);
}

// Branch 2: msg.size() == sizeof(messages::ack) (exactly 4 bytes)
// Expected: Normal processing (no error)
TEST_F(TftpdTests, TestAckPacketMinimalSize_Valid)
{
  using enum messages::opcode_t;
  using namespace std::filesystem;

  // Create a small file to read (less than 512 bytes so transfer completes)
  std::vector<char> test_data(100);
  {
    auto inf = std::ifstream("/dev/random");
    auto outf = std::ofstream(test_file);
    inf.read(test_data.data(), test_data.size());
    outf.write(test_data.data(), test_data.size());
  }

  auto sock = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");

  // Send RRQ
  auto len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = rrq_octet}, 0);
  ASSERT_EQ(len, rrq_octet.size());

  // Receive DATA block 1
  auto recvbuf = std::vector<char>(516);
  auto sockmsg = socket_message{.address = {socket_address<sockaddr_in6>()},
                                .buffers = recvbuf};
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_GT(len, 0);
  ASSERT_LE(len, 516);

  // Send a valid ACK with exactly sizeof(messages::ack) = 4 bytes
  std::vector<char> valid_ack(4);
  auto opc = htons(ACK);
  std::memcpy(valid_ack.data(), &opc, sizeof(opc));
  auto block_num = htons(1);
  std::memcpy(valid_ack.data() + 2, &block_num, sizeof(block_num));

  len = io::sendmsg(
      sock, socket_message{.address = sockmsg.address, .buffers = valid_ack},
      0);
  ASSERT_EQ(len, valid_ack.size());

  // Should NOT receive an error (transfer should complete since file < 512
  // bytes) No response expected as this was the last block
  sockmsg.buffers = recvbuf;
  len = io::recvmsg(sock, sockmsg, MSG_DONTWAIT);

  // Either no response (transfer complete) or we timed out
  // If there's a response, it should NOT be an ILLEGAL_OPERATION error
  if (len > 0)
  {
    // Verify it's not an ILLEGAL_OPERATION error
    ASSERT_NE(
        std::memcmp(recvbuf.data(), errors::illegal_operation().data(), len),
        0);
  }

  remove(test_file);
}

// Branch 2: msg.size() > sizeof(messages::ack) (more than 4 bytes)
// Expected: Normal processing (ACK packets can be larger due to trailing data)
TEST_F(TftpdTests, TestAckPacketLargerThanMinimal_Valid)
{
  using enum messages::opcode_t;
  using namespace std::filesystem;

  // Create a file to read
  std::vector<char> test_data(
      600); // More than 512 bytes for multi-block transfer
  {
    auto inf = std::ifstream("/dev/random");
    auto outf = std::ofstream(test_file);
    inf.read(test_data.data(), test_data.size());
    outf.write(test_data.data(), test_data.size());
  }

  auto sock = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");

  // Send RRQ
  auto len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = rrq_octet}, 0);
  ASSERT_EQ(len, rrq_octet.size());

  // Receive DATA block 1
  auto recvbuf = std::vector<char>(516);
  auto sockmsg = socket_message{.address = {socket_address<sockaddr_in6>()},
                                .buffers = recvbuf};
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_EQ(len, 516); // Should be full block

  // Send a valid ACK with extra padding (8 bytes total)
  std::vector<char> valid_ack(8);
  auto opc = htons(ACK);
  std::memcpy(valid_ack.data(), &opc, sizeof(opc));
  auto block_num = htons(1);
  std::memcpy(valid_ack.data() + 2, &block_num, sizeof(block_num));
  // Bytes 4-7 are padding (ignored by server)

  len = io::sendmsg(
      sock, socket_message{.address = sockmsg.address, .buffers = valid_ack},
      0);
  ASSERT_EQ(len, valid_ack.size());

  // Should receive DATA block 2 (not an error)
  sockmsg.buffers = recvbuf;
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_GT(len, 0);

  // Verify we got a DATA packet, not an error
  auto *data = reinterpret_cast<messages::data *>(recvbuf.data());
  ASSERT_EQ(ntohs(data->opc), DATA);
  ASSERT_EQ(ntohs(data->block_num), 2);

  remove(test_file);
}

// Branch 2: Standard ACK in normal RRQ transfer flow
// Expected: Normal processing through multiple blocks
TEST_F(TftpdTests, TestAckPacketNormalSize_MultiBlock)
{
  using enum messages::opcode_t;
  using namespace std::filesystem;

  // Create a file requiring multiple blocks (> 512 bytes)
  std::vector<char> test_data(1024);
  {
    auto inf = std::ifstream("/dev/random");
    auto outf = std::ofstream(test_file);
    inf.read(test_data.data(), test_data.size());
    outf.write(test_data.data(), test_data.size());
  }

  auto sock = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");

  // Send RRQ
  auto len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = rrq_octet}, 0);
  ASSERT_EQ(len, rrq_octet.size());

  auto recvbuf = std::vector<char>(516);
  auto sockmsg = socket_message{.address = {socket_address<sockaddr_in6>()},
                                .buffers = recvbuf};

  // Process multiple blocks
  for (int block = 1; block <= 2; ++block)
  {
    // Receive DATA block
    len = io::recvmsg(sock, sockmsg, 0);
    ASSERT_GT(len, 0);

    auto *data = reinterpret_cast<messages::data *>(recvbuf.data());
    ASSERT_EQ(ntohs(data->opc), DATA);
    ASSERT_EQ(ntohs(data->block_num), block);

    // Send valid ACK with exactly 4 bytes
    std::vector<char> ack_packet(4);
    auto opc = htons(ACK);
    std::memcpy(ack_packet.data(), &opc, sizeof(opc));
    auto ack_block = htons(block);
    std::memcpy(ack_packet.data() + 2, &ack_block, sizeof(ack_block));

    len = io::sendmsg(
        sock, socket_message{.address = sockmsg.address, .buffers = ack_packet},
        0);
    ASSERT_EQ(len, ack_packet.size());

    sockmsg.buffers = recvbuf;
  }

  // Receive final block (< 512 bytes)
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_GT(len, 0);
  ASSERT_LT(len, 516); // Last block is partial

  auto *data = reinterpret_cast<messages::data *>(recvbuf.data());
  ASSERT_EQ(ntohs(data->opc), DATA);
  ASSERT_EQ(ntohs(data->block_num), 3);

  remove(test_file);
}

// NOLINTEND

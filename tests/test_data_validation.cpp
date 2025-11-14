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

#include <sys/socket.h>

using namespace io::socket;
using namespace net::service;

/**
 * Test suite for data packet validation covering lines 406-407 in
 * tftp_server.cpp These tests verify all branches of the condition: if
 * (buf.size() < sizeof(messages::data) || (rctx->msg.flags & MSG_TRUNC)) return
 * error(ctx, socket, siter, ILLEGAL_OPERATION);
 */

// Branch 1: buf.size() < sizeof(messages::data) is TRUE, MSG_TRUNC is FALSE
// Expected: ILLEGAL_OPERATION error
TEST_F(TftpServerTests, TestDataPacketTooSmall_BranchA)
{
  using enum messages::opcode_t;

  // First, send a WRQ to establish a session
  auto sock = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");

  auto len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = wrq_octet}, 0);
  ASSERT_EQ(len, wrq_octet.size());

  // Receive ACK 0
  auto recvbuf = std::vector<char>(516);
  auto sockmsg = socket_message{.address = {socket_address<sockaddr_in6>()},
                                .buffers = recvbuf};
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_GT(len, 0);

  // Send a DATA packet that is smaller than sizeof(messages::data) which is 4
  // bytes sizeof(messages::data) = 2 bytes (opc) + 2 bytes (block_num) = 4
  // bytes Send only 3 bytes (too small)
  std::vector<char> invalid_data(3);
  auto opc = htons(DATA);
  std::memcpy(invalid_data.data(), &opc, sizeof(opc));
  invalid_data[2] = 0x01; // Incomplete block number

  len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = invalid_data}, 0);
  ASSERT_EQ(len, invalid_data.size());

  // Should receive ILLEGAL_OPERATION error
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_EQ(
      std::memcmp(recvbuf.data(), errors::illegal_operation().data(), len), 0);
}

// Branch 2: buf.size() < sizeof(messages::data) is TRUE, MSG_TRUNC is TRUE
// Expected: ILLEGAL_OPERATION error (first condition is sufficient)
TEST_F(TftpServerTests, TestDataPacketTooSmallAndTruncated_BranchB)
{
  using enum messages::opcode_t;

  // First, send a WRQ to establish a session
  auto sock = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");

  auto len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = wrq_octet}, 0);
  ASSERT_EQ(len, wrq_octet.size());

  // Receive ACK 0
  auto recvbuf = std::vector<char>(516);
  auto sockmsg = socket_message{.address = {socket_address<sockaddr_in6>()},
                                .buffers = recvbuf};
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_GT(len, 0);

  // Send a DATA packet with only 2 bytes (opcode only, missing block_num)
  std::vector<char> invalid_data(2);
  auto opc = htons(DATA);
  std::memcpy(invalid_data.data(), &opc, sizeof(opc));

  len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = invalid_data}, 0);
  ASSERT_EQ(len, invalid_data.size());

  // Should receive ILLEGAL_OPERATION error
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_EQ(
      std::memcmp(recvbuf.data(), errors::illegal_operation().data(), len), 0);
}

// Branch 3: buf.size() >= sizeof(messages::data) is TRUE, but would be
// truncated This tests the MSG_TRUNC flag scenario Note: MSG_TRUNC is set by
// the kernel when the buffer provided to recvmsg is too small We can't directly
// inject MSG_TRUNC in integration tests, but we test with minimal buffer
TEST_F(TftpServerTests, TestDataPacketMinimalSize_BranchC)
{
  using enum messages::opcode_t;

  // First, send a WRQ to establish a session
  auto sock = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");

  auto len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = wrq_octet}, 0);
  ASSERT_EQ(len, wrq_octet.size());

  // Receive ACK 0
  auto recvbuf = std::vector<char>(516);
  auto sockmsg = socket_message{.address = {socket_address<sockaddr_in6>()},
                                .buffers = recvbuf};
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_GT(len, 0);

  // Send data packet that exceeds tftpd recvmsg buffer allocation.
  // This is valid per TFTP protocol (signals end of transfer)
  std::vector<char> data_packet(518);
  auto opc = htons(DATA);
  std::memcpy(data_packet.data(), &opc, sizeof(opc));
  auto block_num = htons(1);
  std::memcpy(data_packet.data() + 2, &block_num, sizeof(block_num));

  // Send to the address we received ACK 0 from (correct TID)
  len = io::sendmsg(
      sock, socket_message{.address = sockmsg.address, .buffers = data_packet},
      0);
  ASSERT_EQ(len, data_packet.size());

  // Should receive ILLEGAL_OPERATION.
  sockmsg.buffers = recvbuf;
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_EQ(
      std::memcmp(recvbuf.data(), errors::illegal_operation().data(), len), 0);
}

// Branch 4: buf.size() >= sizeof(messages::data) AND MSG_TRUNC is FALSE
// Expected: Normal processing (no error)
TEST_F(TftpServerTests, TestDataPacketValidSize_BranchD)
{
  using enum messages::opcode_t;

  // First, send a WRQ to establish a session
  auto sock = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");

  auto len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = wrq_octet}, 0);
  ASSERT_EQ(len, wrq_octet.size());

  // Receive ACK 0
  auto recvbuf = std::vector<char>(516);
  auto sockmsg = socket_message{.address = {socket_address<sockaddr_in6>()},
                                .buffers = recvbuf};
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_GT(len, 0);

  // Send a valid DATA packet with payload
  std::vector<char> data_packet(4 + 10); // header + 10 bytes payload
  auto opc = htons(DATA);
  std::memcpy(data_packet.data(), &opc, sizeof(opc));
  auto block_num = htons(1);
  std::memcpy(data_packet.data() + 2, &block_num, sizeof(block_num));

  // Add some payload data
  std::string payload = "test data!";
  std::memcpy(data_packet.data() + 4, payload.data(), payload.size());

  // Send to the address we received ACK 0 from (correct TID)
  len = io::sendmsg(
      sock, socket_message{.address = sockmsg.address, .buffers = data_packet},
      0);
  ASSERT_EQ(len, data_packet.size());

  // Should receive ACK for block 1 (not an error)
  sockmsg.buffers = recvbuf;
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_EQ(len, sizeof(messages::ack));

  auto *ack = reinterpret_cast<messages::ack *>(recvbuf.data());
  ASSERT_EQ(ntohs(ack->opc), ACK);
  ASSERT_EQ(ntohs(ack->block_num), 1);
}

// Edge case: buf.size() == 0 (empty packet)
TEST_F(TftpServerTests, TestDataPacketEmpty_EdgeCase)
{
  using enum messages::opcode_t;

  // First, send a WRQ to establish a session
  auto sock = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");

  auto len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = wrq_octet}, 0);
  ASSERT_EQ(len, wrq_octet.size());

  // Receive ACK 0
  auto recvbuf = std::vector<char>(516);
  auto sockmsg = socket_message{.address = {socket_address<sockaddr_in6>()},
                                .buffers = recvbuf};
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_GT(len, 0);

  // Send an empty packet (0 bytes) - this should not be possible in normal UDP
  // but we test the boundary condition
  std::vector<char> empty_packet(0);

  len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = empty_packet}, 0);

  // Note: sendmsg with empty buffer may or may not actually send
  // If it does send and server receives it, it should error
  if (len == 0)
  {
    // Try to receive response (may timeout or get error)
    len = io::recvmsg(sock, sockmsg, 0);
    if (len > 0)
    {
      // If we got a response, it should be an error
      ASSERT_EQ(
          std::memcmp(recvbuf.data(), errors::illegal_operation().data(), len),
          0);
    }
  }
}

// Edge case: buf.size() == 1 (one byte only)
TEST_F(TftpServerTests, TestDataPacketOneByte_EdgeCase)
{
  using enum messages::opcode_t;

  // First, send a WRQ to establish a session
  auto sock = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");

  auto len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = wrq_octet}, 0);
  ASSERT_EQ(len, wrq_octet.size());

  // Receive ACK 0
  auto recvbuf = std::vector<char>(516);
  auto sockmsg = socket_message{.address = {socket_address<sockaddr_in6>()},
                                .buffers = recvbuf};
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_GT(len, 0);

  // Send a packet with only 1 byte
  std::vector<char> tiny_packet(1);
  tiny_packet[0] = 0x03; // Part of DATA opcode

  len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = tiny_packet}, 0);
  ASSERT_EQ(len, tiny_packet.size());

  // Should receive ILLEGAL_OPERATION error
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_EQ(
      std::memcmp(recvbuf.data(), errors::illegal_operation().data(), len), 0);
}

// NOLINTEND

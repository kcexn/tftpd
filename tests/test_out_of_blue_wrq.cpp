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
 * Test suite for out-of-the-blue WRQ packet handling covering lines 369-370 in
 * tftp_server.cpp
 *
 * These tests verify both branches of the condition:
 *   if (session.state.opc != 0)
 *     return reader(ctx, socket, rctx);
 *
 * This condition detects when a WRQ is received for a session that already has
 * an active operation in progress (state.opc is set to RRQ or WRQ).
 *
 * Branch 1: state.opc != 0 is TRUE  -> Ignore duplicate WRQ (out-of-the-blue)
 * Branch 2: state.opc == 0 is TRUE  -> Process new WRQ (fresh session)
 */

// Branch 1: state.opc != 0 is TRUE
// Scenario: Client sends duplicate WRQ while upload is in progress
// Expected: Server ignores the duplicate WRQ and continues with existing
// transfer
TEST_F(TftpServerTests, TestOutOfBlueWRQ_DuplicateWRQDuringTransfer)
{
  using enum messages::opcode_t;
  using namespace std::filesystem;

  auto sock = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");

  // Send initial WRQ (establishes session with state.opc = WRQ)
  auto len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = wrq_octet}, 0);
  ASSERT_EQ(len, wrq_octet.size());

  // Receive ACK 0
  auto recvbuf = std::vector<char>(516);
  auto sockmsg = socket_message{.address = {socket_address<sockaddr_in6>()},
                                .buffers = recvbuf};
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_GT(len, 0);

  auto *ack = reinterpret_cast<messages::ack *>(recvbuf.data());
  ASSERT_EQ(ntohs(ack->opc), ACK);
  ASSERT_EQ(ntohs(ack->block_num), 0);

  // At this point, state.opc != 0 (session is active with WRQ)
  // Send a duplicate WRQ from the same client (out-of-the-blue packet)
  len = io::sendmsg(
      sock, socket_message{.address = sockmsg.address, .buffers = wrq_octet},
      0);
  ASSERT_EQ(len, wrq_octet.size());

  // The duplicate WRQ should be ignored (server calls reader() and continues)
  // We should NOT receive a new ACK 0 or any error
  // Instead, we need to send DATA block 1 to continue the original transfer

  // Send DATA block 1
  std::vector<char> data_packet(4 + 50); // header + 50 bytes payload
  auto opc = htons(DATA);
  std::memcpy(data_packet.data(), &opc, sizeof(opc));
  auto block_num = htons(1);
  std::memcpy(data_packet.data() + 2, &block_num, sizeof(block_num));

  // Add some payload
  std::fill(data_packet.begin() + 4, data_packet.end(), 'A');

  len = io::sendmsg(
      sock, socket_message{.address = sockmsg.address, .buffers = data_packet},
      0);
  ASSERT_EQ(len, data_packet.size());

  // Should receive ACK for block 1 (proving the original session continued)
  sockmsg.buffers = recvbuf;
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_EQ(len, sizeof(messages::ack));

  ack = reinterpret_cast<messages::ack *>(recvbuf.data());
  ASSERT_EQ(ntohs(ack->opc), ACK);
  ASSERT_EQ(ntohs(ack->block_num), 1);

  remove(test_file);
}

// Branch 1: state.opc != 0 is TRUE
// Scenario: Client sends multiple duplicate WRQs rapidly
// Expected: All duplicates are ignored, original upload continues
TEST_F(TftpServerTests, TestOutOfBlueWRQ_MultipleDuplicateWRQs)
{
  using enum messages::opcode_t;
  using namespace std::filesystem;

  auto sock = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");

  // Send initial WRQ
  auto len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = wrq_octet}, 0);
  ASSERT_EQ(len, wrq_octet.size());

  // Receive ACK 0
  auto recvbuf = std::vector<char>(516);
  auto sockmsg = socket_message{.address = {socket_address<sockaddr_in6>()},
                                .buffers = recvbuf};
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_GT(len, 0);

  // Send multiple duplicate WRQs (all should be ignored)
  for (int i = 0; i < 3; ++i)
  {
    len = io::sendmsg(
        sock, socket_message{.address = sockmsg.address, .buffers = wrq_octet},
        0);
    ASSERT_EQ(len, wrq_octet.size());
  }

  // Send DATA block 1 to continue transfer
  std::vector<char> data_packet(4 + 100);
  auto opc = htons(DATA);
  std::memcpy(data_packet.data(), &opc, sizeof(opc));
  auto block_num = htons(1);
  std::memcpy(data_packet.data() + 2, &block_num, sizeof(block_num));
  std::fill(data_packet.begin() + 4, data_packet.end(), 'B');

  len = io::sendmsg(
      sock, socket_message{.address = sockmsg.address, .buffers = data_packet},
      0);
  ASSERT_EQ(len, data_packet.size());

  // Should still receive ACK 1 (transfer continues normally)
  sockmsg.buffers = recvbuf;
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_EQ(len, sizeof(messages::ack));

  auto *ack = reinterpret_cast<messages::ack *>(recvbuf.data());
  ASSERT_EQ(ntohs(ack->opc), ACK);
  ASSERT_EQ(ntohs(ack->block_num), 1);

  remove(test_file);
}

// Branch 1: state.opc != 0 is TRUE
// Scenario: WRQ sent while an RRQ is in progress (same client socket)
// Expected: Duplicate WRQ is ignored, RRQ continues
TEST_F(TftpServerTests, TestOutOfBlueWRQ_WRQDuringRRQTransfer)
{
  using enum messages::opcode_t;
  using namespace std::filesystem;

  // Create a file for RRQ
  std::vector<char> test_data(600);
  {
    auto inf = std::ifstream("/dev/random");
    auto outf = std::ofstream(test_file);
    inf.read(test_data.data(), test_data.size());
    outf.write(test_data.data(), test_data.size());
  }

  auto sock = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");

  // Start RRQ (state.opc = RRQ)
  auto len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = rrq_octet}, 0);
  ASSERT_EQ(len, rrq_octet.size());

  // Receive DATA block 1
  auto recvbuf = std::vector<char>(516);
  auto sockmsg = socket_message{.address = {socket_address<sockaddr_in6>()},
                                .buffers = recvbuf};
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_EQ(len, 516);

  // Try to send WRQ while RRQ is active (should be ignored)
  len = io::sendmsg(
      sock, socket_message{.address = sockmsg.address, .buffers = wrq_octet},
      0);
  ASSERT_EQ(len, wrq_octet.size());

  // ACK block 1 to continue RRQ
  std::vector<char> ack_packet(4);
  auto opc = htons(ACK);
  std::memcpy(ack_packet.data(), &opc, sizeof(opc));
  auto block_num = htons(1);
  std::memcpy(ack_packet.data() + 2, &block_num, sizeof(block_num));

  len = io::sendmsg(
      sock, socket_message{.address = sockmsg.address, .buffers = ack_packet},
      0);
  ASSERT_EQ(len, ack_packet.size());

  // Should receive DATA block 2 from RRQ (not ACK 0 from WRQ)
  sockmsg.buffers = recvbuf;
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_GT(len, 0);

  auto *data = reinterpret_cast<messages::data *>(recvbuf.data());
  ASSERT_EQ(ntohs(data->opc), DATA);
  ASSERT_EQ(ntohs(data->block_num), 2);

  remove(test_file);
}

// Branch 2: state.opc == 0 is TRUE
// Scenario: Fresh WRQ for a new upload (no existing session)
// Expected: Server processes the WRQ and starts upload
TEST_F(TftpServerTests, TestOutOfBlueWRQ_FreshWRQ_NewSession)
{
  using enum messages::opcode_t;
  using namespace std::filesystem;

  auto sock = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");

  // Send WRQ (state.opc is initially 0, so this is a fresh session)
  auto len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = wrq_octet}, 0);
  ASSERT_EQ(len, wrq_octet.size());

  // Should receive ACK 0 (proving WRQ was processed)
  auto recvbuf = std::vector<char>(516);
  auto sockmsg = socket_message{.address = {socket_address<sockaddr_in6>()},
                                .buffers = recvbuf};
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_GT(len, 0);

  auto *ack = reinterpret_cast<messages::ack *>(recvbuf.data());
  ASSERT_EQ(ntohs(ack->opc), ACK);
  ASSERT_EQ(ntohs(ack->block_num), 0);

  remove(test_file);
}

// Branch 2: state.opc == 0 is TRUE
// Scenario: WRQ after a previous upload has completed
// Expected: Server processes the new WRQ (state.opc was reset to 0 after
// cleanup)
TEST_F(TftpServerTests, TestOutOfBlueWRQ_FreshWRQ_AfterCompletedTransfer)
{
  using enum messages::opcode_t;
  using namespace std::filesystem;

  auto sock = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");

  // First transfer: Send WRQ
  auto len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = wrq_octet}, 0);
  ASSERT_EQ(len, wrq_octet.size());

  // Receive ACK 0
  auto recvbuf = std::vector<char>(516);
  auto sockmsg = socket_message{.address = {socket_address<sockaddr_in6>()},
                                .buffers = recvbuf};
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_GT(len, 0);

  // Send small DATA block 1 (< 512 bytes, completes transfer)
  std::vector<char> data_packet(4 + 50);
  auto opc = htons(DATA);
  std::memcpy(data_packet.data(), &opc, sizeof(opc));
  auto block_num = htons(1);
  std::memcpy(data_packet.data() + 2, &block_num, sizeof(block_num));
  std::fill(data_packet.begin() + 4, data_packet.end(), 'C');

  len = io::sendmsg(
      sock, socket_message{.address = sockmsg.address, .buffers = data_packet},
      0);
  ASSERT_EQ(len, data_packet.size());

  // Receive ACK 1 (transfer complete)
  sockmsg.buffers = recvbuf;
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_GT(len, 0);

  remove(test_file);

  // Wait for session cleanup
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Create new file for second transfer
  auto test_file2 = (std::filesystem::temp_directory_path() / "test2.")
                        .concat(std::format("{:05d}", test_counter++));

  // Build new WRQ for second file
  std::vector<char> wrq_octet2;
  wrq_octet2.resize(sizeof(messages::opcode_t));
  auto wrq_opc = htons(WRQ);
  std::memcpy(wrq_octet2.data(), &wrq_opc, sizeof(wrq_opc));
  std::ranges::copy(std::string_view(test_file2.c_str()),
                    std::back_inserter(wrq_octet2));
  wrq_octet2.push_back('\0');
  std::ranges::copy("octet", std::back_inserter(wrq_octet2));

  // Second transfer: Send new WRQ (state.opc should be 0 after cleanup)
  auto sock2 = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  len = io::sendmsg(
      sock2, socket_message{.address = {addr_v4}, .buffers = wrq_octet2}, 0);
  ASSERT_EQ(len, wrq_octet2.size());

  // Should receive ACK 0 for new transfer
  auto sockmsg2 = socket_message{.address = {socket_address<sockaddr_in6>()},
                                 .buffers = recvbuf};
  len = io::recvmsg(sock2, sockmsg2, 0);
  ASSERT_GT(len, 0);

  auto *ack = reinterpret_cast<messages::ack *>(recvbuf.data());
  ASSERT_EQ(ntohs(ack->opc), ACK);
  ASSERT_EQ(ntohs(ack->block_num), 0);

  remove(test_file2);
}

// Branch 2: state.opc == 0 is TRUE
// Scenario: Multiple WRQs from different clients (different sockets)
// Expected: Each WRQ creates a new session and is processed independently
TEST_F(TftpServerTests, TestOutOfBlueWRQ_FreshWRQ_MultipleClients)
{
  using enum messages::opcode_t;
  using namespace std::filesystem;

  // Create separate test files for each client
  auto test_file1 = test_file;
  auto test_file2 = (std::filesystem::temp_directory_path() / "test2.")
                        .concat(std::format("{:05d}", test_counter++));

  // Build WRQ for file 1
  std::vector<char> wrq_octet1 = wrq_octet;

  // Build WRQ for file 2
  std::vector<char> wrq_octet2;
  wrq_octet2.resize(sizeof(messages::opcode_t));
  auto wrq_opc = htons(WRQ);
  std::memcpy(wrq_octet2.data(), &wrq_opc, sizeof(wrq_opc));
  std::ranges::copy(std::string_view(test_file2.c_str()),
                    std::back_inserter(wrq_octet2));
  wrq_octet2.push_back('\0');
  std::ranges::copy("octet", std::back_inserter(wrq_octet2));

  // Client 1
  auto sock1 = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");

  auto len = io::sendmsg(
      sock1, socket_message{.address = {addr_v4}, .buffers = wrq_octet1}, 0);
  ASSERT_EQ(len, wrq_octet1.size());

  auto recvbuf1 = std::vector<char>(516);
  auto sockmsg1 = socket_message{.address = {socket_address<sockaddr_in6>()},
                                 .buffers = recvbuf1};
  len = io::recvmsg(sock1, sockmsg1, 0);
  ASSERT_GT(len, 0);

  auto *ack1 = reinterpret_cast<messages::ack *>(recvbuf1.data());
  ASSERT_EQ(ntohs(ack1->opc), ACK);
  ASSERT_EQ(ntohs(ack1->block_num), 0);

  // Client 2 (different socket, so different session with state.opc = 0)
  auto sock2 = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);

  len = io::sendmsg(
      sock2, socket_message{.address = {addr_v4}, .buffers = wrq_octet2}, 0);
  ASSERT_EQ(len, wrq_octet2.size());

  auto recvbuf2 = std::vector<char>(516);
  auto sockmsg2 = socket_message{.address = {socket_address<sockaddr_in6>()},
                                 .buffers = recvbuf2};
  len = io::recvmsg(sock2, sockmsg2, 0);
  ASSERT_GT(len, 0);

  auto *ack2 = reinterpret_cast<messages::ack *>(recvbuf2.data());
  ASSERT_EQ(ntohs(ack2->opc), ACK);
  ASSERT_EQ(ntohs(ack2->block_num), 0);

  remove(test_file1);
  remove(test_file2);
}

// Branch 2: state.opc == 0 is TRUE
// Scenario: Complete upload with multiple blocks
// Expected: WRQ is processed and full upload succeeds
TEST_F(TftpServerTests, TestOutOfBlueWRQ_FreshWRQ_MultiBlockUpload)
{
  using enum messages::opcode_t;
  using namespace std::filesystem;

  auto sock = socket_handle(addr_v4->sin_family, SOCK_DGRAM, 0);
  addr_v4->sin_addr.s_addr = inet_addr("127.0.0.1");

  // Send WRQ (state.opc = 0, fresh session)
  auto len = io::sendmsg(
      sock, socket_message{.address = {addr_v4}, .buffers = wrq_octet}, 0);
  ASSERT_EQ(len, wrq_octet.size());

  // Receive ACK 0
  auto recvbuf = std::vector<char>(516);
  auto sockmsg = socket_message{.address = {socket_address<sockaddr_in6>()},
                                .buffers = recvbuf};
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_GT(len, 0);

  // Send multiple DATA blocks
  for (int block = 1; block <= 2; ++block)
  {
    std::vector<char> data_packet(516); // Full block
    auto opc = htons(DATA);
    std::memcpy(data_packet.data(), &opc, sizeof(opc));
    auto block_num = htons(block);
    std::memcpy(data_packet.data() + 2, &block_num, sizeof(block_num));
    std::fill(data_packet.begin() + 4, data_packet.end(), 'D' + block);

    len = io::sendmsg(
        sock,
        socket_message{.address = sockmsg.address, .buffers = data_packet}, 0);
    ASSERT_EQ(len, data_packet.size());

    // Receive ACK
    sockmsg.buffers = recvbuf;
    len = io::recvmsg(sock, sockmsg, 0);
    ASSERT_EQ(len, sizeof(messages::ack));

    auto *ack = reinterpret_cast<messages::ack *>(recvbuf.data());
    ASSERT_EQ(ntohs(ack->opc), ACK);
    ASSERT_EQ(ntohs(ack->block_num), block);
  }

  // Send final block (< 512 bytes)
  std::vector<char> final_packet(4 + 100);
  auto opc = htons(DATA);
  std::memcpy(final_packet.data(), &opc, sizeof(opc));
  auto block_num = htons(3);
  std::memcpy(final_packet.data() + 2, &block_num, sizeof(block_num));
  std::fill(final_packet.begin() + 4, final_packet.end(), 'F');

  len = io::sendmsg(
      sock, socket_message{.address = sockmsg.address, .buffers = final_packet},
      0);
  ASSERT_EQ(len, final_packet.size());

  // Receive final ACK
  sockmsg.buffers = recvbuf;
  len = io::recvmsg(sock, sockmsg, 0);
  ASSERT_EQ(len, sizeof(messages::ack));

  auto *ack = reinterpret_cast<messages::ack *>(recvbuf.data());
  ASSERT_EQ(ntohs(ack->opc), ACK);
  ASSERT_EQ(ntohs(ack->block_num), 3);

  // Verify file was created with correct size
  ASSERT_TRUE(exists(test_file));
  ASSERT_EQ(file_size(test_file), 512 + 512 + 100);

  remove(test_file);
}

// NOLINTEND

/* Copyright (C) 2025 Kevin Exton (kevin.exton@pm.me)
 *
 * tftpd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * tftpd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with tftpd.  If not, see <https://www.gnu.org/licenses/>.
 */

// NOLINTBEGIN
#include "tftp/filesystem.hpp"
#include "tftp/protocol/tftp_protocol.hpp"
#include "tftp/protocol/tftp_session.hpp"
#include "tftp/tftp.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

using namespace tftp;

class TestTftp : public ::testing::Test {
protected:
  sessions_t sessions;
  io::socket::socket_address<sockaddr_in6> test_addr;

  void SetUp() override
  {
    // Create a test socket address
    sockaddr_in6 addr{};
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(12345);
    test_addr = io::socket::socket_address<sockaddr_in6>(addr);

    // Clear sessions
    sessions.clear();
  }

  void TearDown() override
  {
    // Clean up any files created during tests
    for (auto &[addr, sess] : sessions)
    {
      if (sess.state.file)
      {
        sess.state.file->close();
      }
      if (!sess.state.tmp.empty() && std::filesystem::exists(sess.state.tmp))
      {
        std::filesystem::remove(sess.state.tmp);
      }
      if (!sess.state.target.empty() &&
          std::filesystem::exists(sess.state.target))
      {
        std::filesystem::remove(sess.state.target);
      }
    }
    sessions.clear();
  }

  // Helper to create a test file
  auto create_test_file(const std::string &content = "test content")
      -> std::filesystem::path
  {
    const auto path = filesystem::tmpname();
    std::ofstream(path) << content;
    return path;
  }

  // Helper to create a session
  auto create_session() -> iterator_t
  {
    return sessions.emplace(test_addr, session{});
  }
};

// =============================================================================
// handle_request Tests
// =============================================================================
using request = messages::request;
using ack = messages::ack;
using data = messages::data;
using enum messages::opcode_t;
using enum messages::error_t;
using enum messages::mode_t;
static constexpr auto DATAMSG_MAXLEN = messages::DATAMSG_MAXLEN;
static constexpr auto DATALEN = messages::DATALEN;

TEST_F(TestTftp, HandleRequest_RejectsDataOpcode)
{
  auto siter = create_session();
  request req{.opc = DATA, .mode = OCTET, .filename = "test.txt"};

  const auto result = handle_request(req, siter);

  EXPECT_EQ(result, ILLEGAL_OPERATION);
}

TEST_F(TestTftp, HandleRequest_RejectsAckOpcode)
{
  auto siter = create_session();
  request req{.opc = ACK, .mode = OCTET, .filename = "test.txt"};

  const auto result = handle_request(req, siter);

  EXPECT_EQ(result, ILLEGAL_OPERATION);
}

TEST_F(TestTftp, HandleRequest_RejectsZeroMode)
{
  auto siter = create_session();
  request req{.opc = RRQ, .mode = 0, .filename = "test.txt"};

  const auto result = handle_request(req, siter);

  EXPECT_EQ(result, ILLEGAL_OPERATION);
}

TEST_F(TestTftp, HandleRequest_RejectsRrqWithMailMode)
{
  auto siter = create_session();
  request req{.opc = RRQ, .mode = MAIL, .filename = "test.txt"};

  const auto result = handle_request(req, siter);

  EXPECT_EQ(result, ILLEGAL_OPERATION);
}

TEST_F(TestTftp, HandleRequest_RrqWithNonexistentFile)
{
  auto siter = create_session();
  const auto nonexistent = filesystem::tmpname();
  std::filesystem::remove(nonexistent);

  request req{.opc = RRQ, .mode = OCTET, .filename = nonexistent.c_str()};

  const auto result = handle_request(req, siter);

  EXPECT_EQ(result, FILE_NOT_FOUND);
}

TEST_F(TestTftp, HandleRequest_RrqSuccessWithOctetMode)
{
  const auto test_file = create_test_file("hello world");
  auto siter = create_session();

  request req{.opc = RRQ, .mode = OCTET, .filename = test_file.c_str()};

  const auto result = handle_request(req, siter);

  EXPECT_EQ(result, 0);
  EXPECT_EQ(siter->second.state.opc, RRQ);
  EXPECT_EQ(siter->second.state.mode, OCTET);
  EXPECT_EQ(siter->second.state.target, test_file);
  EXPECT_TRUE(siter->second.state.file);
  EXPECT_TRUE(siter->second.state.file->is_open());
  EXPECT_GT(siter->second.state.buffer.size(), sizeof(messages::data));
  EXPECT_EQ(siter->second.state.block_num, 1);

  std::filesystem::remove(test_file);
}

TEST_F(TestTftp, HandleRequest_RrqSuccessWithNetasciiMode)
{
  const auto test_file = create_test_file("test\ndata");
  auto siter = create_session();

  request req{.opc = RRQ, .mode = NETASCII, .filename = test_file.c_str()};

  const auto result = handle_request(req, siter);

  EXPECT_EQ(result, 0);
  EXPECT_EQ(siter->second.state.opc, RRQ);
  EXPECT_EQ(siter->second.state.mode, NETASCII);
  EXPECT_TRUE(siter->second.state.file);

  std::filesystem::remove(test_file);
}

TEST_F(TestTftp, HandleRequest_WrqSuccessWithOctetMode)
{
  const auto target_file = filesystem::tmpname();
  std::filesystem::remove(target_file);
  auto siter = create_session();

  request req{.opc = WRQ, .mode = OCTET, .filename = target_file.c_str()};

  const auto result = handle_request(req, siter);

  EXPECT_EQ(result, 0);
  EXPECT_EQ(siter->second.state.opc, WRQ);
  EXPECT_EQ(siter->second.state.mode, OCTET);
  EXPECT_EQ(siter->second.state.target, target_file);
  EXPECT_TRUE(siter->second.state.file);
  EXPECT_TRUE(siter->second.state.file->is_open());
  EXPECT_EQ(siter->second.state.block_num, 0);

  std::filesystem::remove(target_file);
}

TEST_F(TestTftp, HandleRequest_WrqWithMailMode)
{
  auto siter = create_session();
  const auto username = "testuser";

  request req{.opc = WRQ, .mode = MAIL, .filename = username};

  const auto result = handle_request(req, siter);

  // Result depends on whether the mail directory exists and user directory can
  // be created
  EXPECT_TRUE(result == 0 || result == FILE_NOT_FOUND ||
              result == NO_SUCH_USER || result == ACCESS_VIOLATION);

  if (result == 0)
  {
    EXPECT_EQ(siter->second.state.opc, WRQ);
    EXPECT_EQ(siter->second.state.mode, MAIL);
    EXPECT_TRUE(siter->second.state.target.string().find(username) !=
                std::string::npos);
    EXPECT_TRUE(siter->second.state.file);
  }
}

TEST_F(TestTftp, HandleRequest_WrqSuccessWithNetasciiMode)
{
  const auto target_file = filesystem::tmpname();
  std::filesystem::remove(target_file);
  auto siter = create_session();

  request req{.opc = WRQ, .mode = NETASCII, .filename = target_file.c_str()};

  const auto result = handle_request(req, siter);

  EXPECT_EQ(result, 0);
  EXPECT_EQ(siter->second.state.mode, NETASCII);

  std::filesystem::remove(target_file);
}

// =============================================================================
// handle_ack Tests
// =============================================================================

TEST_F(TestTftp, HandleAck_ReturnsErrorWhenNotRrq)
{
  auto siter = create_session();
  siter->second.state.opc = WRQ;

  ack ack_msg{.opc = htons(ACK), .block_num = htons(1)};

  const auto result = handle_ack(ack_msg, siter);

  EXPECT_EQ(result, UNKNOWN_TID);
}

TEST_F(TestTftp, HandleAck_SendsNextBlockWhenBufferFull)
{
  const auto test_file = create_test_file(std::string(1024, 'X'));
  auto siter = create_session();

  // Initialize RRQ session
  request req{.opc = RRQ, .mode = OCTET, .filename = test_file.c_str()};
  handle_request(req, siter);

  // Buffer should be full after first request
  ASSERT_GE(siter->second.state.buffer.size(), DATAMSG_MAXLEN);
  const auto initial_block = siter->second.state.block_num;

  ack ack_msg{.opc = htons(ACK), .block_num = htons(initial_block)};

  const auto result = handle_ack(ack_msg, siter);

  EXPECT_EQ(result, 0);
  EXPECT_EQ(siter->second.state.block_num, initial_block + 1);

  std::filesystem::remove(test_file);
}

TEST_F(TestTftp, HandleAck_ClosesFileWhenTransferComplete)
{
  const auto test_file = create_test_file("short");
  auto siter = create_session();

  // Initialize RRQ session
  request req{.opc = RRQ, .mode = OCTET, .filename = test_file.c_str()};
  handle_request(req, siter);

  // Buffer should be less than full for short file
  ASSERT_LT(siter->second.state.buffer.size(), DATAMSG_MAXLEN);
  const auto final_block = siter->second.state.block_num;

  ack ack_msg{.opc = htons(ACK), .block_num = htons(final_block)};

  const auto result = handle_ack(ack_msg, siter);

  EXPECT_EQ(result, 0);
  EXPECT_FALSE(siter->second.state.file->is_open());

  std::filesystem::remove(test_file);
}

TEST_F(TestTftp, HandleAck_IgnoresOldAck)
{
  const auto test_file = create_test_file(std::string(1024, 'Y'));
  auto siter = create_session();

  // Initialize RRQ session
  request req{.opc = RRQ, .mode = OCTET, .filename = test_file.c_str()};
  handle_request(req, siter);

  const auto current_block = siter->second.state.block_num;

  // Send ACK for old block
  ack ack_msg{.opc = htons(ACK), .block_num = htons(current_block - 1)};

  const auto result = handle_ack(ack_msg, siter);

  EXPECT_EQ(result, 0);
  EXPECT_EQ(siter->second.state.block_num, current_block);

  std::filesystem::remove(test_file);
}

TEST_F(TestTftp, HandleAck_HandlesBlockNumberWrapAround)
{
  const auto test_file = create_test_file(std::string(1024, 'Z'));
  auto siter = create_session();

  // Initialize RRQ session
  request req{.opc = RRQ, .mode = OCTET, .filename = test_file.c_str()};
  handle_request(req, siter);

  // Simulate wraparound by setting block_num to max value
  siter->second.state.block_num = 0xFFFF;
  siter->second.state.buffer.resize(DATAMSG_MAXLEN);

  ack ack_msg{.opc = htons(ACK), .block_num = htons(0xFFFF)};

  const auto result = handle_ack(ack_msg, siter);

  EXPECT_EQ(result, 0);
  EXPECT_EQ(siter->second.state.block_num, 0);

  std::filesystem::remove(test_file);
}

// =============================================================================
// handle_data Tests
// =============================================================================

TEST_F(TestTftp, HandleData_ReturnsErrorWhenNotWrq)
{
  auto siter = create_session();
  siter->second.state.opc = RRQ;

  std::vector<char> buffer(sizeof(messages::data) + 100);
  auto *data_msg = reinterpret_cast<messages::data *>(buffer.data());
  data_msg->opc = htons(DATA);
  data_msg->block_num = htons(1);

  const auto result = handle_data(data_msg, buffer.size(), siter);

  EXPECT_EQ(result, UNKNOWN_TID);
}

TEST_F(TestTftp, HandleData_ReAcksDuplicatePacket)
{
  const auto target_file = filesystem::tmpname();
  std::filesystem::remove(target_file);
  auto siter = create_session();

  // Initialize WRQ session
  request req{.opc = WRQ, .mode = OCTET, .filename = target_file.c_str()};
  handle_request(req, siter);

  siter->second.state.block_num = 5;

  // Send duplicate packet (block 5 instead of 6)
  std::vector<char> buffer(sizeof(messages::data) + 100);
  auto *data_msg = reinterpret_cast<messages::data *>(buffer.data());
  data_msg->opc = htons(DATA);
  data_msg->block_num = htons(5);

  const auto result = handle_data(data_msg, buffer.size(), siter);

  EXPECT_EQ(result, 0);
  EXPECT_EQ(siter->second.state.block_num, 5); // Block num should not advance

  std::filesystem::remove(target_file);
}

TEST_F(TestTftp, HandleData_WritesDataToFile)
{
  const auto target_file = filesystem::tmpname();
  std::filesystem::remove(target_file);
  auto siter = create_session();

  // Initialize WRQ session
  request req{.opc = WRQ, .mode = OCTET, .filename = target_file.c_str()};
  handle_request(req, siter);

  ASSERT_EQ(siter->second.state.block_num, 0);

  // Send first data block
  const std::string test_data = "Hello, TFTP!";
  std::vector<char> buffer(sizeof(messages::data) + test_data.size());
  auto *data_msg = reinterpret_cast<messages::data *>(buffer.data());
  data_msg->opc = htons(DATA);
  data_msg->block_num = htons(1);
  std::memcpy(buffer.data() + sizeof(messages::data), test_data.data(),
              test_data.size());

  const auto result = handle_data(data_msg, buffer.size(), siter);

  EXPECT_EQ(result, 0);
  EXPECT_EQ(siter->second.state.block_num, 1);

  std::filesystem::remove(target_file);
}

TEST_F(TestTftp, HandleData_CompletesTransferOnShortPacket)
{
  const auto target_file = filesystem::tmpname();
  std::filesystem::remove(target_file);
  auto siter = create_session();

  // Initialize WRQ session
  request req{.opc = WRQ, .mode = OCTET, .filename = target_file.c_str()};
  handle_request(req, siter);

  // Send short data block (less than 512 bytes signals end of transfer)
  const std::string test_data = "Final data";
  std::vector<char> buffer(sizeof(messages::data) + test_data.size());
  auto *data_msg = reinterpret_cast<messages::data *>(buffer.data());
  data_msg->opc = htons(DATA);
  data_msg->block_num = htons(1);
  std::memcpy(buffer.data() + sizeof(messages::data), test_data.data(),
              test_data.size());

  const auto result = handle_data(data_msg, buffer.size(), siter);

  EXPECT_EQ(result, 0);
  EXPECT_FALSE(siter->second.state.file->is_open());
  EXPECT_TRUE(std::filesystem::exists(target_file));

  // Verify file content
  std::ifstream in(target_file);
  std::string content((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());
  EXPECT_EQ(content, test_data);

  std::filesystem::remove(target_file);
}

TEST_F(TestTftp, HandleData_HandlesFullBlockSize)
{
  const auto target_file = filesystem::tmpname();
  std::filesystem::remove(target_file);
  auto siter = create_session();

  // Initialize WRQ session
  request req{.opc = WRQ, .mode = OCTET, .filename = target_file.c_str()};
  handle_request(req, siter);

  // Send full-size data block (512 bytes)
  std::vector<char> buffer(sizeof(messages::data) + DATALEN);
  auto *data_msg = reinterpret_cast<messages::data *>(buffer.data());
  data_msg->opc = htons(DATA);
  data_msg->block_num = htons(1);
  std::fill(buffer.begin() + sizeof(messages::data), buffer.end(), 'A');

  const auto result = handle_data(data_msg, buffer.size(), siter);

  EXPECT_EQ(result, 0);
  EXPECT_EQ(siter->second.state.block_num, 1);
  EXPECT_TRUE(siter->second.state.file->is_open()); // Should remain open

  std::filesystem::remove(target_file);
}

TEST_F(TestTftp, HandleData_HandlesMultipleBlocks)
{
  const auto target_file = filesystem::tmpname();
  std::filesystem::remove(target_file);
  auto siter = create_session();

  // Initialize WRQ session
  request req{.opc = WRQ, .mode = OCTET, .filename = target_file.c_str()};
  handle_request(req, siter);

  // Send block 1
  std::vector<char> buffer1(sizeof(messages::data) + DATALEN);
  auto *data_msg1 = reinterpret_cast<messages::data *>(buffer1.data());
  data_msg1->opc = htons(DATA);
  data_msg1->block_num = htons(1);
  std::fill(buffer1.begin() + sizeof(messages::data), buffer1.end(), '1');

  auto result = handle_data(data_msg1, buffer1.size(), siter);
  EXPECT_EQ(result, 0);
  EXPECT_EQ(siter->second.state.block_num, 1);

  // Send block 2
  std::vector<char> buffer2(sizeof(messages::data) + DATALEN);
  auto *data_msg2 = reinterpret_cast<messages::data *>(buffer2.data());
  data_msg2->opc = htons(DATA);
  data_msg2->block_num = htons(2);
  std::fill(buffer2.begin() + sizeof(messages::data), buffer2.end(), '2');

  result = handle_data(data_msg2, buffer2.size(), siter);
  EXPECT_EQ(result, 0);
  EXPECT_EQ(siter->second.state.block_num, 2);

  // Send final short block
  std::vector<char> buffer3(sizeof(messages::data) + 10);
  auto *data_msg3 = reinterpret_cast<messages::data *>(buffer3.data());
  data_msg3->opc = htons(DATA);
  data_msg3->block_num = htons(3);
  std::fill(buffer3.begin() + sizeof(messages::data), buffer3.end(), '3');

  result = handle_data(data_msg3, buffer3.size(), siter);
  EXPECT_EQ(result, 0);
  EXPECT_FALSE(siter->second.state.file->is_open());

  // Verify file size
  EXPECT_EQ(std::filesystem::file_size(target_file), DATALEN * 2 + 10);

  std::filesystem::remove(target_file);
}

TEST_F(TestTftp, HandleData_HandlesBlockNumberWrapAround)
{
  const auto target_file = filesystem::tmpname();
  std::filesystem::remove(target_file);
  auto siter = create_session();

  // Initialize WRQ session
  request req{.opc = WRQ, .mode = OCTET, .filename = target_file.c_str()};
  handle_request(req, siter);

  // Simulate being one before max block number
  siter->second.state.block_num = 0xFFFE;

  // Send block 0xFFFF
  std::vector<char> buffer1(sizeof(messages::data) + DATALEN);
  auto *data_msg1 = reinterpret_cast<messages::data *>(buffer1.data());
  data_msg1->opc = htons(DATA);
  data_msg1->block_num = htons(0xFFFF);
  std::fill(buffer1.begin() + sizeof(messages::data), buffer1.end(), 'A');

  auto result = handle_data(data_msg1, buffer1.size(), siter);
  EXPECT_EQ(result, 0);
  EXPECT_EQ(siter->second.state.block_num, 0xFFFF);

  // Send block 0 (wraps from 0xFFFF + 1)
  std::vector<char> buffer2(sizeof(messages::data) + 10);
  auto *data_msg2 = reinterpret_cast<messages::data *>(buffer2.data());
  data_msg2->opc = htons(DATA);
  data_msg2->block_num = htons(0);
  std::fill(buffer2.begin() + sizeof(messages::data), buffer2.end(), 'B');

  result = handle_data(data_msg2, buffer2.size(), siter);
  EXPECT_EQ(result, 0);
  EXPECT_EQ(siter->second.state.block_num, 0);

  std::filesystem::remove(target_file);
}

// NOLINTEND

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

#include <cstdlib>
#include <filesystem>
#include <gtest/gtest.h>

using namespace tftp::filesystem;

class MailDirectoryEnvTest : public ::testing::Test {};

TEST_F(MailDirectoryEnvTest, ReturnsCustomPathWhenEnvSet)
{
  const char *env_path = std::getenv("TFTP_MAIL_PREFIX");

  ASSERT_NE(env_path, nullptr)
      << "TFTP_MAIL_PREFIX must be set to run this test";
  ASSERT_EQ(std::string(env_path), "/custom/test/path")
      << "TFTP_MAIL_PREFIX must be set to '/custom/test/path'";

  const auto &path = mail_directory();

  EXPECT_EQ(path, std::filesystem::path("/custom/test/path"));
}

TEST_F(MailDirectoryEnvTest, ReturnsDefaultPathWhenEnvNotSet)
{
  const char *env_path = std::getenv("TFTP_MAIL_PREFIX");

  ASSERT_EQ(env_path, nullptr)
      << "TFTP_MAIL_PREFIX must NOT be set to run this test";

  const auto &path = mail_directory();

  EXPECT_EQ(path, std::filesystem::path("/var/spool/mail"));
}
// NOLINTEND

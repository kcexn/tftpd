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
#include "tftp/filesystem.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <system_error>

using namespace tftp::filesystem;

class TestFileSystem : public ::testing::Test {};

TEST_F(TestFileSystem, ClearsErrorCode)
{
  std::error_code err =
      std::make_error_code(std::errc::no_such_file_or_directory);
  temp_directory(err);
  EXPECT_FALSE(err);
}

TEST_F(TestFileSystem, ReturnsSamePath)
{
  std::error_code err;
  const auto &path1 = temp_directory(err);
  const auto &path2 = temp_directory(err);

  EXPECT_EQ(&path1, &path2);
}

TEST_F(TestFileSystem, CountReturnsSameReference)
{
  auto &count1 = count();
  auto &count2 = count();

  EXPECT_EQ(&count1, &count2);
}

TEST_F(TestFileSystem, NextReturnsTempFile)
{
  auto err = std::error_code();
  const auto &temp_dir = temp_directory(err);
  const auto path = tmpname();
  const auto filename = path.filename().string();

  EXPECT_TRUE(filename.starts_with("tftp."));
  EXPECT_EQ(path.parent_path(), temp_dir);
}

TEST_F(TestFileSystem, NextIncrementsCounter)
{
  const auto initial_count = count().load();
  const auto path1 = tmpname();
  const auto path2 = tmpname();
  const auto path3 = tmpname();

  EXPECT_NE(path1, path2);
  EXPECT_NE(path2, path3);
  EXPECT_NE(path1, path3);
  EXPECT_EQ(count().load(), initial_count + 3);
}

TEST_F(TestFileSystem, TouchCreatesNewFile)
{
  const auto path = tmpname();
  std::filesystem::remove(path);
  EXPECT_FALSE(std::filesystem::exists(path));

  const auto err = touch(path);

  EXPECT_FALSE(err);
  EXPECT_TRUE(std::filesystem::exists(path));

  std::filesystem::remove(path);
}

TEST_F(TestFileSystem, TouchSucceedsOnExistingFile)
{
  const auto path = tmpname();
  std::filesystem::remove(path);
  std::ofstream(path) << "existing content";

  const auto err = touch(path);

  EXPECT_FALSE(err);
  EXPECT_TRUE(std::filesystem::exists(path));

  std::filesystem::remove(path);
}

TEST_F(TestFileSystem, MailDirectoryReturnsSameReference)
{
  const auto &path1 = mail_directory();
  const auto &path2 = mail_directory();

  EXPECT_EQ(&path1, &path2);
}

TEST_F(TestFileSystem, MailDirectoryReturnsValidPath)
{
  const auto &path = mail_directory();

  EXPECT_FALSE(path.empty());
  EXPECT_TRUE(path.is_absolute());
}

TEST_F(TestFileSystem, OpenReadOpensFileForReading)
{
  const auto path = tmpname();
  std::ofstream(path) << "some data";

  std::error_code err;
  auto fstream = open_read(path, err);

  EXPECT_TRUE(fstream->is_open());
  EXPECT_FALSE(err);

  std::filesystem::remove(path);
}

TEST_F(TestFileSystem, OpenReadReturnsErrorOnNonExistentFile)
{
  const auto path = tmpname();
  std::filesystem::remove(path);

  std::error_code err;
  auto fstream = open_read(path, err);

  EXPECT_FALSE(fstream);
  EXPECT_TRUE(err);
}

TEST_F(TestFileSystem, OpenWriteOpensTempFileForWriting)
{
  const auto path = tmpname();
  std::filesystem::path tmp;
  std::error_code err;
  auto fstream = open_write(path, tmp, err);

  EXPECT_TRUE(fstream->is_open());
  EXPECT_FALSE(err);
  EXPECT_TRUE(std::filesystem::exists(tmp));

  std::filesystem::remove(path);
  std::filesystem::remove(tmp);
}

TEST_F(TestFileSystem, OpenWriteReturnsErrorOnUncreatableDestFile)
{
  const auto path = std::filesystem::path("/non_existent_dir/file");
  std::filesystem::path tmp;
  std::error_code err;
  auto fstream = open_write(path, tmp, err);

  EXPECT_FALSE(fstream);
  EXPECT_TRUE(err);
}

TEST_F(TestFileSystem, OpenWriteReturnsErrorOnUncreatableTempFile)
{
  std::error_code err;
  const auto path = tmpname();
  touch(path);
  std::filesystem::path tmp;

  count()--;

  std::filesystem::permissions(path, std::filesystem::perms::owner_read |
                                         std::filesystem::perms::group_read |
                                         std::filesystem::perms::others_read);

  auto target = std::filesystem::path("/tmp/test");
  auto fstream = open_write(target, tmp, err);

  EXPECT_FALSE(fstream);
  EXPECT_EQ(err, std::errc::permission_denied);

  std::filesystem::remove(path);
  std::filesystem::remove(target);
}
// NOLINTEND

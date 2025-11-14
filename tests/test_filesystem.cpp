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

TEST_F(TestFileSystem, MakeTmpCopiesFile)
{
  using namespace std::filesystem;

  std::error_code err;
  const auto from_path = tmpname();
  remove(from_path);

  std::ofstream(from_path) << "test content";

  auto to_path = std::filesystem::path();

  tmpfile_from(from_path, std::ios::in | std::ios::binary, to_path, err);

  EXPECT_FALSE(err);
  EXPECT_TRUE(std::filesystem::exists(to_path));
  EXPECT_TRUE(std::filesystem::equivalent(from_path, to_path, err) ||
              std::filesystem::file_size(from_path) ==
                  std::filesystem::file_size(to_path));

  remove(from_path);
  remove(to_path);

  std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

TEST_F(TestFileSystem, MakeTmpReturnsEmptyPathOnError)
{
  using namespace std::filesystem;

  std::error_code err;
  const auto nonexistent_path = tmpname();
  auto tmp = std::filesystem::path();

  const auto fstream =
      tmpfile_from(nonexistent_path, std::ios::in | std::ios::binary, tmp, err);

  EXPECT_TRUE(err);
  EXPECT_TRUE(tmp.empty());

  remove(nonexistent_path);
  remove(tmp);
}

TEST_F(TestFileSystem, MakeTmpHandlesOpenFailureAfterCopy)
{
  using namespace std::filesystem;

  std::error_code err;

  const auto from_path = tmpname();
  remove(from_path);

  std::ofstream(from_path) << "test content";

  permissions(from_path, std::filesystem::perms::owner_read,
              std::filesystem::perm_options::replace);

  auto to_path = std::filesystem::path();

  const auto fstream =
      tmpfile_from(from_path, std::ios::out | std::ios::binary, to_path, err);

  EXPECT_FALSE(fstream);
  EXPECT_EQ(err, std::errc::permission_denied);

  EXPECT_TRUE(to_path.empty() || !std::filesystem::exists(to_path));

  std::filesystem::permissions(from_path, std::filesystem::perms::owner_all,
                               std::filesystem::perm_options::replace);

  remove(from_path);
  remove(to_path);
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
// NOLINTEND

/* Copyright (C) 2025 Kevin Exton (kevin.exton@pm.me)
 *
 * Tftp is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Tftp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Tftp.  If not, see <https://www.gnu.org/licenses/>.
 */

// NOLINTBEGIN
#include "tftp/filesystem.hpp"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <system_error>

using namespace tftp::filesystem;

class TestFileManager : public ::testing::Test {};

TEST_F(TestFileManager, ClearsErrorCode)
{
  std::error_code err =
      std::make_error_code(std::errc::no_such_file_or_directory);
  temp_directory(err);
  EXPECT_FALSE(err);
}

TEST_F(TestFileManager, ReturnsSamePath)
{
  std::error_code err;
  const auto &path1 = temp_directory(err);
  const auto &path2 = temp_directory(err);

  EXPECT_EQ(&path1, &path2);
}

TEST_F(TestFileManager, CountReturnsSameReference)
{
  auto &count1 = count();
  auto &count2 = count();

  EXPECT_EQ(&count1, &count2);
}

TEST_F(TestFileManager, NextReturnsTempFile)
{
  auto err = std::error_code();
  const auto &temp_dir = temp_directory(err);
  const auto path = tmpname();
  const auto filename = path.filename().string();

  EXPECT_TRUE(filename.starts_with("tftp."));
  EXPECT_EQ(path.parent_path(), temp_dir);
}

TEST_F(TestFileManager, NextIncrementsCounter)
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

TEST_F(TestFileManager, MakeTmpCopiesFile)
{
  std::error_code err;
  const auto from_path = tmpname();
  std::ofstream(from_path) << "test content";

  auto to_path = std::filesystem::path();

  tmpfile_from(from_path, std::ios::in | std::ios::binary, to_path, err);

  EXPECT_FALSE(err);
  EXPECT_TRUE(std::filesystem::exists(to_path));
  EXPECT_TRUE(std::filesystem::equivalent(from_path, to_path, err) ||
              std::filesystem::file_size(from_path) ==
                  std::filesystem::file_size(to_path));

  std::filesystem::remove(from_path);
  std::filesystem::remove(to_path);
}

TEST_F(TestFileManager, MakeTmpReturnsEmptyPathOnError)
{
  std::error_code err;
  const auto nonexistent_path = tmpname();
  auto tmp = std::filesystem::path();

  const auto fstream =
      tmpfile_from(nonexistent_path, std::ios::in | std::ios::binary, tmp, err);

  EXPECT_TRUE(err);
  EXPECT_TRUE(tmp.empty());
}

TEST_F(TestFileManager, MakeTmpHandlesOpenFailureAfterCopy)
{
  std::error_code err;

  const auto from_path = tmpname();
  std::ofstream(from_path) << "test content";

  std::filesystem::permissions(from_path, std::filesystem::perms::owner_read,
                               std::filesystem::perm_options::replace);

  auto to_path = std::filesystem::path();

  const auto fstream =
      tmpfile_from(from_path, std::ios::out | std::ios::binary, to_path, err);

  EXPECT_FALSE(fstream);
  EXPECT_EQ(err, std::errc::permission_denied);

  EXPECT_TRUE(to_path.empty() || !std::filesystem::exists(to_path));

  std::filesystem::permissions(from_path, std::filesystem::perms::owner_all,
                               std::filesystem::perm_options::replace);
  std::filesystem::remove(from_path);
}

TEST_F(TestFileManager, TouchCreatesNewFile)
{
  const auto path = tmpname();
  EXPECT_FALSE(std::filesystem::exists(path));

  const auto err = touch(path);

  EXPECT_FALSE(err);
  EXPECT_TRUE(std::filesystem::exists(path));

  std::filesystem::remove(path);
}

TEST_F(TestFileManager, TouchSucceedsOnExistingFile)
{
  const auto path = tmpname();
  std::ofstream(path) << "existing content";

  const auto err = touch(path);

  EXPECT_FALSE(err);
  EXPECT_TRUE(std::filesystem::exists(path));

  std::filesystem::remove(path);
}
// NOLINTEND

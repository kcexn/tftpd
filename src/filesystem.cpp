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
/**
 * @file filesystem.cpp
 * @brief This file implements filesystem utilities.
 */
#include "tftp/filesystem.hpp"

#include <cstdio>
#include <format>
#include <system_error>
namespace tftp::filesystem {
auto count() noexcept -> std::atomic<std::uint16_t> &
{
  static auto count = std::atomic<std::uint16_t>(0);
  return count;
}

auto temp_directory(std::error_code &err) noexcept
    -> const std::filesystem::path &
{
  static const auto [path, error] = []() noexcept {
    auto init_err = std::error_code();
    auto path = std::filesystem::temp_directory_path(init_err);
    return std::pair{path, init_err};
  }();

  err = error;
  return path;
}

auto mail_directory() noexcept -> const std::filesystem::path &
{
  static const auto mail_path = []() noexcept {
    if (const char *path = std::getenv("TFTP_MAIL_PREFIX"))
      return std::filesystem::path(path);

    return std::filesystem::path("/var/spool/mail");
  }();

  return mail_path;
}

auto tmpname() -> std::filesystem::path
{
  std::error_code err;
  return (temp_directory(err) / prefix)
      .concat(std::format("{:05d}", count()++));
}

// NOLINTBEGIN(cppcoreguidelines-owning-memory)
auto touch(const std::filesystem::path &file) -> std::error_code
{
  auto *fstream = std::fopen(file.c_str(), "a");
  if (!fstream)
    return {errno, std::system_category()};

  (void)std::fclose(fstream);
  return {};
}
// NOLINTEND(cppcoreguidelines-owning-memory)

auto open_read(const std::filesystem::path &file,
               std::error_code &err) -> std::shared_ptr<std::fstream>
{
  err.clear();
  auto fstream =
      std::make_shared<std::fstream>(file, std::ios::in | std::ios::binary);
  if (!fstream->is_open())
  {
    if (!std::filesystem::exists(file, err))
    {
      err = std::make_error_code(std::errc::no_such_file_or_directory);
      return {};
    }

    err = std::make_error_code(std::errc::permission_denied);
    return {};
  }

  return fstream;
}

auto open_write(const std::filesystem::path &file, std::filesystem::path &tmp,
                std::error_code &err) -> std::shared_ptr<std::fstream>
{
  err.clear();
  err = touch(file);
  if (err)
    return {};

  tmp = tmpname();
  auto fstream = std::make_shared<std::fstream>(
      tmp, std::ios::out | std::ios::trunc | std::ios::binary);
  if (!fstream->is_open())
  {
    err = std::make_error_code(std::errc::permission_denied);
    return {};
  }

  return fstream;
}

} // namespace tftp::filesystem

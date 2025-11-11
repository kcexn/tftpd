/* Copyright (C) 2025 Kevin Exton (kevin.exton@pm.me)
 *
 * Echo is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Echo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Echo.  If not, see <https://www.gnu.org/licenses/>.
 */
/**
 * @file file_manager.cpp
 * @brief This file implements a file manager.
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

/** @brief Copies the src file into a new temporary file. */
auto tmpfile_from(const std::filesystem::path &copy_from,
                  const std::ios::openmode &mode,
                  std::filesystem::path &tmppath,
                  std::error_code &err) -> std::shared_ptr<std::fstream>
{
  err.clear();
  if ((mode & std::ios::out) && !std::filesystem::exists(copy_from, err) &&
      (err = touch(copy_from)))
  {
    return {};
  }

  auto tmp = tmpname();
  if (!std::filesystem::copy_file(copy_from, tmp, err))
    return {};

  auto fstream = std::make_shared<std::fstream>(tmp, mode);
  if (!fstream->is_open())
  {
    std::filesystem::remove(tmp);
    err = std::make_error_code(std::errc::permission_denied);
    return {};
  }

  tmppath = std::move(tmp);
  return fstream;
}

} // namespace tftp::filesystem

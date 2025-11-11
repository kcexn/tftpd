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
/**
 * @file filesystem.hpp
 * @brief This file declares functions for filesystem management.
 */
#pragma once
#ifndef TFTP_FILESYSTEM_HPP
#define TFTP_FILESYSTEM_HPP
#include <atomic>
#include <filesystem>
#include <fstream>
/** @brief For TFTP filesystem management. */
namespace tftp::filesystem {
/** @brief The temporary file prefix. */
constexpr auto prefix = "tftp.";
/** @brief The temporary file count. */
auto count() noexcept -> std::atomic<std::uint16_t> &;
/** @brief Returns the system defined temporary files directory. */
auto temp_directory(std::error_code &err) noexcept
    -> const std::filesystem::path &;
/** @brief Returns the next available temporary file. */
auto tmpname() -> std::filesystem::path;
/** @brief Touches a file. */
auto touch(const std::filesystem::path &file) -> std::error_code;
/** @brief Copies the src file into a new temporary file. */
auto tmpfile_from(const std::filesystem::path &copy_from,
                  const std::ios::openmode &mode,
                  std::filesystem::path &tmppath,
                  std::error_code &err) -> std::shared_ptr<std::fstream>;
} // namespace tftp::filesystem
#endif // TFTP_FILESYSTEM_HPP

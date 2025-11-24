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
/** @brief The temporary file prefix used for generating temporary filenames. */
constexpr auto prefix = "tftp.";

/**
 * @brief Returns a reference to the atomic counter for temporary file
 * generation.
 * @return Reference to an atomic uint16_t counter used for unique filename
 * generation.
 */
auto count() noexcept -> std::atomic<std::uint16_t> &;

/**
 * @brief Returns the system-defined temporary files directory.
 * @param err Output parameter for error reporting if directory cannot be
 * determined.
 * @return Const reference to the temporary directory path.
 */
auto temp_directory(std::error_code &err) noexcept
    -> const std::filesystem::path &;

/**
 * @brief Returns the application's mail directory.
 * @return Const reference to the mail directory path.
 */
auto mail_directory() noexcept -> const std::filesystem::path &;

/**
 * @brief Generates the next available temporary filename.
 * @return Path to a uniquely generated temporary file (not yet created).
 */
auto tmpname() -> std::filesystem::path;

/**
 * @brief Creates a file or updates its modification time if it exists.
 * @param file Path to the file to touch.
 * @return Error code indicating success or failure of the operation.
 */
auto touch(const std::filesystem::path &file) -> std::error_code;

/**
 * @brief Opens a file for reading.
 * @param file The file to open.
 * @param[out] err An error code that is cleared on success and set on error.
 * @returns A shared pointer to an open file stream.
 */
auto open_read(const std::filesystem::path &file,
               std::error_code &err) -> std::shared_ptr<std::fstream>;

/**
 * @brief Opens a file for writing.
 * @details Writing a file to disk involves writing data to a
 * temporary file then renaming it to the target destination.
 * @param file The file to open.
 * @param[in,out] tmp The path of the temporary file.
 * @param[out] err An error code that is cleared on success and set on error.
 * @returns A shared pointer to an open file stream.
 */
auto open_write(const std::filesystem::path &file, std::filesystem::path &tmp,
                std::error_code &err) -> std::shared_ptr<std::fstream>;
} // namespace tftp::filesystem
#endif // TFTP_FILESYSTEM_HPP

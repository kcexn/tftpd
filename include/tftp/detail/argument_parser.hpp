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
 * @file argument_parser.hpp
 * @brief This file declares a CLI argument parser.
 */
#pragma once
#ifndef TFTP_ARGUMENT_PARSER_HPP
#define TFTP_ARGUMENT_PARSER_HPP
#include "generator.hpp"

#include <span>
#include <string_view>
/** @brief For internal tftp server implementation details. */
namespace tftp::detail {
/** @brief A command line argument parser. */
struct argument_parser {
  /** @brief Command-line arguments are parsed into options. */
  struct option {
    /** @brief option flag. */
    std::string_view flag;
    /** @brief option value. */
    std::string_view value;
  };
  /**
   * @brief Parse all command-line arguments.
   * @param args The command line arguments to parse.
   * @returns A generator of options.
   */
  static auto parse(std::span<char const *const> args) -> generator<option>;
  /**
   * @brief Parse all command-line arguments.
   * @param argc The number of command-line arguments.
   * @param argv The command-line arguments.
   * @returns A generator of options.
   */
  static auto parse(int argc, char const *const *argv) -> generator<option>
  {
    return parse({argv, static_cast<std::size_t>(argc)});
  }
};
} // namespace tftp::detail
#endif // TFTP_ARGUMENT_PARSER_HPP

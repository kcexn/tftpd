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
 * @file argument_parser.cpp
 * @brief This file implements a CLI argument parser.
 */
#include "tftp/detail/argument_parser.hpp"

#include <algorithm>
namespace tftp::detail {

struct parser_impl {
  using option_type = argument_parser::option;

  auto next() noexcept -> argument_parser::option
  {
    auto opt = option_type{};
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    for (; current != end; ++current)
    {
      auto token = std::string_view(*current);
      if (token[0] == '-') // short option.
      {
        if (!opt.flag.empty() || !opt.value.empty())
          return opt;

        opt.flag = token;

        if (token.size() > 2 && token[1] == '-') // long option.
        {
          const auto *delim = std::ranges::find(token, '=');
          if (delim != token.cend())
          {
            opt.flag = {token.cbegin(), delim};
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            opt.value = {++delim, token.cend()};
          }
        }
      }
      else
      {
        if (!opt.flag.empty() &&
            // NOLINTNEXTLINE(readability-identifier-length)
            std::ranges::all_of(opt.flag, [](char ch) { return ch == '-'; }))
        {
          return opt;
        }

        if (!opt.value.empty())
          return opt;

        opt.value = std::string_view(*current);
      }
    }
    return opt;
  }

  [[nodiscard]] explicit operator bool() const noexcept
  {
    return current < end;
  }

  char const *const *current;
  char const *const *end;
};

auto argument_parser::parse(std::span<char const *const> args)
    -> generator<option>
{
  auto parser =
      parser_impl{.current = args.data() + 1, .end = args.data() + args.size()};
  while (parser)
  {
    co_yield parser.next();
  }
}
} // namespace tftp::detail

/* Copyright (C) 2025 Kevin Exton (kevin.exton@pm.me)
 *
 * Cloudbus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cloudbus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Cloudbus.  If not, see <https://www.gnu.org/licenses/>.
 */

// NOLINTBEGIN
#include "tftp/detail/argument_parser.hpp"

#include <gtest/gtest.h>

#include <list>

using namespace tftp::detail;

struct TestData {
  int argc;
  const char **argv;

  struct option_type {
    const char *flag;
    const char *value;
  };

  std::list<option_type> expected_options;
};

class ArgParseTest : public ::testing::TestWithParam<TestData> {};

TEST_P(ArgParseTest, ParseArgs)
{
  const auto &data = GetParam();
  auto parser = argument_parser();

  auto expect = data.expected_options.begin();
  auto end = data.expected_options.end();
  for (const auto &option : parser.parse(data.argc, data.argv))
  {
    if (expect == end)
      throw std::range_error("More options than expected.");

    EXPECT_EQ(option.flag, expect->flag);
    EXPECT_EQ(option.value, expect->value);

    expect++;
  }
}

INSTANTIATE_TEST_SUITE_P(
    ArgParseTestCases, ArgParseTest,
    ::testing::Values(
        TestData{2, new const char *[2]{"test", "-h"}, {{"-h", ""}}},
        TestData{2, new const char *[2]{"test", "--help"}, {{"--help", ""}}},
        TestData{2, new const char *[2]{"test", "-help"}, {{"-help", ""}}},
        TestData{2,
                 new const char *[2]{"test", "--port=8080"},
                 {{"--port", "8080"}}},
        TestData{
            3, new const char *[3]{"test", "-p", "8080"}, {{"-p", "8080"}}},
        TestData{3,
                 new const char *[3]{"test", "-p", "-v"},
                 {{"-p", ""}, {"-v", ""}}},
        TestData{
            3, new const char *[3]{"test", "-", "-v"}, {{"-", ""}, {"-v", ""}}},
        TestData{3,
                 new const char *[3]{"test", "--", "-v"},
                 {{"--", ""}, {"-v", ""}}},
        TestData{3,
                 new const char *[3]{"test", "--", "8080"},
                 {{"--", ""}, {"", "8080"}}},
        TestData{3,
                 new const char *[3]{"test", "8080", "-p"},
                 {{"", "8080"}, {"-p", ""}}},
        TestData{5,
                 new const char *[5]{"test", "-v", "--ports", "8080", "8081"},
                 {{"-v", ""}, {"--ports", "8080"}, {"", "8081"}}}));

// NOLINTEND

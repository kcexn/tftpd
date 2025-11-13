/* Copyright (C) 2025 Kevin Exton (kevin.exton@pm.me)
 *
 * This file is part of tftp.
 *
 * tftp is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * tftp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with tftp.  If not, see <https://www.gnu.org/licenses/>.
 */
// NOLINTBEGIN
#include "tftp/protocol/tftp_protocol.hpp"

#include <gtest/gtest.h>

using namespace tftp;

TEST(TFTP_Protocol_Test, ErrStrCoverage)
{
  using enum messages::error_t;

  EXPECT_STREQ(errors::errstr(ACCESS_VIOLATION).data(), "Access violation.");
  EXPECT_STREQ(errors::errstr(FILE_NOT_FOUND).data(), "File not found.");
  EXPECT_STREQ(errors::errstr(DISK_FULL).data(), "Disk full.");
  EXPECT_STREQ(errors::errstr(NO_SUCH_USER).data(), "No such user.");
  EXPECT_STREQ(errors::errstr(UNKNOWN_TID).data(), "Unknown TID.");
  EXPECT_STREQ(errors::errstr(ILLEGAL_OPERATION).data(), "Illegal operation.");
  EXPECT_STREQ(errors::errstr(TIMED_OUT).data(), "Timed out.");
  EXPECT_STREQ(errors::errstr(NOT_DEFINED).data(), "Not defined.");
  EXPECT_STREQ(errors::errstr(FILE_ALREADY_EXISTS).data(),
               "File already exists.");
  EXPECT_STREQ(errors::errstr(99).data(), "Not defined.");
}
// NOLINTEND

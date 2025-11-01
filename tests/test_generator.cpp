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
#include "tftp/detail/generator.hpp"

#include <gtest/gtest.h>
#include <stdexcept>
#include <vector>

using namespace tftp::detail;

class GeneratorTest : public ::testing::Test {};

TEST_F(GeneratorTest, GeneratesCorrectSequence)
{
  constexpr int count = 5;
  constexpr auto iota = [](int count) -> generator<int> {
    for (int i = 0; i < count; ++i)
    {
      co_yield i;
    }
  };

  auto gen = iota(count);

  std::vector<int> yielded_values;
  for (int value : gen)
  {
    yielded_values.push_back(value);
  }

  ASSERT_EQ(yielded_values.size(), count);
  for (int i = 0; i < count; ++i)
  {
    EXPECT_EQ(yielded_values[i], i);
  }
}

TEST_F(GeneratorTest, ThrowsException)
{
  constexpr auto iota_with_exception = []() -> generator<int> {
    co_yield 1;
    co_yield 2;
    throw std::runtime_error("Generator exception");
    co_yield 3; // This should not be reached
  };

  auto gen = iota_with_exception();
  auto it = gen.begin();

  EXPECT_EQ(*it, 1);
  it++;
  EXPECT_EQ(*it, 2);

  EXPECT_THROW(++it, std::runtime_error);
}

TEST_F(GeneratorTest, ThrowsExceptionOnBegin)
{
  constexpr auto iota_with_exception = []() -> generator<int> {
    throw std::runtime_error("Generator exception");
    co_yield 0;
  };

  auto gen = iota_with_exception();
  EXPECT_THROW(gen.begin(), std::runtime_error);
}

TEST_F(GeneratorTest, EmptyGenerator)
{
  auto gen = generator<int>();
  EXPECT_EQ(gen.begin(), gen.end());
  using std::swap;
  swap(gen, gen);
  EXPECT_EQ(gen.begin(), gen.end());
}

TEST_F(GeneratorTest, MoveConstructor)
{
  constexpr int count = 5;
  constexpr auto iota = [](int count) -> generator<int> {
    for (int i = 0; i < count; ++i)
    {
      co_yield i;
    }
  };
  auto gen = iota(count);
  auto gen2 = std::move(gen);

  std::vector<int> yielded_values;
  for (int value : gen2)
  {
    yielded_values.push_back(value);
  }

  ASSERT_EQ(yielded_values.size(), count);
  for (int i = 0; i < count; ++i)
  {
    EXPECT_EQ(yielded_values[i], i);
  }
}

TEST_F(GeneratorTest, MoveAssignment)
{
  constexpr int count = 5;
  constexpr auto iota = [](int count) -> generator<int> {
    for (int i = 0; i < count; ++i)
    {
      co_yield i;
    }
  };
  auto gen = iota(count);
  auto gen2 = generator<int>();
  gen2 = std::move(gen);

  std::vector<int> yielded_values;
  for (int value : gen2)
  {
    yielded_values.push_back(value);
  }

  ASSERT_EQ(yielded_values.size(), count);
  for (int i = 0; i < count; ++i)
  {
    EXPECT_EQ(yielded_values[i], i);
  }
}

TEST_F(GeneratorTest, ArrowException)
{
  struct Point {
    int x;
    int y;
  };

  constexpr auto get_points = []() -> generator<Point> {
    co_yield Point{1, 2};
    throw std::runtime_error("Arrow Exception.");
    co_yield Point{3, 4};
  };

  auto gen = get_points();
  auto it = gen.begin();

  ASSERT_NE(it, gen.end());
  EXPECT_EQ(it->x, 1);
  EXPECT_EQ(it->y, 2);

  EXPECT_THROW(++it, std::runtime_error);
}
// NOLINTEND

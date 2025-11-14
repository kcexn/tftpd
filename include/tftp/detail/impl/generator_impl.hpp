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
 * @file generator_impl.hpp
 * @brief This file defines a generator coroutine.
 */
#pragma once
#ifndef TFTP_GENERATOR_IMPL_HPP
#define TFTP_GENERATOR_IMPL_HPP
#include "tftp/detail/generator.hpp"
namespace tftp::detail {

template <typename T>
auto generator<T>::promise_type::get_return_object() noexcept -> generator<T>
{
  return generator<T>{coroutine_handle::from_promise(*this)};
}

template <typename T>
[[nodiscard]] constexpr auto generator<T>::promise_type::initial_suspend()
    const noexcept -> std::suspend_always
{
  return {};
}

template <typename T>
[[nodiscard]] constexpr auto generator<T>::promise_type::final_suspend()
    const noexcept -> std::suspend_always
{
  return {};
}

template <typename T>
template <typename U>
  requires std::is_same_v<std::remove_reference_t<U>,
                          typename generator<T>::value_type>
// NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
auto generator<T>::promise_type::yield_value(U &&value) noexcept
    -> std::suspend_always
{
  value_ = std::addressof(value);
  return {};
}

template <typename T>
auto generator<T>::promise_type::unhandled_exception() noexcept -> void
{
  exception_ = std::current_exception();
}

template <typename T>
auto generator<T>::promise_type::value() const noexcept -> reference_type
{
  return static_cast<reference_type>(*value_);
}

template <typename T>
auto generator<T>::promise_type::rethrow_if_exception() -> void
{
  if (exception_)
    std::rethrow_exception(exception_);
}

template <typename T>
generator<T>::iterator::iterator(coroutine_handle coro) noexcept
    : coroutine_{std::move(coro)}
{}

template <typename T>
auto generator<T>::iterator::operator==(sentinel_type /*s*/) const noexcept
    -> bool
{
  return !coroutine_ || coroutine_.done();
}

template <typename T> auto generator<T>::iterator::operator++() -> iterator &
{
  coroutine_.resume();
  if (coroutine_.done())
    coroutine_.promise().rethrow_if_exception();

  return *this;
}

template <typename T> auto generator<T>::iterator::operator++(int) -> void
{
  ++(*this);
}

template <typename T>
auto generator<T>::iterator::operator*() const noexcept -> reference_type
{
  return coroutine_.promise().value();
}

template <typename T>
auto generator<T>::iterator::operator->() const noexcept -> pointer_type
{
  return std::addressof(operator*());
}

template <typename T>
generator<T>::generator(generator &&other) noexcept : generator()
{
  using std::swap;
  swap(*this, other);
}

template <typename T>
auto generator<T>::operator=(generator &&other) noexcept -> generator &
{
  using std::swap;
  swap(*this, other);
  return *this;
}

template <typename T> auto generator<T>::begin() -> iterator
{
  if (coroutine_)
  {
    coroutine_.resume();
    if (coroutine_.done())
      coroutine_.promise().rethrow_if_exception();
  }

  return iterator{coroutine_};
}

template <typename T>
[[nodiscard]] constexpr auto generator<T>::end() const noexcept -> sentinel_type
{
  return {};
}

template <typename T> generator<T>::~generator()
{
  if (coroutine_)
    coroutine_.destroy();
}

template <typename T>
generator<T>::generator(coroutine_handle coro) noexcept
    : coroutine_{std::move(coro)}
{}
} // namespace tftp::detail
#endif // TFTP_GENERATOR_IMPL_HPP

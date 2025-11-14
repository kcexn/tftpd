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
 * @file generator.hpp
 * @brief This file defines a generator coroutine.
 */
#pragma once
#ifndef TFTP_GENERATOR_HPP
#define TFTP_GENERATOR_HPP
#include <coroutine>
#include <exception>
#include <iterator>
#include <type_traits>
/** @brief For internal tftp server implementation details. */
namespace tftp::detail {
/**
 * @brief A C++20 coroutine-based generator for creating iterable sequences.
 *
 * @details This class implements a generator that can be used to create a lazy,
 * iterable sequence of values using coroutine mechanics (`co_yield`). It is a
 * move-only type, and it is designed to be used with range-based for loops.
 *
 * @tparam T The type of value that the generator will yield. This can be a
 * value, a reference, or a const reference.
 */
template <typename T> struct generator {
  /** @brief The promise type for the generator coroutine. */
  class promise_type;

public:
  /** @brief The type of the value yielded by the generator. */
  using value_type = std::remove_reference_t<T>;
  /** @brief The reference type of the value yielded by the generator. */
  using reference_type = std::conditional_t<std::is_reference_v<T>, T, T &>;
  /** @brief The pointer type of the value yielded by the generator. */
  using pointer_type = value_type *;
  /** @brief The sentinel type used to mark the end of the sequence. */
  using sentinel_type = std::default_sentinel_t;
  /** @brief The handle to the generator's coroutine frame. */
  using coroutine_handle = std::coroutine_handle<promise_type>;

  /**
   * @brief The promise type for the generator coroutine.
   * @details This class defines the behavior of the generator coroutine,
   * handling the communication between the coroutine and its caller.
   */
  class promise_type {
  public:
    /**
     * @brief Creates the generator object from the promise.
     * @return The generator object.
     */
    auto get_return_object() noexcept -> generator<T>;

    /**
     * @brief Suspends the coroutine immediately upon starting.
     * @return A suspend_always object to suspend execution.
     */
    [[nodiscard]] constexpr auto
    initial_suspend() const noexcept -> std::suspend_always;

    /**
     * @brief Suspends the coroutine when it finishes execution.
     * @return A suspend_always object to suspend execution.
     */
    [[nodiscard]] constexpr auto
    final_suspend() const noexcept -> std::suspend_always;

    /**
     * @brief Handles a `co_yield` expression from the coroutine.
     *
     * @note Coroutines extend the lifetime of temporary values
     * like `prvalues` until the coroutine is resumed or destroyed.
     *
     * @param value The value being yielded.
     * @return A suspend_always object to suspend execution and return to the
     * caller.
     */
    template <typename U>
      requires std::is_same_v<std::remove_reference_t<U>, value_type>
    auto yield_value(U &&value) noexcept -> std::suspend_always;

    /** @brief Handles any unhandled exceptions within the coroutine. */
    auto unhandled_exception() noexcept -> void;

    /** @brief Called when the coroutine executes `co_return;`. */
    constexpr auto return_void() const noexcept -> void {}

    /**
     * @brief Gets the currently yielded value.
     * @return A reference to the yielded value.
     */
    auto value() const noexcept -> reference_type;

    /**
     * @brief Deleted await_transform.
     * @details Don't allow 'co_await' inside a generator coroutine.
     */
    auto await_transform(T &&value) -> std::suspend_never = delete;

    /** @brief Rethrows any exception caught by the coroutine. */
    auto rethrow_if_exception() -> void;

  private:
    /** @brief Pointer to the currently yielded value. */
    pointer_type value_{nullptr};
    /** @brief Holds any exception thrown by the coroutine. */
    std::exception_ptr exception_{nullptr};
  };

  /**
   * @brief An input iterator for the generator.
   * @details This allows the generator to be used in range-based for loops and
   * with standard library algorithms.
   */
  class iterator {
  public:
    /** @brief The category of iterator */
    using iterator_category = std::input_iterator_tag;
    /** @brief The type of differences between iterators. */
    using difference_type = std::ptrdiff_t;
    /** @brief The type of the value pointed to by the iterator. */
    using value_type = generator::value_type;
    /** @brief The reference type of the value pointed to by the iterator. */
    using reference_type = generator::reference_type;
    /** @brief The pointer type of the value pointed to by the iterator. */
    using pointer_type = generator::pointer_type;

    /** @brief Default constructor. */
    constexpr iterator() noexcept = default;

    /**
     * @brief Constructs an iterator from a coroutine handle.
     * @param coro The coroutine handle.
     */
    explicit iterator(coroutine_handle coro) noexcept;

    /**
     * @brief Compares the iterator with the end-of-sequence sentinel.
     * @param sentinel The sentinel to compare against
     * @return True if the iterator has reached the end, false otherwise.
     */
    auto operator==(sentinel_type sentinel) const noexcept -> bool;

    /**
     * @brief Pre-increments the iterator to the next element.
     * @return A reference to the advanced iterator.
     */
    auto operator++() -> iterator &;

    /**
     * @brief Post-increments the iterator to the next element.
     *
     * @details Coroutine handles point to shared-state so this operator
     * can't return a copy of the iterator prior to the increment. To
     * get post-increment like behavior, you must explicitly dereference,
     * then increment.
     *
     * @par Example:
     * @code{.cpp}
     * auto it = begin();
     * auto value = *it;
     * it++;
     * @endcode
     */
    auto operator++(int) -> void;

    /**
     * @brief Dereferences the iterator to get the current value.
     * @return A reference to the current value.
     */
    auto operator*() const noexcept -> reference_type;

    /**
     * @brief Accesses the current value through a pointer.
     * @return A pointer to the current value.
     */
    auto operator->() const noexcept -> pointer_type;

  private:
    /** @brief The handle to the generator's coroutine frame. */
    coroutine_handle coroutine_{nullptr};
  };

  /** @brief Default constructor. */
  generator() noexcept = default;

  /** @brief Deleted copy constructor. Generators are move-only. */
  generator(const generator &other) = delete;

  /**
   * @brief Move constructor.
   * @param other The generator to move from.
   */
  generator(generator &&other) noexcept;

  /** @brief Deleted copy assignment operator. */
  auto operator=(const generator &other) noexcept -> generator & = delete;

  /**
   * @brief Move assignment operator.
   * @param other The generator to move from.
   * @return A reference to this generator.
   */
  auto operator=(generator &&other) noexcept -> generator &;

  /**
   * @brief Gets an iterator to the beginning of the generated sequence.
   * @return An iterator to the first element.
   */
  auto begin() -> iterator;

  /**
   * @brief Gets a sentinel marking the end of the generated sequence.
   * @return A sentinel object.
   */
  [[nodiscard]] constexpr auto end() const noexcept -> sentinel_type;

  /** @brief Destructor. */
  ~generator();

private:
  /**
   * @brief Swaps two generator objects.
   * @param lhs The left-hand side generator.
   * @param rhs The right-hand side generator.
   */
  friend auto swap(generator &lhs, generator &rhs) noexcept -> void
  {
    using std::swap;
    if (&lhs == &rhs)
      return;

    swap(lhs.coroutine_, rhs.coroutine_);
  }

  /**
   * @brief Private constructor used by the promise_type.
   * @param coro The coroutine handle.
   */
  explicit generator(coroutine_handle coro) noexcept;

  /** @brief The handle to the generator's coroutine frame. */
  coroutine_handle coroutine_{nullptr};
};

} // namespace tftp::detail

#include "impl/generator_impl.hpp" // IWYU pragma: export

#endif // TFTP_GENERATOR_HPP

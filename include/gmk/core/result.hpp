// GameMemoryKit — Result.
//
// A small `std::expected`-style value/error union used instead of exceptions
// for expected failures (bad input, missing processes, permission errors,
// malformed binaries, ...). Accessing the value of a failed Result is a
// programming error; call `ok()` first.
//
// Thread safety: Result is a value type; sharing a single Result between
// threads without synchronization is not safe (same as any mutable object).
//
// SPDX-License-Identifier: MIT

#pragma once

#include <cassert>
#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

#include "gmk/core/error.hpp"

namespace gmk {

/// A value or an Error. Never both, never neither.
template <typename T>
class [[nodiscard]] Result {
public:
    using value_type = T;
    using error_type = Error;

    /// Constructs a successful Result holding `value`.
    template <typename U = T,
              typename = std::enable_if_t<std::is_constructible_v<T, U &&>>>
    Result(U&& value)  // NOLINT(google-explicit-constructor) — implicit is intended
        : has_value_(true)
    {
        new (&storage_) T(std::forward<U>(value));
    }

    /// Constructs a failed Result.
    Result(Error error)  // NOLINT(google-explicit-constructor) — implicit is intended
        : error_(std::move(error)), has_value_(false)
    {
    }

    Result(const Result& other) : has_value_(other.has_value_)
    {
        if (has_value_) {
            new (&storage_) T(other.value_ref());
        } else {
            new (&error_) Error(other.error_);
        }
    }

    Result(Result&& other) noexcept(
        std::is_nothrow_move_constructible_v<T>)
        : has_value_(other.has_value_)
    {
        if (has_value_) {
            new (&storage_) T(std::move(other.value_ref()));
        } else {
            new (&error_) Error(std::move(other.error_));
        }
    }

    ~Result()
    {
        if (has_value_) {
            value_ref().~T();
        } else {
            error_.~Error();
        }
    }

    Result& operator=(const Result& other)
    {
        if (this == &other) {
            return *this;
        }
        this->~Result();
        new (this) Result(other);
        return *this;
    }

    Result& operator=(Result&& other) noexcept(
        std::is_nothrow_move_constructible_v<T>)
    {
        if (this == &other) {
            return *this;
        }
        this->~Result();
        new (this) Result(std::move(other));
        return *this;
    }

    /// True when the Result holds a value.
    [[nodiscard]] bool ok() const noexcept { return has_value_; }
    [[nodiscard]] bool has_value() const noexcept { return has_value_; }
    [[nodiscard]] bool failed() const noexcept { return !has_value_; }

    /// The contained value. Undefined behavior if `!ok()`.
    [[nodiscard]] T& value() noexcept
    {
        assert(has_value_ && "Result::value() called on a failed Result");
        return value_ref();
    }

    /// The contained value. Undefined behavior if `!ok()`.
    [[nodiscard]] const T& value() const noexcept
    {
        assert(has_value_ && "Result::value() called on a failed Result");
        return value_ref();
    }

    [[nodiscard]] T& operator*() noexcept { return value(); }
    [[nodiscard]] const T& operator*() const noexcept { return value(); }

    [[nodiscard]] T* operator->() noexcept { return &value(); }
    [[nodiscard]] const T* operator->() const noexcept { return &value(); }

    /// The error. Undefined behavior if `ok()`.
    [[nodiscard]] const Error& error() const noexcept
    {
        assert(!has_value_ && "Result::error() called on a successful Result");
        return error_;
    }

    /// Returns the value, or `fallback` when the Result failed.
    template <typename U>
    [[nodiscard]] T value_or(U&& fallback) const&
    {
        return has_value_ ? value_ref() : static_cast<T>(std::forward<U>(fallback));
    }

    template <typename U>
    [[nodiscard]] T value_or(U&& fallback) &&
    {
        return has_value_ ? std::move(value_ref())
                          : static_cast<T>(std::forward<U>(fallback));
    }

    /// Maps a successful value with `fn`, forwarding failures unchanged.
    template <typename Fn>
    auto map(Fn&& fn) const& -> Result<std::invoke_result_t<Fn&, const T&>>
    {
        using U = std::invoke_result_t<Fn&, const T&>;
        if (has_value_) {
            return U{std::forward<Fn>(fn)(value_ref())};
        }
        return error_;
    }

    // Factories.
    static Result ok(T value) { return Result{std::move(value)}; }
    static Result err(Error error) { return Result{std::move(error)}; }

private:
    const T& value_ref() const noexcept { return *std::launder(reinterpret_cast<const T*>(&storage_)); }
    T& value_ref() noexcept { return *std::launder(reinterpret_cast<T*>(&storage_)); }

    union {
        alignas(T) std::byte storage_[sizeof(T)];
        Error error_;
    };
    bool has_value_{false};
};

/// Specialization for void: a pure success/failure status.
template <>
class [[nodiscard]] Result<void> {
public:
    Result() = default;  // success
    Result(Error error) : error_(std::move(error)), failed_(true) {}  // NOLINT

    [[nodiscard]] bool ok() const noexcept { return !failed_; }
    [[nodiscard]] bool has_value() const noexcept { return !failed_; }
    [[nodiscard]] bool failed() const noexcept { return failed_; }

    /// The error. Undefined behavior if `ok()`.
    [[nodiscard]] const Error& error() const noexcept
    {
        assert(failed_ && "Result<void>::error() called on a successful Result");
        return error_;
    }

    // Named differently from the primary template's ok()/err() because the
    // parameter-less success case would collide with the member ok().
    static Result success() { return Result<void>{}; }
    static Result failure(Error error) { return Result<void>{std::move(error)}; }

private:
    Error error_;
    bool failed_{false};
};

/// Convenience alias for the void specialization.
using Status = Result<void>;

}  // namespace gmk

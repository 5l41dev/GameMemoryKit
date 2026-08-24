// GameMemoryKit — Address.
//
// A safe, explicit wrapper around a raw memory address. The class exists so
// that raw-address arithmetic is never implicit: adding offsets, converting to
// pointers, and comparing addresses all require deliberate, visible code.
//
// A null (zero) address is treated as "invalid" and arithmetic that would
// overflow or underflow yields a null address instead of silently wrapping.
//
// Thread safety: an Address is an immutable value type and is safe to share
// between threads.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

namespace gmk {

/// A memory address.
///
/// Wraps a `std::uintptr_t`. A value of zero means "no address" and is the
/// only representation of an invalid address.
class Address {
public:
    /// Constructs a null (invalid) address.
    constexpr Address() noexcept = default;

    /// Wraps a raw address value.
    constexpr explicit Address(std::uintptr_t value) noexcept : value_(value) {}

    /// Wraps a raw pointer.
    template <typename T>
    constexpr explicit Address(T* pointer) noexcept
        : value_(reinterpret_cast<std::uintptr_t>(pointer))
    {
    }

    /// Constructs an address from an integral type other than `uintptr_t`.
    /// This is `constexpr`-safe and avoids narrowing surprises.
    template <typename Int,
              typename = std::enable_if_t<std::is_integral_v<Int> &&
                                          !std::is_same_v<Int, std::uintptr_t>>>
    constexpr explicit Address(Int value) noexcept
        : value_(static_cast<std::uintptr_t>(static_cast<std::int64_t>(value)))
    {
    }

    /// The raw address value. Use with care — prefer the arithmetic helpers.
    [[nodiscard]] constexpr std::uintptr_t value() const noexcept { return value_; }

    /// True when the address is non-null.
    [[nodiscard]] constexpr bool valid() const noexcept { return value_ != 0; }

    /// True when the address is null.
    [[nodiscard]] constexpr bool is_null() const noexcept { return value_ == 0; }

    /// Returns this address plus `offset` (which may be negative).
    /// Returns a null address if the result would underflow or overflow.
    [[nodiscard]] constexpr Address add(std::ptrdiff_t offset) const noexcept
    {
        if (offset >= 0) {
            return add(static_cast<std::size_t>(offset));
        }
        const std::uintptr_t magnitude = static_cast<std::uintptr_t>(-(offset + 1)) + 1;
        if (value_ < magnitude) {
            return Address{};
        }
        return Address{value_ - magnitude};
    }

    /// Returns this address plus a non-negative `offset`.
    /// Returns a null address if the result would overflow.
    [[nodiscard]] constexpr Address add(std::size_t offset) const noexcept
    {
        if (value_ > std::numeric_limits<std::uintptr_t>::max() - offset) {
            return Address{};
        }
        return Address{value_ + offset};
    }

    /// Returns the signed difference `this - other`.
    [[nodiscard]] constexpr std::ptrdiff_t difference(Address other) const noexcept
    {
        return static_cast<std::ptrdiff_t>(value_ - other.value_);
    }

    /// Aligns the address down to a power-of-two `alignment`.
    [[nodiscard]] constexpr Address align_down(std::size_t alignment) const noexcept
    {
        if (alignment == 0) {
            return Address{};
        }
        return Address{value_ & ~(static_cast<std::uintptr_t>(alignment) - 1)};
    }

    /// Aligns the address up to a power-of-two `alignment`.
    /// Returns a null address on overflow.
    [[nodiscard]] constexpr Address align_up(std::size_t alignment) const noexcept
    {
        if (alignment == 0) {
            return Address{};
        }
        const std::uintptr_t mask = static_cast<std::uintptr_t>(alignment) - 1;
        if (value_ > std::numeric_limits<std::uintptr_t>::max() - mask) {
            return Address{};
        }
        return Address{(value_ + mask) & ~mask};
    }

    constexpr Address& operator+=(std::ptrdiff_t offset) noexcept
    {
        *this = add(offset);
        return *this;
    }

    constexpr Address& operator-=(std::ptrdiff_t offset) noexcept
    {
        *this = add(-offset);
        return *this;
    }

    /// Converts to a typed pointer. The caller is responsible for ensuring the
    /// address is valid and correctly aligned for `T`.
    template <typename T>
    [[nodiscard]] T* as_ptr() const noexcept
    {
        return reinterpret_cast<T*>(value_);
    }

    /// Three-way comparison (C++20).
    friend constexpr auto operator<=>(Address lhs, Address rhs) noexcept = default;

    friend constexpr bool operator==(Address lhs, Address rhs) noexcept = default;

    friend constexpr Address operator+(Address lhs, std::ptrdiff_t rhs) noexcept
    {
        return lhs.add(rhs);
    }

    friend constexpr Address operator-(Address lhs, std::ptrdiff_t rhs) noexcept
    {
        return lhs.add(-rhs);
    }

private:
    std::uintptr_t value_{};
};

static_assert(std::is_trivially_copyable_v<Address>);

/// Formats an address as lowercase hexadecimal, e.g. `0x7ff6a1b2c000`.
[[nodiscard]] std::string to_string(Address address);

inline std::ostream& operator<<(std::ostream& out, Address address)
{
    return out << to_string(address);
}

}  // namespace gmk

namespace std {

template <>
struct hash<gmk::Address> {
    size_t operator()(gmk::Address address) const noexcept
    {
        return hash<uintptr_t>{}(address.value());
    }
};

}  // namespace std

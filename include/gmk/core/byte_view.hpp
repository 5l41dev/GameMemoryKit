// GameMemoryKit — byte views and buffer helpers.
//
// A thin layer over std::span providing the byte-oriented views used across
// the library (binary parsing, pattern scanning, memory reads).
//
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <span>
#include <string>
#include <string_view>

namespace gmk {

/// A non-owning view over immutable bytes.
using ByteView = std::span<const std::byte>;

/// A non-owning view over mutable bytes.
using MutableByteView = std::span<std::byte>;

/// Interprets a span of trivially-copyable elements as a byte view.
template <typename T>
constexpr ByteView as_bytes(const std::span<T>& span) noexcept
{
    return ByteView{reinterpret_cast<const std::byte*>(span.data()),
                    span.size() * sizeof(T)};
}

template <typename T>
constexpr MutableByteView as_mutable_bytes(std::span<T> span) noexcept
{
    return MutableByteView{reinterpret_cast<std::byte*>(span.data()),
                           span.size() * sizeof(T)};
}

/// Creates a byte view from a string literal / std::string_view.
inline ByteView as_bytes(std::string_view text) noexcept
{
    return ByteView{reinterpret_cast<const std::byte*>(text.data()), text.size()};
}

/// Converts a byte view to a string_view (no copy).
inline std::string_view as_string_view(ByteView bytes) noexcept
{
    return std::string_view{reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

/// Formats bytes as lowercase hex, e.g. `48 8b c0` (spaces between bytes).
[[nodiscard]] std::string hex_string(ByteView bytes);

/// Formats bytes as uppercase hex without separators.
[[nodiscard]] std::string hex_string_dense(ByteView bytes);

}  // namespace gmk

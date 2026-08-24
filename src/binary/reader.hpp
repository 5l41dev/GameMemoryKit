// Internal bounds-checked reader used by the binary parsers.
// Not installed; not part of the public API.

#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "gmk/core/byte_view.hpp"
#include "gmk/core/error.hpp"
#include "gmk/core/result.hpp"

namespace gmk::binary::detail {

/// A read-only cursor over a byte buffer with explicit bounds checks on
/// every access. Offsets from untrusted input must go through this reader.
class Reader {
public:
    explicit Reader(ByteView data) : data_(data) {}

    [[nodiscard]] std::size_t size() const noexcept { return data_.size(); }

    [[nodiscard]] bool in_bounds(std::size_t offset, std::size_t length) const noexcept
    {
        return offset <= data_.size() && length <= data_.size() - offset;
    }

    Result<std::uint8_t> u8(std::size_t offset) const
    {
        return read_le<std::uint8_t>(offset);
    }

    Result<std::uint16_t> u16(std::size_t offset) const
    {
        return read_le<std::uint16_t>(offset);
    }

    Result<std::uint32_t> u32(std::size_t offset) const
    {
        return read_le<std::uint32_t>(offset);
    }

    Result<std::uint64_t> u64(std::size_t offset) const
    {
        return read_le<std::uint64_t>(offset);
    }

    Result<ByteView> bytes(std::size_t offset, std::size_t length) const
    {
        if (!in_bounds(offset, length)) {
            return Error::out_of_range(
                "read of " + std::to_string(length) + " bytes at offset " +
                std::to_string(offset) + " exceeds buffer size " + std::to_string(data_.size()));
        }
        return data_.subspan(offset, length);
    }

    /// Reads a NUL-terminated string starting at `offset`, bounded by
    /// `max_length`. Fails when no terminator is found within the bound.
    Result<std::string_view> cstring(std::size_t offset, std::size_t max_length) const
    {
        if (offset >= data_.size()) {
            return Error::out_of_range("string offset " + std::to_string(offset) +
                                       " is beyond buffer size " + std::to_string(data_.size()));
        }
        const auto* p = data_.data() + offset;
        std::size_t len = 0;
        while (len < max_length && offset + len < data_.size() && p[len] != std::byte{0}) {
            ++len;
        }
        if (len == max_length || offset + len >= data_.size()) {
            return Error::out_of_range("unterminated string at offset " + std::to_string(offset));
        }
        return std::string_view(reinterpret_cast<const char*>(p), len);
    }

private:
    template <typename T>
    Result<T> read_le(std::size_t offset) const
    {
        if (!in_bounds(offset, sizeof(T))) {
            return Error::out_of_range(
                "read of " + std::to_string(sizeof(T)) + " bytes at offset " +
                std::to_string(offset) + " exceeds buffer size " + std::to_string(data_.size()));
        }
        T value{};
        std::memcpy(&value, data_.data() + offset, sizeof(T));
        if constexpr (std::endian::native == std::endian::big) {
            // The parsers assume little-endian files; this branch only
            // matters on big-endian hosts.
            auto* bytes = reinterpret_cast<std::byte*>(&value);
            for (std::size_t i = 0; i < sizeof(T) / 2; ++i) {
                std::swap(bytes[i], bytes[sizeof(T) - 1 - i]);
            }
        }
        return value;
    }

    ByteView data_;
};

/// Copies a bounded string, replacing non-printable characters with '?' so
/// names from untrusted files cannot corrupt terminal output.
[[nodiscard]] inline std::string sanitize_name(std::string_view raw)
{
    std::string out;
    out.reserve(raw.size());
    for (char c : raw) {
        const auto uc = static_cast<unsigned char>(c);
        out.push_back((uc >= 0x20 && uc < 0x7F) ? c : '?');
    }
    return out;
}

}  // namespace gmk::binary::detail

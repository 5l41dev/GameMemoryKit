// A tiny little-endian buffer builder used by the test fixtures.
// Not part of the public library.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

class BufferBuilder {
public:
    BufferBuilder& u8(std::uint8_t value)
    {
        buf_.push_back(static_cast<std::byte>(value));
        return *this;
    }

    BufferBuilder& u16(std::uint16_t value)
    {
        buf_.push_back(static_cast<std::byte>(value & 0xFF));
        buf_.push_back(static_cast<std::byte>((value >> 8) & 0xFF));
        return *this;
    }

    BufferBuilder& u32(std::uint32_t value)
    {
        buf_.push_back(static_cast<std::byte>(value & 0xFF));
        buf_.push_back(static_cast<std::byte>((value >> 8) & 0xFF));
        buf_.push_back(static_cast<std::byte>((value >> 16) & 0xFF));
        buf_.push_back(static_cast<std::byte>((value >> 24) & 0xFF));
        return *this;
    }

    BufferBuilder& u64(std::uint64_t value)
    {
        for (int i = 0; i < 8; ++i) {
            buf_.push_back(static_cast<std::byte>((value >> (i * 8)) & 0xFF));
        }
        return *this;
    }

    BufferBuilder& raw(std::string_view bytes)
    {
        for (char c : bytes) {
            buf_.push_back(static_cast<std::byte>(c));
        }
        return *this;
    }

    BufferBuilder& cstr(std::string_view text)
    {
        raw(text);
        buf_.push_back(std::byte{0});
        return *this;
    }

    BufferBuilder& zeros(std::size_t count)
    {
        buf_.insert(buf_.end(), count, std::byte{0});
        return *this;
    }

    /// Pads with zeros until the buffer is at least `position` bytes long.
    BufferBuilder& seek(std::size_t position)
    {
        if (buf_.size() < position) {
            zeros(position - buf_.size());
        }
        return *this;
    }

    /// Overwrites a little-endian u32 in place (used to patch computed
    /// offsets after the layout is known).
    void mutate_u32(std::size_t offset, std::uint32_t value)
    {
        buf_[offset + 0] = static_cast<std::byte>(value & 0xFF);
        buf_[offset + 1] = static_cast<std::byte>((value >> 8) & 0xFF);
        buf_[offset + 2] = static_cast<std::byte>((value >> 16) & 0xFF);
        buf_[offset + 3] = static_cast<std::byte>((value >> 24) & 0xFF);
    }

    /// Overwrites a little-endian u64 in place.
    void mutate_u64(std::size_t offset, std::uint64_t value)
    {
        for (int i = 0; i < 8; ++i) {
            buf_[offset + i] = static_cast<std::byte>((value >> (i * 8)) & 0xFF);
        }
    }

    [[nodiscard]] std::size_t pos() const { return buf_.size(); }

    [[nodiscard]] const std::vector<std::byte>& data() const { return buf_; }

    std::vector<std::byte> take() { return std::move(buf_); }

private:
    std::vector<std::byte> buf_;
};

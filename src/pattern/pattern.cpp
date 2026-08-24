#include "gmk/pattern/pattern.hpp"

#include <cctype>
#include <string>

namespace gmk {

namespace {

/// Converts a single hex digit to its value, or returns false.
bool hex_value(char c, std::uint8_t& out) noexcept
{
    if (c >= '0' && c <= '9') {
        out = static_cast<std::uint8_t>(c - '0');
        return true;
    }
    if (c >= 'a' && c <= 'f') {
        out = static_cast<std::uint8_t>(c - 'a' + 10);
        return true;
    }
    if (c >= 'A' && c <= 'F') {
        out = static_cast<std::uint8_t>(c - 'A' + 10);
        return true;
    }
    return false;
}

}  // namespace

Result<Pattern> Pattern::parse(std::string_view text)
{
    std::vector<Pattern::Byte> bytes;
    bytes.reserve(text.size() / 2 + 1);

    std::size_t token_start = 0;
    const std::size_t length = text.size();
    std::size_t i = 0;

    while (i <= length) {
        const bool at_end = (i == length);
        const char c = at_end ? '\0' : text[i];

        if (at_end || std::isspace(static_cast<unsigned char>(c))) {
            // Token boundary.
            if (i > token_start) {
                const std::string_view token = text.substr(token_start, i - token_start);
                if (token == "??" || token == "?") {
                    bytes.push_back(Pattern::Byte{0, true});
                } else if (token.size() == 2) {
                    std::uint8_t hi = 0;
                    std::uint8_t lo = 0;
                    if (!hex_value(token[0], hi) || !hex_value(token[1], lo)) {
                        return Error::parse("invalid hex byte \"" + std::string(token) +
                                            "\" at position " + std::to_string(token_start));
                    }
                    bytes.push_back(Pattern::Byte{
                        static_cast<std::uint8_t>((hi << 4) | lo), false});
                } else {
                    return Error::parse("invalid token \"" + std::string(token) +
                                        "\" at position " + std::to_string(token_start));
                }
            }
            token_start = i + 1;
        }

        if (at_end) {
            break;
        }
        ++i;
    }

    if (bytes.empty()) {
        return Error::parse("empty pattern");
    }

    return Pattern::from_bytes(std::move(bytes));
}

Pattern Pattern::from_bytes(std::vector<Byte> bytes)
{
    return Pattern(std::move(bytes));
}

std::optional<std::size_t> Pattern::first_fixed_index() const noexcept
{
    for (std::size_t i = 0; i < bytes_.size(); ++i) {
        if (!bytes_[i].wildcard) {
            return i;
        }
    }
    return std::nullopt;
}

bool Pattern::all_wildcards() const noexcept
{
    return !first_fixed_index().has_value();
}

bool matches_at(ByteView buffer, std::size_t offset, const Pattern& pattern) noexcept
{
    const std::size_t pattern_size = pattern.size();
    if (pattern_size == 0) {
        return false;
    }
    if (offset > buffer.size() || pattern_size > buffer.size() - offset) {
        return false;
    }

    const auto& bytes = pattern.bytes();
    for (std::size_t i = 0; i < pattern_size; ++i) {
        const Pattern::Byte& expected = bytes[i];
        if (!expected.wildcard &&
            static_cast<std::uint8_t>(buffer[offset + i]) != expected.value) {
            return false;
        }
    }
    return true;
}

}  // namespace gmk

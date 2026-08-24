#include "gmk/pattern/scanner.hpp"

#include <cstring>

namespace gmk {

namespace {

/// Core single-match search starting at or after `from`.
Result<std::optional<std::size_t>> find_impl(ByteView buffer, const Pattern& pattern,
                                             std::size_t from)
{
    if (pattern.empty()) {
        return Error::invalid_argument("cannot scan with an empty pattern");
    }

    const std::size_t n = buffer.size();
    const std::size_t m = pattern.size();
    if (from > n || m > n - from) {
        return std::optional<std::size_t>{std::nullopt};
    }

    const auto anchor_index = pattern.first_fixed_index();
    if (!anchor_index) {
        // All-wildcard pattern: matches everywhere the pattern fits.
        return std::optional<std::size_t>{from};
    }

    const std::size_t anchor = *anchor_index;
    const std::uint8_t anchor_byte = pattern.bytes()[anchor].value;

    // Search for the anchor byte with memchr, then verify the whole pattern.
    const std::byte* data = buffer.data();
    const std::size_t last_start = n - m;
    const std::byte* p = data + from + anchor;
    const std::byte* const end = data + last_start + anchor + 1;

    while (p < end) {
        const void* found =
            std::memchr(p, static_cast<int>(anchor_byte), static_cast<std::size_t>(end - p));
        if (found == nullptr) {
            break;
        }
        const auto* found_bytes = static_cast<const std::byte*>(found);
        const std::size_t start = static_cast<std::size_t>(found_bytes - data) - anchor;
        if (matches_at(buffer, start, pattern)) {
            return std::optional<std::size_t>{start};
        }
        p = found_bytes + 1;
    }

    return std::optional<std::size_t>{std::nullopt};
}

}  // namespace

Result<std::optional<std::size_t>> PatternScanner::find_first(ByteView buffer,
                                                              const Pattern& pattern)
{
    return find_impl(buffer, pattern, 0);
}

Result<std::optional<std::size_t>> PatternScanner::find_first_from(ByteView buffer,
                                                                   const Pattern& pattern,
                                                                   std::size_t from)
{
    return find_impl(buffer, pattern, from);
}

Result<std::vector<std::size_t>> PatternScanner::find_all(ByteView buffer,
                                                          const Pattern& pattern)
{
    std::vector<std::size_t> matches;
    if (pattern.empty()) {
        return Error::invalid_argument("cannot scan with an empty pattern");
    }

    std::size_t pos = 0;
    while (true) {
        auto found = find_impl(buffer, pattern, pos);
        if (found.failed()) {
            return found.error();
        }
        if (!*found) {
            break;
        }
        matches.push_back(**found);
        pos = **found + 1;
    }
    return matches;
}

Result<std::size_t> PatternScanner::count(ByteView buffer, const Pattern& pattern)
{
    if (pattern.empty()) {
        return Error::invalid_argument("cannot scan with an empty pattern");
    }

    std::size_t total = 0;
    std::size_t pos = 0;
    while (true) {
        auto found = find_impl(buffer, pattern, pos);
        if (found.failed()) {
            return found.error();
        }
        if (!*found) {
            break;
        }
        ++total;
        pos = **found + 1;
    }
    return total;
}

}  // namespace gmk

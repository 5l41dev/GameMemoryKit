// GameMemoryKit — Pattern.
//
// A compiled byte pattern used for signature scanning. Patterns are written
// as whitespace-separated hexadecimal bytes; "??" marks a wildcard byte:
//
//     "48 8B ?? ?? ?? 48 85 C0"
//
// Compilation is fail-fast: any malformed token (bad hex digit, partial
// byte, stray characters) is rejected with a ParseError that includes the
// offending position.
//
// A Pattern is immutable after construction and safe to share between
// threads.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "gmk/core/byte_view.hpp"
#include "gmk/core/error.hpp"
#include "gmk/core/result.hpp"

namespace gmk {

/// A compiled signature pattern.
class Pattern {
public:
    /// One element of a compiled pattern.
    struct Byte {
        std::uint8_t value{0};  ///< Byte value (meaningless when wildcard).
        bool wildcard{false};   ///< True when this position matches any byte.

        friend constexpr bool operator==(const Byte&, const Byte&) = default;
        friend constexpr bool operator!=(const Byte&, const Byte&) = default;
    };

    /// Compiles a pattern string, e.g. "48 8B ?? ?? ?? 48 85 C0".
    ///
    /// \param text whitespace-separated hex tokens; "??" is a wildcard.
    /// \return the compiled pattern, or a ParseError describing the first
    ///         malformed token (empty input, bad hex, partial byte).
    static Result<Pattern> parse(std::string_view text);

    /// Constructs a pattern from an explicit byte list (programmatic use).
    static Pattern from_bytes(std::vector<Byte> bytes);

    /// Number of bytes (including wildcards) in the pattern.
    [[nodiscard]] std::size_t size() const noexcept { return bytes_.size(); }

    /// True when the pattern contains no bytes at all.
    [[nodiscard]] bool empty() const noexcept { return bytes_.empty(); }

    /// The compiled bytes.
    [[nodiscard]] const std::vector<Byte>& bytes() const noexcept { return bytes_; }

    /// Index of the first non-wildcard byte, or nullopt when every byte is a
    /// wildcard. Scanners use this as the anchor for fast first-byte search.
    [[nodiscard]] std::optional<std::size_t> first_fixed_index() const noexcept;

    /// True when every byte of the pattern is a wildcard.
    [[nodiscard]] bool all_wildcards() const noexcept;

private:
    explicit Pattern(std::vector<Byte> bytes) : bytes_(std::move(bytes)) {}

    std::vector<Byte> bytes_;
};

/// True when `pattern` matches `buffer` at `offset`.
///
/// The check is fully bounds-checked: it returns false (never reads out of
/// bounds) when the pattern does not fit in the buffer at that offset.
[[nodiscard]] bool matches_at(ByteView buffer, std::size_t offset,
                              const Pattern& pattern) noexcept;

}  // namespace gmk

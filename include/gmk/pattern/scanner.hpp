// GameMemoryKit — PatternScanner.
//
// Searches for compiled Patterns inside byte buffers. The scanner never
// touches process memory directly: it operates only on the buffer the caller
// supplies. Reading from a process is the process layer's job (which validates
// memory regions before reading), keeping "scan what you were given" an
// explicit contract.
//
// Complexity: the scan anchors on the first fixed byte of the pattern using a
// memchr-style search, giving O(n) behaviour with a low constant factor for
// typical patterns. The worst case (a pattern whose anchor byte appears almost
// everywhere and fails only at its final byte) is O(n*m).
//
// Thread safety: the scanner holds no state; a Pattern is immutable. Any
// number of threads may scan concurrently.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "gmk/core/byte_view.hpp"
#include "gmk/core/error.hpp"
#include "gmk/core/result.hpp"
#include "gmk/pattern/pattern.hpp"

namespace gmk {

/// Searches for patterns in byte buffers.
class PatternScanner {
public:
    /// Returns the offset of the first match, or nullopt when there is none.
    ///
    /// Fails with InvalidArgument when `pattern` is empty.
    [[nodiscard]] static Result<std::optional<std::size_t>> find_first(
        ByteView buffer, const Pattern& pattern);

    /// Returns the offset of the first match at or after `from`, or nullopt.
    [[nodiscard]] static Result<std::optional<std::size_t>> find_first_from(
        ByteView buffer, const Pattern& pattern, std::size_t from);

    /// Returns the offsets of every match, in ascending order.
    ///
    /// Note: for large buffers with many matches the returned vector can be
    /// large; use count() when only the number of matches is needed.
    [[nodiscard]] static Result<std::vector<std::size_t>> find_all(
        ByteView buffer, const Pattern& pattern);

    /// Counts the number of non-overlapping matches.
    [[nodiscard]] static Result<std::size_t> count(ByteView buffer, const Pattern& pattern);
};

}  // namespace gmk

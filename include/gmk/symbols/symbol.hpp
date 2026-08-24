// GameMemoryKit — Symbol.
//
// A normalized representation of a symbol regardless of source format
// (PE exports, ELF dynamic symbols, ...). Not every source provides every
// field; fields that are unknown are left defaulted.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <string>

#include "gmk/core/address.hpp"

namespace gmk {

/// A single symbol (typically an exported function or variable).
struct Symbol {
    std::string name;   ///< Symbol name, or the ordinal form for unnamed exports.
    Address address;    ///< Address in the binary's address space (may be null when unknown).
    std::size_t size{0};  ///< Size in bytes (0 when unknown).
};

}  // namespace gmk

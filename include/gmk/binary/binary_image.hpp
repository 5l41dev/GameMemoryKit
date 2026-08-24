// GameMemoryKit — BinaryImage.
//
// The common abstraction over parsed executable images. It detects the
// container format (PE or ELF), validates the file, and exposes a normalized
// view (architecture, entry point, sections, exported symbols). Format-
// specific detail remains available through as_pe() / as_elf().
//
// Parsing is eager and purely read-only: the parser never trusts offsets
// found in the file, always checks bounds, and never retains a reference to
// the input buffer (so the buffer may be freed after parse()).
//
// Thread safety: a BinaryImage is an immutable value type after parse() and
// is safe to share between threads.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <filesystem>
#include <ostream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "gmk/binary/elf.hpp"
#include "gmk/binary/pe.hpp"
#include "gmk/core/address.hpp"
#include "gmk/core/byte_view.hpp"
#include "gmk/core/error.hpp"
#include "gmk/core/result.hpp"
#include "gmk/symbols/symbol.hpp"

namespace gmk {

/// The container format of a binary image.
enum class BinaryFormat {
    Unknown = 0,
    Pe,
    Elf,
};

/// Returns a stable name for a format ("PE", "ELF", "Unknown").
[[nodiscard]] std::string_view to_string(BinaryFormat format) noexcept;

/// Streams the format name.
inline std::ostream& operator<<(std::ostream& out, BinaryFormat format)
{
    return out << to_string(format);
}

/// A parsed executable image (PE or ELF).
class BinaryImage {
public:
    /// A section normalized across formats.
    struct Section {
        std::string name;              ///< Section name (e.g. ".text").
        Address address;               ///< Virtual address in the image.
        std::uint64_t size{0};         ///< Size in bytes (virtual size where relevant).
        std::uint64_t flags{0};        ///< Raw format-specific characteristics/flags.
        bool readable{false};
        bool writable{false};
        bool executable{false};
    };

    /// Parses an image from memory. Fails with InvalidBinary when the data is
    /// not a recognizable PE or ELF image or is malformed.
    static Result<BinaryImage> parse(ByteView data);

    /// Reads and parses a file from disk. Fails with IoError on file errors
    /// and InvalidBinary on malformed content.
    static Result<BinaryImage> parse_file(const std::filesystem::path& path);

    /// The detected container format.
    [[nodiscard]] BinaryFormat format() const noexcept;

    /// Human-readable format name ("PE", "ELF").
    [[nodiscard]] std::string_view format_name() const noexcept;

    /// The target architecture.
    [[nodiscard]] Architecture architecture() const noexcept;

    /// True when the image is 64-bit.
    [[nodiscard]] bool is_64_bit() const noexcept;

    /// The entry point in the image's address space (null when unknown).
    [[nodiscard]] Address entry_point() const noexcept;

    /// Normalized sections.
    [[nodiscard]] const std::vector<Section>& sections() const noexcept;

    /// Exported symbols (named exports; forwarded/ordinal-only exports are
    /// exposed through the format-specific views).
    [[nodiscard]] const std::vector<Symbol>& exports() const noexcept;

    /// The parsed PE image, or nullptr when the format is not PE.
    [[nodiscard]] const PeImage* as_pe() const noexcept;

    /// The parsed ELF image, or nullptr when the format is not ELF.
    [[nodiscard]] const ElfImage* as_elf() const noexcept;

private:
    std::variant<std::monostate, PeImage, ElfImage> image_;
    std::vector<Section> sections_;
    std::vector<Symbol> exports_;
};

}  // namespace gmk

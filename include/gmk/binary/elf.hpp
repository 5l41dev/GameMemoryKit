// GameMemoryKit — ELF parser.
//
// A safe parser for ELF images (ELF32 and ELF64, little-endian). All offsets
// and sizes from the file are validated against the provided buffer before
// use; malformed input produces an error, never an out-of-bounds read.
//
// Supported: identification, machine/architecture, section headers (with
// names via .shstrtab, including extended numbering), program headers, and
// dynamic symbols (exports) via .dynsym/.dynstr with .symtab fallback.
//
// Not supported (documented limitation): big-endian images and DWARF debug
// information are rejected/ignored, not silently misparsed.
//
// Thread safety: an ElfImage is an immutable value type after parse() and is
// safe to share between threads.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "gmk/binary/pe.hpp"  // for Architecture
#include "gmk/core/byte_view.hpp"
#include "gmk/core/error.hpp"
#include "gmk/core/result.hpp"
#include "gmk/symbols/symbol.hpp"

namespace gmk {

/// The e_type classification of an ELF file.
enum class ElfType : std::uint16_t {
    None = 0,
    Relocatable = 1,  ///< ET_REL (.o files)
    Executable = 2,   ///< ET_EXEC
    Shared = 3,       ///< ET_DYN (shared libraries / PIE)
    Core = 4,         ///< ET_CORE
};

/// Returns a stable name for an ELF type ("executable", "shared", ...).
[[nodiscard]] std::string_view to_string(ElfType type) noexcept;

/// Streams the ELF type name.
inline std::ostream& operator<<(std::ostream& out, ElfType type)
{
    return out << to_string(type);
}

/// A section header (normalized across ELF32/ELF64).
struct ElfSection {
    std::string name;             ///< Section name via .shstrtab.
    std::uint32_t type{0};        ///< Raw SHT_* value.
    std::uint64_t flags{0};       ///< Raw SHF_* value.
    std::uint64_t address{0};     ///< Virtual address when loaded.
    std::uint64_t offset{0};      ///< File offset.
    std::uint64_t size{0};        ///< Size in bytes.
    std::uint32_t link{0};        ///< Section index of the linked section.
    std::uint32_t info{0};        ///< Extra info (depends on type).
    std::uint64_t alignment{0};   ///< Alignment.
    std::uint64_t entry_size{0};  ///< Size of entries (for tables).
    std::uint32_t name_index{0};  ///< Raw sh_name offset into .shstrtab.

    [[nodiscard]] bool executable() const noexcept;
    [[nodiscard]] bool writable() const noexcept;
    [[nodiscard]] bool allocatable() const noexcept;
};

/// A program header (normalized across ELF32/ELF64).
struct ElfProgramHeader {
    std::uint32_t type{0};            ///< Raw PT_* value.
    std::uint64_t flags{0};           ///< Raw PF_* value (1=execute, 2=write, 4=read).
    std::uint64_t offset{0};          ///< Offset in the file.
    std::uint64_t virtual_address{0}; ///< Virtual address when loaded.
    std::uint64_t physical_address{0};
    std::uint64_t file_size{0};       ///< Bytes in the file.
    std::uint64_t memory_size{0};     ///< Bytes when loaded.
    std::uint64_t alignment{0};
};

/// A validated, parsed ELF image.
class ElfImage {
public:
    /// Parses an ELF image from memory. Fails with InvalidBinary on malformed
    /// content, Unsupported for big-endian images, and OutOfRange when the
    /// data is truncated.
    static Result<ElfImage> parse(ByteView data);

    /// True when the file uses ELF64.
    [[nodiscard]] bool is_64_bit() const noexcept { return is_64_bit_; }

    /// The target architecture.
    [[nodiscard]] Architecture architecture() const noexcept { return architecture_; }

    /// The raw e_machine value.
    [[nodiscard]] std::uint16_t machine() const noexcept { return machine_; }

    /// The file type (executable, shared, ...).
    [[nodiscard]] ElfType type() const noexcept { return type_; }

    /// The entry point (e_entry).
    [[nodiscard]] std::uint64_t entry_point() const noexcept { return entry_point_; }

    /// The sections, in file order (section header table).
    [[nodiscard]] const std::vector<ElfSection>& sections() const noexcept
    {
        return sections_;
    }

    /// The program headers.
    [[nodiscard]] const std::vector<ElfProgramHeader>& program_headers() const noexcept
    {
        return program_headers_;
    }

    /// Symbols from the dynamic symbol table (or .symtab when .dynsym is
    /// absent): defined, global symbols with names. This is the practical
    /// "exported symbols" view for ELF images.
    [[nodiscard]] const std::vector<Symbol>& symbols() const noexcept { return symbols_; }

private:
    ElfImage() = default;

    bool is_64_bit_{false};
    Architecture architecture_{Architecture::Unknown};
    std::uint16_t machine_{0};
    ElfType type_{ElfType::None};
    std::uint64_t entry_point_{0};
    std::vector<ElfSection> sections_;
    std::vector<ElfProgramHeader> program_headers_;
    std::vector<Symbol> symbols_;

    friend Result<ElfImage> parse_elf_impl(ByteView data);
};

}  // namespace gmk

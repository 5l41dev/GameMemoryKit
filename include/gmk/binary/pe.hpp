// GameMemoryKit — PE parser.
//
// A safe parser for Windows PE images (both PE32 and PE32+). All offsets and
// sizes found in the file are validated against the provided buffer before
// use; malformed input produces an error, never an out-of-bounds read.
//
// The parser is purely read-only and does not retain the input buffer: all
// extracted data (sections, exports, imports) is copied into the PeImage
// value during parse().
//
// Thread safety: a PeImage is an immutable value type after parse() and is
// safe to share between threads.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "gmk/core/address.hpp"
#include "gmk/core/byte_view.hpp"
#include "gmk/core/error.hpp"
#include "gmk/core/result.hpp"

namespace gmk {

/// CPU architectures recognized from executable headers.
enum class Architecture : std::uint8_t {
    Unknown = 0,
    X86,      ///< 32-bit x86
    X86_64,   ///< 64-bit x86
    Arm,      ///< 32-bit ARM
    Arm64,    ///< 64-bit ARM (AArch64)
    RiscV32,  ///< 32-bit RISC-V
    RiscV64,  ///< 64-bit RISC-V
    Other,    ///< Recognized but unsupported/unknown in detail
};

/// Returns a stable name for an architecture ("x86-64", "arm64", ...).
[[nodiscard]] std::string_view to_string(Architecture architecture) noexcept;

/// Streams the architecture name.
inline std::ostream& operator<<(std::ostream& out, Architecture architecture)
{
    return out << to_string(architecture);
}

/// A PE section header.
struct PeSection {
    std::string name;            ///< Section name (up to 8 characters, sanitized).
    std::uint32_t virtual_size{0};   ///< Size of the section when loaded.
    std::uint32_t virtual_address{0};  ///< RVA of the section.
    std::uint32_t raw_size{0};        ///< Size of the section data on disk.
    std::uint32_t raw_offset{0};      ///< File offset of the section data.
    std::uint32_t characteristics{0};  ///< Raw IMAGE_SCN_* flags.

    [[nodiscard]] bool executable() const noexcept;
    [[nodiscard]] bool readable() const noexcept;
    [[nodiscard]] bool writable() const noexcept;
};

/// A named or ordinal export.
struct PeExport {
    std::string name;          ///< Export name (empty for ordinal-only exports).
    std::uint32_t rva{0};      ///< RVA of the exported symbol.
    std::uint16_t ordinal{0};  ///< Ordinal (base + index).
    bool forwarded{false};     ///< True when the export forwards to another module.
    std::string forwarder;     ///< Forwarder target, e.g. "KERNEL32.Sleep".
};

/// One imported function (or ordinal) of a PE import descriptor.
struct PeImportFunction {
    std::string name;      ///< Imported name, or "ordinal_<n>" when by ordinal.
    std::uint16_t ordinal{0};
    bool by_ordinal{false};
};

/// All imports attributed to one module (e.g. kernel32.dll).
struct PeImportModule {
    std::string name;  ///< DLL name.
    std::vector<PeImportFunction> functions;
};

/// A validated, parsed PE image.
class PeImage {
public:
    /// Parses a PE image from memory. Fails with InvalidBinary on malformed
    /// content and OutOfRange on truncated data.
    static Result<PeImage> parse(ByteView data);

    /// True when the optional header is PE32+ (64-bit).
    [[nodiscard]] bool is_64_bit() const noexcept { return is_64_bit_; }

    /// The target architecture derived from the machine field.
    [[nodiscard]] Architecture architecture() const noexcept { return architecture_; }

    /// The raw IMAGE_FILE_MACHINE value.
    [[nodiscard]] std::uint16_t machine() const noexcept { return machine_; }

    /// The preferred load address (ImageBase).
    [[nodiscard]] std::uint64_t image_base() const noexcept { return image_base_; }

    /// The entry point as an absolute address (image base + entry RVA).
    [[nodiscard]] Address entry_point() const noexcept
    {
        return Address{image_base_}.add(entry_rva_);
    }

    /// The entry point RVA (AddressOfEntryPoint).
    [[nodiscard]] std::uint32_t entry_rva() const noexcept { return entry_rva_; }

    /// Size of the loaded image (SizeOfImage).
    [[nodiscard]] std::uint32_t size_of_image() const noexcept { return size_of_image_; }

    /// Raw IMAGE_FILE_HEADER.Characteristics.
    [[nodiscard]] std::uint16_t characteristics() const noexcept { return characteristics_; }

    /// TimeDateStamp from the COFF header.
    [[nodiscard]] std::uint32_t timestamp() const noexcept { return timestamp_; }

    /// The sections, in file order.
    [[nodiscard]] const std::vector<PeSection>& sections() const noexcept { return sections_; }

    /// Exports parsed from the export directory (empty when none present).
    [[nodiscard]] const std::vector<PeExport>& exports() const noexcept { return exports_; }

    /// Imports parsed from the import directory (empty when none present).
    [[nodiscard]] const std::vector<PeImportModule>& imports() const noexcept
    {
        return imports_;
    }

    /// Maps an RVA to a file offset using the section table.
    /// Returns nullopt when the RVA is not covered by any section.
    [[nodiscard]] std::optional<std::uint32_t> rva_to_offset(std::uint32_t rva) const noexcept;

private:
    PeImage() = default;

    bool is_64_bit_{false};
    Architecture architecture_{Architecture::Unknown};
    std::uint16_t machine_{0};
    std::uint64_t image_base_{0};
    std::uint32_t entry_rva_{0};
    std::uint32_t size_of_image_{0};
    std::uint16_t characteristics_{0};
    std::uint32_t timestamp_{0};
    std::vector<PeSection> sections_;
    std::vector<PeExport> exports_;
    std::vector<PeImportModule> imports_;

    friend Result<PeImage> parse_pe_impl(ByteView data);
};

}  // namespace gmk

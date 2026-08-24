// Builders for minimal but valid PE images (PE32 and PE32+), used by the
// binary parser tests. The layout is produced programmatically so the
// expected values in the tests are computed, not hand-counted.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "buffer_builder.hpp"

namespace fixture {

/// The expectations a test can assert against a built PE fixture.
struct PeFixture {
    std::vector<std::byte> data;
    bool is_64{false};
    std::uint64_t image_base{0};
    std::uint32_t entry_rva{0x1000};
    std::vector<std::string> sections;               // in order
    std::vector<std::pair<std::string, std::uint32_t>> exports;  // name -> rva
    std::vector<std::pair<std::string, std::vector<std::string>>> imports;  // dll -> funcs
};

/// Builds a PE image with two sections (.text, .rdata), two named exports
/// and one import (kernel32.dll!sleep).
inline PeFixture build_pe(bool is_64)
{
    constexpr std::uint32_t kRdataRva = 0x2000;
    constexpr std::uint32_t kTextRva = 0x1000;

    BufferBuilder b;
    // --- DOS header -------------------------------------------------------
    b.cstr("MZ");
    b.seek(0x3C);
    b.u32(0x80);  // e_lfanew
    b.seek(0x80);

    // --- NT headers -------------------------------------------------------
    b.raw(std::string_view{"PE\0\0", 4});
    b.u16(is_64 ? 0x8664 : 0x014C);  // machine
    b.u16(2);                        // number of sections
    b.u32(0x5A4D0000);               // timestamp
    b.u32(0);                        // pointer to symbol table
    b.u32(0);                        // number of symbols
    b.u16(is_64 ? 240 : 224);        // size of optional header
    b.u16(0x22);                     // characteristics

    // --- Optional header --------------------------------------------------
    const std::uint32_t optional_offset = static_cast<std::uint32_t>(b.pos());
    b.u16(is_64 ? 0x20B : 0x10B);  // magic
    b.u8(14);
    b.u8(0);       // linker version
    b.u32(0x1000);  // size of code
    b.u32(0x1000);  // size of initialized data
    b.u32(0);       // size of uninitialized data
    b.u32(0x1000);  // address of entry point
    b.u32(0x1000);  // base of code
    if (is_64) {
        b.u64(0x140000000ull);  // image base (PE32+ has no BaseOfData)
    } else {
        b.u32(0x400000);  // base of data (PE32 only)
        b.u32(0x400000);  // image base
    }
    b.u32(0x1000);  // section alignment
    b.u32(0x200);   // file alignment
    b.u16(6);
    b.u16(0);  // OS version
    b.u16(0);
    b.u16(0);  // image version
    b.u16(6);
    b.u16(0);  // subsystem version
    b.u32(0);  // win32 version value
    b.u32(0x3000);  // size of image
    b.u32(0x200);   // size of headers
    b.u32(0);       // checksum
    b.u16(3);       // subsystem: console
    b.u16(0);       // dll characteristics
    if (is_64) {
        b.u64(0x100000);
        b.u64(0x1000);
        b.u64(0x100000);
        b.u64(0x1000);
    } else {
        b.u32(0x100000);
        b.u32(0x1000);
        b.u32(0x100000);
        b.u32(0x1000);
    }
    b.u32(0);   // loader flags
    b.u32(4);   // number of rva and sizes

    // --- Data directories (filled below with computed RVAs) ---------------
    const std::uint32_t dir_offset = static_cast<std::uint32_t>(b.pos());
    b.zeros(4 * 8);  // 4 directories

    // --- Section table ----------------------------------------------------
    // The optional header must occupy exactly the declared SizeOfOptionalHeader
    // bytes (240 for PE32+, 224 for PE32) or the parser will look for the
    // section table in the wrong place.
    b.seek(optional_offset + (is_64 ? 240 : 224));
    // Note: the names contain NUL padding, so they must be written with an
    // explicit length (raw() measures strlen and would truncate them).
    b.raw(std::string_view{".text\0\0\0", 8});
    b.u32(0x1000);  // virtual size
    b.u32(kTextRva);
    b.u32(0x200);  // raw size
    b.u32(0x200);  // raw offset
    b.u32(0);      // relocations
    b.u32(0);
    b.u16(0);
    b.u16(0);
    b.u32(0x60000020);  // code | execute | read

    b.raw(std::string_view{".rdata\0\0", 8});
    b.u32(0x1000);  // virtual size
    b.u32(kRdataRva);
    b.u32(0x400);  // raw size
    b.u32(0x400);  // raw offset
    b.u32(0);
    b.u32(0);
    b.u16(0);
    b.u16(0);
    b.u32(0x40000040);  // initialized data | read

    // --- File content -----------------------------------------------------
    b.seek(0x200);  // .text raw
    b.zeros(0x200);
    b.seek(0x400);  // .rdata raw

    // Export directory at rva 0x2000 (file 0x400).
    const std::uint32_t export_dir = static_cast<std::uint32_t>(b.pos());
    b.u32(0);        // characteristics
    b.u32(0);        // timestamp
    b.u16(0);
    b.u16(0);        // version
    b.u32(0);        // name rva (patched below)
    b.u32(1);        // base
    b.u32(2);        // number of functions
    b.u32(2);        // number of names
    b.u32(0);        // address of functions (patched)
    b.u32(0);        // address of names (patched)
    b.u32(0);        // address of ordinals (patched)

    // Functions array: 2 x u32 RVAs.
    const std::uint32_t funcs_off = static_cast<std::uint32_t>(b.pos());
    b.u32(kTextRva);
    b.u32(kTextRva + 0x10);
    // Names array: 2 x u32 name RVAs.
    const std::uint32_t names_off = static_cast<std::uint32_t>(b.pos());
    b.u32(0);  // patched
    b.u32(0);  // patched
    // Ordinals: 2 x u16.
    const std::uint32_t ordinals_off = static_cast<std::uint32_t>(b.pos());
    b.u16(0);
    b.u16(1);
    // Name strings.
    const std::uint32_t name1_off = static_cast<std::uint32_t>(b.pos());
    b.cstr("export_one");
    const std::uint32_t name2_off = static_cast<std::uint32_t>(b.pos());
    b.cstr("export_two");
    const std::uint32_t module_name_off = static_cast<std::uint32_t>(b.pos());
    b.cstr("testmod.dll");

    // Import descriptors.
    const std::uint32_t import_desc_off = static_cast<std::uint32_t>(b.pos());
    b.u32(0);  // original first thunk (patched)
    b.u32(0);  // timestamp
    b.u32(0);  // forwarder chain
    b.u32(0);  // name rva (patched)
    b.u32(0);  // first thunk (patched)
    b.zeros(20);  // terminating empty descriptor

    // Thunk array.
    const std::uint32_t thunk_off = static_cast<std::uint32_t>(b.pos());
    b.u32(0);  // patched (import-by-name rva); u32 vs u64 handled below
    b.u32(0);
    if (is_64) {
        b.zeros(8);  // second 8-byte entry: terminator
    }

    // Import-by-name structure: hint + name.
    const std::uint32_t by_name_off = static_cast<std::uint32_t>(b.pos());
    b.u16(0);
    b.cstr("sleep");
    const std::uint32_t kernel32_off = static_cast<std::uint32_t>(b.pos());
    b.cstr("kernel32.dll");

    // --- Patch computed values -------------------------------------------
    auto rva = [](std::uint32_t file_offset, std::uint32_t base) {
        return kRdataRva + (file_offset - base);
    };

    const std::uint32_t export_rva = rva(export_dir, export_dir);
    const std::uint32_t import_rva = rva(import_desc_off, export_dir);
    const std::uint32_t thunk_rva = rva(thunk_off, export_dir);
    const std::uint32_t by_name_rva = rva(by_name_off, export_dir);
    const std::uint32_t kernel32_rva = rva(kernel32_off, export_dir);
    const std::uint32_t module_name_rva = rva(module_name_off, export_dir);

    // Directories.
    b.mutate_u32(dir_offset + 0, export_rva);
    b.mutate_u32(dir_offset + 4, 0x60);
    b.mutate_u32(dir_offset + 8, import_rva);
    b.mutate_u32(dir_offset + 12, 0x28);

    // Export directory fields (Name at +12, Base at +16 per the spec).
    b.mutate_u32(export_dir + 12, module_name_rva);  // Name -> "testmod.dll"
    b.mutate_u32(export_dir + 28, rva(funcs_off, export_dir));
    b.mutate_u32(export_dir + 32, rva(names_off, export_dir));
    b.mutate_u32(export_dir + 36, rva(ordinals_off, export_dir));
    b.mutate_u32(names_off, rva(name1_off, export_dir));
    b.mutate_u32(names_off + 4, rva(name2_off, export_dir));

    // Import descriptor.
    b.mutate_u32(import_desc_off + 0, thunk_rva);  // original first thunk
    b.mutate_u32(import_desc_off + 12, kernel32_rva);  // name
    b.mutate_u32(import_desc_off + 16, thunk_rva);  // first thunk

    // Thunk entries: import-by-name RVA then terminator.
    if (is_64) {
        b.mutate_u64(thunk_off, by_name_rva);
    } else {
        b.mutate_u32(thunk_off, by_name_rva);
    }

    PeFixture fixture;
    fixture.is_64 = is_64;
    fixture.image_base = is_64 ? 0x140000000ull : 0x400000ull;
    fixture.sections = {".text", ".rdata"};
    fixture.exports = {{"export_one", kTextRva}, {"export_two", kTextRva + 0x10}};
    fixture.imports = {{"kernel32.dll", {"sleep"}}};
    fixture.data = b.take();
    return fixture;
}

}  // namespace fixture

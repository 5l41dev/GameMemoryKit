// Builders for minimal but valid ELF images (ELF32 and ELF64), used by the
// binary parser tests.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "buffer_builder.hpp"

namespace fixture {

/// The expectations a test can assert against a built ELF fixture.
struct ElfFixture {
    std::vector<std::byte> data;
    bool is_64{false};
    std::uint64_t entry{0};
    std::vector<std::string> sections;  // named sections in order
    std::vector<std::pair<std::string, std::uint64_t>> exports;  // name -> value
};

/// Builds an ELF image with one program header, five sections
/// (null, .text, .dynsym, .dynstr, .shstrtab), three dynamic symbols
/// (two defined exports and one undefined import) and one PT_LOAD segment.
inline ElfFixture build_elf(bool is_64)
{
    BufferBuilder b;

    // --- ELF identification ----------------------------------------------
    b.raw("\x7F" "ELF");
    b.u8(is_64 ? 2 : 1);  // EI_CLASS
    b.u8(1);              // EI_DATA: little-endian
    b.u8(1);              // EI_VERSION
    b.zeros(9);           // padding

    // --- Header -----------------------------------------------------------
    b.u16(is_64 ? 3 : 2);  // e_type: ET_DYN for 64, ET_EXEC for 32
    b.u16(is_64 ? 62 : 3);  // e_machine: x86-64 / i386
    b.u32(1);               // e_version
    const std::uint64_t entry = is_64 ? 0x401000 : 0x08048000;
    if (is_64) {
        b.u64(entry);   // e_entry
        b.u64(0x40);    // e_phoff
        b.u64(0x200);   // e_shoff
        b.u32(0);       // e_flags
        b.u16(64);      // e_ehsize
        b.u16(56);      // e_phentsize
        b.u16(1);       // e_phnum
        b.u16(64);      // e_shentsize
        b.u16(5);       // e_shnum
        b.u16(4);       // e_shstrndx
    } else {
        b.u32(static_cast<std::uint32_t>(entry));  // e_entry
        b.u32(0x34);    // e_phoff
        b.u32(0x200);   // e_shoff
        b.u32(0);       // e_flags
        b.u16(52);      // e_ehsize
        b.u16(32);      // e_phentsize
        b.u16(1);       // e_phnum
        b.u16(40);      // e_shentsize
        b.u16(5);       // e_shnum
        b.u16(4);       // e_shstrndx
    }

    // --- Program header ---------------------------------------------------
    b.seek(is_64 ? 0x40 : 0x34);
    if (is_64) {
        b.u32(1);       // p_type: PT_LOAD
        b.u32(5);       // p_flags: R+X
        b.u64(0);       // p_offset
        b.u64(0x400000);
        b.u64(0x400000);
        b.u64(0x400);
        b.u64(0x400);
        b.u64(0x1000);  // p_align
    } else {
        b.u32(1);       // p_type
        b.u32(0);       // p_offset
        b.u32(0x8048000);
        b.u32(0x8048000);
        b.u32(0x400);
        b.u32(0x400);
        b.u32(5);       // p_flags
        b.u32(0x1000);  // p_align
    }

    // --- .text raw content ------------------------------------------------
    b.seek(0x100);
    b.zeros(0x100);

    // --- Section headers --------------------------------------------------
    // Content sections live at higher file offsets than the header table;
    // BufferBuilder::seek can only pad forward, so headers come before them.
    b.seek(0x200);
    const std::size_t sh_entsize = is_64 ? 64 : 40;
    // [0] null section.
    b.zeros(sh_entsize);
    // [1] .text.
    if (is_64) {
        b.u32(1); b.u32(1); b.u64(0x6); b.u64(0x401000);
        b.u64(0x100); b.u64(0x100); b.u32(0); b.u32(0); b.u64(16); b.u64(0);
    } else {
        b.u32(1); b.u32(1); b.u32(0x6); b.u32(0x8048000);
        b.u32(0x100); b.u32(0x100); b.u32(0); b.u32(0); b.u32(16); b.u32(0);
    }
    // [2] .dynsym.
    if (is_64) {
        b.u32(7); b.u32(11); b.u64(0x2); b.u64(0x402000);
        b.u64(0x380); b.u64(4 * 24); b.u32(3); b.u32(1); b.u64(8); b.u64(24);
    } else {
        b.u32(7); b.u32(11); b.u32(0x2); b.u32(0x8049000);
        b.u32(0x300); b.u32(4 * 16); b.u32(3); b.u32(1); b.u32(4); b.u32(16);
    }
    // [3] .dynstr.
    if (is_64) {
        b.u32(15); b.u32(3); b.u64(0x2); b.u64(0x402080);
        b.u64(0x400); b.u64(0x20); b.u32(0); b.u32(0); b.u64(1); b.u64(0);
    } else {
        b.u32(15); b.u32(3); b.u32(0x2); b.u32(0x8049080);
        b.u32(0x360); b.u32(0x20); b.u32(0); b.u32(0); b.u32(1); b.u32(0);
    }
    // [4] .shstrtab.
    if (is_64) {
        b.u32(23); b.u32(3); b.u64(0); b.u64(0);
        b.u64(0x430); b.u64(0x30); b.u32(0); b.u32(0); b.u64(1); b.u64(0);
    } else {
        b.u32(23); b.u32(3); b.u32(0); b.u32(0);
        b.u32(0x390); b.u32(0x30); b.u32(0); b.u32(0); b.u32(1); b.u32(0);
    }

    // --- .dynsym raw content ----------------------------------------------
    // Must not overlap the section headers (0x200..0x340 for ELF64).
    const std::uint32_t dynsym_off = is_64 ? 0x380 : 0x300;
    b.seek(dynsym_off);
    // Entry 0: null symbol.
    b.zeros(is_64 ? 24 : 16);
    // Entry 1: fixture_add (name at 1 in dynstr), GLOBAL FUNC, defined.
    if (is_64) {
        b.u32(1);
        b.u8(0x12);
        b.u8(0);
        b.u16(1);
        b.u64(entry);
        b.u64(8);
    } else {
        b.u32(1);
        b.u32(static_cast<std::uint32_t>(entry));
        b.u32(8);
        b.u8(0x12);
        b.u8(0);
        b.u16(1);
    }
    // Entry 2: fixture_mul (name at 13), GLOBAL FUNC, defined.
    if (is_64) {
        b.u32(13);
        b.u8(0x12);
        b.u8(0);
        b.u16(1);
        b.u64(entry + 0x20);
        b.u64(16);
    } else {
        b.u32(13);
        b.u32(static_cast<std::uint32_t>(entry + 0x20));
        b.u32(16);
        b.u8(0x12);
        b.u8(0);
        b.u16(1);
    }
    // Entry 3: puts (name at 25), GLOBAL FUNC, UNDEFINED (must be excluded).
    if (is_64) {
        b.u32(25);
        b.u8(0x12);
        b.u8(0);
        b.u16(0);
        b.u64(0);
        b.u64(0);
    } else {
        b.u32(25);
        b.u32(0);
        b.u32(0);
        b.u8(0x12);
        b.u8(0);
        b.u16(0);
    }

    // --- .dynstr raw content ----------------------------------------------
    const std::uint32_t dynstr_off = is_64 ? 0x400 : 0x360;
    b.seek(dynstr_off);
    b.u8(0);
    b.cstr("fixture_add");  // name index 1
    b.cstr("fixture_mul");  // name index 13
    b.cstr("puts");         // name index 25

    // --- .shstrtab raw content --------------------------------------------
    const std::uint32_t shstrtab_off = is_64 ? 0x430 : 0x390;
    b.seek(shstrtab_off);
    b.u8(0);
    b.cstr(".text");     // index 1
    b.cstr(".dynsym");   // index 7
    b.cstr(".dynstr");   // index 15
    b.cstr(".shstrtab"); // index 23
    // Pad so the file actually contains the full declared sh_size (0x30).
    b.seek(shstrtab_off + 0x30);

    ElfFixture fixture;
    fixture.is_64 = is_64;
    fixture.entry = entry;
    fixture.sections = {".text", ".dynsym", ".dynstr", ".shstrtab"};
    fixture.exports = {{"fixture_add", entry}, {"fixture_mul", entry + 0x20}};
    fixture.data = b.take();
    return fixture;
}

}  // namespace fixture

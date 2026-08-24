#include "gmk/binary/elf.hpp"

#include "../fixtures/elf_fixture.hpp"
#include "../support/minitest.hpp"

#include <cstdint>
#include <string>

using gmk::Architecture;
using gmk::ElfImage;
using gmk::ElfType;
using gmk::ErrorCode;

MT_TEST(elf, parses_elf64_fixture)
{
    auto fixture = fixture::build_elf(true);
    auto result = ElfImage::parse(fixture.data);
    MT_REQUIRE_OK(result);

    const ElfImage& image = *result;
    MT_CHECK(image.is_64_bit());
    MT_CHECK_EQ(image.architecture(), Architecture::X86_64);
    MT_CHECK_EQ(image.entry_point(), std::uint64_t{0x401000});
    MT_CHECK_EQ(image.sections().size(), std::size_t{5});  // includes null section
    MT_CHECK_EQ(image.sections()[1].name, std::string{".text"});
    MT_CHECK_EQ(image.sections()[2].name, std::string{".dynsym"});
    MT_CHECK_EQ(image.sections()[3].name, std::string{".dynstr"});
    MT_CHECK_EQ(image.sections()[4].name, std::string{".shstrtab"});
    MT_CHECK(image.sections()[1].executable());
    MT_CHECK(image.sections()[1].allocatable());
    MT_CHECK_EQ(image.program_headers().size(), std::size_t{1});
    MT_CHECK_EQ(image.program_headers()[0].type, std::uint32_t{1});  // PT_LOAD
    MT_CHECK_EQ(image.program_headers()[0].virtual_address, std::uint64_t{0x400000});
}

MT_TEST(elf, parses_elf32_fixture)
{
    auto fixture = fixture::build_elf(false);
    auto result = ElfImage::parse(fixture.data);
    MT_REQUIRE_OK(result);

    const ElfImage& image = *result;
    MT_CHECK(!image.is_64_bit());
    MT_CHECK_EQ(image.architecture(), Architecture::X86);
    MT_CHECK_EQ(image.entry_point(), std::uint64_t{0x08048000});
    MT_CHECK_EQ(image.sections().size(), std::size_t{5});
}

MT_TEST(elf, parses_dynamic_symbols)
{
    auto fixture = fixture::build_elf(true);
    auto result = ElfImage::parse(fixture.data);
    MT_REQUIRE_OK(result);

    const auto& symbols = result->symbols();
    MT_CHECK_EQ(symbols.size(), std::size_t{2});  // undefined "puts" excluded
    MT_CHECK_EQ(symbols[0].name, std::string{"fixture_add"});
    MT_CHECK_EQ(symbols[0].address.value(), std::uint64_t{0x401000});
    MT_CHECK_EQ(symbols[0].size, std::size_t{8});
    MT_CHECK_EQ(symbols[1].name, std::string{"fixture_mul"});
    MT_CHECK_EQ(symbols[1].address.value(), std::uint64_t{0x401020});
    MT_CHECK_EQ(symbols[1].size, std::size_t{16});
}

MT_TEST(elf, elf32_symbols)
{
    auto fixture = fixture::build_elf(false);
    auto result = ElfImage::parse(fixture.data);
    MT_REQUIRE_OK(result);

    const auto& symbols = result->symbols();
    MT_CHECK_EQ(symbols.size(), std::size_t{2});
    MT_CHECK_EQ(symbols[0].name, std::string{"fixture_add"});
    MT_CHECK_EQ(symbols[0].address.value(), std::uint64_t{0x08048000});
}

MT_TEST(elf, rejects_non_elf)
{
    std::vector<std::byte> garbage{std::byte{0x7F}, std::byte{'E'}, std::byte{'L'}};
    auto result = ElfImage::parse(garbage);
    MT_REQUIRE_ERR(result);
    MT_CHECK_EQ(result.error().code(), ErrorCode::InvalidBinary);
}

MT_TEST(elf, rejects_big_endian)
{
    auto fixture = fixture::build_elf(true);
    fixture.data[5] = std::byte{2};  // EI_DATA = big-endian
    auto result = ElfImage::parse(fixture.data);
    MT_REQUIRE_ERR(result);
    MT_CHECK_EQ(result.error().code(), ErrorCode::Unsupported);
}

MT_TEST(elf, rejects_bad_class)
{
    auto fixture = fixture::build_elf(true);
    fixture.data[4] = std::byte{99};  // EI_CLASS
    auto result = ElfImage::parse(fixture.data);
    MT_REQUIRE_ERR(result);
}

MT_TEST(elf, rejects_truncated)
{
    auto fixture = fixture::build_elf(true);
    for (std::size_t size = 0; size < fixture.data.size(); size += 13) {
        std::vector<std::byte> truncated(fixture.data.begin(),
                                         fixture.data.begin() + size);
        auto result = ElfImage::parse(truncated);
        (void)result;  // must not crash
    }
}

MT_TEST(elf, rejects_shoff_out_of_range)
{
    auto fixture = fixture::build_elf(true);
    // e_shoff at offset 0x28 (u64) -> point far beyond the buffer.
    for (int i = 0; i < 8; ++i) {
        fixture.data[0x28 + i] = std::byte{0xFF};
    }
    auto result = ElfImage::parse(fixture.data);
    MT_REQUIRE_ERR(result);
}

MT_TEST(elf, rejects_bad_shstrndx)
{
    auto fixture = fixture::build_elf(true);
    // e_shstrndx = 0xFFFF (SHN_XINDEX) with a section-0 that lacks real info:
    // section 0 is all zeros, so shstrndx resolves to 0 -> sections unnamed.
    fixture.data[0x3E] = std::byte{0xFF};
    fixture.data[0x3F] = std::byte{0xFF};
    auto result = ElfImage::parse(fixture.data);
    // Should still parse (names optional), or fail cleanly — never crash.
    if (result.ok()) {
        MT_CHECK(result->sections()[1].name.empty());
    }
}

MT_TEST(elf, extended_section_numbering)
{
    // e_shnum == 0 means the real count lives in section 0's sh_size and the
    // real shstrndx in section 0's sh_link (ELF64: offsets 32 and 40 within
    // the section header at e_shoff = 0x200).
    auto fixture = fixture::build_elf(true);
    auto write_u64 = [&fixture](std::size_t offset, std::uint64_t value) {
        for (int i = 0; i < 8; ++i) {
            fixture.data[offset + i] = static_cast<std::byte>((value >> (i * 8)) & 0xFF);
        }
    };
    write_u64(0x200 + 32, 5);   // section 0 sh_size = real count
    write_u64(0x200 + 40, 4);   // section 0 sh_link = real shstrndx
    fixture.data[0x3C] = std::byte{0x00};  // e_shnum = 0
    fixture.data[0x3D] = std::byte{0x00};
    auto result = ElfImage::parse(fixture.data);
    MT_REQUIRE_OK(result);
    MT_CHECK_EQ(result->sections().size(), std::size_t{5});
    MT_CHECK_EQ(result->sections()[1].name, std::string{".text"});
}

MT_TEST(elf, type_classification)
{
    auto fixture = fixture::build_elf(true);
    auto result = ElfImage::parse(fixture.data);
    MT_REQUIRE_OK(result);
    MT_CHECK_EQ(result->type(), ElfType::Shared);  // e_type = 3 (ET_DYN)
}

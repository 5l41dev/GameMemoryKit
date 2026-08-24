#include "gmk/binary/pe.hpp"

#include "../fixtures/pe_fixture.hpp"
#include "../support/minitest.hpp"

#include <cstdint>
#include <string>

using gmk::Architecture;
using gmk::ErrorCode;
using gmk::PeImage;

MT_TEST(pe, parses_pe64_fixture)
{
    auto fixture = fixture::build_pe(true);
    auto result = PeImage::parse(fixture.data);
    MT_REQUIRE_OK(result);

    const PeImage& image = *result;
    MT_CHECK(image.is_64_bit());
    MT_CHECK_EQ(image.architecture(), Architecture::X86_64);
    MT_CHECK_EQ(image.image_base(), std::uint64_t{0x140000000ull});
    MT_CHECK_EQ(image.entry_rva(), std::uint32_t{0x1000});
    MT_CHECK_EQ(image.entry_point().value(), std::uint64_t{0x140001000ull});
    MT_CHECK_EQ(image.size_of_image(), std::uint32_t{0x3000});
    MT_CHECK_EQ(image.sections().size(), std::size_t{2});
    MT_CHECK_EQ(image.sections()[0].name, std::string{".text"});
    MT_CHECK(image.sections()[0].executable());
    MT_CHECK(image.sections()[0].readable());
    MT_CHECK(!image.sections()[0].writable());
    MT_CHECK_EQ(image.sections()[1].name, std::string{".rdata"});
    MT_CHECK(!image.sections()[1].executable());
}

MT_TEST(pe, parses_pe32_fixture)
{
    auto fixture = fixture::build_pe(false);
    auto result = PeImage::parse(fixture.data);
    MT_REQUIRE_OK(result);

    const PeImage& image = *result;
    MT_CHECK(!image.is_64_bit());
    MT_CHECK_EQ(image.architecture(), Architecture::X86);
    MT_CHECK_EQ(image.image_base(), std::uint64_t{0x400000});
    MT_CHECK_EQ(image.entry_point().value(), std::uint64_t{0x401000});
}

MT_TEST(pe, parses_exports)
{
    auto fixture = fixture::build_pe(true);
    auto result = PeImage::parse(fixture.data);
    MT_REQUIRE_OK(result);

    const auto& exports = result->exports();
    MT_CHECK_EQ(exports.size(), std::size_t{2});
    MT_CHECK_EQ(exports[0].name, std::string{"export_one"});
    MT_CHECK_EQ(exports[0].rva, std::uint32_t{0x1000});
    MT_CHECK_EQ(exports[0].ordinal, std::uint16_t{1});
    MT_CHECK(!exports[0].forwarded);
    MT_CHECK_EQ(exports[1].name, std::string{"export_two"});
    MT_CHECK_EQ(exports[1].rva, std::uint32_t{0x1010});
    MT_CHECK_EQ(exports[1].ordinal, std::uint16_t{2});
}

MT_TEST(pe, parses_imports)
{
    auto fixture = fixture::build_pe(true);
    auto result = PeImage::parse(fixture.data);
    MT_REQUIRE_OK(result);

    const auto& imports = result->imports();
    MT_CHECK_EQ(imports.size(), std::size_t{1});
    MT_CHECK_EQ(imports[0].name, std::string{"kernel32.dll"});
    MT_CHECK_EQ(imports[0].functions.size(), std::size_t{1});
    MT_CHECK_EQ(imports[0].functions[0].name, std::string{"sleep"});
    MT_CHECK(!imports[0].functions[0].by_ordinal);
}

MT_TEST(pe, rva_to_offset)
{
    auto fixture = fixture::build_pe(true);
    auto result = PeImage::parse(fixture.data);
    MT_REQUIRE_OK(result);

    // .text at rva 0x1000 maps to file 0x200.
    MT_CHECK_EQ(*result->rva_to_offset(0x1000), std::uint32_t{0x200});
    // .rdata at rva 0x2000 maps to file 0x400.
    MT_CHECK_EQ(*result->rva_to_offset(0x2000), std::uint32_t{0x400});
    // Not covered by any section.
    MT_CHECK(!result->rva_to_offset(0x4000).has_value());
}

MT_TEST(pe, rejects_non_pe)
{
    std::vector<std::byte> garbage{std::byte{0x00}, std::byte{0x11}, std::byte{0x22}};
    auto result = PeImage::parse(garbage);
    MT_REQUIRE_ERR(result);
    MT_CHECK_EQ(result.error().code(), ErrorCode::InvalidBinary);
}

MT_TEST(pe, rejects_truncated)
{
    auto fixture = fixture::build_pe(true);
    for (std::size_t size = 0; size < fixture.data.size(); size += 17) {
        std::vector<std::byte> truncated(fixture.data.begin(),
                                         fixture.data.begin() + size);
        auto result = PeImage::parse(truncated);
        // Must never crash; must either fail cleanly or succeed.
        (void)result;
    }
}

MT_TEST(pe, rejects_bad_dos_signature)
{
    auto fixture = fixture::build_pe(true);
    fixture.data[0] = std::byte{'N'};
    fixture.data[1] = std::byte{'Z'};
    auto result = PeImage::parse(fixture.data);
    MT_REQUIRE_ERR(result);
}

MT_TEST(pe, rejects_e_lfanew_out_of_range)
{
    auto fixture = fixture::build_pe(true);
    // Point e_lfanew far beyond the file.
    auto& bytes = fixture.data;
    bytes[0x3C] = std::byte{0xFF};
    bytes[0x3D] = std::byte{0xFF};
    bytes[0x3E] = std::byte{0xFF};
    bytes[0x3F] = std::byte{0x7F};
    auto result = PeImage::parse(bytes);
    MT_REQUIRE_ERR(result);
    MT_CHECK_EQ(result.error().code(), ErrorCode::InvalidBinary);
}

MT_TEST(pe, rejects_bad_pe_signature)
{
    auto fixture = fixture::build_pe(true);
    // e_lfanew = 0x80; corrupt the "PE\0\0" signature.
    fixture.data[0x80] = std::byte{'X'};
    auto result = PeImage::parse(fixture.data);
    MT_REQUIRE_ERR(result);
}

MT_TEST(pe, rejects_huge_section_count)
{
    auto fixture = fixture::build_pe(true);
    // NumberOfSections = 0xFFFF but only 2 section headers exist.
    fixture.data[0x86] = std::byte{0xFF};
    fixture.data[0x87] = std::byte{0xFF};
    auto result = PeImage::parse(fixture.data);
    MT_REQUIRE_ERR(result);
}

MT_TEST(pe, rejects_unmapped_export_directory)
{
    auto fixture = fixture::build_pe(true);
    // For PE32+: optional header starts at 0x98, data directories at +112,
    // so directory[0].rva lives at file offset 0x108. Point it into no
    // section (0x7000).
    fixture.data[0x108] = std::byte{0x00};
    fixture.data[0x109] = std::byte{0x70};
    fixture.data[0x10A] = std::byte{0x00};
    fixture.data[0x10B] = std::byte{0x00};
    auto result = PeImage::parse(fixture.data);
    MT_REQUIRE_ERR(result);
    MT_CHECK_EQ(result.error().code(), ErrorCode::InvalidBinary);
}

MT_TEST(pe, rejects_truncated_export_name)
{
    auto fixture = fixture::build_pe(true);
    // Corrupt the first export name RVA to point into .text (no NUL-terminated
    // string there is guaranteed) — simpler: point it at an unmapped RVA.
    // Names array is at rva 0x2030 -> file 0x430; overwrite first entry.
    fixture.data[0x430] = std::byte{0x00};
    fixture.data[0x431] = std::byte{0x90};
    fixture.data[0x432] = std::byte{0x00};
    fixture.data[0x433] = std::byte{0x00};
    auto result = PeImage::parse(fixture.data);
    MT_REQUIRE_ERR(result);
}

MT_TEST(pe, empty_optional_header_size_rejected)
{
    auto fixture = fixture::build_pe(true);
    // SizeOfOptionalHeader lives at COFF+16 = 0x84+16 = 0x94. Zero it.
    fixture.data[0x94] = std::byte{0x00};
    fixture.data[0x95] = std::byte{0x00};
    auto result = PeImage::parse(fixture.data);
    MT_REQUIRE_ERR(result);
}

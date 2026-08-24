#include "gmk/binary/binary_image.hpp"

#include "../fixtures/elf_fixture.hpp"
#include "../fixtures/pe_fixture.hpp"
#include "../support/minitest.hpp"

#include <cstdio>
#include <filesystem>
#include <string>

#if defined(GMK_OS_WINDOWS)
#  define NOMINMAX
#  include <windows.h>
#endif

using gmk::BinaryFormat;
using gmk::BinaryImage;
using gmk::ByteView;

MT_TEST(binary_image, detects_pe)
{
    auto fixture = fixture::build_pe(true);
    auto result = BinaryImage::parse(fixture.data);
    MT_REQUIRE_OK(result);
    MT_CHECK_EQ(result->format(), BinaryFormat::Pe);
    MT_CHECK_EQ(std::string{result->format_name()}, std::string{"PE"});
    MT_CHECK(result->is_64_bit());
    MT_CHECK_EQ(result->architecture(), gmk::Architecture::X86_64);
    MT_CHECK_EQ(result->entry_point().value(), std::uint64_t{0x140001000ull});
}

MT_TEST(binary_image, detects_elf)
{
    auto fixture = fixture::build_elf(true);
    auto result = BinaryImage::parse(fixture.data);
    MT_REQUIRE_OK(result);
    MT_CHECK_EQ(result->format(), BinaryFormat::Elf);
    MT_CHECK_EQ(std::string{result->format_name()}, std::string{"ELF"});
    MT_CHECK(result->is_64_bit());
    MT_CHECK_EQ(result->architecture(), gmk::Architecture::X86_64);
}

MT_TEST(binary_image, rejects_unknown_format)
{
    std::vector<std::byte> garbage{std::byte{0xDE}, std::byte{0xAD},
                                   std::byte{0xBE}, std::byte{0xEF}};
    auto result = BinaryImage::parse(garbage);
    MT_REQUIRE_ERR(result);
    MT_CHECK_EQ(result.error().code(), gmk::ErrorCode::InvalidBinary);
}

MT_TEST(binary_image, rejects_empty_input)
{
    auto result = BinaryImage::parse(ByteView{});
    MT_REQUIRE_ERR(result);
}

MT_TEST(binary_image, normalized_pe_sections)
{
    auto fixture = fixture::build_pe(true);
    auto result = BinaryImage::parse(fixture.data);
    MT_REQUIRE_OK(result);

    const auto& sections = result->sections();
    MT_CHECK_EQ(sections.size(), std::size_t{2});
    MT_CHECK_EQ(sections[0].name, std::string{".text"});
    MT_CHECK_EQ(sections[0].address.value(), std::uint64_t{0x140001000ull});
    MT_CHECK(sections[0].executable);
    MT_CHECK(sections[0].readable);
    MT_CHECK(!sections[0].writable);
    MT_CHECK_EQ(sections[1].name, std::string{".rdata"});
    MT_CHECK(!sections[1].executable);
}

MT_TEST(binary_image, pe_exports)
{
    auto fixture = fixture::build_pe(true);
    auto result = BinaryImage::parse(fixture.data);
    MT_REQUIRE_OK(result);

    const auto& exports = result->exports();
    MT_CHECK_EQ(exports.size(), std::size_t{2});
    MT_CHECK_EQ(exports[0].name, std::string{"export_one"});
    MT_CHECK_EQ(exports[0].address.value(), std::uint64_t{0x140001000ull});
}

MT_TEST(binary_image, elf_exports)
{
    auto fixture = fixture::build_elf(true);
    auto result = BinaryImage::parse(fixture.data);
    MT_REQUIRE_OK(result);

    const auto& exports = result->exports();
    MT_CHECK_EQ(exports.size(), std::size_t{2});
    MT_CHECK_EQ(exports[0].name, std::string{"fixture_add"});
    MT_CHECK_EQ(exports[0].address.value(), std::uint64_t{0x401000});
}

MT_TEST(binary_image, format_specific_views)
{
    auto pe = fixture::build_pe(true);
    auto pe_result = BinaryImage::parse(pe.data);
    MT_REQUIRE_OK(pe_result);
    MT_CHECK(pe_result->as_pe() != nullptr);
    MT_CHECK(pe_result->as_elf() == nullptr);

    auto elf = fixture::build_elf(true);
    auto elf_result = BinaryImage::parse(elf.data);
    MT_REQUIRE_OK(elf_result);
    MT_CHECK(elf_result->as_elf() != nullptr);
    MT_CHECK(elf_result->as_pe() == nullptr);
}

MT_TEST(binary_image, parse_file_roundtrip)
{
    auto fixture = fixture::build_pe(true);
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "gmk_test_binary_image.bin";
    {
        std::FILE* file = std::fopen(path.string().c_str(), "wb");
        MT_REQUIRE(file != nullptr);
        std::fwrite(fixture.data.data(), 1, fixture.data.size(), file);
        std::fclose(file);
    }

    auto result = BinaryImage::parse_file(path);
    MT_REQUIRE_OK(result);
    MT_CHECK_EQ(result->format(), BinaryFormat::Pe);
    MT_CHECK_EQ(result->exports().size(), std::size_t{2});

    // Note: DeleteFileW (kernel32) rather than std::remove, because some
    // Windows builds lack the UCRT filesystem api-set that MinGW links
    // remove() against; kernel32 is always present.
#if defined(GMK_OS_WINDOWS)
    DeleteFileW(path.wstring().c_str());
#else
    std::remove(path.string().c_str());
#endif
}

MT_TEST(binary_image, parse_file_missing)
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "gmk_no_such_file_xyz.bin";
    auto result = BinaryImage::parse_file(path);
    MT_REQUIRE_ERR(result);
    MT_CHECK_EQ(result.error().code(), gmk::ErrorCode::IoError);
}

#include "gmk/pattern/scanner.hpp"

#include "../support/minitest.hpp"

#include <cstddef>
#include <string>

using gmk::ByteView;
using gmk::Pattern;
using gmk::PatternScanner;

namespace {

std::vector<std::byte> make_buffer(std::string_view hex)
{
    std::vector<std::byte> out;
    auto digit = [](char c) -> int {
        if (c >= '0' && c <= '9') {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f') {
            return c - 'a' + 10;
        }
        return c - 'A' + 10;
    };
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
        out.push_back(static_cast<std::byte>((digit(hex[i]) << 4) | digit(hex[i + 1])));
    }
    return out;
}

}  // namespace

MT_TEST(scanner, finds_first_match)
{
    auto p = Pattern::parse("48 85 C0");
    MT_REQUIRE_OK(p);
    auto buffer = make_buffer("90904885c090");
    auto found = PatternScanner::find_first(buffer, *p);
    MT_REQUIRE_OK(found);
    MT_REQUIRE(found->has_value());
    MT_CHECK_EQ(**found, std::size_t{2});
}

MT_TEST(scanner, no_match_returns_nullopt)
{
    auto p = Pattern::parse("48 85 C0");
    MT_REQUIRE_OK(p);
    auto buffer = make_buffer("9090488bc090");
    auto found = PatternScanner::find_first(buffer, *p);
    MT_REQUIRE_OK(found);
    MT_CHECK(!found->has_value());
}

MT_TEST(scanner, match_at_offset_zero)
{
    auto p = Pattern::parse("48 8B C0");
    MT_REQUIRE_OK(p);
    auto buffer = make_buffer("488bc0");
    auto found = PatternScanner::find_first(buffer, *p);
    MT_REQUIRE_OK(found);
    MT_REQUIRE(found->has_value());
    MT_CHECK_EQ(**found, std::size_t{0});
}

MT_TEST(scanner, match_at_end_of_buffer)
{
    auto p = Pattern::parse("85 C0");
    MT_REQUIRE_OK(p);
    auto buffer = make_buffer("909085c0");
    auto found = PatternScanner::find_first(buffer, *p);
    MT_REQUIRE_OK(found);
    MT_REQUIRE(found->has_value());
    MT_CHECK_EQ(**found, std::size_t{2});  // exact tail fit
}

MT_TEST(scanner, pattern_larger_than_buffer)
{
    auto p = Pattern::parse("48 8B C0 90 90");
    MT_REQUIRE_OK(p);
    auto buffer = make_buffer("488bc0");
    auto found = PatternScanner::find_first(buffer, *p);
    MT_REQUIRE_OK(found);
    MT_CHECK(!found->has_value());
}

MT_TEST(scanner, wildcard_matching)
{
    auto p = Pattern::parse("48 8B ?? ?? 48 85");
    MT_REQUIRE_OK(p);
    // 48 8b 0c 00 48 85 c0 — matches the pattern at offset 0 (and nowhere else).
    auto buffer = make_buffer("488b0c004885c0");
    auto found = PatternScanner::find_first(buffer, *p);
    MT_REQUIRE_OK(found);
    MT_REQUIRE(found->has_value());
    MT_CHECK_EQ(**found, std::size_t{0});
    MT_CHECK(!PatternScanner::find_first_from(buffer, *p, 1)->has_value());
}

MT_TEST(scanner, all_wildcard_pattern)
{
    auto p = Pattern::parse("?? ??");
    MT_REQUIRE_OK(p);
    auto buffer = make_buffer("9090");
    auto found = PatternScanner::find_first(buffer, *p);
    MT_REQUIRE_OK(found);
    MT_REQUIRE(found->has_value());
    MT_CHECK_EQ(**found, std::size_t{0});
}

MT_TEST(scanner, find_all_multiple_matches)
{
    auto p = Pattern::parse("90 90");
    MT_REQUIRE_OK(p);
    auto buffer = make_buffer("9090909090");  // 4 consecutive matches at 0,1,2,3
    auto all = PatternScanner::find_all(buffer, *p);
    MT_REQUIRE_OK(all);
    MT_CHECK_EQ(all->size(), std::size_t{4});
    MT_CHECK_EQ((*all)[0], std::size_t{0});
    MT_CHECK_EQ((*all)[3], std::size_t{3});
}

MT_TEST(scanner, find_all_no_matches)
{
    auto p = Pattern::parse("CC CC");
    MT_REQUIRE_OK(p);
    auto buffer = make_buffer("90909090");
    auto all = PatternScanner::find_all(buffer, *p);
    MT_REQUIRE_OK(all);
    MT_CHECK(all->empty());
}

MT_TEST(scanner, count_matches)
{
    auto p = Pattern::parse("AB");
    MT_REQUIRE_OK(p);
    auto buffer = make_buffer("ab00abab00ab");
    auto count = PatternScanner::count(buffer, *p);
    MT_REQUIRE_OK(count);
    MT_CHECK_EQ(*count, std::size_t{4});
}

MT_TEST(scanner, count_zero)
{
    auto p = Pattern::parse("AB");
    MT_REQUIRE_OK(p);
    auto buffer = make_buffer("cdcdcd");
    auto count = PatternScanner::count(buffer, *p);
    MT_REQUIRE_OK(count);
    MT_CHECK_EQ(*count, std::size_t{0});
}

MT_TEST(scanner, empty_pattern_rejected)
{
    Pattern empty = Pattern::from_bytes({});
    auto buffer = make_buffer("9090");
    auto found = PatternScanner::find_first(buffer, empty);
    MT_REQUIRE_ERR(found);
    MT_CHECK_EQ(found.error().code(), gmk::ErrorCode::InvalidArgument);
}

MT_TEST(scanner, empty_buffer)
{
    auto p = Pattern::parse("48 8B");
    MT_REQUIRE_OK(p);
    auto found = PatternScanner::find_first(ByteView{}, *p);
    MT_REQUIRE_OK(found);
    MT_CHECK(!found->has_value());
}

MT_TEST(scanner, find_first_from)
{
    auto p = Pattern::parse("AB");
    MT_REQUIRE_OK(p);
    // 3 bytes: ab 00 ab — matches at offsets 0 and 2.
    auto buffer = make_buffer("ab00ab");
    auto first = PatternScanner::find_first_from(buffer, *p, 1);
    MT_REQUIRE_OK(first);
    MT_REQUIRE(first->has_value());
    MT_CHECK_EQ(**first, std::size_t{2});
    auto none = PatternScanner::find_first_from(buffer, *p, 3);
    MT_REQUIRE_OK(none);
    MT_CHECK(!none->has_value());
}

MT_TEST(scanner, single_byte_pattern)
{
    auto p = Pattern::parse("C3");
    MT_REQUIRE_OK(p);
    auto buffer = make_buffer("90c3c3");
    auto all = PatternScanner::find_all(buffer, *p);
    MT_REQUIRE_OK(all);
    MT_CHECK_EQ(all->size(), std::size_t{2});
}

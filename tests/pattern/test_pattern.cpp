#include "gmk/pattern/pattern.hpp"

#include "../support/minitest.hpp"

#include <string>

using gmk::Pattern;

namespace {

std::vector<std::byte> hex_bytes(std::string_view hex)
{
    std::vector<std::byte> out;
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
        const char hi = hex[i];
        const char lo = hex[i + 1];
        auto digit = [](char c) {
            if (c >= '0' && c <= '9') {
                return static_cast<int>(c - '0');
            }
            return static_cast<int>(c - 'a' + 10);
        };
        out.push_back(static_cast<std::byte>((digit(hi) << 4) | digit(lo)));
    }
    return out;
}

}  // namespace

MT_TEST(pattern, parse_basic)
{
    auto result = Pattern::parse("48 8B 0C 48 85 C0");
    MT_REQUIRE_OK(result);
    MT_CHECK_EQ(result->size(), std::size_t{6});
    MT_CHECK(!result->bytes()[0].wildcard);
    MT_CHECK_EQ(result->bytes()[0].value, 0x48);
    MT_CHECK_EQ(result->bytes()[5].value, 0xC0);
}

MT_TEST(pattern, parse_wildcards)
{
    auto result = Pattern::parse("48 8B ?? ?? ?? 48 85 C0");
    MT_REQUIRE_OK(result);
    MT_CHECK_EQ(result->size(), std::size_t{8});
    MT_CHECK(result->bytes()[2].wildcard);
    MT_CHECK(result->bytes()[3].wildcard);
    MT_CHECK(result->bytes()[4].wildcard);
    MT_CHECK(!result->bytes()[0].wildcard);
    MT_CHECK(!result->bytes()[7].wildcard);
}

MT_TEST(pattern, parse_case_insensitive)
{
    auto upper = Pattern::parse("48 8B C0");
    auto lower = Pattern::parse("48 8b c0");
    auto mixed = Pattern::parse("48 8B c0");
    MT_REQUIRE_OK(upper);
    MT_REQUIRE_OK(lower);
    MT_REQUIRE_OK(mixed);
    MT_CHECK(upper->bytes()[2].value == lower->bytes()[2].value);
    MT_CHECK(lower->bytes()[2].value == mixed->bytes()[2].value);
}

MT_TEST(pattern, parse_single_question_mark_wildcard)
{
    auto result = Pattern::parse("48 ? 8B");
    MT_REQUIRE_OK(result);
    MT_CHECK(result->bytes()[1].wildcard);
}

MT_TEST(pattern, parse_tolerates_extra_whitespace)
{
    auto result = Pattern::parse("  48\t8B\r\n??  ");
    MT_REQUIRE_OK(result);
    MT_CHECK_EQ(result->size(), std::size_t{3});
}

MT_TEST(pattern, parse_empty_fails)
{
    auto result = Pattern::parse("");
    MT_REQUIRE_ERR(result);
    MT_CHECK_EQ(result.error().code(), gmk::ErrorCode::ParseError);
}

MT_TEST(pattern, parse_whitespace_only_fails)
{
    auto result = Pattern::parse("   \t ");
    MT_REQUIRE_ERR(result);
}

MT_TEST(pattern, parse_single_hex_digit_fails)
{
    auto result = Pattern::parse("4");
    MT_REQUIRE_ERR(result);
}

MT_TEST(pattern, parse_bad_hex_fails)
{
    auto result = Pattern::parse("48 zz 8B");
    MT_REQUIRE_ERR(result);
    MT_CHECK_EQ(result.error().code(), gmk::ErrorCode::ParseError);
}

MT_TEST(pattern, parse_three_char_token_fails)
{
    auto result = Pattern::parse("48 8B1 C0");
    MT_REQUIRE_ERR(result);
}

MT_TEST(pattern, parse_partial_wildcard_fails)
{
    auto result = Pattern::parse("48 ?2 C0");
    MT_REQUIRE_ERR(result);
}

MT_TEST(pattern, parse_stray_character_fails)
{
    auto result = Pattern::parse("48 8B C0!");
    MT_REQUIRE_ERR(result);
}

MT_TEST(pattern, parse_error_reports_position)
{
    auto result = Pattern::parse("48 8B xx");
    MT_REQUIRE_ERR(result);
    MT_CHECK(result.error().message().find("position") != std::string::npos);
}

MT_TEST(pattern, first_fixed_index)
{
    auto p = Pattern::parse("?? 48 ?? 8B");
    MT_REQUIRE_OK(p);
    MT_CHECK_EQ(*p->first_fixed_index(), std::size_t{1});
}

MT_TEST(pattern, first_fixed_index_all_wildcards)
{
    auto p = Pattern::parse("?? ?? ??");
    MT_REQUIRE_OK(p);
    MT_CHECK(!p->first_fixed_index().has_value());
    MT_CHECK(p->all_wildcards());
}

MT_TEST(pattern, from_bytes_programmatic)
{
    std::vector<Pattern::Byte> bytes{
        Pattern::Byte{0x90, false},
        Pattern::Byte{0, true},
        Pattern::Byte{0xCC, false},
    };
    Pattern p = Pattern::from_bytes(std::move(bytes));
    MT_CHECK_EQ(p.size(), std::size_t{3});
    MT_CHECK(p.bytes()[1].wildcard);
}

MT_TEST(pattern, matches_at_basic)
{
    auto p = Pattern::parse("48 8B C0");
    MT_REQUIRE_OK(p);
    std::vector<std::byte> buffer = hex_bytes("9090488bc0");
    MT_CHECK(gmk::matches_at(buffer, 2, *p));
    MT_CHECK(!gmk::matches_at(buffer, 0, *p));
    MT_CHECK(!gmk::matches_at(buffer, 3, *p));
}

MT_TEST(pattern, matches_at_wildcards)
{
    auto p = Pattern::parse("48 ?? C0");
    MT_REQUIRE_OK(p);
    std::vector<std::byte> buffer = hex_bytes("488bc0");
    MT_CHECK(gmk::matches_at(buffer, 0, *p));
    MT_CHECK(!gmk::matches_at(buffer, 1, *p));
}

MT_TEST(pattern, matches_at_bounds_checked)
{
    auto p = Pattern::parse("48 8B C0");
    MT_REQUIRE_OK(p);
    std::vector<std::byte> buffer = hex_bytes("488bc0");
    // Pattern fits exactly at offset 0, so offset 1 is out of bounds.
    MT_CHECK(!gmk::matches_at(buffer, 1, *p));
    // Huge offset must not read out of bounds.
    MT_CHECK(!gmk::matches_at(buffer, std::size_t{1} << 60, *p));
}

MT_TEST(pattern, matches_at_empty_pattern_never_matches)
{
    std::vector<std::byte> buffer{std::byte{0x48}};
    auto p = Pattern::parse("48");
    MT_REQUIRE_OK(p);
    (void)p;
    // An empty (size 0) pattern must not match; construct via from_bytes.
    Pattern empty = Pattern::from_bytes({});
    MT_CHECK(!gmk::matches_at(buffer, 0, empty));
}

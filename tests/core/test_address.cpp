#include "gmk/core/address.hpp"

#include "../support/minitest.hpp"

#include <cstdint>
#include <string>

using gmk::Address;

MT_TEST(address, default_is_null)
{
    Address a;
    MT_CHECK(!a.valid());
    MT_CHECK(a.is_null());
    MT_CHECK_EQ(a.value(), std::uintptr_t{0});
}

MT_TEST(address, explicit_value_roundtrip)
{
    Address a{std::uintptr_t{0x7ff6a1b2c000}};
    MT_CHECK(a.valid());
    MT_CHECK_EQ(a.value(), std::uintptr_t{0x7ff6a1b2c000});
}

MT_TEST(address, null_is_invalid)
{
    Address a{std::uintptr_t{0}};
    MT_CHECK(!a.valid());
    MT_CHECK(a.is_null());
}

MT_TEST(address, add_positive)
{
    Address a{std::uintptr_t{0x1000}};
    MT_CHECK_EQ(a.add(std::size_t{0x20}).value(), std::uintptr_t{0x1020});
    MT_CHECK_EQ((a + std::ptrdiff_t{0x20}).value(), std::uintptr_t{0x1020});
}

MT_TEST(address, add_negative)
{
    Address a{std::uintptr_t{0x1000}};
    MT_CHECK_EQ(a.add(std::ptrdiff_t{-0x100}).value(), std::uintptr_t{0xF00});
    MT_CHECK_EQ((a - std::ptrdiff_t{0x100}).value(), std::uintptr_t{0xF00});
}

MT_TEST(address, add_underflow_returns_null)
{
    Address a{std::uintptr_t{0x10}};
    MT_CHECK(!a.add(std::ptrdiff_t{-0x20}).valid());
}

MT_TEST(address, add_overflow_returns_null)
{
    Address a{std::uintptr_t{0xFFFFFFFFFFFFFFFFull}};
    MT_CHECK(!a.add(std::size_t{1}).valid());
}

MT_TEST(address, add_overflow_wraps_midpoint)
{
    // 0xFFFF...FF + 0x10 must not wrap to 0x0F; it must report failure.
    Address a{std::uintptr_t{0xFFFFFFFFFFFFFFF0ull}};
    MT_CHECK(!a.add(std::size_t{0x20}).valid());
}

MT_TEST(address, difference)
{
    Address a{std::uintptr_t{0x2000}};
    Address b{std::uintptr_t{0x1000}};
    MT_CHECK_EQ(a.difference(b), std::ptrdiff_t{0x1000});
    MT_CHECK_EQ(b.difference(a), std::ptrdiff_t{-0x1000});
}

MT_TEST(address, align_down)
{
    Address a{std::uintptr_t{0x1234}};
    MT_CHECK_EQ(a.align_down(0x1000).value(), std::uintptr_t{0x1000});
}

MT_TEST(address, align_up)
{
    Address a{std::uintptr_t{0x1234}};
    MT_CHECK_EQ(a.align_up(0x1000).value(), std::uintptr_t{0x2000});
}

MT_TEST(address, align_up_exact)
{
    Address a{std::uintptr_t{0x2000}};
    MT_CHECK_EQ(a.align_up(0x1000).value(), std::uintptr_t{0x2000});
}

MT_TEST(address, comparison)
{
    Address a{std::uintptr_t{0x1000}};
    Address b{std::uintptr_t{0x2000}};
    MT_CHECK(a < b);
    MT_CHECK(b > a);
    MT_CHECK(a == a);
    MT_CHECK(a != b);
    MT_CHECK(a <= b);
    MT_CHECK(b >= a);
}

MT_TEST(address, from_pointer)
{
    int value = 42;
    Address a{&value};
    MT_CHECK(a.valid());
    MT_CHECK_EQ(a.as_ptr<int>(), &value);
}

MT_TEST(address, to_string_format)
{
    Address a{std::uintptr_t{0xabcdef}};
    MT_CHECK_EQ(gmk::to_string(a), std::string{"0xabcdef"});
}

MT_TEST(address, stream_format)
{
    Address a{std::uintptr_t{0x1234}};
    std::ostringstream out;
    out << a;
    MT_CHECK_EQ(out.str(), std::string{"0x1234"});
}

MT_TEST(address, hash_support)
{
    Address a{std::uintptr_t{0x1234}};
    std::hash<Address> hasher;
    MT_CHECK_EQ(hasher(a), std::hash<std::uintptr_t>{}(std::uintptr_t{0x1234}));
}

MT_TEST(address, add_assign)
{
    Address a{std::uintptr_t{0x1000}};
    a += std::ptrdiff_t{0x10};
    MT_CHECK_EQ(a.value(), std::uintptr_t{0x1010});
    a -= std::ptrdiff_t{0x20};
    MT_CHECK_EQ(a.value(), std::uintptr_t{0xFF0});
}

MT_TEST(address, is_trivially_copyable)
{
    static_assert(std::is_trivially_copyable_v<Address>);
    static_assert(sizeof(Address) == sizeof(std::uintptr_t));
}

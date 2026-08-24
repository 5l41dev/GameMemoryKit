#include "gmk/core/byte_view.hpp"

#include "../support/minitest.hpp"

#include <cstddef>
#include <string>
#include <vector>

using gmk::ByteView;
using gmk::as_bytes;

MT_TEST(byte_view, span_conversion)
{
    std::vector<std::uint32_t> data{0x01020304u, 0x11223344u};
    ByteView view = gmk::as_bytes(std::span{data});
    MT_CHECK_EQ(view.size(), std::size_t{8});
    MT_CHECK_EQ(static_cast<int>(view[0]), 4);  // little-endian first byte
    MT_CHECK_EQ(static_cast<int>(view[4]), 0x44);
}

MT_TEST(byte_view, string_conversion)
{
    std::string text = "GMK";
    ByteView view = gmk::as_bytes(std::string_view{text});
    MT_CHECK_EQ(view.size(), std::size_t{3});
    MT_CHECK_EQ(gmk::as_string_view(view), std::string_view{"GMK"});
}

MT_TEST(byte_view, hex_string)
{
    std::vector<std::byte> bytes{std::byte{0x48}, std::byte{0x8B}, std::byte{0xC0}};
    MT_CHECK_EQ(gmk::hex_string(bytes), std::string{"48 8b c0"});
    MT_CHECK_EQ(gmk::hex_string_dense(bytes), std::string{"488BC0"});
}

MT_TEST(byte_view, hex_string_empty)
{
    MT_CHECK_EQ(gmk::hex_string(ByteView{}), std::string{});
}

MT_TEST(byte_view, mutable_view)
{
    std::vector<std::byte> bytes{std::byte{0x01}};
    auto view = gmk::as_mutable_bytes(std::span{bytes});
    view[0] = std::byte{0xFF};
    MT_CHECK_EQ(static_cast<int>(bytes[0]), 0xFF);
}

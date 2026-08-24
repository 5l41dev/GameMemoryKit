#include "gmk/core/byte_view.hpp"

namespace gmk {

namespace {

constexpr char kHexLower[] = "0123456789abcdef";
constexpr char kHexUpper[] = "0123456789ABCDEF";

}  // namespace

std::string hex_string(ByteView bytes)
{
    std::string out;
    out.reserve(bytes.size() * 3);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i != 0) {
            out.push_back(' ');
        }
        const auto b = static_cast<unsigned char>(bytes[i]);
        out.push_back(kHexLower[b >> 4]);
        out.push_back(kHexLower[b & 0x0F]);
    }
    return out;
}

std::string hex_string_dense(ByteView bytes)
{
    std::string out;
    out.reserve(bytes.size() * 2);
    for (std::byte b : bytes) {
        const auto v = static_cast<unsigned char>(b);
        out.push_back(kHexUpper[v >> 4]);
        out.push_back(kHexUpper[v & 0x0F]);
    }
    return out;
}

}  // namespace gmk

#include "gmk/core/error.hpp"

#include "../support/minitest.hpp"

#include <string>

using gmk::Error;
using gmk::ErrorCode;

MT_TEST(error, default_constructs_ok)
{
    Error e;
    MT_CHECK(e.ok());
    MT_CHECK_EQ(e.code(), ErrorCode::None);
    MT_CHECK_EQ(e.native_code(), 0);
}

MT_TEST(error, factory_sets_code)
{
    Error e = Error::not_found("no such process");
    MT_CHECK(!e.ok());
    MT_CHECK_EQ(e.code(), ErrorCode::NotFound);
    MT_CHECK_EQ(e.message(), std::string{"no such process"});
}

MT_TEST(error, what_formatting)
{
    Error e{ErrorCode::InvalidBinary, "bad dos header"};
    MT_CHECK_EQ(e.what(), std::string{"InvalidBinary: bad dos header"});
}

MT_TEST(error, what_includes_native_code)
{
    Error e{ErrorCode::PlatformError, "open failed", 5};
    MT_CHECK_EQ(e.what(), std::string{"PlatformError: open failed (native code 5)"});
}

MT_TEST(error, to_string_name)
{
    MT_CHECK_EQ(std::string{gmk::to_string(ErrorCode::PermissionDenied)}, std::string{"PermissionDenied"});
    MT_CHECK_EQ(std::string{gmk::to_string(ErrorCode::None)}, std::string{"None"});
}

MT_TEST(error, copyable)
{
    Error a = Error::io("disk full", 28);
    Error b = a;
    MT_CHECK_EQ(b.what(), a.what());
    b = Error::parse("bad token");
    MT_CHECK_EQ(b.code(), ErrorCode::ParseError);
    MT_CHECK_EQ(a.code(), ErrorCode::IoError);  // original untouched
}

MT_TEST(error, movable)
{
    Error a = Error::invalid_address("null pointer");
    Error b = std::move(a);
    MT_CHECK_EQ(b.code(), ErrorCode::InvalidAddress);
}

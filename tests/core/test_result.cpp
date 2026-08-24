#include "gmk/core/result.hpp"

#include "../support/minitest.hpp"

#include <string>
#include <utility>
#include <vector>

using gmk::Error;
using gmk::Result;
using gmk::Status;

MT_TEST(result, holds_value)
{
    Result<int> r{42};
    MT_CHECK(r.ok());
    MT_CHECK(r.has_value());
    MT_CHECK(!r.failed());
    MT_CHECK_EQ(r.value(), 42);
    MT_CHECK_EQ(*r, 42);
}

MT_TEST(result, holds_error)
{
    Result<int> r{Error::not_found("nope")};
    MT_CHECK(!r.ok());
    MT_CHECK(r.failed());
    MT_CHECK_EQ(r.error().code(), gmk::ErrorCode::NotFound);
}

MT_TEST(result, implicit_conversion_from_value)
{
    // Implicit construction from a value is intentional and ergonomic.
    Result<std::string> r = std::string{"hello"};
    MT_CHECK(r.ok());
    MT_CHECK_EQ(r.value(), std::string{"hello"});
}

MT_TEST(result, implicit_conversion_from_error)
{
    Result<std::string> r = Error::invalid_argument("bad");
    MT_CHECK(r.failed());
}

MT_TEST(result, value_or)
{
    Result<int> ok{7};
    Result<int> err{Error::not_found("x")};
    MT_CHECK_EQ(ok.value_or(-1), 7);
    MT_CHECK_EQ(err.value_or(-1), -1);
}

MT_TEST(result, map_preserves_error)
{
    Result<int> err{Error::io("failed")};
    auto mapped = err.map([](int v) { return v * 2; });
    MT_CHECK(mapped.failed());
    MT_CHECK_EQ(mapped.error().code(), gmk::ErrorCode::IoError);
}

MT_TEST(result, map_transforms_value)
{
    Result<int> ok{21};
    auto mapped = ok.map([](int v) { return v * 2; });
    MT_CHECK(mapped.ok());
    MT_CHECK_EQ(mapped.value(), 42);
}

MT_TEST(result, copy_and_move)
{
    Result<std::vector<int>> a{std::vector<int>{1, 2, 3}};
    Result<std::vector<int>> b = a;
    MT_CHECK_EQ(b.value().size(), std::size_t{3});
    Result<std::vector<int>> c = std::move(a);
    MT_CHECK_EQ(c.value().size(), std::size_t{3});
}

MT_TEST(result, assignment)
{
    Result<int> r{1};
    r = Result<int>{2};
    MT_CHECK_EQ(r.value(), 2);
    r = Error::invalid_argument("boom");
    MT_CHECK(r.failed());
    r = Result<int>{3};
    MT_CHECK_EQ(r.value(), 3);
}

MT_TEST(result, void_specialization)
{
    Status ok;
    MT_CHECK(ok.ok());
    Status err{Error::permission_denied("denied")};
    MT_CHECK(err.failed());
    MT_CHECK_EQ(err.error().code(), gmk::ErrorCode::PermissionDenied);
}

MT_TEST(result, void_factories)
{
    Status ok = Status::success();
    Status err = Status::failure(Error::unsupported("not here"));
    MT_CHECK(ok.ok());
    MT_CHECK(err.failed());
    MT_CHECK_EQ(err.error().code(), gmk::ErrorCode::Unsupported);
}

MT_TEST(result, non_trivial_type_roundtrip)
{
    struct Widget {
        int id;
        std::string name;
    };
    Result<Widget> r{Widget{1, "gear"}};
    MT_CHECK(r.ok());
    MT_CHECK_EQ(r->name, std::string{"gear"});
    r = Error::not_found("gone");
    MT_CHECK(r.failed());
    MT_CHECK_EQ(r.error().code(), gmk::ErrorCode::NotFound);
}

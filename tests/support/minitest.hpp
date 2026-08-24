// minitest — a tiny, dependency-free test harness for GameMemoryKit.
//
// This is an internal tool of the GameMemoryKit test suite, not part of the
// public library API. It exists so the repository can be cloned, built, and
// tested with no network access and no third-party test framework. It
// deliberately covers only the features the suite needs:
//
//   * auto-registered test cases with suite/name
//   * CHECK (continue) and REQUIRE (abort test) assertions
//   * value comparison with diagnostics
//   * command-line filtering by substring
//   * a summary report and a process exit code equal to the failure count
//
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdio>
#include <exception>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mt {

struct TestCase {
    std::string suite;
    std::string name;
    void (*fn)();
};

inline std::vector<TestCase>& registry()
{
    static std::vector<TestCase> tests;
    return tests;
}

inline int register_test(const char* suite, const char* name, void (*fn)())
{
    registry().push_back(TestCase{suite, name, fn});
    return 0;
}

struct Failure {
    std::string file;
    int line;
    std::string message;
};

inline std::vector<Failure>& failures()
{
    static std::vector<Failure> list;
    return list;
}

inline bool check_impl(bool ok, const char* expr, const char* file, int line)
{
    if (!ok) {
        failures().push_back(Failure{file, line, std::string("CHECK failed: ") + expr});
    }
    return ok;
}

template <typename A, typename B>
inline bool check_eq_impl(const A& a, const B& b, const char* ea, const char* eb,
                          const char* file, int line)
{
    if (a == b) {
        return true;
    }
    std::ostringstream msg;
    msg << "CHECK_EQ failed: " << ea << " == " << eb << "\n";
    msg << "    lhs (" << ea << ") = " << a << "\n";
    msg << "    rhs (" << eb << ") = " << b;
    failures().push_back(Failure{file, line, msg.str()});
    return false;
}

inline bool check_ne_impl(const auto& a, const auto& b, const char* ea, const char* eb,
                          const char* file, int line)
{
    if (a != b) {
        return true;
    }
    std::ostringstream msg;
    msg << "CHECK_NE failed: " << ea << " != " << eb << "\n";
    msg << "    value = " << a;
    failures().push_back(Failure{file, line, msg.str()});
    return false;
}

inline bool check_true_impl(bool value, const char* expr, const char* file, int line)
{
    return check_impl(value, expr, file, line);
}

// Checks that an expression returns ok(); on failure also prints the error.
template <typename ResultLike>
inline bool check_ok_impl(const ResultLike& result, const char* expr, const char* file, int line)
{
    if (result.ok()) {
        return true;
    }
    std::ostringstream msg;
    msg << "CHECK_OK failed: " << expr << "\n";
    msg << "    error: " << result.error().what();
    failures().push_back(Failure{file, line, msg.str()});
    return false;
}

// Checks that an expression fails; on unexpected success, prints the value.
template <typename ResultLike>
inline bool check_err_impl(const ResultLike& result, const char* expr, const char* file, int line)
{
    if (result.failed()) {
        return true;
    }
    std::ostringstream msg;
    msg << "CHECK_ERR failed (expected failure): " << expr << "\n";
    if constexpr (requires { result.value(); }) {
        msg << "    value: " << result.value();
    }
    failures().push_back(Failure{file, line, msg.str()});
    return false;
}

}  // namespace mt

#define MT_TEST(suite_name, test_name)                                                  \
    static void mt_test_##suite_name##_##test_name();                                   \
    static int mt_reg_##suite_name##_##test_name =                                      \
        ::mt::register_test(#suite_name, #test_name, &mt_test_##suite_name##_##test_name); \
    static void mt_test_##suite_name##_##test_name()

#define MT_CHECK(expr) ::mt::check_impl(static_cast<bool>(expr), #expr, __FILE__, __LINE__)
#define MT_REQUIRE(expr)                                                              \
    do {                                                                               \
        if (!::mt::check_impl(static_cast<bool>(expr), #expr, __FILE__, __LINE__)) {   \
            return;                                                                    \
        }                                                                              \
    } while (0)

#define MT_CHECK_EQ(a, b) ::mt::check_eq_impl((a), (b), #a, #b, __FILE__, __LINE__)
#define MT_REQUIRE_EQ(a, b)                                                            \
    do {                                                                               \
        if (!::mt::check_eq_impl((a), (b), #a, #b, __FILE__, __LINE__)) {              \
            return;                                                                    \
        }                                                                              \
    } while (0)

#define MT_CHECK_NE(a, b) ::mt::check_ne_impl((a), (b), #a, #b, __FILE__, __LINE__)
#define MT_REQUIRE_NE(a, b)                                                            \
    do {                                                                               \
        if (!::mt::check_ne_impl((a), (b), #a, #b, __FILE__, __LINE__)) {              \
            return;                                                                    \
        }                                                                              \
    } while (0)

#define MT_CHECK_OK(expr) ::mt::check_ok_impl((expr), #expr, __FILE__, __LINE__)
#define MT_REQUIRE_OK(expr)                                                            \
    do {                                                                               \
        if (!::mt::check_ok_impl((expr), #expr, __FILE__, __LINE__)) {                 \
            return;                                                                    \
        }                                                                              \
    } while (0)

#define MT_CHECK_ERR(expr) ::mt::check_err_impl((expr), #expr, __FILE__, __LINE__)
#define MT_REQUIRE_ERR(expr)                                                           \
    do {                                                                               \
        if (!::mt::check_err_impl((expr), #expr, __FILE__, __LINE__)) {                \
            return;                                                                    \
        }                                                                              \
    } while (0)

namespace mt {

/// Runs all registered tests, optionally filtered by substring (each argument
/// is matched against "suite.name"; a test runs when any filter matches).
/// Returns the number of failed tests.
inline int run_all(const std::vector<std::string>& filters)
{
    int ran = 0;
    int failed = 0;

    for (const auto& test : registry()) {
        const std::string full = test.suite + "." + test.name;
        bool selected = filters.empty();
        for (const auto& filter : filters) {
            if (full.find(filter) != std::string::npos) {
                selected = true;
                break;
            }
        }
        if (!selected) {
            continue;
        }

        const std::size_t before = failures().size();
        std::fprintf(stdout, "[ RUN      ] %s\n", full.c_str());
        try {
            test.fn();
        } catch (const std::exception& ex) {
            failures().push_back(Failure{"<exception>", 0,
                                         std::string("uncaught std::exception: ") + ex.what()});
        } catch (...) {
            failures().push_back(Failure{"<exception>", 0, "uncaught unknown exception"});
        }
        ++ran;

        if (failures().size() == before) {
            std::fprintf(stdout, "[       OK ] %s\n", full.c_str());
        } else {
            ++failed;
            std::fprintf(stdout, "[  FAILED  ] %s\n", full.c_str());
            for (std::size_t i = before; i < failures().size(); ++i) {
                const Failure& f = failures()[i];
                std::fprintf(stdout, "    %s:%d: %s\n", f.file.c_str(), f.line, f.message.c_str());
            }
        }
    }

    std::fprintf(stdout,
                 "\n%d test(s), %d failed, %d passed\n",
                 ran, failed, ran - failed);
    return failed;
}

}  // namespace mt

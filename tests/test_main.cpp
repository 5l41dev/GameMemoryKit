// Shared entry point for every test executable.
//
// Usage: <test-binary> [filter ...]
// Each filter is matched as a substring against "suite.name"; a test runs
// when any filter matches. With no filters, all tests run.
//
// The exit code equals the number of failed tests (0 = all green), which is
// what CTest and CI observe.

#include "support/minitest.hpp"

#include <string>
#include <vector>

int main(int argc, char** argv)
{
    std::vector<std::string> filters;
    for (int i = 1; i < argc; ++i) {
        filters.emplace_back(argv[i]);
    }
    return mt::run_all(filters);
}

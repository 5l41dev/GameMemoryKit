# Test suite. Tests are grouped into small executables, one per subsystem,
# so a failure in one area does not obscure the rest.

# Internal test harness (header-only, not installed).
add_library(gmk_minitest INTERFACE)
target_include_directories(gmk_minitest INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/support")

function(gmk_add_test target)
    add_executable(${target} test_main.cpp ${ARGN})
    target_link_libraries(${target} PRIVATE gmk::gmk gmk_minitest)
    gmk_enable_warnings_test(${target})
    if(GMK_ENABLE_SANITIZERS)
        gmk_enable_sanitizers(${target})
    endif()
    add_test(NAME ${target} COMMAND ${target})
endfunction()

gmk_add_test(gmk_tests_core
    core/test_address.cpp
    core/test_byte_view.cpp
    core/test_error.cpp
    core/test_logging.cpp
    core/test_result.cpp
)

gmk_add_test(gmk_tests_pattern
    pattern/test_pattern.cpp
    pattern/test_scanner.cpp
)

gmk_add_test(gmk_tests_binary
    binary/test_binary_image.cpp
    binary/test_elf.cpp
    binary/test_malformed.cpp
    binary/test_pe.cpp
)

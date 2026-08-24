# GameMemoryKit — compiler warning configuration.
#
# Applies a sensible, portable warning set to a target. We deliberately avoid
# enabling every possible warning: the goal is catching real bugs across the
# supported compilers (MSVC, GCC, Clang) without drowning in false positives.

function(gmk_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wnon-virtual-dtor
            -Wold-style-cast
            -Wcast-align
            -Wunused
            -Woverloaded-virtual
            -Wconversion
            -Wsign-conversion
            -Wnull-dereference
            -Wdouble-promotion
            -Wformat=2
        )
    endif()
endfunction()

# Warning set for test/example targets, where strictness is relaxed slightly
# (tests intentionally exercise edge cases that trip -Wconversion).
function(gmk_enable_warnings_test target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W3 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

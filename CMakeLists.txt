cmake_minimum_required(VERSION 3.21)

project(
    GameMemoryKit
    VERSION 0.1.0
    DESCRIPTION "Cross-platform C++ toolkit for process inspection, executable analysis, and developer instrumentation"
    HOMEPAGE_URL "https://github.com/gamememorykit/GameMemoryKit"
    LANGUAGES CXX
)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------

# NOTE: tests/examples/cli default to ON only when GameMemoryKit is the
# top-level project, so embedding it via add_subdirectory stays lightweight.
option(GMK_BUILD_TESTS "Build the test suite" ${PROJECT_IS_TOP_LEVEL})
option(GMK_BUILD_EXAMPLES "Build the example programs" ${PROJECT_IS_TOP_LEVEL})
option(GMK_BUILD_CLI "Build the gmk command-line tool" ${PROJECT_IS_TOP_LEVEL})
option(GMK_BUILD_BENCHMARKS "Build the benchmarks" OFF)
option(GMK_ENABLE_SANITIZERS "Enable Address/UndefinedBehavior sanitizers" OFF)

# ---------------------------------------------------------------------------
# Modules
# ---------------------------------------------------------------------------

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Sanitizers)

# ---------------------------------------------------------------------------
# Library
# ---------------------------------------------------------------------------

set(GMK_SOURCES
    src/binary/binary_image.cpp
    src/binary/elf.cpp
    src/binary/pe.cpp
    src/core/address.cpp
    src/core/byte_view.cpp
    src/core/error.cpp
    src/core/logging.cpp
    src/pattern/pattern.cpp
    src/pattern/scanner.cpp
)

add_library(gmk STATIC ${GMK_SOURCES})

add_library(gmk::gmk ALIAS gmk)

target_compile_features(gmk PUBLIC cxx_std_20)

target_include_directories(gmk PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(NOT MSVC)
    target_compile_options(gmk PRIVATE -fvisibility=hidden)
endif()

gmk_enable_warnings(gmk)
if(GMK_ENABLE_SANITIZERS)
    gmk_enable_sanitizers(gmk)
endif()

# Generated version header.
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/gmk/version.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/gmk/version.hpp"
    @ONLY
)

# ---------------------------------------------------------------------------
# Install / package
# ---------------------------------------------------------------------------

install(TARGETS gmk EXPORT GameMemoryKitTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/gmk
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp"
    PATTERN "version.hpp.in" EXCLUDE
)

install(EXPORT GameMemoryKitTargets
    FILE GameMemoryKitTargets.cmake
    NAMESPACE gmk::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GameMemoryKit
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GameMemoryKitConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/GameMemoryKitConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GameMemoryKit
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/GameMemoryKitConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
    ARCH_INDEPENDENT
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/GameMemoryKitConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/GameMemoryKitConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GameMemoryKit
)

# ---------------------------------------------------------------------------
# Subdirectories
# ---------------------------------------------------------------------------

if(GMK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(GMK_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(GMK_BUILD_CLI)
    add_subdirectory(tools/gmk)
endif()

if(GMK_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

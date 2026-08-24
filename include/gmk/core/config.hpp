// GameMemoryKit — platform and toolchain configuration macros.
//
// This header centralizes OS/architecture detection so that the rest of the
// library (and consumer code) can write portable code without scattering
// preprocessor checks through every file.
//
// SPDX-License-Identifier: MIT

#pragma once

// ---------------------------------------------------------------------------
// Operating system
// ---------------------------------------------------------------------------

// GMK_OS_* can be predefined on the command line to build platform-independent
// code with a toolchain whose standard macros do not describe a supported
// target (e.g. cross-compiling the core library for an embedded target). Do
// not override these when compiling platform code: the backend expects the
// real OS APIs of the defined platform.

#if !defined(GMK_OS_WINDOWS) && !defined(GMK_OS_LINUX) && !defined(GMK_OS_MACOS)
#  if defined(_WIN32) || defined(_WIN64)
#    define GMK_OS_WINDOWS 1
#  elif defined(__linux__)
#    define GMK_OS_LINUX 1
#  elif defined(__APPLE__)
#    define GMK_OS_MACOS 1
#  else
#    error "GameMemoryKit: unsupported operating system (predefine GMK_OS_WINDOWS/GMK_OS_LINUX/GMK_OS_MACOS to override)"
#  endif
#endif

// ---------------------------------------------------------------------------
// Architecture
// ---------------------------------------------------------------------------

#if defined(__x86_64__) || defined(_M_X64)
#  define GMK_ARCH_X86_64 1
#elif defined(__i386__) || defined(_M_IX86)
#  define GMK_ARCH_X86 1
#elif defined(__aarch64__) || defined(_M_ARM64)
#  define GMK_ARCH_ARM64 1
#elif defined(__arm__) || defined(_M_ARM)
#  define GMK_ARCH_ARM 1
#else
#  error "GameMemoryKit: unsupported CPU architecture"
#endif

// ---------------------------------------------------------------------------
// Compiler
// ---------------------------------------------------------------------------

#if defined(_MSC_VER)
#  define GMK_COMPILER_MSVC 1
#elif defined(__clang__)
#  define GMK_COMPILER_CLANG 1
#elif defined(__GNUC__)
#  define GMK_COMPILER_GCC 1
#endif

// ---------------------------------------------------------------------------
// Utility macros
// ---------------------------------------------------------------------------

// Marks an API as part of the stable public surface. Consumers can use this
// to detect ABI-affecting changes between releases.
#if defined(_WIN32) && defined(GMK_SHARED)
#  if defined(GMK_BUILDING_LIBRARY)
#    define GMK_API __declspec(dllexport)
#  else
#    define GMK_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) || defined(__clang__)
#  define GMK_API __attribute__((visibility("default")))
#else
#  define GMK_API
#endif

// Likely/unlikely branch hints (no-op when the compiler lacks support).
#if defined(__GNUC__) || defined(__clang__)
#  define GMK_LIKELY(x) __builtin_expect(!!(x), 1)
#  define GMK_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#  define GMK_LIKELY(x) (x)
#  define GMK_UNLIKELY(x) (x)
#endif

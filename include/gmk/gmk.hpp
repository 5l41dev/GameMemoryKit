// GameMemoryKit — umbrella header.
//
// Including this single header pulls in the entire public API. Individual
// subsystem headers can be included directly when only part of the library
// is needed.
//
// SPDX-License-Identifier: MIT

#pragma once

#include "gmk/version.hpp"

#include "gmk/core/address.hpp"
#include "gmk/core/byte_view.hpp"
#include "gmk/core/config.hpp"
#include "gmk/core/error.hpp"
#include "gmk/core/logging.hpp"
#include "gmk/core/result.hpp"

#include "gmk/memory/memory_protection.hpp"
#include "gmk/memory/memory_region.hpp"

#include "gmk/process/process.hpp"

#include "gmk/module/module.hpp"

#include "gmk/binary/binary_image.hpp"
#include "gmk/binary/elf.hpp"
#include "gmk/binary/pe.hpp"

#include "gmk/pattern/pattern.hpp"
#include "gmk/pattern/scanner.hpp"

#include "gmk/symbols/symbol.hpp"

#include "gmk/diagnostics/instrumentation.hpp"

// GameMemoryKit — structured logging.
//
// A small, dependency-free logging facility. The library itself logs sparingly
// (errors and important warnings only); the level and destination are
// configurable by the consumer. All functions are thread-safe.
//
// By default the level is Warning and output goes to stderr, so a program
// that links GameMemoryKit stays quiet unless something goes wrong.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <functional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>

#include "gmk/core/config.hpp"

namespace gmk::log {

/// Severity levels, ordered from most to least verbose.
enum class Level : int {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warning = 3,
    Error = 4,
    Critical = 5,
    Off = 6,
};

/// Returns the canonical name of a level, e.g. "warning".
[[nodiscard]] constexpr std::string_view level_name(Level level) noexcept
{
    using namespace std::string_view_literals;
    switch (level) {
        case Level::Trace: return "trace"sv;
        case Level::Debug: return "debug"sv;
        case Level::Info: return "info"sv;
        case Level::Warning: return "warning"sv;
        case Level::Error: return "error"sv;
        case Level::Critical: return "critical"sv;
        case Level::Off: return "off"sv;
    }
    return "unknown"sv;
}

/// Streams the canonical level name, e.g. "warning".
inline std::ostream& operator<<(std::ostream& out, Level level)
{
    return out << level_name(level);
}

/// Sets the minimum level that will be emitted. Default: Warning.
/// Passing Level::Off disables all output.
void set_level(Level level) noexcept;

/// Returns the current minimum level.
[[nodiscard]] Level level() noexcept;

/// Installs a custom sink. The sink is invoked (on the calling thread) for
/// every record at or above the current level. Passing nullptr restores the
/// default stderr sink.
void set_sink(std::function<void(Level, std::string_view)> sink);

/// The default sink writes "level: message\n" to stderr.
void default_sink(Level level, std::string_view message);

namespace detail {

/// Internal: emits a record. Returns true if the record was emitted (level
/// enabled and a sink exists).
GMK_API bool emit(Level level, const std::string& message);

template <typename... Args>
std::string join(Args&&... args)
{
    std::ostringstream out;
    (out << ... << std::forward<Args>(args));
    return out.str();
}

template <typename... Args>
inline void log_at(Level level, Args&&... args)
{
    if (level >= log::level()) {
        detail::emit(level, detail::join(std::forward<Args>(args)...));
    }
}

}  // namespace detail

template <typename... Args>
inline void trace(Args&&... args)
{
    detail::log_at(Level::Trace, std::forward<Args>(args)...);
}

template <typename... Args>
inline void debug(Args&&... args)
{
    detail::log_at(Level::Debug, std::forward<Args>(args)...);
}

template <typename... Args>
inline void info(Args&&... args)
{
    detail::log_at(Level::Info, std::forward<Args>(args)...);
}

template <typename... Args>
inline void warning(Args&&... args)
{
    detail::log_at(Level::Warning, std::forward<Args>(args)...);
}

template <typename... Args>
inline void error(Args&&... args)
{
    detail::log_at(Level::Error, std::forward<Args>(args)...);
}

template <typename... Args>
inline void critical(Args&&... args)
{
    detail::log_at(Level::Critical, std::forward<Args>(args)...);
}

/// RAII guard that temporarily changes the log level and restores it on
/// destruction. Not thread-safe by design (use only around single-threaded
/// sections); nesting guards composes correctly.
class ScopedLevel {
public:
    explicit ScopedLevel(Level level) noexcept : previous_(log::level())
    {
        log::set_level(level);
    }

    ~ScopedLevel() { log::set_level(previous_); }

    ScopedLevel(const ScopedLevel&) = delete;
    ScopedLevel& operator=(const ScopedLevel&) = delete;

private:
    Level previous_;
};

}  // namespace gmk::log

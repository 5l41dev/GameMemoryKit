#include "gmk/core/logging.hpp"

#include <cstdio>
#include <mutex>

namespace gmk::log {

namespace {

struct State {
    std::mutex mutex;
    Level level{Level::Warning};
    std::function<void(Level, std::string_view)> sink{&default_sink};
};

State& state()
{
    static State s;
    return s;
}

}  // namespace

void set_level(Level new_level) noexcept
{
    std::lock_guard lock(state().mutex);
    state().level = new_level;
}

Level level() noexcept
{
    std::lock_guard lock(state().mutex);
    return state().level;
}

void set_sink(std::function<void(Level, std::string_view)> sink)
{
    std::lock_guard lock(state().mutex);
    state().sink = sink ? std::move(sink) : std::function<void(Level, std::string_view)>{&default_sink};
}

void default_sink(Level level, std::string_view message)
{
    std::fprintf(stderr, "%s: %.*s\n", std::string(level_name(level)).c_str(),
                 static_cast<int>(message.size()), message.data());
}

namespace detail {

bool emit(Level level, const std::string& message)
{
    std::function<void(Level, std::string_view)> sink;
    {
        std::lock_guard lock(state().mutex);
        if (level < state().level || !state().sink) {
            return false;
        }
        sink = state().sink;
    }
    sink(level, message);
    return true;
}

}  // namespace detail

}  // namespace gmk::log

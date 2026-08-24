#include "gmk/core/logging.hpp"

#include "../support/minitest.hpp"

#include <string>
#include <vector>

using gmk::log::Level;

namespace {

struct Captured {
    std::vector<Level> levels;
    std::vector<std::string> messages;
};

void install_capture(Captured& capture)
{
    gmk::log::set_sink([&capture](Level level, std::string_view message) {
        capture.levels.push_back(level);
        capture.messages.emplace_back(message);
    });
}

}  // namespace

MT_TEST(logging, levels_are_filtered)
{
    Captured capture;
    install_capture(capture);
    gmk::log::set_level(Level::Info);

    gmk::log::trace("t");
    gmk::log::debug("d");
    gmk::log::info("i");
    gmk::log::warning("w");
    gmk::log::error("e");

    MT_CHECK_EQ(capture.messages.size(), std::size_t{3});
    MT_CHECK_EQ(capture.levels[0], Level::Info);
    MT_CHECK_EQ(capture.messages[0], std::string{"i"});
    MT_CHECK_EQ(capture.levels[2], Level::Error);

    gmk::log::set_level(Level::Warning);
}

MT_TEST(logging, off_suppresses_everything)
{
    Captured capture;
    install_capture(capture);
    gmk::log::set_level(Level::Off);

    gmk::log::critical("hidden");

    MT_CHECK(capture.messages.empty());
    gmk::log::set_level(Level::Warning);
}

MT_TEST(logging, variadic_composition)
{
    Captured capture;
    install_capture(capture);
    gmk::log::set_level(Level::Debug);

    int code = 42;
    gmk::log::debug("code=", code, " name=", std::string{"x"});

    MT_CHECK_EQ(capture.messages.size(), std::size_t{1});
    MT_CHECK_EQ(capture.messages[0], std::string{"code=42 name=x"});

    gmk::log::set_level(Level::Warning);
}

MT_TEST(logging, scoped_level_restores)
{
    gmk::log::set_level(Level::Error);
    {
        gmk::log::ScopedLevel guard{Level::Debug};
        MT_CHECK_EQ(gmk::log::level(), Level::Debug);
    }
    MT_CHECK_EQ(gmk::log::level(), Level::Error);
}

MT_TEST(logging, level_names)
{
    MT_CHECK_EQ(std::string{gmk::log::level_name(Level::Trace)}, std::string{"trace"});
    MT_CHECK_EQ(std::string{gmk::log::level_name(Level::Critical)}, std::string{"critical"});
    MT_CHECK_EQ(std::string{gmk::log::level_name(Level::Off)}, std::string{"off"});
}

MT_TEST(logging, default_sink_restored)
{
    // Reinstalling nullptr restores the default stderr sink; emitting to it
    // must not crash. The output itself goes to stderr and is not asserted.
    gmk::log::set_sink(nullptr);
    gmk::log::set_level(Level::Error);
    gmk::log::error("this goes to stderr");
    gmk::log::set_level(Level::Warning);
}

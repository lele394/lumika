#include "e3d/core/log.hpp"

#include <cstdio>
#include <cstdlib>

namespace {

e3d::LogSink g_sink = nullptr;

constexpr const char* level_name(e3d::LogLevel level) noexcept {
    switch (level) {
        case e3d::LogLevel::Debug: return "debug";
        case e3d::LogLevel::Info: return "info";
        case e3d::LogLevel::Warn: return "warn";
        case e3d::LogLevel::Error: return "error";
    }
    return "?";
}

} // namespace

namespace e3d {

void set_log_sink(LogSink sink) noexcept { g_sink = sink; }

void log_v(LogLevel level, const char* fmt, std::va_list args) {
    char buffer[1024];
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);

    if (g_sink != nullptr) {
        g_sink(level, buffer);
        return;
    }

#if !defined(__3DS__)
    std::fprintf(stderr, "[%s] %s\n", level_name(level), buffer);
#endif
}

void log(LogLevel level, const char* fmt, ...) {
    std::va_list args;
    va_start(args, fmt);
    log_v(level, fmt, args);
    va_end(args);
}

void assert_failed(const char* expression, const char* file, int line) {
    log(LogLevel::Error, "assertion failed: %s (%s:%d)", expression, file, line);
    std::abort();
}

} // namespace e3d

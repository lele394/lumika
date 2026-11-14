#pragma once

#include "e3d/core/types.hpp"

#include <cstdarg>

namespace e3d {

enum class LogLevel : u8 { Debug = 0, Info, Warn, Error };

/// Receives fully formatted messages. Set by the platform layer at startup.
using LogSink = void (*)(LogLevel, const char*);

void set_log_sink(LogSink sink) noexcept;
void log(LogLevel level, const char* fmt, ...);
void log_v(LogLevel level, const char* fmt, std::va_list args);

[[noreturn]] void assert_failed(const char* expression, const char* file, int line);

} // namespace e3d

#define E3D_LOGD(...) ::e3d::log(::e3d::LogLevel::Debug, __VA_ARGS__)
#define E3D_LOGI(...) ::e3d::log(::e3d::LogLevel::Info, __VA_ARGS__)
#define E3D_LOGW(...) ::e3d::log(::e3d::LogLevel::Warn, __VA_ARGS__)
#define E3D_LOGE(...) ::e3d::log(::e3d::LogLevel::Error, __VA_ARGS__)

#if defined(NDEBUG)
#define E3D_ASSERT(expr) ((void)0)
#else
#define E3D_ASSERT(expr)                                                       \
    do {                                                                       \
        if (!(expr)) ::e3d::assert_failed(#expr, __FILE__, __LINE__);          \
    } while (0)
#endif

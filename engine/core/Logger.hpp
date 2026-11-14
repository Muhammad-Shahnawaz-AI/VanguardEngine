#pragma once
#include <cstdio>

namespace vg {

enum class LogLevel { Info, Warn, Error };

inline void log_msg(LogLevel level, const char* tag, const char* msg) noexcept {
    const char* prefix = "[INFO]";
    if (level == LogLevel::Warn)  prefix = "[WARN]";
    if (level == LogLevel::Error) prefix = "[ERR ]";
    std::fprintf(stderr, "%s [%s] %s\n", prefix, tag, msg);
}

} // namespace vg

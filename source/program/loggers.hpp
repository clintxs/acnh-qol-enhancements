#pragma once

#include <lib/log/svc_logger.hpp>
#include "core/logging_policy.hpp"

namespace qol::logging {
class Logger : public exl::log::LoggerMgr<exl::log::SvcLogger> {
    Preferences preferences{};
    template<class... Args>
    void Write(Category category, const char *format, Args &&...args) {
        if (Enabled(preferences, category)) Log(format, std::forward<Args>(args)...);
    }
public:
    void Configure(Preferences value) { preferences = value; }
    bool DebugEnabled() const { return preferences.debug; }
    template<class... Args> void Version(const char *format, Args &&...args) {
        Write(Category::Version, format, std::forward<Args>(args)...);
    }
    template<class... Args> void Settings(const char *format, Args &&...args) {
        Write(Category::Settings, format, std::forward<Args>(args)...);
    }
    template<class... Args> void Debug(const char *format, Args &&...args) {
        Write(Category::Debug, format, std::forward<Args>(args)...);
    }
    template<class... Args> void Warning(const char *format, Args &&...args) {
        Write(Category::Warning, format, std::forward<Args>(args)...);
    }
    template<class... Args> void Error(const char *format, Args &&...args) {
        Write(Category::Error, format, std::forward<Args>(args)...);
    }
};
} // namespace qol::logging

inline qol::logging::Logger Logging;

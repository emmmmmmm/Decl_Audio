#pragma once
#include "declsound_export.hpp"
#include <string>

enum class LogCategory {
    SoundManager,
    BehaviorLoader,
    CLI,
    General, 
    AudioCore
};

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warning,
    Error
};

DECLSOUND_API void LogSetMinimumLevel(LogCategory category, LogLevel level);
extern "C" DECLSOUND_API void LogMessageC(const char* message, int category, int level);

inline void LogMessage(const std::string& message, LogCategory category, LogLevel level) {
    LogMessageC(message.c_str(), static_cast<int>(category), static_cast<int>(level));
}

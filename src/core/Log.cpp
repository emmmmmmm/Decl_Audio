#include "pch.h"
#include "Log.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <deque>
#include <iostream>
#include <mutex>
#include <string>
#include <tuple>
#include <unordered_map>

namespace {

std::atomic<LogCallbackFn> gLogCallback{ nullptr };
std::mutex gMutex;
std::deque<std::tuple<std::string, int, int>> gBuffer;
std::unordered_map<LogCategory, LogLevel> gMinimumLevels = {
	{ LogCategory::BehaviorLoader, LogLevel::Debug },
	{ LogCategory::CLI, LogLevel::Debug },
	{ LogCategory::General, LogLevel::Debug },
	{ LogCategory::AudioCore, LogLevel::Debug },
	{ LogCategory::AudioDevice, LogLevel::Debug },
	{ LogCategory::AudioBuffer, LogLevel::Debug },
	{ LogCategory::AudioManager, LogLevel::Debug },
	{ LogCategory::Entity, LogLevel::Debug },
	{ LogCategory::Leaf, LogLevel::Debug },
	{ LogCategory::Parser, LogLevel::Debug },
};

const char* ToString(LogLevel level)
{
	switch (level) {
	case LogLevel::Trace: return "Trace";
	case LogLevel::Debug: return "Debug";
	case LogLevel::Info: return "Info";
	case LogLevel::Warning: return "Warning";
	case LogLevel::Error: return "Error";
	default: return "Unknown";
	}
}

const char* ToString(LogCategory category)
{
	switch (category) {
	case LogCategory::BehaviorLoader: return "BehaviorLoader";
	case LogCategory::CLI: return "CLI";
	case LogCategory::General: return "General";
	case LogCategory::AudioCore: return "AudioCore";
	case LogCategory::AudioDevice: return "AudioDevice";
	case LogCategory::AudioBuffer: return "AudioBuffer";
	case LogCategory::AudioManager: return "AudioManager";
	case LogCategory::Entity: return "Entity";
	case LogCategory::Leaf: return "Leaf";
	case LogCategory::Parser: return "Parser";
	default: return "Unknown";
	}
}

} // namespace

extern "C" DECLSOUND_API void AudioManager_LogSetMinimumLevel(LogCategory category, LogLevel level)
{
	gMinimumLevels[category] = level;
}

void LogMessageC(const char* message, int category, int level)
{
	LogCategory cat = static_cast<LogCategory>(category);
	LogLevel lvl = static_cast<LogLevel>(level);
	if (lvl >= gMinimumLevels[cat]) {
		std::cout << "[Log-" << ToString(cat) << "] " << message << std::endl;
	}

	if (const auto callback = gLogCallback.load(std::memory_order_acquire); callback != nullptr) {
		callback(category, level, message);
	}

	std::lock_guard<std::mutex> lock(gMutex);
	gBuffer.emplace_back(message, static_cast<int>(cat), static_cast<int>(lvl));
}

extern "C" DECLSOUND_API void AudioManager_SetLogCallback(LogCallbackFn cb)
{
	gLogCallback.store(cb, std::memory_order_release);
}

extern "C" DECLSOUND_API
bool AudioManager_PollLog(int* outCat, int* outLvl, char* outMsg, int maxLen)
{
	if (outCat == nullptr || outLvl == nullptr || outMsg == nullptr || maxLen <= 0) {
		return false;
	}

	std::lock_guard<std::mutex> lock(gMutex);
	if (gBuffer.empty()) {
		return false;
	}

	auto entry = gBuffer.front();
	const std::string& s = std::get<0>(entry);

	*outCat = std::get<1>(entry);
	*outLvl = std::get<2>(entry);

	int copyLen = (std::min)(static_cast<int>(s.size()), maxLen - 1);
	std::memcpy(outMsg, s.c_str(), copyLen);
	outMsg[copyLen] = '\0';

	gBuffer.pop_front();
	return true;
}


void LogFunctionCall(const std::source_location& loc)
{
	std::string_view fn = loc.function_name();
	if (auto par = fn.find('('); par != fn.npos) {
		if (auto sp = fn.rfind(' ', par); sp != fn.npos) {
			fn.remove_prefix(sp + 1);
		}
	}

	std::cout << fn << " (line:" << loc.line() << ')' << std::endl;
}

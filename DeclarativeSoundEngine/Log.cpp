﻿#include "pch.h"
#include "Log.hpp"
#include <iostream>
#include <unordered_map>
#include <atomic>

#include <format>
#include <chrono>
#include <string>
#include <cstring>

using namespace std::chrono_literals;


static std::atomic<LogCallbackFn> g_logCallback{ nullptr };
static std::mutex    g_mtx;
static std::vector<std::tuple<std::string, int, int>> g_buffer;


// Default Per-Category-LogLevels
static std::unordered_map<LogCategory, LogLevel> minimumLevels = {
	{ LogCategory::BehaviorLoader, LogLevel::Debug },
	{ LogCategory::CLI, LogLevel::Debug },
	{ LogCategory::General, LogLevel::Debug },
	{ LogCategory::AudioCore, LogLevel::Debug },
	{ LogCategory::AudioDevice, LogLevel::Debug},
	{ LogCategory::AudioBuffer, LogLevel::Debug},
	{ LogCategory::Entity, LogLevel::Debug},
	{ LogCategory::Leaf, LogLevel::Debug},
	{ LogCategory::Parser, LogLevel::Debug},
};

static const char* ToString(LogLevel level) {
	switch (level) {
	case LogLevel::Trace: return "Trace";
	case LogLevel::Debug: return "Debug";
	case LogLevel::Info: return "Info";
	case LogLevel::Warning: return "Warning";
	case LogLevel::Error: return "Error";
	default: return "Unknown";
	}
}

static const char* ToString(LogCategory category) {
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


extern "C" DECLSOUND_API void AudioManager_LogSetMinimumLevel(LogCategory category, LogLevel level) {
	minimumLevels[category] = level;
}

void LogMessageC(const char* message, int category, int level) {
	LogCategory cat = static_cast<LogCategory>(category);
	LogLevel lvl = static_cast<LogLevel>(level);
	if (lvl >= minimumLevels[cat]) {
		
		auto now = std::chrono::system_clock::now();
		//std::cout << std::format("{:%H %M %S}", now) << " [Log-" << ToString(cat) << "] " << message << std::endl;
		std::cout << "[Log-" << ToString(cat) << "] " << message << std::endl;
	}

	std::lock_guard<std::mutex> lk(g_mtx);
	g_buffer.emplace_back(message, (int)cat, (int)lvl);
}

// i don't think we're actally using this?
extern "C" DECLSOUND_API void AudioManager_SetLogCallback(LogCallbackFn cb) {
	g_logCallback.store(cb, std::memory_order_release);
}


extern "C" DECLSOUND_API
bool AudioManager_PollLog(int* outCat, int* outLvl, char* outMsg, int maxLen)
{
	std::lock_guard<std::mutex> lk(g_mtx);
	if (g_buffer.empty()) 
		return false;

	auto& entry = g_buffer.front();
	const std::string& s = std::get<0>(entry);

	*outCat = std::get<1>(entry);
	*outLvl = std::get<2>(entry);

	// copy up to maxLen-1, then null-terminate
	int copyLen = (std::min)(static_cast<int>(s.size()), maxLen - 1);
	memcpy(outMsg, s.c_str(), copyLen);
	outMsg[copyLen] = '\0';

	g_buffer.erase(g_buffer.begin());
	return true;
}


void LogFunctionCall(const std::source_location& loc)
{
	/*
	How it works:
		loc.function_name() returns
		"void __cdecl SoundManager::Update(void)".
		fn.find('(') locates the parameter‐list start.
		fn.rfind(' ', par) finds the space just before your class-scope name.
		fn.remove_prefix(sp+1) chops off "void __cdecl " (or any return type + calling convention).
	*/

	std::string_view fn = loc.function_name();
	if (auto par = fn.find('('); par != fn.npos) {
		if (auto sp = fn.rfind(' ', par); sp != fn.npos)
			fn.remove_prefix(sp + 1);
	}

	std::cout
		<< fn //loc.function_name()
		<< " ("
		// << loc.file_name()
		<< "line:" << loc.line()
		<< ")"
		<< std::endl;
}



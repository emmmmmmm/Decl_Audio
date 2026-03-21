#pragma once
#include <string>
#include <unordered_map>
#include "AudioBuffer.hpp"
#include <future>
#include <optional>
#include <list>


class AudioBufferManager {
public:
	AudioBufferManager(uint32_t deviceSampleRate) : deviceSampleRate(deviceSampleRate) {};
	~AudioBufferManager();
	bool TryGet( const std::string& path, AudioBuffer*& outBuf);
	bool TryLoad(const std::string& path, AudioBuffer*& outBuf);
	void         Unload(const std::string& path);
	void         PurgeUnused();
	size_t       GetMemoryUsage();
	void SetAssetpath(const std::string& path);
private:
	std::mutex mutex;
	std::unordered_map<std::string, std::pair<AudioBuffer, int /*refcount*/>> cache;
	std::list<std::string> lruList;
	size_t currentMemoryUsage{};
	std::string assetPath{};
	uint32_t deviceSampleRate{};
};

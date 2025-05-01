#pragma once
#include <string>
#include <unordered_map>
#include "AudioBuffer.hpp"
#include <future>
#include <optional>


class AudioBufferManager {
public:
	AudioBufferManager();
	~AudioBufferManager();
	bool TryLoad(const std::string& path, AudioBuffer*& outBuf);
	//std::future<AudioBuffer*> LoadAsync(const std::string& path);
	void         Unload(const std::string& path);
	void         PurgeUnused();
	size_t       GetMemoryUsage();
	void SetAssetpath(std::string& path);
private:
	std::mutex mutex;
	std::unordered_map<std::string, std::pair<AudioBuffer, int /*refcount*/>> cache;
	std::list<std::string> lruList;
	size_t currentMemoryUsage{};
	std::string assetPath{};
};

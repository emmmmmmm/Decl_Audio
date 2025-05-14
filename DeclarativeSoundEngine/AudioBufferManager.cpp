#include "pch.h"
#include "AudioBufferManager.hpp"
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include "Log.hpp"
#include <filesystem>

AudioBufferManager::AudioBufferManager()
{
	// Constructor, nothing to init beyond default members
}

AudioBufferManager::~AudioBufferManager()
{
	LogMessage("~AudioBufferManager", LogCategory::AudioBuffer, LogLevel::Debug);

	// Cleanup all buffers
	std::lock_guard<std::mutex> lock(mutex);
	cache.clear();
	lruList.clear();
}
bool AudioBufferManager::TryGet(const std::string& path, AudioBuffer*& outBuf) {
//	std::lock_guard<std::mutex> lock(mutex);
	// already cached?

	std::cerr << "[AudioBufferManager::TryGet] this=" << this
		<< "  cache@=" << &cache
		<< "  cache.size()=" << cache.size() << "\n";


	auto it = cache.find(path);
	if (it != cache.end()) {
		// Move this path to front of LRU
		lruList.remove(path);
		lruList.push_front(path);
		// Increment ref count
		it->second.second++;
		outBuf = &it->second.first;
		return true;
	}
	return false;
}
bool AudioBufferManager::TryLoad(const std::string& path, AudioBuffer*& outBuf)
{

	std::lock_guard<std::mutex> lock(mutex);
	// already cached?
	if (TryGet(path, outBuf)) {
		LogMessage("AudioBufferManager: file aready loaded: " + path,
			LogCategory::AudioBuffer, LogLevel::Debug);
		return true;
	}

	


	//if (it != cache.end()) {
	//	// Move this path to front of LRU
	//	lruList.remove(path);
	//	lruList.push_front(path);
	//	// Increment ref count
	//	it->second.second++;
	//	outBuf = &it->second.first;

	//	LogMessage("AudioBufferManager: file aready loaded: " + path,
	//		LogCategory::AudioBuffer, LogLevel::Debug);

	//	return true;
	//}


	// Load new buffer
	namespace fs = std::filesystem;
	fs::path assetDir{ assetPath };
	fs::path fileName{ path };

	// join them:
	fs::path fullPath = assetDir / fileName;

	std::string fullStr = fullPath.string(); 
	AudioBuffer buffer(fullStr);

	

	if (buffer.Empty()) {
		LogMessage("AudioBufferManager: failed to load " + path,
			LogCategory::AudioBuffer, LogLevel::Warning);
		outBuf = nullptr;
		return false;
	

	}

	size_t mem = buffer.GetFrameCount()
		* buffer.GetChannelCount()
		* sizeof(float);


	// emplace a pair<AudioBuffer,int>
	auto emplaced = cache.emplace(
		path,
		std::make_pair(std::move(buffer), 1)
	);

	// emplaced.first is iterator into the new element
	auto it = emplaced.first;

	// LRU and memory tracking
	lruList.push_front(path);
	currentMemoryUsage += mem;
	outBuf = & it->second.first;

	LogMessage("AudioBufferManager: finished loading " + path,
		LogCategory::AudioBuffer, LogLevel::Warning);
	return true;
}

void AudioBufferManager::PurgeUnused()
{
	std::lock_guard<std::mutex> lock(mutex);
	for (auto it = lruList.rbegin(); it != lruList.rend();) {
		const std::string& path = *it;
		auto cacheIt = cache.find(path);
		if (cacheIt != cache.end() && cacheIt->second.second == 0) {
			// Evict
			size_t memoryUsed = cacheIt->second.first.GetFrameCount() * cacheIt->second.first.GetChannelCount() * sizeof(float);
			currentMemoryUsage -= memoryUsed;
			cache.erase(cacheIt);
			// Remove from LRU (convert reverse_iterator to normal)
			it = std::make_reverse_iterator(lruList.erase(std::next(it).base()));
		}
		else {
			++it;
		}
	}
}

size_t AudioBufferManager::GetMemoryUsage()
{
	std::lock_guard<std::mutex> lock(mutex);
	return currentMemoryUsage;
}

void AudioBufferManager::SetAssetpath(const std::string& path)
{
	assetPath = path;
	LogMessage("updated path to assets: " + assetPath, LogCategory::AudioBuffer, LogLevel::Debug);
}

#include "pch.h"
#include "AudioBufferManager.hpp"
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include "Log.hpp"

AudioBufferManager::AudioBufferManager()
{
	// Constructor, nothing to init beyond default members
}

AudioBufferManager::~AudioBufferManager()
{
	// Cleanup all buffers
	std::lock_guard<std::mutex> lock(mutex);
	cache.clear();
	lruList.clear();
}

bool AudioBufferManager::TryLoad(const std::string& path, AudioBuffer*& outBuf)
{
	std::lock_guard<std::mutex> lock(mutex);
	// already cached?
	auto it = cache.find(path);
	if (it != cache.end()) {
		// Move this path to front of LRU
		lruList.remove(path);
		lruList.push_front(path);
		// Increment ref count
		it->second.second++;
		outBuf = &it->second.first;

		LogMessage("AudioBufferManager: file aready loaded: " + path,
			LogCategory::AudioCore, LogLevel::Debug);

		return true;
	}


	// Load new buffer
	AudioBuffer buffer(path);

	

	if (buffer.Empty()) {
		LogMessage("AudioBufferManager: failed to load " + path,
			LogCategory::AudioCore, LogLevel::Warning);
		outBuf = &buffer;
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
	it = emplaced.first;

	// LRU and memory tracking
	lruList.push_front(path);
	currentMemoryUsage += mem;
	outBuf = & it->second.first;
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

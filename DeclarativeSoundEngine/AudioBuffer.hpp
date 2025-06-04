#pragma once
#include <string>
#include <vector>

class AudioBuffer {
public:
	AudioBuffer(const std::string& filePath, uint32_t targetSampleRate = 0);
	uint32_t    GetSampleRate() const;
	uint16_t    GetChannelCount() const;
	uint64_t    GetFrameCount() const;
	const float*      GetData() const;
	size_t GetSampleCount() const;
	void        ReadSamples(float* dest, uint64_t offset, uint64_t frames) const;
	const bool Empty() const;
	const static AudioBuffer* Get(const std::string& filePath, uint32_t targetSampleRate = 0);



	std::vector<float> samples;  // interleaved
	uint32_t sampleRate;
	uint16_t channelCount;
	uint64_t frameCount;
private:

};
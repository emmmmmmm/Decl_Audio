#pragma once
#include <string>
#include <vector>

class AudioBuffer {
public:
	AudioBuffer(const std::string& filePath);
	uint32_t    GetSampleRate() const;
	uint16_t    GetChannelCount() const;
	uint64_t    GetFrameCount() const;
	const float*      GetData() const;
	void        ReadSamples(float* dest, uint64_t offset, uint64_t frames) const;
	bool Empty();
private:
	std::vector<float> samples;  // interleaved
	uint32_t sampleRate;
	uint16_t channelCount;
	uint64_t frameCount;
};
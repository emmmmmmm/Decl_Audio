#include "pch.h"
#include "AudioBuffer.hpp"
#include <stdexcept>
#include <filesystem>
#include <cstring>
#include <algorithm>

// We're using miniaudio for decoding.
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <iostream>
#include "Log.hpp"

AudioBuffer::AudioBuffer(const std::string& filePath, uint32_t targetSampleRate)
    : sampleRate(0), channelCount(0), frameCount(0)
{
    using namespace std::filesystem;
    if (!exists(filePath)) {
        LogMessage("AudioBuffer: file not found: " + filePath,
            LogCategory::AudioCore, LogLevel::Error);
        return;
    }

    ma_decoder_config cfg = ma_decoder_config_init(
        ma_format_f32,  // floats
        0,              // native channels
        0               // native sample rate
    );
    ma_decoder decoder;
    ma_result r = ma_decoder_init_file(filePath.c_str(), &cfg, &decoder);
    if (r != MA_SUCCESS) {
        LogMessage("ma_decoder_init_file failed: " +
            std::string(ma_result_description(r)),
            LogCategory::AudioCore, LogLevel::Error);
        return;
    }

    ma_uint64 totalPCMFrameCount = 0;
    r = ma_decoder_get_length_in_pcm_frames(&decoder, &totalPCMFrameCount);
    if (r != MA_SUCCESS) {
        ma_decoder_uninit(&decoder);
        throw std::runtime_error(
            "ma_decoder_get_length_in_pcm_frames failed for: " + filePath);
    }

    frameCount = static_cast<uint64_t>(totalPCMFrameCount);
    sampleRate = decoder.outputSampleRate;
    channelCount = decoder.outputChannels;

    if (frameCount == 0 || channelCount == 0) {
        ma_decoder_uninit(&decoder);
        throw std::runtime_error("Empty or invalid audio file: " + filePath);
    }

    samples.resize(frameCount * channelCount);

    ma_uint64 totalRead = 0;
    while (totalRead < frameCount) {
        ma_uint64 thisRead = 0;
        r = ma_decoder_read_pcm_frames(
            &decoder,
            samples.data() + totalRead * channelCount,
            frameCount - totalRead,
            &thisRead
        );
        if (r != MA_SUCCESS) {
            ma_decoder_uninit(&decoder);
            throw std::runtime_error(
                "Decoding error " + std::to_string(r) + " for " + filePath
            );
        }
        if (thisRead == 0) {
            break;  // EOF
        }
        totalRead += thisRead;
    }


    if (totalRead != frameCount) {
        samples.resize(totalRead * channelCount);
        frameCount = totalRead;
    }

    ma_decoder_uninit(&decoder);

    if (targetSampleRate != 0 && targetSampleRate != sampleRate) {
        LogMessage(
            "Sample rate mismatch for " + filePath + ": " +
            std::to_string(sampleRate) + " -> " +
            std::to_string(targetSampleRate),
            LogCategory::AudioBuffer, LogLevel::Warning);

        uint64_t newFrameCount = static_cast<uint64_t>(
            (static_cast<double>(frameCount) * targetSampleRate) / sampleRate);
        std::vector<float> resampled(newFrameCount * channelCount);
        for (uint64_t i = 0; i < newFrameCount; ++i) {
            double srcPos = (static_cast<double>(i) * frameCount) / newFrameCount;
            uint64_t pos0 = static_cast<uint64_t>(srcPos);
            double frac = srcPos - pos0;
            uint64_t pos1 = (std::min)(pos0 + 1, frameCount - 1);
            for (uint16_t c = 0; c < channelCount; ++c) {
                float s0 = samples[pos0 * channelCount + c];
                float s1 = samples[pos1 * channelCount + c];
                resampled[i * channelCount + c] = s0 + static_cast<float>(frac) * (s1 - s0);
            }
        }
        samples.swap(resampled);
        frameCount = newFrameCount;
        sampleRate = targetSampleRate;
    }

    LogMessage("Loaded " + filePath + ": frames=" +
        std::to_string(frameCount) + " ch=" +
        std::to_string(channelCount) + " SR=" +
        std::to_string(sampleRate),
        LogCategory::AudioCore, LogLevel::Info);


}


uint32_t AudioBuffer::GetSampleRate() const {
	return sampleRate;
}

uint16_t AudioBuffer::GetChannelCount() const {
	return channelCount;
}

uint64_t AudioBuffer::GetFrameCount() const {
	return frameCount;
}

const float* AudioBuffer::GetData() const {
	return samples.data();
}
size_t AudioBuffer::GetSampleCount() const { return samples.size(); }

void AudioBuffer::ReadSamples(float* dest, uint64_t offsetFrame, uint64_t frameCountToRead) const {
	if (offsetFrame + frameCountToRead > frameCount) {
		throw std::out_of_range("ReadSamples range exceeds buffer length");
	}
	uint64_t startSample = offsetFrame * channelCount;
	uint64_t sampleCount = frameCountToRead * channelCount;
	std::memcpy(dest, samples.data() + startSample, sampleCount * sizeof(float));
}

const bool AudioBuffer::Empty()  const
{
	return frameCount == 0;
}

const AudioBuffer* AudioBuffer::Get(const std::string& filePath, uint32_t targetSampleRate)
{
    return  new AudioBuffer(filePath, targetSampleRate);
}

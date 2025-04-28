#include "pch.h"
#include "AudioBuffer.hpp"
#include <stdexcept>

// We're using miniaudio for decoding. Make sure to include miniaudio.h and link against it.
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <iostream>

AudioBuffer::AudioBuffer(const std::string& filePath)
    : sampleRate(0), channelCount(0), frameCount(0)
{
    ma_result result;
    ma_decoder decoder;
    
    // Initialize the decoder for the given file path
    result = ma_decoder_init_file(filePath.c_str(), NULL, &decoder);
    if ( result != MA_SUCCESS) {
        //std::cout << "buffer load failed for: " << filePath << " with result: " << result << std::endl;
        //throw std::runtime_error("Failed to load audio file: " + filePath);
        return;
    }
    
    sampleRate = decoder.outputSampleRate;
    channelCount = decoder.outputChannels;

    // Determine total frame count
    ma_uint64 totalPCMFrameCount = 0;
    result = ma_decoder_get_length_in_pcm_frames(&decoder, &totalPCMFrameCount);
    if (result != MA_SUCCESS) {
        ma_decoder_uninit(&decoder);
        throw std::runtime_error("Failed to get frame count for file: " + filePath);
    }
   
    frameCount = static_cast<uint64_t>(totalPCMFrameCount);
    samples.resize(frameCount * channelCount);

    // Read all samples into the buffer
    ma_uint64 framesRead;
    result = ma_decoder_read_pcm_frames(&decoder, samples.data(), totalPCMFrameCount, &framesRead);
    if (result != MA_SUCCESS) {
        ma_decoder_uninit(&decoder);
        throw std::runtime_error("Failed to read samples from file: " + filePath);
    }
    
    // Clean up decoder
    ma_decoder_uninit(&decoder);
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

void AudioBuffer::ReadSamples(float* dest, uint64_t offsetFrame, uint64_t frameCountToRead) const {
    if (offsetFrame + frameCountToRead > frameCount) {
        throw std::out_of_range("ReadSamples range exceeds buffer length");
    }
    uint64_t startSample = offsetFrame * channelCount;
    uint64_t sampleCount = frameCountToRead * channelCount;
    std::memcpy(dest, samples.data() + startSample, sampleCount * sizeof(float));
}

bool AudioBuffer::Empty()
{
    return frameCount==0;
}

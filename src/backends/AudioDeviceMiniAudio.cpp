// AudioDeviceMiniaudio.cpp

#include "pch.h"
#include "AudioDeviceMiniAudio.hpp"
#include "Log.hpp"

AudioDeviceMiniaudio::AudioDeviceMiniaudio(int channels, int sampleRate, int bufferFrames) {
    ma_result r = ma_context_init(nullptr, 0, nullptr, &context_);
    if (r != MA_SUCCESS) {
        LogMessage("ma_context_init failed: " + std::string(ma_result_description(r)), LogCategory::AudioDevice, LogLevel::Error);
        processingEnabled_ = false;
        return;
    }
    contextInitialized_ = true;

    ma_device_config dc = ma_device_config_init(ma_device_type_playback);
    dc.playback.format = ma_format_f32;
    dc.playback.channels = channels;
    dc.sampleRate = sampleRate;
    dc.periodSizeInFrames = bufferFrames;           // e.g. 2048
    dc.periods = 3;
    dc.dataCallback = AudioDeviceMiniaudio::dataCallback;
    dc.pUserData = this;

    r = ma_device_init(&context_, &dc, &device_);
    bool usedDefaultPeriodFallback = false;
    if (r != MA_SUCCESS) {
        LogMessage(
            "ma_device_init failed with requested period " + std::to_string(bufferFrames) +
            ": " + std::string(ma_result_description(r)),
            LogCategory::AudioDevice,
            LogLevel::Warning);

        ma_device_config fallback = ma_device_config_init(ma_device_type_playback);
        fallback.playback.format = ma_format_f32;
        fallback.playback.channels = channels;
        fallback.sampleRate = sampleRate;
        fallback.dataCallback = AudioDeviceMiniaudio::dataCallback;
        fallback.pUserData = this;

        r = ma_device_init(&context_, &fallback, &device_);
        if (r != MA_SUCCESS) {
            LogMessage("ma_device_init failed: " + std::string(ma_result_description(r)), LogCategory::AudioDevice, LogLevel::Error);
            processingEnabled_ = false;
            return;
        }

        usedDefaultPeriodFallback = true;
    }

    bufferFrames_ = device_.playback.internalPeriodSizeInFrames;
    if (usedDefaultPeriodFallback) {
        LogMessage(
            "[Miniaudio] Falling back to backend-selected period " + std::to_string(bufferFrames_),
            LogCategory::AudioDevice,
            LogLevel::Warning);
    } else if (bufferFrames_ != dc.periodSizeInFrames) {
        LogMessage("[Miniaudio] Requested period " + std::to_string(dc.periodSizeInFrames) + " got " + std::to_string(bufferFrames_), LogCategory::AudioDevice, LogLevel::Info);
    }
    else {
        LogMessage("[Miniaudio] initialized with buffer: " + std::to_string(dc.periodSizeInFrames), LogCategory::AudioDevice, LogLevel::Debug);
    }

    deviceInitialized_ = true;
}

AudioDeviceMiniaudio::~AudioDeviceMiniaudio() {
    if (deviceInitialized_) {
        ma_device_uninit(&device_);
    }
    if (contextInitialized_) {
        ma_context_uninit(&context_);
    }
}

SoundHandle AudioDeviceMiniaudio::Play(AudioBuffer* /*buf*/, float /*volume*/, float /*pitch*/, bool /*loop*/) {
	// Playback is mixed by the engine render callback instead of the backend.
	return 0;
}

void AudioDeviceMiniaudio::Stop(SoundHandle) { /* no-op for manual mix */ }
void AudioDeviceMiniaudio::SetVolume(SoundHandle, float) { /* track in Voice */ }
void AudioDeviceMiniaudio::SetPitch(SoundHandle, float) { /* track in Voice */ }

void AudioDeviceMiniaudio::SetRenderCallback(std::function<void(float*, int)> cb) {
    bool shouldStart = false;
    {
        std::lock_guard<std::mutex> lk(cbMutex_);
        renderCb_ = std::move(cb);
        shouldStart = deviceInitialized_ && !deviceStarted_ && static_cast<bool>(renderCb_);
        if (shouldStart) {
            deviceStarted_ = true;
        }
    }

    if (!shouldStart) {
        return;
    }

    ma_result r = ma_device_start(&device_);
    if (r != MA_SUCCESS) {
        LogMessage("ma_device_start failed: " + std::string(ma_result_description(r)), LogCategory::AudioDevice, LogLevel::Error);
        processingEnabled_ = false;
        std::lock_guard<std::mutex> lk(cbMutex_);
        deviceStarted_ = false;
        return;
    }
}


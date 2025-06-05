// AudioDeviceMiniaudio.cpp

#include "pch.h"
#include "AudioDeviceMiniAudio.hpp"
#include "Log.hpp"

AudioDeviceMiniaudio::AudioDeviceMiniaudio(int channels, int sampleRate, int bufferFrames) {
    ma_engine_config cfg = ma_engine_config_init();
    cfg.listenerCount = 1;          // stereo
    cfg.sampleRate = sampleRate;
    cfg.channels = channels;
    // setup the data callback:
    cfg.dataCallback = nullptr;     // well open a raw device instead of engine_playSource

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
    dc.periodSizeInFrames = 0; //bufferFrames;           // e.g. 2048
    dc.periods = 3;


    dc.dataCallback = AudioDeviceMiniaudio::dataCallback;
    dc.pUserData = this;

    r = ma_device_init(&context_, &dc, &device_);
    if (r != MA_SUCCESS) {
        LogMessage("ma_device_init failed: " + std::string(ma_result_description(r)), LogCategory::AudioDevice, LogLevel::Error);
        processingEnabled_ = false;
        return;
    }

    bufferFrames_ = device_.playback.internalPeriodSizeInFrames;
    if (bufferFrames_ != dc.periodSizeInFrames) {
        LogMessage("[Miniaudio] Requested period " + std::to_string(dc.periodSizeInFrames) + " got " + std::to_string(bufferFrames_), LogCategory::AudioDevice, LogLevel::Info);
    }
    deviceInitialized_ = true;

    r = ma_device_start(&device_);
    if (r != MA_SUCCESS) {
        LogMessage("ma_device_start failed: " + std::string(ma_result_description(r)), LogCategory::AudioDevice, LogLevel::Error);
        processingEnabled_ = false;
        return;
    }


    LogMessage("AudioDeviceMiniaudio::AudioDeviceMiniaudio: DONE.", LogCategory::AudioDevice, LogLevel::Debug);
}

AudioDeviceMiniaudio::~AudioDeviceMiniaudio() {
    if (deviceInitialized_) {
        ma_device_uninit(&device_);
    }
    if (contextInitialized_) {
        ma_context_uninit(&context_);
    }
}

SoundHandle AudioDeviceMiniaudio::Play(AudioBuffer* buf, float volume, float pitch, bool loop) {
	// Obsolete! we’re driving PCM manually in render-callback—no per-voice backend here.
	return 0;
}

void AudioDeviceMiniaudio::Stop(SoundHandle) { /* no-op for manual mix */ }
void AudioDeviceMiniaudio::SetVolume(SoundHandle, float) { /* track in Voice */ }
void AudioDeviceMiniaudio::SetPitch(SoundHandle, float) { /* track in Voice */ }

void AudioDeviceMiniaudio::SetRenderCallback(std::function<void(float*, int)> cb) {
	std::lock_guard<std::mutex> lk(cbMutex_);
	renderCb_ = std::move(cb);
}


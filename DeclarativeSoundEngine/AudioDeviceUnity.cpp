#include "pch.h"
#include "AudioDeviceUnity.hpp"
#include "Log.hpp"
#include "UnityAudioPluginBridge.h"    // Unity's audio plugin SDK
#include <cstring>


static AudioDeviceUnity* s_Device = nullptr;
static std::function<void(float*, int)> s_RenderCallback;

AudioDeviceUnity::AudioDeviceUnity(int channels, int sampleRate, int bufferFrames) {
    LogMessage("[UnityDevice] init.", LogCategory::AudioDevice, LogLevel::Debug);
    s_Device = this;
}

AudioDeviceUnity::~AudioDeviceUnity() {
    LogMessage("[UnityDevice] shutdown.", LogCategory::AudioDevice, LogLevel::Debug);
    s_Device = nullptr;
}

SoundHandle AudioDeviceUnity::Play(AudioBuffer* buf, float volume, float pitch, bool loop) {
    // No-op: Unity drives mixing directly
    return SoundHandle();
}

void AudioDeviceUnity::Stop(SoundHandle handle) {
    // No-op
}

void AudioDeviceUnity::SetVolume(SoundHandle handle, float volume) {
    // No-op
}

void AudioDeviceUnity::SetPitch(SoundHandle handle, float pitch) {
    // No-op
}

void AudioDeviceUnity::SetRenderCallback(std::function<void(float*, int)> cb) {
    s_RenderCallback = std::move(cb);
}

extern "C" DECLSOUND_API void SoundAPI_FillBuffer(float* outBuffer, int frames, int channels) {
    if (!s_RenderCallback) {
        // silence
        memset(outBuffer, 0, sizeof(float) * size_t(frames) * channels);
        return;
    }
    s_RenderCallback(outBuffer, frames);
}
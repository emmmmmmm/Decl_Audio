// AudioDeviceStub.cpp
#include "pch.h"
#include "AudioDeviceStub.hpp"
#include "AudioDevice.hpp"

#include "Log.hpp"

#include <cstring>

AudioDeviceStub::AudioDeviceStub(int /*channels*/, int /*sampleRate*/, int bufferFrames) {
    bufferFrames_ = bufferFrames;
}


SoundHandle AudioDeviceStub::Play(AudioBuffer* buffer, float volume, float pitch, bool loop) {
	auto h = nextHandle++;
	LogMessage("AudioDeviceStub::Play handle=" + std::to_string(h)
		+ " vol=" + std::to_string(volume)
		+ " pitch=" + std::to_string(pitch)
		+ " loop=" + (loop ? "yes" : "no"),
		LogCategory::CLI, LogLevel::Info);
	return h;
}
void AudioDeviceStub::Stop(SoundHandle handle) {
	LogMessage("AudioDeviceStub::Stop handle=" + std::to_string(handle),
		LogCategory::CLI, LogLevel::Info);
}
void AudioDeviceStub::SetVolume(SoundHandle handle,
	float volume) {
	LogMessage("AudioDeviceStub::SetVolume handle="
		+ std::to_string(handle)
		+ " vol=" + std::to_string(volume),
		LogCategory::CLI, LogLevel::Info);
}
void AudioDeviceStub::SetPitch(SoundHandle handle,
	float pitch) {
	LogMessage("AudioDeviceStub::SetPitch handle="
		+ std::to_string(handle)
		+ " pitch=" + std::to_string(pitch),
		LogCategory::CLI, LogLevel::Info);
}
void AudioDeviceStub::SetRenderCallback(std::function<void(float*, int)> cb) {
    cb_ = std::move(cb);
}



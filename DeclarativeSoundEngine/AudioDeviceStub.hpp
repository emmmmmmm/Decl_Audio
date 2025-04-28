// AudioDeviceStub.hpp
#pragma once
#include "AudioDevice.hpp"
#include "Log.hpp"
#include <string>

class AudioDeviceStub : public AudioDevice {
	SoundHandle nextHandle = 1;
public:
	SoundHandle Play(AudioBuffer* buffer, float volume, float pitch, bool loop) override;
	void Stop(SoundHandle handle) override;
	void SetVolume(SoundHandle handle, float volume) override;
	void SetPitch(SoundHandle handle, float pitch) override;
};

#pragma once
#include "AudioDevice.hpp"


class AudioDeviceUnity : public AudioDevice {
public:
	AudioDeviceUnity(int channels, int sampleRate, int bufferFrames);
	~AudioDeviceUnity();

	SoundHandle Play(AudioBuffer* buf, float volume, float pitch, bool loop)	override;
	void Stop(SoundHandle handle)												override;
	void SetVolume(SoundHandle, float)											override;
	void SetPitch(SoundHandle, float)											override;
	void SetRenderCallback(std::function<void(float*, int)> cb)					override;
};
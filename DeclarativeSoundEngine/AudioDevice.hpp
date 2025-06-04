// AudioDevice.hpp
#pragma once
#include "AudioBuffer.hpp"
#include <functional>


enum AudioBackend : int {
	Miniaudio = 0,
	Unity = 1
};

struct AudioConfig {
	AudioBackend backend;
	uint32_t     sampleRate;
	uint32_t     bufferFrames;
	uint32_t     channels;
};

using SoundHandle = uint32_t;

struct AudioDevice {
	virtual ~AudioDevice() = default;
	virtual SoundHandle Play(AudioBuffer* buffer, float volume, float pitch, bool loop) = 0;
	virtual void Stop(SoundHandle handle) = 0;
	virtual void SetVolume(SoundHandle handle, float volume) = 0;
	virtual void SetPitch(SoundHandle handle, float pitch) = 0;
	virtual void SetRenderCallback(std::function<void(float*, int)> cb) = 0;
	//virtual int GetBufferSize() = 0;

};
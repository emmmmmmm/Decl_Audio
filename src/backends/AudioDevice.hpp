// AudioDevice.hpp
#pragma once

#include <functional>

#include <DeclarativeSoundEngine/AudioConfig.hpp>

#include "AudioBuffer.hpp"

using SoundHandle = uint32_t;

struct AudioDevice {
	virtual ~AudioDevice() = default;
	virtual SoundHandle Play(AudioBuffer* buffer, float volume, float pitch, bool loop) = 0;
	virtual void Stop(SoundHandle handle) = 0;
	virtual void SetVolume(SoundHandle handle, float volume) = 0;
	virtual void SetPitch(SoundHandle handle, float pitch) = 0;
	virtual void SetRenderCallback(std::function<void(float*, int)> cb) = 0;
	virtual uint32_t GetBufferFrames() const = 0;
};

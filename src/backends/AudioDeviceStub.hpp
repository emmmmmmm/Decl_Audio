// AudioDeviceStub.hpp
#pragma once
#include "AudioDevice.hpp"
#include "Log.hpp"
#include <string>

class AudioDeviceStub : public AudioDevice {
        SoundHandle nextHandle = 1;
        uint32_t bufferFrames_{};
public:
        AudioDeviceStub(int channels, int sampleRate, int bufferFrames);
        SoundHandle Play(AudioBuffer* buffer, float volume, float pitch, bool loop) override;
        void Stop(SoundHandle handle) override;
        void SetVolume(SoundHandle handle, float volume) override;
        void SetPitch(SoundHandle handle, float pitch) override;
        void SetRenderCallback(std::function<void(float*, int)> cb) override;
        uint32_t GetBufferFrames() const override { return bufferFrames_; }
private:
        std::function<void(float*, int)> cb_{};
};

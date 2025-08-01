// AudioDeviceMiniaudio.hpp
#pragma once
#include "AudioDevice.hpp"
#include "miniaudio.h"
#include <functional>
#include <mutex>
#include <unordered_map>

class AudioDeviceMiniaudio : public AudioDevice {
public:
    AudioDeviceMiniaudio(int channels, int sampleRate, int bufferFrames);
    ~AudioDeviceMiniaudio();

    SoundHandle Play(AudioBuffer* buf, float volume, float pitch, bool loop)    override;
    void Stop(SoundHandle handle)                                               override;
    void SetVolume(SoundHandle, float)                                          override;
    void SetPitch(SoundHandle, float)                                           override;
    void SetRenderCallback(std::function<void(float*, int)> cb)                 override;
   // int GetBufferSize() override;
    uint32_t GetBufferFrames() const override { return bufferFrames_; }
private:
    ma_engine                           engine_{};
    ma_context                          context_{};
    ma_device                           device_{};
    bool                                contextInitialized_{ false };
    bool                                deviceInitialized_{ false };
    bool                                processingEnabled_{ true };
    std::function<void(float*, int)>    renderCb_;
    std::mutex                          cbMutex_;

    uint32_t bufferFrames_{};

    static void dataCallback(ma_device* pDevice, void* pOutput, const void*, ma_uint32 frameCount) {
        auto* self = static_cast<AudioDeviceMiniaudio*>(pDevice->pUserData);
        float* out = static_cast<float*>(pOutput);
        std::lock_guard<std::mutex> lk(self->cbMutex_);
        if (self->processingEnabled_ && self->renderCb_) {
            self->renderCb_(out, frameCount);
        }
        else {
            // silence if no callback set
            std::fill(out, out + frameCount * pDevice->playback.channels, 0.f);
        }
    }
};

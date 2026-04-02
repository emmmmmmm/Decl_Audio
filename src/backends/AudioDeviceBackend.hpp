#pragma once

#include "Decl_Audio/Decl_Audio.h"
#include "../playback/AudioRuntime.hpp"

#include <memory>
#include <string>

namespace decl_audio::backends
{
    class AudioDeviceBackend
    {
    public:
        virtual ~AudioDeviceBackend() = default;

        virtual bool Start(playback::AudioRuntime &runtime,
                           const AudioConfig &config,
                           std::string &error_message) noexcept = 0;
        virtual void Stop() noexcept = 0;
        [[nodiscard]] virtual bool IsStarted() const noexcept = 0;
    };

    [[nodiscard]] std::unique_ptr<AudioDeviceBackend> CreateAudioDeviceBackend(DeclAudioBackend backend);
} // namespace decl_audio::backends

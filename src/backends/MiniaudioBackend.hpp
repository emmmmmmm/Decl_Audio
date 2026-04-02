#pragma once

#include "AudioDeviceBackend.hpp"

#include <memory>

namespace decl_audio::backends
{
    class MiniaudioBackend final : public AudioDeviceBackend
    {
    public:
        MiniaudioBackend();
        ~MiniaudioBackend() override;

        bool Start(playback::AudioRuntime &runtime,
                   const AudioConfig &config,
                   std::string &error_message) noexcept override;
        void Stop() noexcept override;
        [[nodiscard]] bool IsStarted() const noexcept override;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
} // namespace decl_audio::backends

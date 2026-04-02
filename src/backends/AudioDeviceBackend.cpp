#include "pch.h"

#include "AudioDeviceBackend.hpp"
#include "MiniaudioBackend.hpp"

namespace decl_audio::backends
{
    std::unique_ptr<AudioDeviceBackend> CreateAudioDeviceBackend(const DeclAudioBackend backend)
    {
        switch (backend)
        {
        case DECL_AUDIO_BACKEND_PLATFORM_DEFAULT:
            return std::make_unique<MiniaudioBackend>();

        case DECL_AUDIO_BACKEND_SILENT:
            return nullptr;
        }

        std::terminate();
    }
} // namespace decl_audio::backends

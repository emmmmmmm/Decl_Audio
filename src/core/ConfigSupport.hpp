#pragma once

#include "Decl_Audio/Decl_Audio.h"

#include <cstddef>

namespace decl_audio
{
    inline constexpr std::uint32_t kDefaultSampleRate = 48000;
    inline constexpr std::uint32_t kDefaultOutputChannelCount = 2;
    inline constexpr std::uint32_t kDefaultCallbackFrameCount = 1024;
    inline constexpr std::uint32_t kMaxCallbackFrameCount = 262144;

    inline AudioConfig MakeDefaultAudioConfig() noexcept
    {
        AudioConfig config{};
        config.struct_size = sizeof(AudioConfig);
        config.sample_rate = kDefaultSampleRate;
        config.output_channel_count = kDefaultOutputChannelCount;
        config.sample_format = DECL_AUDIO_SAMPLE_FORMAT_F32;
        config.callback_frame_count = kDefaultCallbackFrameCount;
        config.backend = DECL_AUDIO_BACKEND_SILENT;
        return config;
    }

    inline bool HasExplicitAudioConfig(const EngineConfig &config) noexcept
    {
        return config.struct_size >= offsetof(EngineConfig, audio) + sizeof(AudioConfig) &&
               config.audio.struct_size >= sizeof(AudioConfig);
    }

    inline AudioConfig ResolveAudioConfig(const EngineConfig &config) noexcept
    {
        if (!HasExplicitAudioConfig(config))
        {
            return MakeDefaultAudioConfig();
        }

        return config.audio;
    }

    inline bool ValidateAudioConfig(const AudioConfig &config) noexcept
    {
        if (config.struct_size < sizeof(AudioConfig))
        {
            return false;
        }

        if (config.sample_rate != kDefaultSampleRate)
        {
            return false;
        }

        if (config.output_channel_count != kDefaultOutputChannelCount)
        {
            return false;
        }

        if (config.sample_format != DECL_AUDIO_SAMPLE_FORMAT_F32)
        {
            return false;
        }

        if (config.callback_frame_count == 0 || config.callback_frame_count > kMaxCallbackFrameCount)
        {
            return false;
        }

        if (config.backend != DECL_AUDIO_BACKEND_SILENT &&
            config.backend != DECL_AUDIO_BACKEND_PLATFORM_DEFAULT)
        {
            return false;
        }

        return true;
    }
} // namespace decl_audio

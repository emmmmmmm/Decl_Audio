#include "pch.h"

#include "MiniaudioBackend.hpp"
#include "../third_party/Miniaudio.hpp"

#include <sstream>

namespace decl_audio::backends
{
    struct MiniaudioBackend::Impl final
    {
#if defined(_WIN32)
        ma_device device{};
        bool started = false;
#endif
    };

    namespace
    {
        [[nodiscard]] std::string FormatMiniaudioError(const ma_result result)
        {
            std::ostringstream stream;
            stream << "miniaudio backend failed with result " << static_cast<int>(result);
            return stream.str();
        }

#if defined(_WIN32)
        void DataCallback(ma_device *device, void *output, const void *input, ma_uint32 frame_count)
        {
            (void)input;

            playback::AudioRuntime *runtime = static_cast<playback::AudioRuntime *>(device->pUserData);
            runtime->Render(static_cast<float *>(output), static_cast<std::uint32_t>(frame_count));
        }
#endif
    } // namespace

    MiniaudioBackend::MiniaudioBackend()
        : impl_(std::make_unique<Impl>())
    {
    }

    MiniaudioBackend::~MiniaudioBackend()
    {
        Stop();
    }

    bool MiniaudioBackend::Start(playback::AudioRuntime &runtime,
                                 const EngineConfig &config,
                                 std::string &error_message) noexcept
    {
#if !defined(_WIN32)
        (void)runtime;
        (void)config;
        error_message = "platform default audio backend is currently implemented for Windows builds only";
        return false;
#else
        if (impl_->started)
        {
            Stop();
        }

        ma_device_config device_config = ma_device_config_init(ma_device_type_playback);
        device_config.playback.format = ma_format_f32;
        device_config.playback.channels = config.output_channel_count;
        device_config.sampleRate = config.sample_rate;
        device_config.periodSizeInFrames = config.callback_frame_count;
        device_config.dataCallback = DataCallback;
        device_config.pUserData = &runtime;

        const ma_result init_result = ma_device_init(nullptr, &device_config, &impl_->device);
        if (init_result != MA_SUCCESS)
        {
            error_message = FormatMiniaudioError(init_result);
            return false;
        }

        const ma_result start_result = ma_device_start(&impl_->device);
        if (start_result != MA_SUCCESS)
        {
            ma_device_uninit(&impl_->device);
            error_message = FormatMiniaudioError(start_result);
            return false;
        }

        impl_->started = true;
        return true;
#endif
    }

    void MiniaudioBackend::Stop() noexcept
    {
#if defined(_WIN32)
        if (!impl_->started)
        {
            return;
        }

        ma_device_uninit(&impl_->device);
        impl_->started = false;
#endif
    }

    bool MiniaudioBackend::IsStarted() const noexcept
    {
#if defined(_WIN32)
        return impl_->started;
#else
        return false;
#endif
    }
} // namespace decl_audio::backends

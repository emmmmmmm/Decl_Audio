#include "pch.h"

#include "StubBackend.hpp"

#include <exception>

namespace decl_audio::backends
{
    StubBackend::StubBackend(const std::uint32_t max_block_frames)
        : max_block_frames_(max_block_frames)
    {
        discard_buffer_.resize(static_cast<std::size_t>(max_block_frames_) * playback::AudioRuntime::OutputChannelCount);
    }

    void StubBackend::Pump(playback::AudioRuntime &runtime, const std::uint32_t frames) noexcept
    {
        if (frames > max_block_frames_)
        {
            std::terminate();
        }

        runtime.Render(discard_buffer_.data(), frames);
    }
} // namespace decl_audio::backends

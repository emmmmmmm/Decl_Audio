#pragma once

#include <cstdint>
#include <vector>

#include "../playback/AudioRuntime.hpp"

namespace decl_audio::backends
{
    class StubBackend final
    {
    public:
        explicit StubBackend(std::uint32_t max_block_frames = 4096);

        void Pump(playback::AudioRuntime &runtime, std::uint32_t frames) noexcept;

    private:
        std::vector<float> discard_buffer_;
        std::uint32_t max_block_frames_ = 0;
    };
} // namespace decl_audio::backends

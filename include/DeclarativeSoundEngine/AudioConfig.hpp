#pragma once

#include <cstdint>

enum AudioBackend : int {
    Miniaudio = 0,
    Unity = 1,
    Stub = 2,
};

struct AudioConfig {
    AudioBackend backend = AudioBackend::Miniaudio;
    std::uint32_t sampleRate = 44100;
    std::uint32_t bufferFrames = 512;
    std::uint32_t channels = 2;
};

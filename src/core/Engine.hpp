#pragma once

#include <cstdint>

#include "Decl_Audio/Decl_Audio.h"

namespace decl_audio
{
class Engine
{
public:
    explicit Engine(const DeclAudioEngineConfig& config) noexcept;
    virtual ~Engine() = default;

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;

    virtual DeclAudioResult LoadBehaviors(const char* source_path) noexcept;
    virtual DeclAudioResult Update() noexcept;

    [[nodiscard]] uint32_t GetApiVersion() const noexcept;
    [[nodiscard]] void* GetUserData() const noexcept;

private:
    uint32_t api_version_;
    void* user_data_;
};
} // namespace decl_audio

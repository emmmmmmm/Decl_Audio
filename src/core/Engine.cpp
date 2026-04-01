#include "pch.h"

#include "Engine.hpp"

namespace decl_audio
{
Engine::Engine(const DeclAudioEngineConfig& config) noexcept
    : api_version_(config.api_version),
      user_data_(config.user_data)
{
}

DeclAudioResult Engine::LoadBehaviors(const char* source_path) noexcept
{
    if (source_path == nullptr || source_path[0] == '\0')
    {
        return DeclAudioResult_InvalidArgument;
    }

    return DeclAudioResult_NotImplemented;
}

DeclAudioResult Engine::Update() noexcept
{
    return DeclAudioResult_Ok;
}

uint32_t Engine::GetApiVersion() const noexcept
{
    return api_version_;
}

void* Engine::GetUserData() const noexcept
{
    return user_data_;
}
} // namespace decl_audio

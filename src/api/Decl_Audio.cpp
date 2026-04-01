#include "pch.h"

#include "../core/Engine.hpp"

#include <new>

struct DeclAudioEngine
{
    explicit DeclAudioEngine(const DeclAudioEngineConfig& config) noexcept
        : engine(config)
    {
    }

    decl_audio::Engine engine;
};

namespace
{
DeclAudioEngineConfig MakeDefaultConfig() noexcept
{
    DeclAudioEngineConfig config{};
    config.struct_size = sizeof(DeclAudioEngineConfig);
    config.api_version = DECL_AUDIO_API_VERSION;
    config.user_data = nullptr;
    return config;
}

DeclAudioResult ValidateConfig(const DeclAudioEngineConfig* config) noexcept
{
    if (config == nullptr)
    {
        return DeclAudioResult_Ok;
    }

    if (config->struct_size < sizeof(DeclAudioEngineConfig))
    {
        return DeclAudioResult_InvalidArgument;
    }

    if (config->api_version != DECL_AUDIO_API_VERSION)
    {
        return DeclAudioResult_VersionMismatch;
    }

    return DeclAudioResult_Ok;
}
} // namespace

extern "C"
{
void DeclAudioEngineConfig_Init(DeclAudioEngineConfig* out_config)
{
    if (out_config == nullptr)
    {
        return;
    }

    *out_config = MakeDefaultConfig();
}

uint32_t DeclAudioGetApiVersion(void)
{
    return DECL_AUDIO_API_VERSION;
}

const char* DeclAudioResultToString(DeclAudioResult result)
{
    switch (result)
    {
    case DeclAudioResult_Ok:
        return "ok";
    case DeclAudioResult_InvalidArgument:
        return "invalid argument";
    case DeclAudioResult_OutOfMemory:
        return "out of memory";
    case DeclAudioResult_VersionMismatch:
        return "version mismatch";
    case DeclAudioResult_NotImplemented:
        return "not implemented";
    default:
        return "unknown";
    }
}

DeclAudioResult DeclAudioCreateEngine(const DeclAudioEngineConfig* config, DeclAudioEngine** out_engine)
{
    if (out_engine == nullptr)
    {
        return DeclAudioResult_InvalidArgument;
    }

    *out_engine = nullptr;

    const DeclAudioResult validation = ValidateConfig(config);
    if (validation != DeclAudioResult_Ok)
    {
        return validation;
    }

    const DeclAudioEngineConfig resolved_config = (config != nullptr) ? *config : MakeDefaultConfig();

    DeclAudioEngine* engine = new (std::nothrow) DeclAudioEngine(resolved_config);
    if (engine == nullptr)
    {
        return DeclAudioResult_OutOfMemory;
    }

    *out_engine = engine;
    return DeclAudioResult_Ok;
}

void DeclAudioDestroyEngine(DeclAudioEngine* engine)
{
    delete engine;
}

DeclAudioResult DeclAudioLoadBehaviors(DeclAudioEngine* engine, const char* source_path)
{
    if (engine == nullptr)
    {
        return DeclAudioResult_InvalidArgument;
    }

    return engine->engine.LoadBehaviors(source_path);
}

DeclAudioResult DeclAudioUpdate(DeclAudioEngine* engine)
{
    if (engine == nullptr)
    {
        return DeclAudioResult_InvalidArgument;
    }

    return engine->engine.Update();
}
}

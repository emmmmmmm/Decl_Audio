#include "pch.h"

#include "../core/Engine.hpp"
#include "../core/ConfigSupport.hpp"

#include <new>

struct DeclAudioEngine
{
    explicit DeclAudioEngine(const EngineConfig &config) noexcept
        : engine(config)
    {
    }

    decl_audio::Engine engine;
};

namespace
{
    EngineConfig MakeDefaultEngineConfig() noexcept
    {
        EngineConfig config{};
        config.struct_size = sizeof(EngineConfig);
        config.api_version = DECL_AUDIO_API_VERSION;
        config.user_data = nullptr;
        config.audio = decl_audio::MakeDefaultAudioConfig();
        return config;
    }

    bool ValidateConfig(const EngineConfig *config) noexcept
    {
        if (config == nullptr)
            return false;

        if (config->struct_size < sizeof(EngineConfig))
            return false;

        if (config->api_version != DECL_AUDIO_API_VERSION)
            return false;

        if (config->struct_size >= offsetof(EngineConfig, audio) + sizeof(AudioConfig) &&
            config->audio.struct_size != 0 &&
            !decl_audio::ValidateAudioConfig(config->audio))
        {
            return false;
        }

        return true;
    }

    EngineConfig ResolveConfig(const EngineConfig *config) noexcept
    {
        EngineConfig resolved = MakeDefaultEngineConfig();
        if (config == nullptr)
        {
            return resolved;
        }

        resolved.struct_size = sizeof(EngineConfig);
        resolved.api_version = config->api_version;
        resolved.user_data = config->user_data;
        resolved.audio = decl_audio::ResolveAudioConfig(*config);
        return resolved;
    }
} // namespace

extern "C"
{
    void InitAudioConfig(AudioConfig *out_config)
    {
        *out_config = decl_audio::MakeDefaultAudioConfig();
    }

    void Init(EngineConfig *out_config)
    {
        *out_config = MakeDefaultEngineConfig();
    }

    uint32_t GetApiVersion(void)
    {
        return DECL_AUDIO_API_VERSION;
    }

    bool CreateEngine(const EngineConfig *config, DeclAudioEngine **out_engine)
    {
        if (out_engine == nullptr)
            return false;

        *out_engine = nullptr;

        if (config != nullptr && !ValidateConfig(config))
            return false;

        const EngineConfig resolved_config = ResolveConfig(config);

        DeclAudioEngine *engine = new (std::nothrow) DeclAudioEngine(resolved_config);

        *out_engine = engine;
        return engine != nullptr;
    }

    void DestroyEngine(DeclAudioEngine *engine)
    {
        delete engine;
    }

    bool LoadBehaviors(DeclAudioEngine *engine, const char *source_path)
    {
        if (engine == nullptr)
            return false;

        return engine->engine.LoadBehaviors(source_path);
    }

    void Update(DeclAudioEngine *engine)
    {
        engine->engine.Update();
    }

    void SetTag(DeclAudioEngine *engine, const char *entity_id, const char *tag)
    {
        engine->engine.SetTag(entity_id, tag);
    }

    void RemoveTag(DeclAudioEngine *engine, const char *entity_id, const char *tag)
    {
        engine->engine.RemoveTag(entity_id, tag);
    }

    void SetValue(DeclAudioEngine *engine, const char *entity_id, const char *parameter, float value)
    {
        engine->engine.SetValue(entity_id, parameter, value);
    }

    void DestroyEntity(DeclAudioEngine *engine, const char *entity_id)
    {
        engine->engine.DestroyEntity(entity_id);
    }
}

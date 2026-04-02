#include "pch.h"

#include "../core/Engine.hpp"

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
    EngineConfig MakeDefaultConfig() noexcept
    {
        EngineConfig config{};
        config.struct_size = sizeof(EngineConfig);
        config.api_version = DECL_AUDIO_API_VERSION;
        config.user_data = nullptr;
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

        return true;
    }
} // namespace

extern "C"
{
    void Init(EngineConfig *out_config)
    {
        *out_config = MakeDefaultConfig();
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

        const EngineConfig resolved_config = (config != nullptr) ? *config : MakeDefaultConfig();

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
}

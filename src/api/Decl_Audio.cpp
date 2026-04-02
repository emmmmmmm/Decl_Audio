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

    void CreateEngine(const EngineConfig *config, DeclAudioEngine **out_engine)
    {

        *out_engine = nullptr;

        if (ValidateConfig(config))
            return;

        const EngineConfig resolved_config = (config != nullptr) ? *config : MakeDefaultConfig();

        DeclAudioEngine *engine = new (std::nothrow) DeclAudioEngine(resolved_config);

        *out_engine = engine;
    }

    void DestroyEngine(DeclAudioEngine *engine)
    {
        delete engine;
    }

    void LoadBehaviors(DeclAudioEngine *engine, const char *source_path)
    {
        engine->engine.LoadBehaviors(source_path);
    }

    void Update(DeclAudioEngine *engine)
    {
        engine->engine.Update();
    }
}

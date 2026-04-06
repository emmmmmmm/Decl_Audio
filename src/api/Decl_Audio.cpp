#include "pch.h"

#include "../core/Engine.hpp"

#include <new>

struct DeclAudioEngine
{
    explicit DeclAudioEngine(const EngineConfig *config) noexcept
        : engine(*config)
    {
    }
    decl_audio::Engine engine;
};

// not sure where to put those
namespace decl_audio
{
    inline constexpr std::uint32_t kDefaultSampleRate = 48000;
    inline constexpr std::uint32_t kDefaultOutputChannelCount = 2;
    inline constexpr std::uint32_t kDefaultCallbackFrameCount = 1024;
    inline constexpr std::uint32_t kDefaultMaxInstances = 256;
    inline constexpr std::uint32_t kDefaultMaxBlockFrames = kDefaultCallbackFrameCount * 4;
    inline constexpr std::uint32_t kDefaultCommandQueueCapacity = 1024;
    inline constexpr std::uint32_t kDefaultHostQueueCapacity = 1024;
}
extern "C"
{
    EngineConfig GetDefaultConfig()
    {
        EngineConfig config{};
        config.sample_rate = decl_audio::kDefaultSampleRate;
        config.output_channel_count = decl_audio::kDefaultOutputChannelCount;
        config.callback_frame_count = decl_audio::kDefaultCallbackFrameCount;
        config.max_instances = decl_audio::kDefaultMaxInstances;
        config.max_block_frames = decl_audio::kDefaultMaxBlockFrames;
        config.command_queue_capacity = decl_audio::kDefaultCommandQueueCapacity;
        config.host_queue_capacity = decl_audio::kDefaultHostQueueCapacity;
        config.backend = DECL_AUDIO_BACKEND_PLATFORM_DEFAULT;
        return config;
    }

    uint32_t GetApiVersion(void)
    {
        return DECL_AUDIO_API_VERSION;
    }
    bool ValidateConfig(const EngineConfig *config)
    {
        if (config->output_channel_count < 1 || config->output_channel_count > 2)
            return false;
        if (config->callback_frame_count == 0)
            return false;
        if (config->max_instances == 0)
            return false;
        if (config->max_block_frames < config->callback_frame_count)
            return false;
        if (config->command_queue_capacity < 2)
            return false;
        if (config->host_queue_capacity < 2)
            return false;
        return true;
    }
    bool CreateEngine(const EngineConfig *config, DeclAudioEngine **out_engine)
    {
        if (out_engine == nullptr)
            return false;

        *out_engine = nullptr;

        if (config == nullptr)
            return false;

        if (!ValidateConfig(config))
            return false;

        DeclAudioEngine *engine = new (std::nothrow) DeclAudioEngine(config);

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
    void SetTransientTag(DeclAudioEngine *engine, const char *entity_id, const char *tag)
    {
        engine->engine.SetTransientTag(entity_id, tag);
    }

    void RemoveTag(DeclAudioEngine *engine, const char *entity_id, const char *tag)
    {
        engine->engine.RemoveTag(entity_id, tag);
    }

    void SetValue(DeclAudioEngine *engine, const char *entity_id, const char *parameter, float value)
    {
        engine->engine.SetValue(entity_id, parameter, value);
    }

    void SetPosition(DeclAudioEngine *engine, const char *entity_id, const float x, const float y, const float z)
    {
        engine->engine.SetPosition(entity_id, x, y, z);
    }

    void SetListenerPosition(DeclAudioEngine *engine, const float x, const float y, const float z)
    {
        engine->engine.SetListenerPosition(x, y, z);
    }

    void DestroyEntity(DeclAudioEngine *engine, const char *entity_id)
    {
        engine->engine.DestroyEntity(entity_id);
    }
}

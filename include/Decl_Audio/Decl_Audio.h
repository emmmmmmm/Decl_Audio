#pragma once

#ifdef __cplusplus
#include <cstdbool>
#else
#include <stdbool.h>
#endif
#include <stdint.h>

#include "Export.h"

#define DECL_AUDIO_VERSION_MAJOR 0u
#define DECL_AUDIO_VERSION_MINOR 1u
#define DECL_AUDIO_VERSION_PATCH 0u
#define DECL_AUDIO_MAKE_VERSION(major, minor, patch) (((major) << 22u) | ((minor) << 12u) | (patch))
#define DECL_AUDIO_API_VERSION DECL_AUDIO_MAKE_VERSION(DECL_AUDIO_VERSION_MAJOR, DECL_AUDIO_VERSION_MINOR, DECL_AUDIO_VERSION_PATCH)

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct DeclAudioEngine DeclAudioEngine;

    typedef struct EngineConfig
    {
        uint32_t struct_size;
        uint32_t api_version;
        void *user_data;
    } EngineConfig;

    DECL_AUDIO_API void Init(EngineConfig *out_config);
    DECL_AUDIO_API uint32_t GetApiVersion(void);
    DECL_AUDIO_API bool CreateEngine(const EngineConfig *config, DeclAudioEngine **out_engine);
    DECL_AUDIO_API void DestroyEngine(DeclAudioEngine *engine);
    DECL_AUDIO_API bool LoadBehaviors(DeclAudioEngine *engine, const char *source_path);
    DECL_AUDIO_API void Update(DeclAudioEngine *engine);

    // TODO
    DECL_AUDIO_API void SetTag(DeclAudioEngine *engine, const char *entityId, const char *key, const char *tag);
    DECL_AUDIO_API void SetValue(DeclAudioEngine *engine, const char *entityId, const char *key, float value);
    DECL_AUDIO_API void SetPosition(DeclAudioEngine *engine, const char *entityId, const char *key, float x, float y, float z);
    DECL_AUDIO_API void AudioManager_SetQuatValue(DeclAudioEngine *engine, const char *entityId, const char *key, float a, float b, float c, float d);
    DECL_AUDIO_API void AudioManager_SetTransform(DeclAudioEngine *engine, const char *entityId, float x, float y, float z, float a, float b, float c, float d);
    DECL_AUDIO_API void AudioManager_ClearValue(DeclAudioEngine *engine, const char *entityId, const char *key);
    DECL_AUDIO_API void AudioManager_ClearEntity(DeclAudioEngine *engine, const char *entityId);

#ifdef __cplusplus
}
#endif

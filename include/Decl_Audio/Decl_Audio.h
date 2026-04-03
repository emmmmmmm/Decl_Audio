#pragma once

#ifdef __cplusplus
#include <cstdbool>
#else
#include <stdbool.h>
#endif
#include <stdint.h>

#include "Export.h"

#define DECL_AUDIO_VERSION_MAJOR 0u
#define DECL_AUDIO_VERSION_MINOR 2u
#define DECL_AUDIO_VERSION_PATCH 0u
#define DECL_AUDIO_MAKE_VERSION(major, minor, patch) (((major) << 22u) | ((minor) << 12u) | (patch))
#define DECL_AUDIO_API_VERSION DECL_AUDIO_MAKE_VERSION(DECL_AUDIO_VERSION_MAJOR, DECL_AUDIO_VERSION_MINOR, DECL_AUDIO_VERSION_PATCH)

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct DeclAudioEngine DeclAudioEngine;

    typedef enum DeclAudioSampleFormat
    {
        DECL_AUDIO_SAMPLE_FORMAT_F32 = 1
    } DeclAudioSampleFormat;

    typedef enum DeclAudioBackend
    {
        DECL_AUDIO_BACKEND_SILENT = 0,
        DECL_AUDIO_BACKEND_PLATFORM_DEFAULT = 1
    } DeclAudioBackend;

    typedef struct AudioConfig
    {
        uint32_t struct_size;
        uint32_t sample_rate;
        uint32_t output_channel_count;
        DeclAudioSampleFormat sample_format;
        uint32_t callback_frame_count;
        DeclAudioBackend backend;
    } AudioConfig;

    typedef struct EngineConfig
    {
        uint32_t struct_size;
        uint32_t api_version;
        void *user_data;
        AudioConfig audio;
    } EngineConfig;

    DECL_AUDIO_API void InitAudioConfig(AudioConfig *out_config);
    DECL_AUDIO_API void Init(EngineConfig *out_config);
    DECL_AUDIO_API uint32_t GetApiVersion(void);
    DECL_AUDIO_API bool CreateEngine(const EngineConfig *config, DeclAudioEngine **out_engine);
    DECL_AUDIO_API void DestroyEngine(DeclAudioEngine *engine);
    DECL_AUDIO_API bool LoadBehaviors(DeclAudioEngine *engine, const char *source_path);
    DECL_AUDIO_API void Update(DeclAudioEngine *engine);

    DECL_AUDIO_API void SetTag(DeclAudioEngine *engine, const char *entity_id, const char *tag);
    DECL_AUDIO_API void RemoveTag(DeclAudioEngine *engine, const char *entity_id, const char *tag);
    DECL_AUDIO_API void SetValue(DeclAudioEngine *engine, const char *entity_id, const char *parameter, float value);
    DECL_AUDIO_API void DestroyEntity(DeclAudioEngine *engine, const char *entity_id);

    DECL_AUDIO_API void SetPosition(DeclAudioEngine *engine, const char *entityId, float x, float y, float z);
    DECL_AUDIO_API void SetListenerPosition(DeclAudioEngine *engine, float x, float y, float z);
    // Future typed setters.
    DECL_AUDIO_API void SetQuatValue(DeclAudioEngine *engine, const char *entityId, const char *key, float a, float b, float c, float d);
    DECL_AUDIO_API void SetTransform(DeclAudioEngine *engine, const char *entityId, float x, float y, float z, float a, float b, float c, float d);
    DECL_AUDIO_API void ClearValue(DeclAudioEngine *engine, const char *entityId, const char *key);
    DECL_AUDIO_API void ClearEntity(DeclAudioEngine *engine, const char *entityId);

#ifdef __cplusplus
}
#endif

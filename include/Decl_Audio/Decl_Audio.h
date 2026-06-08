#pragma once

#ifdef __cplusplus
#include <cstdbool>
#else
#include <stdbool.h>
#endif
#include <stdint.h>

#include "Export.h"

#define DECL_AUDIO_VERSION_MAJOR 0u
#define DECL_AUDIO_VERSION_MINOR 6u
#define DECL_AUDIO_VERSION_PATCH 0u
#define DECL_AUDIO_MAKE_VERSION(major, minor, patch) (((major) << 22u) | ((minor) << 12u) | (patch))
#define DECL_AUDIO_API_VERSION DECL_AUDIO_MAKE_VERSION(DECL_AUDIO_VERSION_MAJOR, DECL_AUDIO_VERSION_MINOR, DECL_AUDIO_VERSION_PATCH)
#define DECL_AUDIO_LOG_MESSAGE_MAX_LENGTH 512u

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct DeclAudioEngine DeclAudioEngine;

    typedef enum DeclAudioBackend
    {
        DECL_AUDIO_BACKEND_SILENT = 0,
        DECL_AUDIO_BACKEND_PLATFORM_DEFAULT = 1
    } DeclAudioBackend;

    typedef struct DeclAudioLogMessage
    {
        uint32_t length;
        char message[DECL_AUDIO_LOG_MESSAGE_MAX_LENGTH];
    } DeclAudioLogMessage;

    typedef struct EngineConfig
    {
        // todo: add bankpath to config.

        // device-facing settings
        uint32_t sample_rate;
        uint32_t output_channel_count;
        uint32_t callback_frame_count;

        // runtime capacity settings
        uint32_t max_instances;
        uint32_t max_block_frames;
        uint32_t command_queue_capacity;
        uint32_t host_queue_capacity;

        DeclAudioBackend backend;
    } EngineConfig;

    DECL_AUDIO_API uint32_t GetApiVersion(void);
    DECL_AUDIO_API EngineConfig GetDefaultConfig();
    DECL_AUDIO_API bool CreateEngine(const EngineConfig *config, DeclAudioEngine **out_engine);
    DECL_AUDIO_API void DestroyEngine(DeclAudioEngine *engine);
    DECL_AUDIO_API bool LoadBehaviors(DeclAudioEngine *engine, const char *source_path);
    DECL_AUDIO_API bool LoadBank(DeclAudioEngine *engine, const char *bank_path);
    // Non-blocking bank load. Returns false if a load is already in flight or the
    // path is empty. The bank is wired in during a subsequent Update().
    DECL_AUDIO_API bool LoadBankAsync(DeclAudioEngine *engine, const char *bank_path);
    DECL_AUDIO_API void Update(DeclAudioEngine *engine);
    DECL_AUDIO_API bool TryDequeueLog(DeclAudioEngine *engine, DeclAudioLogMessage *out_message);

    DECL_AUDIO_API void SetTag(DeclAudioEngine *engine, const char *entity_id, const char *tag);
    DECL_AUDIO_API void RemoveTag(DeclAudioEngine *engine, const char *entity_id, const char *tag);
    DECL_AUDIO_API void SetValue(DeclAudioEngine *engine, const char *entity_id, const char *parameter, float value);
    DECL_AUDIO_API void DestroyEntity(DeclAudioEngine *engine, const char *entity_id);

    DECL_AUDIO_API void SetPosition(DeclAudioEngine *engine, const char *entityId, float x, float y, float z);
    DECL_AUDIO_API void SetListenerPosition(DeclAudioEngine *engine, float x, float y, float z);
    DECL_AUDIO_API void SetTransientTag(DeclAudioEngine *engine, const char *entity_id, const char *tag);

    DECL_AUDIO_API void SetGlobalTag(DeclAudioEngine *engine, const char *tag);
    DECL_AUDIO_API void RemoveGlobalTag(DeclAudioEngine *engine, const char *tag);
    DECL_AUDIO_API void SetGlobalValue(DeclAudioEngine *engine, const char *parameter, float value);

    DECL_AUDIO_API void SetMasterGain(DeclAudioEngine *engine, float gain);

    // Future typed setters.

    DECL_AUDIO_API void SetQuatValue(DeclAudioEngine *engine, const char *entityId, const char *key, float a, float b, float c, float d);
    DECL_AUDIO_API void SetTransform(DeclAudioEngine *engine, const char *entityId, float x, float y, float z, float a, float b, float c, float d);

#ifdef __cplusplus
}
#endif

#pragma once

#include <stdint.h>

#include "Export.h"

#define DECL_AUDIO_VERSION_MAJOR 0u
#define DECL_AUDIO_VERSION_MINOR 1u
#define DECL_AUDIO_VERSION_PATCH 0u
#define DECL_AUDIO_MAKE_VERSION(major, minor, patch) (((major) << 22u) | ((minor) << 12u) | (patch))
#define DECL_AUDIO_API_VERSION DECL_AUDIO_MAKE_VERSION(DECL_AUDIO_VERSION_MAJOR, DECL_AUDIO_VERSION_MINOR, DECL_AUDIO_VERSION_PATCH)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DeclAudioEngine DeclAudioEngine;

typedef enum DeclAudioResult
{
    DeclAudioResult_Ok = 0,
    DeclAudioResult_InvalidArgument = 1,
    DeclAudioResult_OutOfMemory = 2,
    DeclAudioResult_VersionMismatch = 3,
    DeclAudioResult_NotImplemented = 4
} DeclAudioResult;

typedef struct DeclAudioEngineConfig
{
    uint32_t struct_size;
    uint32_t api_version;
    void* user_data;
} DeclAudioEngineConfig;

DECL_AUDIO_API void DeclAudioEngineConfig_Init(DeclAudioEngineConfig* out_config);
DECL_AUDIO_API uint32_t DeclAudioGetApiVersion(void);
DECL_AUDIO_API const char* DeclAudioResultToString(DeclAudioResult result);
DECL_AUDIO_API DeclAudioResult DeclAudioCreateEngine(const DeclAudioEngineConfig* config, DeclAudioEngine** out_engine);
DECL_AUDIO_API void DeclAudioDestroyEngine(DeclAudioEngine* engine);
DECL_AUDIO_API DeclAudioResult DeclAudioLoadBehaviors(DeclAudioEngine* engine, const char* source_path);
DECL_AUDIO_API DeclAudioResult DeclAudioUpdate(DeclAudioEngine* engine);

#ifdef __cplusplus
}
#endif

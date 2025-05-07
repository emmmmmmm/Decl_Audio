#pragma once

// ─── Export / calling‐convention macros ───────────────────────────────────────
#if defined(_WIN32)
#  define UNITY_INTERFACE_EXPORT __declspec(dllexport)
#  define UNITY_INTERFACE_API    __cdecl
#else
#  define UNITY_INTERFACE_EXPORT
#  define UNITY_INTERFACE_API
#endif

// ─── Forward declarations for Unity types ────────────────────────────────────
struct IUnityInterfaces;            // host‐side interface registry
struct UnityAudioEffectState;       // opaque token for the DSP callback

// ─── DSP callback result and signature macros ───────────────────────────────
typedef enum UNITY_AUDIODSP_RESULT {
    UNITY_AUDIODSP_OK = 0
} UNITY_AUDIODSP_RESULT;
#define UNITY_AUDIODSP_CALLBACK UNITY_INTERFACE_API

// ─── AudioEffectDefinition ──────────────────────────────────────────────────
typedef UNITY_AUDIODSP_RESULT(UNITY_AUDIODSP_CALLBACK* UnityAudioPluginProcess_t)(
    UnityAudioEffectState* state,
    float* inbuffer, float* outbuffer,
    unsigned int length,
    int inchannels, int outchannels
    );

typedef struct AudioEffectDefinition
{
    const char* name;
    void* create;   // unused by our stub
    void* release;  // unused by our stub
    UnityAudioPluginProcess_t   process;
} AudioEffectDefinition;

#pragma once

// Public DLL import/export macros for the engine's C API.
#if defined(_WIN32)
#if defined(DECL_AUDIO_BUILD_DLL)
#define DECL_AUDIO_API __declspec(dllexport)
#elif defined(DECL_AUDIO_USE_DLL)
#define DECL_AUDIO_API __declspec(dllimport)
#else
#define DECL_AUDIO_API
#endif
#else
#if defined(DECL_AUDIO_BUILD_DLL)
#define DECL_AUDIO_API __attribute__((visibility("default")))
#else
#define DECL_AUDIO_API
#endif
#endif

#pragma once

#include "declsound_export.hpp"
#include "AudioCommand.hpp"
#include <thread>

class AudioManager;
struct AudioConfig;

extern "C" {

	DECLSOUND_API void AudioManager_Create(AudioConfig* cfg);
	DECLSOUND_API void AudioManager_Destroy();

	DECLSOUND_API void AudioManager_LoadBehaviorsFromFile(const char* behaviorPath, const char* assetPath);


	DECLSOUND_API void AudioManager_SetTag(const char* entityId, const char* tag);
	DECLSOUND_API void AudioManager_SetTransientTag(const char* entityId, const char* tag);
	DECLSOUND_API void AudioManager_ClearTag(const char* entityId, const char* tag);
	DECLSOUND_API void AudioManager_SetFloatValue(const char* entityId, const char* key, float value);
	DECLSOUND_API void AudioManager_SetStringValue(const char* entityId, const char* key, const char* value);
	DECLSOUND_API void AudioManager_SetVectorValue(const char* entityId, const char* key, float x, float y, float z);
	DECLSOUND_API void AudioManager_SetQuatValue(const char* entityId, const char* key, float a, float b, float c, float d);
	DECLSOUND_API void AudioManager_SetTransform(const char* entityId, float x, float y, float z, float a, float b, float c, float d); // position/quaternion
	DECLSOUND_API void AudioManager_ClearValue(const char* entityId, const char* key);
	DECLSOUND_API void AudioManager_ClearEntity(const char* entityId);
	// etc

	DECLSOUND_API void AudioManager_DebugPrintState();

	// Logging via DECLSOUND_API bool SoundAPI_PollLog();
}

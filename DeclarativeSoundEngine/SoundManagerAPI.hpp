#pragma once

#include "declsound_export.hpp"
#include "AudioBehavior.hpp"
class SoundManager;

extern "C" {
	

	DECLSOUND_API void* CreateSoundManager();
	DECLSOUND_API void DestroySoundManager(void* mgr);
	DECLSOUND_API void SoundManager_Update(void* mgr);

	
	// global tags and values
	DECLSOUND_API void SoundManager_SetGlobalTag(void* mgr, const char* tag);
	DECLSOUND_API void SoundManager_ClearGlobalTag(void* mgr, const char* tag);
	DECLSOUND_API void SoundManager_SetGlobalValue(void* mgr, const char* key, float value);

	// entity-level tags and values
	DECLSOUND_API void SoundManager_SetTag(void* mgr, const char* entityId, const char* tag);
	DECLSOUND_API void SoundManager_ClearTag(void* mgr, const char* entityId, const char* tag);
	DECLSOUND_API void SoundManager_SetValue(void* mgr, const char* entityId, const char* key, float value);
	DECLSOUND_API void SoundManager_ClearValue(void* mgr, const char* entityId, const char* key);
	DECLSOUND_API void SoundManager_ClearEntity(void* mgr, const char* entityId);

	// behavior loading
	DECLSOUND_API void SoundManager_LoadBehaviorsFromFile(void* mgr, const char* path);


	// debug, tests, etc
	DECLSOUND_API void SoundManager_DebugPrintState(void* mgr);
	DECLSOUND_API int SoundManager_GetLastEmitCount(void* mgr);
	DECLSOUND_API const char* SoundManager_GetLastEmitName(void* mgr, int index);

}
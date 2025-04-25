#pragma once

#include "declsound_export.hpp"
#include "AudioBehavior.hpp"
class SoundManager;

extern "C" {
	DECLSOUND_API SoundManager* CreateSoundManager();
	DECLSOUND_API void DestroySoundManager(SoundManager* mgr);
	DECLSOUND_API void SoundManager_Update(SoundManager* mgr);

	DECLSOUND_API void SoundManager_AddBehavior(SoundManager* mgr, const AudioBehavior* behavior);

	DECLSOUND_API void SoundManager_SetGlobalTag(SoundManager* mgr, const char* tag);
	DECLSOUND_API void SoundManager_ClearGlobalTag(SoundManager* mgr, const char* tag);
	DECLSOUND_API void SoundManager_SetGlobalValue(SoundManager* mgr, const char* key, float value);

	// override for entity-level tags and values
	DECLSOUND_API void SoundManager_SetTag(SoundManager* mgr, const char* entityId, const char* tag);
	DECLSOUND_API void SoundManager_ClearTag(SoundManager* mgr, const char* entityId, const char* tag);
	DECLSOUND_API void SoundManager_SetValue(SoundManager* mgr, const char* entityId, const char* key, float value);
	DECLSOUND_API void SoundManager_ClearValue(SoundManager* mgr, const char* entityId, const char* key);
	DECLSOUND_API void SoundManager_ClearEntity(SoundManager* mgr, const char* entityId);


	DECLSOUND_API void SoundManager_LoadBehaviorsFromFile(SoundManager* mgr, const char* path);
	DECLSOUND_API void SoundManager_DebugPrintState(SoundManager* mgr);

	DECLSOUND_API int SoundManager_GetLastEmitCount(SoundManager* mgr);
	DECLSOUND_API const char* SoundManager_GetLastEmitName(SoundManager* mgr, int index);

}
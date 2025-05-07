#pragma once

#include "declsound_export.hpp"
#include "AudioBehavior.hpp"
#include "Log.hpp"
class SoundManager;



extern "C" {
	
	enum AudioBackend : int {
		Miniaudio = 0,
		Unity = 1
	};

	struct AudioConfig {
		AudioBackend backend;
		uint32_t     sampleRate;
		uint32_t     bufferFrames;
		uint32_t     channels;
		//char* behaviorPath;
		//char* audioPath;
	};



	DECLSOUND_API void* CreateSoundManager(AudioConfig* cfg);
	DECLSOUND_API void DestroySoundManager(void* mgr);
	DECLSOUND_API void SoundManager_Init(
		void* mgr,
		const char* assetPath,
		const char** behaviorFolders,
		int folderCount,
		const char** globalKeys,
		const float* globalValues,
		int globalCount);
	DECLSOUND_API void SoundManager_Update(void* mgr);

	// global tags and values
	DECLSOUND_API void SoundManager_SetGlobalTag(void* mgr, const char* tag);
	DECLSOUND_API void SoundManager_ClearGlobalTag(void* mgr, const char* tag);
	DECLSOUND_API void SoundManager_SetGlobalValue(void* mgr, const char* key, float value);

	// entity-level tags and values
	DECLSOUND_API void SoundManager_SetTag(void* mgr, const char* entityId, const char* tag);
	DECLSOUND_API void SoundManager_ClearTag(void* mgr, const char* entityId, const char* tag);
	DECLSOUND_API void SoundManager_SetTransientTag(void* mgr, const char* entityId, const char* tag);

	DECLSOUND_API void SoundManager_SetValue(void* mgr, const char* entityId, const char* key, float value);
	DECLSOUND_API void SoundManager_ClearValue(void* mgr, const char* entityId, const char* key);
	DECLSOUND_API void SoundManager_ClearEntity(void* mgr, const char* entityId);
	DECLSOUND_API void SoundManager_SetBusGain(void* mgr, const char* entityId, float gain);
	DECLSOUND_API void SoundManager_SetBusGainExpression(void* mgr, const char* entityId, const char* gain);
	DECLSOUND_API void SoundManager_SetAssetPath(void* mgr, const char* path);

	// behavior loading
	DECLSOUND_API void SoundManager_LoadBehaviorsFromFile(void* mgr, const char* path);

	// debug, tests, etc
	DECLSOUND_API void SoundManager_DebugPrintState(void* mgr);
	DECLSOUND_API int SoundManager_GetLastEmitCount(void* mgr);
	DECLSOUND_API const char* SoundManager_GetLastEmitName(void* mgr, int index);

	DECLSOUND_API void SoundAPI_SetLogCallback(LogCallbackFn cb);


}
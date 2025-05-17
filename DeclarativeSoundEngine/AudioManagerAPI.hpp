#pragma once

#include "declsound_export.hpp"
#include "AudioCommand.hpp"
#include <thread>

class AudioManager;


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
	};

	/*
	 // we need: 
		- create an audiomanager
		- detach it on a new thread
			- for this we need to keep some kind of reference to the queues though!
		- create commands when we get api calls
		- send those commands to audiomanager

	*/



	DECLSOUND_API void AudioManager_Create(AudioConfig* cfg);
	DECLSOUND_API void AudioManager_Destroy();

	// behavior loading
	DECLSOUND_API void AudioManager_LoadBehaviorsFromFile(const char* behaviorPath, const char* assetPath);


	DECLSOUND_API void AudioManager_SetTag(const char* entityId, const char* tag);
	DECLSOUND_API void AudioManager_SetTransientTag(const char* entityId, const char* tag);
	DECLSOUND_API void AudioManager_ClearTag(const char* entityId, const char* tag);
	DECLSOUND_API void AudioManager_SetFloatValue(const char* entityId, const char* key, float value);
	DECLSOUND_API void AudioManager_SetStringValue(const char* entityId, const char* key, const char* value);
	DECLSOUND_API void AudioManager_SetVectorValue(const char* entityId, const char* key, float x, float y, float z);
	DECLSOUND_API void AudioManager_ClearValue(const char* entityId, const char* key);
	// etc

	DECLSOUND_API void AudioManager_DebugPrintState();

	// Logging via DECLSOUND_API bool SoundAPI_PollLog();
}

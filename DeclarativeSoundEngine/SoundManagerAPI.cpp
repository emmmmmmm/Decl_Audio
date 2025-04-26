#include "pch.h"
#include "SoundManagerAPI.hpp"
#include "SoundManager.hpp"
#include "BehaviorLoader.hpp"
#include <memory>


DECLSOUND_API void* CreateSoundManager() {
	return new SoundManager();
}

DECLSOUND_API void DestroySoundManager(void* mgr) {
	delete static_cast<SoundManager*>(mgr);
}

DECLSOUND_API void SoundManager_Update(void* mgr) {
	static_cast<SoundManager*>(mgr)->Update();
}


DECLSOUND_API void SoundManager_SetGlobalTag(void* mgr, const char* tag) {
	static_cast<SoundManager*>(mgr)->SetTag("global", std::string(tag));
}
DECLSOUND_API void SoundManager_ClearGlobalTag(void* mgr, const char* tag) {
	static_cast<SoundManager*>(mgr)->ClearTag("global", std::string(tag));
}
DECLSOUND_API void SoundManager_SetGlobalValue(void* mgr, const char* key, float value) {
	static_cast<SoundManager*>(mgr)->SetValue("global", std::string(key), value);
}


DECLSOUND_API void SoundManager_SetTag(void* mgr, const char* entityId, const char* tag) {
	static_cast<SoundManager*>(mgr)->SetTag(std::string(entityId), std::string(tag));
}

DECLSOUND_API void SoundManager_ClearTag(void* mgr, const char* entityId, const char* tag) {
	static_cast<SoundManager*>(mgr)->ClearTag(std::string(entityId), std::string(tag));
}

DECLSOUND_API void SoundManager_SetValue(void* mgr, const char* entityId, const char* key, float value) {
	static_cast<SoundManager*>(mgr)->SetValue(std::string(entityId), std::string(key), value);
}

DECLSOUND_API void SoundManager_ClearValue(void* mgr, const char* entityId, const char* key)
{
	static_cast<SoundManager*>(mgr)->ClearValue(std::string(entityId), std::string(key));
}

DECLSOUND_API void SoundManager_ClearEntity(void* mgr, const char* entityId)
{
	static_cast<SoundManager*>(mgr)->ClearEntity(std::string(entityId));
}


DECLSOUND_API void SoundManager_LoadBehaviorsFromFile(void* mgr, const char* path) {
	auto loaded = BehaviorLoader::LoadAudioBehaviorsFromFolder(path);
	for (auto& b : loaded) {
		static_cast<SoundManager*>(mgr)->AddBehavior(b);
	}
}

DECLSOUND_API void SoundManager_DebugPrintState(void* mgr) {
	static_cast<SoundManager*>(mgr)->DebugPrintState();
}

DECLSOUND_API int SoundManager_GetLastEmitCount(void* mgr)
{
	return (int) static_cast<SoundManager*>(mgr)->lastEmittedSoundIds.size();
}

DECLSOUND_API const char* SoundManager_GetLastEmitName(void* mgr, int index)
{
	return static_cast<SoundManager*>(mgr)->lastEmittedSoundIds[index].c_str();
}


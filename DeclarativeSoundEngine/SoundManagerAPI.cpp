#include "pch.h"
#include "SoundManagerAPI.hpp"
#include "SoundManager.hpp"
#include "BehaviorLoader.hpp"

DECLSOUND_API SoundManager* CreateSoundManager() {
	return new SoundManager();
}

DECLSOUND_API void DestroySoundManager(SoundManager* mgr) {
	delete mgr;
}

DECLSOUND_API void SoundManager_Update(SoundManager* mgr) {
	mgr->Update();
}

DECLSOUND_API void SoundManager_AddBehavior(SoundManager* mgr, const AudioBehavior* behavior) {
	mgr->AddBehavior(*behavior);
}

DECLSOUND_API void SoundManager_SetGlobalTag(SoundManager* mgr, const char* tag) {
	mgr->SetTag("global", std::string(tag));
}
DECLSOUND_API void SoundManager_ClearGlobalTag(SoundManager* mgr, const char* tag) {
	mgr->ClearTag("global", std::string(tag));
}
DECLSOUND_API void SoundManager_SetGlobalValue(SoundManager* mgr, const char* key, float value) {
	mgr->SetValue("global", std::string(key), value);
}


DECLSOUND_API void SoundManager_SetTag(SoundManager* mgr, const char* entityId, const char* tag) {
	mgr->SetTag(std::string(entityId), std::string(tag));
}

DECLSOUND_API void SoundManager_ClearTag(SoundManager* mgr, const char* entityId, const char* tag) {
	mgr->ClearTag(std::string(entityId), std::string(tag));
}

DECLSOUND_API void SoundManager_SetValue(SoundManager* mgr, const char* entityId, const char* key, float value) {
	mgr->SetValue(std::string(entityId), std::string(key), value);
}

DECLSOUND_API void SoundManager_ClearValue(SoundManager* mgr, const char* entityId, const char* key)
{
	mgr->ClearValue(std::string(entityId), std::string(key));
}

DECLSOUND_API void SoundManager_ClearEntity(SoundManager* mgr, const char* entityId)
{
	mgr->ClearEntity(std::string(entityId));
}


DECLSOUND_API void SoundManager_LoadBehaviorsFromFile(SoundManager* mgr, const char* path) {
	auto loaded = LoadAudioBehaviorsFromFile(path);
	for (const auto b : loaded) {
		mgr->AddBehavior(b);
	}
}

DECLSOUND_API void SoundManager_DebugPrintState(SoundManager* mgr) {
	mgr->DebugPrintState();
}

DECLSOUND_API int SoundManager_GetLastEmitCount(SoundManager* mgr)
{
	return (int) mgr->lastEmittedSoundIds.size();
}

DECLSOUND_API const char* SoundManager_GetLastEmitName(SoundManager* mgr, int index)
{
	return mgr->lastEmittedSoundIds[index].c_str();
}


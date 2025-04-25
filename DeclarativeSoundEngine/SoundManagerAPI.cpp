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

DECLSOUND_API void SoundManager_SetTag(SoundManager* mgr, const char* tag) {
    mgr->SetTag(std::string(tag));
}
DECLSOUND_API void SoundManager_ClearTag(SoundManager* mgr, const char* tag) {
    mgr->ClearTag(std::string(tag));
}DECLSOUND_API void SoundManager_SetValue(SoundManager* mgr, const char* key, float value) {
    mgr->SetValue(std::string(key), value);
}

DECLSOUND_API void SoundManager_LoadBehaviorsFromFile(SoundManager* mgr, const char* path) {
    auto loaded = LoadAudioBehaviorsFromFile(path);
    for (const auto& b : loaded) {
        mgr->AddBehavior(b);
    }
}

DECLSOUND_API void SoundManager_DebugPrintState(SoundManager* mgr) {
    mgr->DebugPrintState();
}


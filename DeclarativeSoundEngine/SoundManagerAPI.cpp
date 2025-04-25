#include "pch.h"
#include "SoundManagerAPI.hpp"
#include "SoundManager.hpp"

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

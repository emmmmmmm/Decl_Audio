#include "pch.h"
#include "SoundManagerAPI.hpp"
#include "SoundManager.hpp"
#include "BehaviorLoader.hpp"
#include <memory>
#include <iostream>



static AudioConfig g_config;
//static SoundManager* g_SM = nullptr; // this is such a mess currently, either I go with the singleton, or without it - DECIDE! // TODO

DECLSOUND_API void* CreateSoundManager(AudioConfig* cfg) {


	if (!cfg) {
		// throw exception?
		return nullptr;
	}

	g_config = *cfg;
	auto mgr = new SoundManager(&g_config);

	return mgr;
}


DECLSOUND_API void SoundManager_Init(
	void* mgr,
	const char* assetPath,
	const char** behaviorFolders,
	int folderCount,
	const char** globalKeys,
	const float* globalValues,
	int globalCount) {

	//if (!g_SM)

		SoundManager* soundManager = static_cast<SoundManager*>(mgr);


		// might be unneccessary if we move assetpaths to new Cfg file!
		soundManager->SetAssetPath(assetPath);
		//g_SM = soundManager;
	

	for (int i = 0; i < folderCount; ++i) {
		SoundManager_LoadBehaviorsFromFile(soundManager, behaviorFolders[i]);
	}


	// set global values
	for (int i = 0; i < globalCount; ++i) {
		const char* key = globalKeys[i];
		float       val = globalValues[i];
		soundManager->SetValue("global", key, val);
	}

}

DECLSOUND_API void DestroySoundManager(void* mgr) {

	LogMessage("Destroy Sound Manager", LogCategory::SoundManager, LogLevel::Debug);
	delete static_cast<SoundManager*>(mgr);
	//g_SM = nullptr;

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

void SoundManager_SetTransientTag(void* mgr, const char* entityId, const char* tag)
{
	static_cast<SoundManager*>(mgr)->SetTransientTag(std::string(entityId), std::string(tag));
}

DECLSOUND_API void SoundManager_SetValue(void* mgr, const char* entityId, const char* key, float value) {
	static_cast<SoundManager*>(mgr)->SetValue(std::string(entityId), std::string(key), value);
}

DECLSOUND_API void SoundManager_ClearValue(void* mgr, const char* entityId, const char* key)
{
	static_cast<SoundManager*>(mgr)->ClearValue(std::string(entityId), std::string(key));
}

DECLSOUND_API void SoundManager_SetPosition(void* mgr, const char* entityId, float x, float y, float z)
{
	auto manager = static_cast<SoundManager*>(mgr);
	manager->SetEntityPosition(entityId, x, y, z);
}

DECLSOUND_API void SoundManager_ClearEntity(void* mgr, const char* entityId)
{
	static_cast<SoundManager*>(mgr)->ClearEntity(std::string(entityId));
}

DECLSOUND_API void SoundManager_SetBusGain(void* mgr, const char* entityId, float gain)
{
	auto manager = static_cast<SoundManager*>(mgr);
	manager->SetBusGain(entityId, gain);
}

DECLSOUND_API void SoundManager_SetBusGainExpression(void* mgr, const char* entityId, const char* gain)
{
	auto manager = static_cast<SoundManager*>(mgr);
	manager->SetBusGainExpr(entityId, gain);
}

DECLSOUND_API void SoundManager_SetAssetPath(void* mgr, const char* path)
{
	auto manager = static_cast<SoundManager*>(mgr);
	manager->SetAssetPath(path);
}


DECLSOUND_API void SoundManager_LoadBehaviorsFromFile(void* mgr, const char* path) {
	auto manager = static_cast<SoundManager*>(mgr);
	// load audio definitions from files
	manager->defsProvider->LoadFilesFromFolder(path);
	LogMessage("LoadBehaviorsFromFile: Files Loaded", LogCategory::CLI, LogLevel::Debug);

	// set match definitions
	manager->matchDefinitions = manager->defsProvider->GetMatchDefs();
	LogMessage("LoadBehaviorsFromFile: GetMatchDefs() done", LogCategory::CLI, LogLevel::Debug);

	// set play definitions:
	Command cmd;
	cmd.type = CommandType::RefreshDefinitions;
	manager->managerToCore.push(cmd);
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


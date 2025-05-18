#include "pch.h"
#include "AudioManagerAPI.hpp"
#include "AudioManager.hpp"
#include "Vec3.hpp"
#include "AudioDevice.hpp"

CommandQueue apiToManager;
CommandQueue managerToApi;

AudioManager* mgr = nullptr;
std::thread audioThread;


void AudioManager_Create(AudioConfig* cfg)
{
	mgr = new AudioManager(cfg, &apiToManager, &managerToApi);

	audioThread = std::thread (&AudioManager::ThreadMain, mgr);
}

void AudioManager_Destroy()
{
	Command cmd;
	cmd.type = CommandType::Shutdown;
	apiToManager.push(cmd);
	// keep in mind that join() is blocking..!
	audioThread.join();
	delete mgr;
	mgr = nullptr;
}

void AudioManager_LoadBehaviorsFromFile(const char* behaviorPath, const char* assetPath)
{
	Command cmd2;
	cmd2.type = CommandType::AssetPath;
	cmd2.value = assetPath;
	apiToManager.push(cmd2);

	Command cmd;
	cmd.type = CommandType::LoadBehaviors;
	cmd.value = behaviorPath;
	apiToManager.push(cmd);

	
}

void AudioManager_SetTag(const char* entityId, const char* tag)
{
	Command cmd;
	cmd.type = CommandType::SetTag;
	cmd.entityId = entityId;
	cmd.value = tag;
	apiToManager.push(cmd);
}

void AudioManager_SetTransientTag(const char* entityId, const char* tag)
{
	Command cmd;
	cmd.type = CommandType::SetTransient;
	cmd.entityId = entityId;
	cmd.value = tag;
	apiToManager.push(cmd);
}

void AudioManager_ClearTag(const char* entityId, const char* tag)
{
	Command cmd;
	cmd.type = CommandType::ClearTag;
	cmd.entityId = entityId;
	cmd.value = tag;
	apiToManager.push(cmd);
}

void AudioManager_SetFloatValue(const char* entityId, const char* key, float value)
{
	Command cmd;
	cmd.type = CommandType::SetValue;
	cmd.entityId = entityId;
	cmd.key = key;
	cmd.value = value;
	apiToManager.push(cmd);
}
void AudioManager_SetStringValue(const char* entityId, const char* key, const char* value)
{
	Command cmd;
	cmd.type = CommandType::SetValue;
	cmd.entityId = entityId;
	cmd.key = key;
	cmd.value = value;
	apiToManager.push(cmd);
}
void AudioManager_SetVectorValue(const char* entityId, const char* key, float x,float y, float z)
{
	Command cmd;
	cmd.type = CommandType::SetValue;
	cmd.entityId = entityId;
	cmd.key = key;
	cmd.value = Vec3(x,y,z);
	apiToManager.push(cmd);
}
void AudioManager_ClearValue(const char* entityId, const char* key)
{
	Command cmd;
	cmd.type = CommandType::ClearValue;
	cmd.entityId = entityId;
	cmd.key = key;
	apiToManager.push(cmd);
}

void AudioManager_ClearEntity(const char* entityId)
{
	Command cmd;
	cmd.type = CommandType::ClearEntity;
	cmd.entityId = entityId;
	apiToManager.push(cmd);
}

void AudioManager_DebugPrintState()
{
	mgr->DebugPrintState();
}

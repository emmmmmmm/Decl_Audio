#include "pch.h"
#include "AudioManagerAPI.hpp"
#include "AudioManager.hpp"
#include "AudioDevice.hpp"
#include "Log.hpp"
#include "Vec3.hpp"
#include "Quaternion.h"

#include <thread>
#include <utility>

namespace {

CommandQueue gApiToManager;
CommandQueue gManagerToApi;
AudioManager* gManager = nullptr;
std::thread gAudioThread;

void DrainQueue(CommandQueue& queue)
{
	Command command;
	while (queue.pop(command)) {
	}
}

void ResetApiState()
{
	DrainQueue(gApiToManager);
	DrainQueue(gManagerToApi);
}

bool PushCommand(Command command)
{
	if (gApiToManager.push(command)) {
		return true;
	}

	LogMessage("Audio command queue overflow", LogCategory::AudioManager, LogLevel::Error);
	return false;
}

} // namespace

void AudioManager_Create(AudioConfig* cfg)
{
	if (cfg == nullptr) {
		LogMessage("AudioManager_Create called with a null config", LogCategory::AudioManager, LogLevel::Error);
		return;
	}

	AudioManager_Destroy();
	ResetApiState();

	gManager = new AudioManager(cfg, &gApiToManager, &gManagerToApi);
	gAudioThread = std::thread(&AudioManager::ThreadMain, gManager);
}

void AudioManager_Destroy()
{
	if (gManager != nullptr) {
		gManager->Shutdown();
	}
	if (gAudioThread.joinable()) {
		gAudioThread.join();
	}

	delete gManager;
	gManager = nullptr;
	ResetApiState();
}

void AudioManager_LoadBehaviorsFromFile(const char* behaviorPath, const char* assetPath)
{
	if (behaviorPath == nullptr || assetPath == nullptr) {
		LogMessage("AudioManager_LoadBehaviorsFromFile requires non-null paths", LogCategory::AudioManager, LogLevel::Error);
		return;
	}

	Command cmd2;
	cmd2.type = CommandType::AssetPath;
	cmd2.value = std::string(assetPath);
	PushCommand(std::move(cmd2));

	Command cmd;
	cmd.type = CommandType::LoadBehaviors;
	cmd.value = std::string(behaviorPath);
	PushCommand(std::move(cmd));
}

void AudioManager_SetTag(const char* entityId, const char* tag)
{
	Command cmd;
	cmd.type = CommandType::SetTag;
	cmd.entityId = entityId;
	cmd.value = std::string(tag);
	PushCommand(std::move(cmd));
}

void AudioManager_SetTransientTag(const char* entityId, const char* tag)
{
	Command cmd;
	cmd.type = CommandType::SetTransient;
	cmd.entityId = entityId;
	cmd.value = std::string(tag);
	PushCommand(std::move(cmd));
}

void AudioManager_ClearTag(const char* entityId, const char* tag)
{
	Command cmd;
	cmd.type = CommandType::ClearTag;
	cmd.entityId = entityId;
	cmd.value = std::string(tag);
	PushCommand(std::move(cmd));
}

void AudioManager_SetFloatValue(const char* entityId, const char* key, float value)
{
	Command cmd;
	cmd.type = CommandType::SetValue;
	cmd.entityId = entityId;
	cmd.key = key;
	cmd.value = value;
	PushCommand(std::move(cmd));
}
void AudioManager_SetStringValue(const char* entityId, const char* key, const char* value)
{
	Command cmd;
	cmd.type = CommandType::SetValue;
	cmd.entityId = entityId;
	cmd.key = key;
	cmd.value = std::string(value);
	PushCommand(std::move(cmd));
}
void AudioManager_SetVectorValue(const char* entityId, const char* key, float x, float y, float z)
{
	Command cmd;
	cmd.type = CommandType::SetValue;
	cmd.entityId = entityId;
	cmd.key = key;
	cmd.value = Vec3(x, y, z);
	PushCommand(std::move(cmd));
}
void AudioManager_SetQuatValue(const char* entityId, const char* key, float a, float b, float c, float d)
{
	Command cmd;
	cmd.type = CommandType::SetValue;
	cmd.entityId = entityId;
	cmd.key = key;
	cmd.value = quaternion::Quaternion(a, b, c, d);
	PushCommand(std::move(cmd));
}
void AudioManager_SetTransform(const char* entityId, float x, float y, float z, float a, float b, float c, float d)
{
	AudioManager_SetVectorValue(entityId, "position", x, y, z);
	AudioManager_SetQuatValue(entityId, "rotation", a, b, c, d);
}
void AudioManager_ClearValue(const char* entityId, const char* key)
{
	Command cmd;
	cmd.type = CommandType::ClearValue;
	cmd.entityId = entityId;
	cmd.key = key;
	PushCommand(std::move(cmd));
}

void AudioManager_ClearEntity(const char* entityId)
{
	Command cmd;
	cmd.type = CommandType::ClearEntity;
	cmd.entityId = entityId;
	PushCommand(std::move(cmd));
}

void AudioManager_DebugPrintState()
{
	if (gManager != nullptr) {
		gManager->DebugPrintState();
	}
}

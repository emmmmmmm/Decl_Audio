#include "pch.h"
#include "AudioManager.hpp"
#include <variant>
#include <chrono>
#include "Entity.hpp"
#include "Voice.hpp"
#include "Bus.hpp"
#include "Snapshot.hpp"
#include "AudioDeviceMiniAudio.hpp"
#include "AudioDeviceUnity.hpp"
#include "AudioBufferManager.hpp"
#include "BehaviorLoader.hpp"
#include "AudioDevice.hpp"
#include "Log.hpp"

const double M_PI = 3.14159265358979323846;

namespace {
	Snapshot::Snapshot gSnapshots[Snapshot::kSnapCount];   // single, file-local definition
}

AudioManager::AudioManager(AudioConfig* deviceCfg, CommandQueue* inQueue, CommandQueue* outQueue)
	:deviceCfg(deviceCfg), inQueue(inQueue), outQueue(outQueue) {

	bufferManager = new AudioBufferManager();

	// create audio device
	switch (deviceCfg->backend) {
	case AudioBackend::Miniaudio: { device = std::make_unique<AudioDeviceMiniaudio>(deviceCfg->channels, deviceCfg->sampleRate, deviceCfg->bufferFrames); break; }
	case AudioBackend::Unity: { device = std::make_unique<AudioDeviceUnity>(deviceCfg->channels, deviceCfg->sampleRate, deviceCfg->bufferFrames); break; }
	default: { LogMessage("unknown audio device!", LogCategory::AudioManager, LogLevel::Warning);break; }
	}

	// size buffers
	size_t bufSamples = size_t(deviceCfg->bufferFrames * deviceCfg->channels);

	for (Snapshot::Snapshot& snap : gSnapshots)
		for (int b = 0; b < Snapshot::kMaxBuses; ++b)
			snap.bus[b].resize(bufSamples, 0.0f);

	// set callback
	device->SetRenderCallback(
		[this](float* output, int frameCount) {RenderCallback(output, frameCount); });

	// Create Master Bus at buses[0]
	buses.clear();
	buses.push_back({ std::vector<float>(deviceCfg->bufferFrames * deviceCfg->channels),{} }); // master

}

void AudioManager::ThreadMain() {
	uint32_t lastSeen = 0; // block counter

	using Clock = std::chrono::steady_clock;
	auto start = Clock::now();

	while (!shouldQuit) {
		uint32_t head = cbBlockIndex.load(std::memory_order_acquire);
		if (head == lastSeen) {
			std::this_thread::yield();
			continue;
		}
		lastSeen = head;


		auto now = Clock::now();
		std::chrono::duration<float> elapsed = now - start;

		Update(elapsed.count());
		TakeSnapshot();


	}
	LogMessage("END OF AUDIO THREAD", LogCategory::AudioManager, LogLevel::Info);
}



AudioManager::~AudioManager() {
	LogMessage("~AudioCore", LogCategory::AudioManager, LogLevel::Debug);
	delete bufferManager;
}

void AudioManager::Update(float dt)
{
	// drain command queue
	size_t commandsToDrain = inQueue->Length();
	Command cmd;
	// can we really do this without a lock? :thinking:
	while (inQueue->pop(cmd)) {
		LogMessage(
			"ProcessCommands: " + cmd.GetTypeName() + "  (queue length: " + std::to_string(inQueue->Length()) + " / " + std::to_string(commandsToDrain) + ")",
			LogCategory::AudioManager, LogLevel::Debug);

		switch (cmd.type) {
		case CommandType::SetTag:		SetTag(cmd);		break;
		case CommandType::SetTransient:	SetTransient(cmd);	break;
		case CommandType::ClearTag:		ClearTag(cmd);		break;
		case CommandType::SetValue:		SetValue(cmd);		break;
		case CommandType::ClearValue:	ClearValue(cmd);	break;
		case CommandType::LoadBehaviors:LoadBehaviorsFromFolder(cmd);break;
		case CommandType::Shutdown:		Shutdown();			break;
		case CommandType::AssetPath:	SetAssetPath(cmd);	break;
		case CommandType::ClearEntity:	ClearEntity(cmd);	break;
		default: LogMessage("UNKNOWN COMMAND: " + cmd.GetTypeName(), LogCategory::AudioManager, LogLevel::Debug);

		}
	}

	const auto& globalValues = entities["global"].GetValues();
	const auto& globalTags = entities["global"].GetTags();

	// update entities
	for (auto& entityValue : entities) {
		entityValue.second.Update(definitions, globalTags, globalValues, deviceCfg, bufferManager);
	}

}

void AudioManager::Shutdown() {
	shouldQuit = true; 
	// stops the looping thread function
	// what do we actually need to do here?
	// maybe unload buffers / cleanup voices? 
	// we should do most required things already on destruction, but ... can a thing selfdestruct??
}

// build a fresh read‐only snapshot for render
void AudioManager::TakeSnapshot()
{

	int front = currentReadBuffer.load(std::memory_order_acquire);
	int backIndex = 1 - front;
	Snapshot::Snapshot& back = gSnapshots[backIndex];

	// reset counters
	back.voiceCount = 0;
	back.busCount = uint32_t(buses.size());
	
	// bus gains
	for (uint32_t i = 0; i < back.busCount; ++i) {
		back.busGain[i] = buses[i].volume.eval(entities.empty() ? ValueMap{} : entities.begin()->second.GetValues());
		back.busParent.resize(back.busCount); // maybe fixed size in the future (some max bus count)
		back.busParent[i] = buses[i].parent;
	}

	// listener position (fallback to origin)
	Vec3 listenerPos{ 0,0,0 };
	if (!currentListener.empty()) {
		auto it = entities.find(currentListener);
		if (it != entities.end()) {
			it->second.GetValues().TryGetValue("position", listenerPos);
		}
	}

	back.listenerPosition = listenerPos;

	
	// flatten voices
	for (auto& [eid, ed] : entities)

		for (auto& inst : ed.GetBehaviors()) {
			for (auto& v : inst.GetVoices()) {
				if (back.voiceCount >= Snapshot::kMaxVoices) 
					continue;
				Vec3 srcPos;
				bool isSpatial = ed.GetValues().TryGetValue("position", srcPos);
				float att = 1.f;
				float panL = 1.f, panR = 1.f;
				std::vector<float> pan;
				if (isSpatial) {

					float dx = srcPos.x - listenerPos.x;
					float dy = srcPos.y - listenerPos.y;
					float dz = srcPos.z - listenerPos.z;

					float dist = std::sqrt(dx * dx + dz * dz + dy * dy);
					float radius = 20.0f; // fallthrough default value
					ed.GetValues().TryGetValue("radius", radius); // TODO check if radius>0!
					att = std::clamp(1.f - dist / radius, 0.f, 1.f);

					// stereo azimuth panning
					float az = std::atan2(dx, dz);
					panL = std::clamp(0.5f - az / float(M_PI), 0.f, 1.f);
					panR = 1.f - panL;
					pan.push_back(panL); // TODO: MULTICHANNEL
					pan.push_back(panR);
				}
				else {
					pan.push_back(1); // TODO: MULTICHANNEL
					pan.push_back(1);
				}


				back.voices[back.voiceCount++] = Snapshot::VoiceSnap{
					v.buffer,
					v.playhead,
					v.currentVol * att,   // pre‐attenuated
					v.loop,
					uint8_t(v.busIndex),
					v.startSample,
					pan
				};
			}
		}

	AdvancePlayheads();

	currentReadBuffer.store(backIndex, std::memory_order_release);
}


void AudioManager::AdvancePlayheads()
{
	uint32_t step = pendingFrames.exchange(0, std::memory_order_acquire);
	if (step == 0) return;

	for (auto& [eid, ed] : entities)
		for (auto& inst : ed.GetBehaviors())
			for (auto& v : inst.GetVoices()) {
				if (!v.buffer) continue;
				if (v.buffer->Empty()) continue; // TODO: buffer not ready. but this would offset our start, right? not sure how to properly handle this tbh...?
				size_t len = v.buffer->GetFrameCount();
				if (v.loop)
					v.playhead = (v.playhead + step) % len;
				else
					v.playhead = (std::min)(v.playhead + step, len);
			}
}


int AudioManager::GetOrCreateBus(const std::string& entityId) {
	auto it = entityBus.find(entityId);
	if (it != entityBus.end()) return it->second;
	// allocate new sub‐bus
	int newIndex = (int)buses.size();
	buses.push_back({ std::vector<float>(deviceCfg->bufferFrames * deviceCfg->channels), {} });
	entityBus.emplace(entityId, newIndex);

	LogMessage("[BUS] added new bus for entity: " + entityId, LogCategory::AudioManager, LogLevel::Debug);
	return newIndex;
}


void AudioManager::SetTag(Command cmd) {
	std::string entityId = cmd.entityId;
	auto* tag = std::get_if<std::string>(&cmd.value);
	auto& entity = entities[entityId]; // create if missing
	entity.SetBus(GetOrCreateBus(entityId));
	entity.SetTag(*tag);

	if (*tag == "listener")
		currentListener = cmd.entityId;
}
void AudioManager::SetTransient(Command cmd) {
	std::string entityId = cmd.entityId;
	auto* tag = std::get_if<std::string>(&cmd.value);
	auto& entity = entities[entityId];
	entity.SetTransientTag(*tag);
}
void AudioManager::ClearTag(Command cmd) {
	std::string entityId = cmd.entityId;
	auto* tag = std::get_if<std::string>(&cmd.value);
	auto& entity = entities[entityId];
	entity.ClearTag(*tag);
}
void AudioManager::SetValue(Command cmd) {
	std::string entityId = cmd.entityId;
	auto& entity = entities[entityId];
	entity.SetValue(cmd.key, cmd.value);
}
void AudioManager::ClearValue(Command cmd) {
	std::string entityId = cmd.entityId;
	auto& entity = entities[entityId];
	entity.ClearValue(cmd.key);
}

void AudioManager::LoadBehaviorsFromFolder(Command cmd)
{
	auto* path = std::get_if<std::string>(&cmd.value);
	definitions = BehaviorLoader::LoadAudioBehaviorsFromFolder(*path); 
}
void AudioManager::SetAssetPath(Command cmd)
{
	const auto* path = std::get_if<std::string>(&cmd.value);
	bufferManager->SetAssetpath(*path);
}
void AudioManager::ClearEntity(Command cmd) {
	// TODO: Clear this Entity
	entities.erase(cmd.entityId); // THATS PROBABLY A VERY BAD IDEA
}


void AudioManager::DebugPrintState()
{
	for (auto& e : entities) {
		std::cout << e.first << ":" << std::endl;
		std::cout << "  tags:" << std::endl;;
		for (auto& tag : e.second.GetTags().GetAllTags()) {
			std::cout << "    - " << tag << std::endl;
		}
		std::cout << "  vals:" << std::endl;

		for (auto& val : e.second.GetValues().GetAllValues()) {
			auto n = val.first;
			auto sv = "";
			auto fv = 0.f;
			auto vv = Vec3{};
			if (e.second.GetValues().TryGetValue(n, sv)) {
				std::cout << "    - " << n << ": " << sv << std::endl;
			}
			if (e.second.GetValues().TryGetValue(n, fv)) {
				std::cout << "    - " << n << ": " << std::to_string(fv) << std::endl;
			}
			if (e.second.GetValues().TryGetValue(n, vv)) {
				std::cout << "    - " << n << ": " << "("
					<< std::to_string(vv.x) << ", "
					<< std::to_string(vv.y) << ", "
					<< std::to_string(vv.z) << ")" << std::endl;
			}
		}

		std::cout << "  behaviors: " << std::endl;
		for (auto& ab : e.second.GetBehaviors()) {
			std::cout << "    - " << ab.Name() << std::endl;
		}
	}
}


void AudioManager::RenderCallback(float* out, int frames)
{
	// load snapshot
	int front = currentReadBuffer.load(std::memory_order_acquire);
	const Snapshot::Snapshot& s = gSnapshots[front];

	uint64_t blockStart = globalSampleCounter;
	int samples = frames * deviceCfg->channels;      

	// clear bus buffers
	for (uint32_t b = 0; b < s.busCount; ++b)
		std::fill(s.bus[b].begin(), s.bus[b].end(), 0.0f);

	// mix voices
	for (uint32_t v = 0; v < s.voiceCount; ++v)
	{
		const Snapshot::VoiceSnap& vs = s.voices[v];

		// causes crash with delay node... :thinking:
		if (!vs.buf || vs.buf->Empty())
		{
			continue; // buffer not loaded...?
		}
		const float*	pcm = vs.buf->GetData();
		int				ch = vs.buf->GetChannelCount();
		size_t			len = vs.buf->GetFrameCount();
		size_t			basePos = size_t(vs.playhead);


		float* busBuf = s.bus[vs.bus].data();
		for (int i = 0; i < frames; ++i) {
			size_t pos = basePos + size_t(i);
			float s0 = 0.f;

			if (pos < len) 
				s0 = pcm[pos * ch] * vs.gain;
			else if (vs.loop) 
				s0 = pcm[(pos % len) * ch] * vs.gain;

			for (uint32_t c = 0; c < deviceCfg->channels; ++c)
				busBuf[i * deviceCfg->channels + c] += s0 * vs.pan[c]; // TODO: ChannelMatrix
		}
	}

	for (uint32_t b = 0; b < s.busCount; ++b) {
		float maxSample = 0;
		for (float f : s.bus[b])
			maxSample = (std::max)(maxSample, std::abs(f));
	}


	// fold busses onto master bus (master = buses[0])
	for (int b = int(s.busCount) - 1; b > 0; --b) {
		float gain = s.busGain[b];
		int parent = parent = s.busParent[b];
		float* src = s.bus[b].data();
		float* dst = s.bus[parent].data();
		for (int i = 0; i < samples; ++i)
			dst[i] += src[i] * gain;
	}

	// copy to device
	std::memcpy(out, s.bus[0].data(), size_t(frames * deviceCfg->channels) * sizeof(float));

	// advance counters
	pendingFrames.fetch_add(frames, std::memory_order_relaxed);
	globalSampleCounter += frames; // this might be a little redundant
	cbBlockIndex.fetch_add(1, std::memory_order_release); 
}
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

	LogMessage("INIT", LogCategory::AudioManager, LogLevel::Debug);

	bufferManager = new AudioBufferManager();
	LogMessage("... create audio device", LogCategory::AudioManager, LogLevel::Debug);

	// create audio device
	switch (deviceCfg->backend) {
	case AudioBackend::Miniaudio: { device = std::make_unique<AudioDeviceMiniaudio>(deviceCfg->channels, deviceCfg->sampleRate, deviceCfg->bufferFrames); break; }
	case AudioBackend::Unity: { device = std::make_unique<AudioDeviceUnity>(deviceCfg->channels, deviceCfg->sampleRate, deviceCfg->bufferFrames); break; }
	default: { LogMessage("unknown audio device!", LogCategory::AudioManager, LogLevel::Warning);break; }
	}

	LogMessage("... create buffers", LogCategory::AudioManager, LogLevel::Debug);

	// size buffers
	size_t bufSamples = size_t(deviceCfg->bufferFrames * deviceCfg->channels);

	for (Snapshot::Snapshot& snap : gSnapshots)
		for (int b = 0; b < Snapshot::kMaxBuses; ++b)
			//snap.bus[b].resize(bufSamples, 0.0f);
			std::memset(snap.bus[b], 0, bufSamples * sizeof(float));
	LogMessage("... set callback", LogCategory::AudioManager, LogLevel::Debug);

	// set callback
	// TODO: REENABLE
	device->SetRenderCallback(
		[this](float* output, int frameCount) {RenderCallback(output, frameCount); });


	LogMessage("... create master bus", LogCategory::AudioManager, LogLevel::Debug);

	// Create Master Bus at buses[0]
	buses.clear();
	buses.push_back({ std::vector<float>(deviceCfg->bufferFrames * deviceCfg->channels),{} }); // master
	LogMessage("... Init Done.", LogCategory::AudioManager, LogLevel::Debug);

	LogMessage("... starting buffertest.", LogCategory::AudioManager, LogLevel::Debug);




	// BufferTest();

}
void AudioManager::BufferTest() {


	const int numVoices = 255;
	const int frames = deviceCfg->bufferFrames;
	const int channels = deviceCfg->channels;
	std::vector<float> outBuffer(frames * channels);  // This will be your "fake audio output"

	// create a dummy buffer
	auto dummyBuf = new AudioBuffer("");
	dummyBuf->channelCount = channels;
	dummyBuf->frameCount = frames * 2;  // give it some length
	dummyBuf->samples.resize(frames * 2 * channels, 0.25f);  // fill with audible dummy data

	// set as loaded
	//dummyBuf->loaded = true;

	// get snapshot reference
	int backIndex = 1 - currentReadBuffer.load();
	int frontIndex = currentReadBuffer.load(std::memory_order_acquire);
	auto& snap = gSnapshots[frontIndex];

	snap.voiceCount = numVoices;

	for (int i = 0; i < numVoices; ++i) {
		snap.voices[i].buf = dummyBuf;
		snap.voices[i].playhead = i * 7 % dummyBuf->GetFrameCount(); // staggered
		snap.voices[i].gain = 0.8f;
		snap.voices[i].loop = true;
		snap.voices[i].bus = GetOrCreateBus(std::to_string(i + 1));
		snap.voices[i].startSample = 0;
		snap.voices[i].pan[0] = (i % 2 == 0) ? 1.0f : 0.7f;
		snap.voices[i].pan[1] = 1.0f - snap.voices[i].pan[0];

	}
	snap.busCount = numVoices + 1;
	const int bufferSize = frames * channels;

	for (int i = 1; i < snap.busCount; ++i) {
		snap.busParent[i] = 0;        // all sub-buses fold into master
		snap.busGain[i] = 1.0f;       // no attenuation
	}
	snap.busParent[0] = 0;            // master bus is its own parent
	snap.busGain[0] = 1.0f;           // unity gain


	currentReadBuffer.store(frontIndex, std::memory_order_release);

	for (int i = 0;i < 100;i++) {

		RenderCallback(outBuffer.data(), frames);
	}


}

void AudioManager::ThreadMain() {
	// TODO: We still get stuck when quitting from unity, need to rethink how we get out of this loop!
	LogMessage("BEGINNING OF AUDIO THREAD", LogCategory::AudioManager, LogLevel::Info);
	uint32_t lastSeen = 0; // block counter

	using Clock = std::chrono::steady_clock;
	auto start = Clock::now();

	while (!shouldQuit) {
		uint32_t head = cbBlockIndex.load(std::memory_order_acquire);

		if (shouldQuit) break;

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
			"ProcessCommands: "
			+ cmd.GetTypeName() + "  (queue length: " + std::to_string(inQueue->Length()) + " / " + std::to_string(commandsToDrain) + ")",
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
	shouldQuit.store(true, std::memory_order_release);
	cbBlockIndex.fetch_add(1, std::memory_order_release);     // Force the spin to observe a new block
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
		//back.busParent.resize(back.busCount); // maybe fixed size in the future (some max bus count)
		back.busParent[i] = buses[i].parent;
		back.busParent[i] = buses[i].parent;
	}

	std::memset(back.numVoicesInBus, 0, sizeof(back.numVoicesInBus));

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
	{
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
					pan[0],
					pan[1]
				};
			}
		}
	}
	// after filling all snap.voices[0..voiceCount-1]
	for (uint32_t v = 0; v < back.voiceCount; ++v) {
		uint8_t B = back.voices[v].bus;          // which bus this voice targets
		uint32_t idx = back.numVoicesInBus[B]++;  // grab next free slot
		back.voicesByBus[B][idx] = v;            // store voice‐index in that slot
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

	//LogMessage("[BUS] added new bus for entity: " + entityId, LogCategory::AudioManager, LogLevel::Debug);
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

	LogMessage("Tag set: e: " + entityId + " t: " + *tag, LogCategory::AudioManager, LogLevel::Debug);
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
		LogMessage(e.first + ":", LogCategory::AudioManager, LogLevel::Debug);
		LogMessage("  tags:", LogCategory::AudioManager, LogLevel::Debug);
		for (auto& tag : e.second.GetTags().GetAllTags()) {
			LogMessage("    - " + tag, LogCategory::AudioManager, LogLevel::Debug);
		}
		LogMessage("  vals:", LogCategory::AudioManager, LogLevel::Debug);

		for (auto& val : e.second.GetValues().GetAllValues()) {
			auto n = val.first;
			auto sv = "";
			auto fv = 0.f;
			auto vv = Vec3{};
			if (e.second.GetValues().TryGetValue(n, sv)) {
				LogMessage("    - " + n + ": " + sv, LogCategory::AudioManager, LogLevel::Debug);
			}
			if (e.second.GetValues().TryGetValue(n, fv)) {
				LogMessage("    - " + n + ": " + std::to_string(fv), LogCategory::AudioManager, LogLevel::Debug);
			}
			if (e.second.GetValues().TryGetValue(n, vv)) {
				LogMessage("    - " + n + ": " + "("
					+ std::to_string(vv.x) + ", "
					+ std::to_string(vv.y) + ", "
					+ std::to_string(vv.z) + ")", LogCategory::AudioManager, LogLevel::Debug);
			}
		}

		LogMessage("  behaviors: ", LogCategory::AudioManager, LogLevel::Debug);
		for (auto& ab : e.second.GetBehaviors()) {
			LogMessage("    - " + ab.Name(), LogCategory::AudioManager, LogLevel::Debug);
		}
	}
}


void AudioManager::RenderCallback(float* out, int frames) {
	auto t0 = std::chrono::high_resolution_clock::now();
	const auto& s = gSnapshots[currentReadBuffer.load()];
	int bufferSize = frames * deviceCfg->channels; // e.g. 2048 frames × 2 = 4096

	// 1) Zero the master once
	std::memset(out, 0, bufferSize * sizeof(float));

	// 2) Iterate each sub‐bus B = 1..(busCount−1)
	for (uint32_t B = 1; B < s.busCount; ++B) {
		float* busBuf = s.bus[B];
		float  bGain = s.busGain[B];

		// 2a) Zero this bus buffer
		std::memset(busBuf, 0, bufferSize * sizeof(float));

		// 2b) Mix each voice in bus B
		uint32_t nVoices = s.numVoicesInBus[B];
		for (uint32_t vi = 0; vi < nVoices; ++vi) {
			uint32_t vIndex = s.voicesByBus[B][vi];        // which entry in s.voices[]
			const auto& vs = s.voices[vIndex];
			const float* pcm = vs.buf->GetData();
			size_t        ph = vs.playhead;               // in frames
			float         gL = vs.gain * vs.pan[0];
			float         gR = vs.gain * vs.pan[1];
			size_t totalFrames = vs.buf->GetFrameCount();   // e.g. 4096
			size_t totalSamples = vs.buf->GetSampleCount();   // e.g. 22050×2 = 44100
			size_t bufferFrames = frames;                              // how many frames to render now
			size_t bufferSize = bufferFrames * deviceCfg->channels;  // e.g. 2048×2 = 4096 floats

			int srcChannels = vs.buf->GetChannelCount();
			int dstChannels = deviceCfg->channels;

			
			// 1) If ph is already out of range, handle that first:
			if (ph >= totalFrames) {
				if (vs.loop) {
					ph %= totalFrames;
				}
				else {
					// non-looping voice is already “done” before we start
					continue;
				}
			}

			size_t samplePosInFloats = ph * srcChannels;  // starting float index into pcm[]
			// 2) Compute how many frames remain until we hit the end of the buffer:
			size_t framesLeft = totalFrames - ph;             // e.g. if ph=20000, totalFrames=22050 => framesLeft=2050
			size_t firstChunkFrames = (std::min)(bufferFrames, framesLeft);
			// This is “how many frames we can mix without wrapping.”
			// “firstChunkFloats” = floats we can safely read before wrapping:
			size_t firstChunkFloats = firstChunkFrames * srcChannels;
			size_t dstIndex = 0;         // write offset in busBuf (in floats)
			size_t srcIndex = samplePosInFloats; // read offset in pcm[] (in floats)


			for (size_t f = 0; f < firstChunkFrames; ++f) {
				if (srcChannels == 1) {
					// Mono source: read one float, apply pan/gain, write to each dst channel
					float s = pcm[srcIndex + 0];
					busBuf[dstIndex + 0] += s * gL;  // left
					busBuf[dstIndex + 1] += s * gR;  // right
					srcIndex += 1;                      // advance source by 1 float
					dstIndex += dstChannels;            // advance dest by 2 floats
				}
				else {
					// Stereo source: read two floats, apply per-channel gain
					float sL = pcm[srcIndex + 0] * gL;
					float sR = pcm[srcIndex + 1] * gR;
					busBuf[dstIndex + 0] += sL;
					busBuf[dstIndex + 1] += sR;
					srcIndex += 2;     // advance source by 2 floats
					dstIndex += dstChannels; // usually +2
				}
			}

			// 4) Did we fill all bufferFrames already?
			if (firstChunkFrames == bufferFrames) {
				continue;  // on to the next voice
			}

			// 5) Otherwise, we still need to fill “remainingFrames” at the front of the buffer––only valid if looping
			size_t remainingFrames = bufferFrames - firstChunkFrames;  // e.g. 2048 − 2050 => negative? No, std::min prevents that
			if (!vs.loop) {
				continue;
			}


			// PHASE 2: wrap ph → 0 in source, then mix “remainingFrames” from start
			srcIndex = 0;  // now read from the very start of pcm[]
			for (size_t f = 0; f < remainingFrames; ++f) {
				if (srcChannels == 1) {
					float s = pcm[srcIndex + 0];
					busBuf[dstIndex + 0] += s * gL;
					busBuf[dstIndex + 1] += s * gR;
					srcIndex += 1;
					dstIndex += dstChannels;
				}
				else {
					float sL = pcm[srcIndex + 0] * gL;
					float sR = pcm[srcIndex + 1] * gR;
					busBuf[dstIndex + 0] += sL;
					busBuf[dstIndex + 1] += sR;
					srcIndex += 2;
					dstIndex += dstChannels;
				}
				// If we wrapped past the end of pcm[], clamp or wrap again:
				if (srcIndex >= bufferSize) {
					srcIndex %= bufferSize;
				}
			}
		}
		// 2c) Fold sub‐bus B into master
			for (int i = 0; i < bufferSize; ++i) {
				out[i] += busBuf[i] * bGain;
			}
		}

		// 3) If you have voices assigned directly to bus 0 (master), mix them here
		//    in exactly the same way. Or if bus 0 is always “parent only,” skip it.

		// 4) Update counters / atomics
		pendingFrames.fetch_add(frames, std::memory_order_relaxed);
		globalSampleCounter += frames;
		cbBlockIndex.fetch_add(1, std::memory_order_release);

		auto t1 = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::micro> elapsed = t1 - t0;
		double elapsedMicros = elapsed.count();  // duration in microseconds

		double timeBudget = double(frames) * 1'000'000.0 / deviceCfg->sampleRate;
		if (elapsedMicros > timeBudget) {
			LogMessage("RenderCallback OVERRUN: " + std::to_string(elapsedMicros) + "µs (budget: " + std::to_string(timeBudget) + "µs)", LogCategory::AudioManager, LogLevel::Warning);
		}
		else {
			LogMessage("RenderCallback: " + std::to_string(elapsedMicros) + "µs", LogCategory::AudioManager, LogLevel::Debug);
		}
		
		// 1 voice currently renders at around 2 µs on a 256 frames buffer
	}


	// around 10 times slower than current implementation.
	void AudioManager::RenderCallbackOld(float* out, int frames)
	{
		auto t0 = std::chrono::high_resolution_clock::now();

		auto t_sn0 = std::chrono::high_resolution_clock::now();
		// load snapshot
		int front = currentReadBuffer.load(std::memory_order_acquire);
		const Snapshot::Snapshot& s = gSnapshots[front];
		auto t_sn1 = std::chrono::high_resolution_clock::now();
		LogMessage("load snapshot time: " + std::to_string((t_sn1 - t_sn0).count()) + "ns", LogCategory::AudioManager, LogLevel::Debug);

		/*
		if (s.voiceCount == 0) {
			std::memset(out, 0, frames * deviceCfg->channels * sizeof(float));
			pendingFrames.fetch_add(frames, std::memory_order_relaxed);
			globalSampleCounter += frames;
			cbBlockIndex.fetch_add(1, std::memory_order_release);
			return;
		}
		*/



		uint64_t blockStart = globalSampleCounter;
		int samples = frames * deviceCfg->channels;
		auto t_fill0 = std::chrono::high_resolution_clock::now();

		// clear bus buffers
		for (uint32_t b = 0; b < s.busCount; ++b)
			std::memset(s.bus[b], 0, samples * sizeof(float));
		//std::fill(s.bus[b].begin(), s.bus[b].end(), 0.0f);

		auto t_fill1 = std::chrono::high_resolution_clock::now();
		LogMessage("Clear time: " + std::to_string((t_fill1 - t_fill0).count()) + "ns", LogCategory::AudioManager, LogLevel::Debug);


		auto t_mix0 = std::chrono::high_resolution_clock::now();
		// mix voices
		for (uint32_t v = 0; v < s.voiceCount; ++v)
		{
			const Snapshot::VoiceSnap& vs = s.voices[v];
			if (!vs.buf || vs.buf->Empty()) continue;


			const float* pcm = vs.buf->GetData();
			const int ch = vs.buf->GetChannelCount();       // source channels
			const size_t len = vs.buf->GetFrameCount();     // source frame count
			size_t pos = size_t(vs.playhead);               // current playhead

			const float gainL = vs.gain * vs.pan[0];        // pre-multiplied gain per channel
			const float gainR = vs.gain * vs.pan[1];

			float* busBuf = s.bus[vs.bus];

			//LogMessage("Voice #" + std::to_string(v) + ": frames=" + std::to_string(vs.buf->GetFrameCount()), LogCategory::AudioManager, LogLevel::Debug);

			if (ch == 1) {
				// Mono source → pan into stereo bus
				for (int i = 0; i < frames; ++i) {
					float sample = 0.f;
					if (pos < len)
						sample = pcm[pos];
					else if (vs.loop)
						sample = pcm[pos % len];  // still has % but only if looping
					else
						break;

					busBuf[i * 2 + 0] += sample * gainL;
					busBuf[i * 2 + 1] += sample * gainR;

					++pos;
				}
			}
			else if (ch == 2) {
				// Stereo source
				for (int i = 0; i < frames; ++i) {
					if (pos >= len && !vs.loop) break;

					size_t frameIndex = pos;
					if (frameIndex >= len && vs.loop)
						frameIndex %= len;

					float sL = pcm[frameIndex * 2 + 0] * gainL;
					float sR = pcm[frameIndex * 2 + 1] * gainR;

					busBuf[i * 2 + 0] += sL;
					busBuf[i * 2 + 1] += sR;

					++pos;
				}
			}
			else {
				// Multichannel fallback
				for (int i = 0; i < frames; ++i) {
					if (pos >= len && !vs.loop) break;

					size_t frameIndex = pos;
					if (frameIndex >= len && vs.loop)
						frameIndex %= len;

					for (uint32_t c = 0; c < deviceCfg->channels; ++c) {
						float s = pcm[frameIndex * ch + (c % ch)] * vs.gain * vs.pan[c];
						busBuf[i * deviceCfg->channels + c] += s;
					}

					++pos;
				}
			}
		}

		for (uint32_t b = 0; b < s.busCount; ++b) {
			float maxSample = 0;
			for (float f : s.bus[b])
				maxSample = (std::max)(maxSample, std::abs(f));
		}


		auto t_fold0 = std::chrono::high_resolution_clock::now();
		// fold busses onto master bus (master = buses[0])
		for (int b = int(s.busCount) - 1; b > 0; --b) {
			float gain = s.busGain[b];
			int parent = parent = s.busParent[b];
			float* src = s.bus[b];
			float* dst = s.bus[parent];
			for (int i = 0; i < samples; ++i)
				dst[i] += src[i] * gain;
		}
		auto t_fold1 = std::chrono::high_resolution_clock::now();
		LogMessage("bus fold time: " + std::to_string((t_fold1 - t_fold0).count()) + "ns", LogCategory::AudioManager, LogLevel::Debug);
		auto t_mix1 = std::chrono::high_resolution_clock::now();
		LogMessage("mix time: " + std::to_string((t_mix1 - t_mix0).count()) + "ns", LogCategory::AudioManager, LogLevel::Debug);


		// copy to device
		auto t_mem0 = std::chrono::high_resolution_clock::now();
		std::memcpy(out, s.bus[0], size_t(frames * deviceCfg->channels) * sizeof(float));
		auto t_mem1 = std::chrono::high_resolution_clock::now();
		LogMessage("Memcpy time: " + std::to_string((t_mem1 - t_mem0).count()) + "ns", LogCategory::AudioManager, LogLevel::Debug);
		// advance counters

		pendingFrames.fetch_add(frames, std::memory_order_relaxed);
		globalSampleCounter += frames; // this might be a little redundant
		cbBlockIndex.fetch_add(1, std::memory_order_release);




		auto t1 = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::micro> elapsed = t1 - t0;
		double elapsedMicros = elapsed.count();  // duration in microseconds





		double timeBudget = double(frames) * 1'000'000.0 / deviceCfg->sampleRate;
		if (elapsedMicros > timeBudget) {
			LogMessage("RenderCallback OVERRUN: " + std::to_string(elapsedMicros) + "µs (budget: " + std::to_string(timeBudget) + "µs)", LogCategory::AudioManager, LogLevel::Warning);
		}
		else {
			LogMessage("RenderCallback: " + std::to_string(elapsedMicros) + "µs", LogCategory::AudioManager, LogLevel::Debug);
		}
		LogMessage("VoiceCount: " + std::to_string(s.voiceCount), LogCategory::AudioManager, LogLevel::Debug);
		LogMessage("BusCount: " + std::to_string(s.busCount), LogCategory::AudioManager, LogLevel::Debug);
		LogMessage("Bus0.size: " + std::to_string(sizeof(s.bus[0])), LogCategory::AudioManager, LogLevel::Debug);


	}
#include "pch.h"
#include "AudioManager.hpp"
#include <variant>
#include <chrono>
#include <cstring>
#include <numbers>
#include "Entity.hpp"
#include "Voice.hpp"
#include "Bus.hpp"
#include "Snapshot.hpp"
#include "AudioDeviceMiniAudio.hpp"
#include "AudioDeviceUnity.hpp"
#include "AudioDeviceStub.hpp"
#include "AudioBufferManager.hpp"
#include "BehaviorLoader.hpp"
#include "AudioDevice.hpp"
#include "Log.hpp"
#include "SpeakerLayout.hpp"


namespace {
	Snapshot::Snapshot gSnapshots[Snapshot::kSnapCount];   // single, file-local definition
}

AudioManager::AudioManager(AudioConfig* deviceCfg, CommandQueue* inQueue, CommandQueue* outQueue)
	:deviceCfg(deviceCfg), inQueue(inQueue), outQueue(outQueue) {

	LogMessage("INIT", LogCategory::AudioManager, LogLevel::Debug);

	bufferManager = new AudioBufferManager(deviceCfg->sampleRate);
	LogMessage("... create audio device", LogCategory::AudioManager, LogLevel::Debug);

	// create audio device
	switch (deviceCfg->backend) {
	case AudioBackend::Miniaudio: {
		device = std::make_unique<AudioDeviceMiniaudio>(deviceCfg->channels, deviceCfg->sampleRate, deviceCfg->bufferFrames);
		break;
	}
	case AudioBackend::Unity: {
		device = std::make_unique<AudioDeviceUnity>(deviceCfg->channels, deviceCfg->sampleRate, deviceCfg->bufferFrames);
		break;
	}
	case AudioBackend::Stub: {
		device = std::make_unique<AudioDeviceStub>(deviceCfg->channels, deviceCfg->sampleRate, deviceCfg->bufferFrames);
		break;
	}
	default: {
		LogMessage("unknown audio device!", LogCategory::AudioManager, LogLevel::Warning);
		break;
	}
	}

	LogMessage("... create buffers", LogCategory::AudioManager, LogLevel::Debug);

	// size buffers
	uint32_t realFrames = device->GetBufferFrames();
	if (realFrames != deviceCfg->bufferFrames) {
		LogMessage("[AudioDevice] requested period " + std::to_string(deviceCfg->bufferFrames) +
			" got " + std::to_string(realFrames), LogCategory::AudioDevice, LogLevel::Info);
	}

	size_t bufSamples = size_t(realFrames * deviceCfg->channels);
	for (Snapshot::Snapshot& snap : gSnapshots)
		for (int b = 0; b < Snapshot::kMaxBuses; ++b)
			//snap.bus[b].resize(bufSamples, 0.0f);
			std::memset(snap.bus[b], 0, bufSamples * sizeof(float));
	LogMessage("... set callback", LogCategory::AudioManager, LogLevel::Debug);

	// set callback
	device->SetRenderCallback(
		[this](float* output, int frameCount) {RenderCallback(output, frameCount); });



	// TODO: make this more ... "dynamic"?
	if (deviceCfg->channels == 2)
		speakerLayout = SpeakerLayout::Stereo();
	else if (deviceCfg->channels == 6)
		speakerLayout = SpeakerLayout::FivePointOne();
	else {
		LogMessage("unknown channel format: " + std::to_string(deviceCfg->channels), LogCategory::AudioManager, LogLevel::Warning);
		speakerLayout = SpeakerLayout::Stereo();

	}




	LogMessage("... create master bus", LogCategory::AudioManager, LogLevel::Debug);

	// Create Master Bus at buses[0]
	buses.clear();
	buses.push_back({ std::vector<float>(realFrames * deviceCfg->channels),{} }); // master
	LogMessage("... Init Done.", LogCategory::AudioManager, LogLevel::Debug);

}


void AudioManager::ThreadMain() {
	// TODO: We still get stuck when quitting from unity, need to rethink how we get out of this loop!
	LogMessage("BEGINNING OF AUDIO THREAD", LogCategory::AudioManager, LogLevel::Info);
	LogMessage("Speakers: " + std::to_string(speakerLayout.speakers.size()), LogCategory::AudioManager, LogLevel::Debug);
	for (size_t i = 0; i < speakerLayout.speakers.size(); ++i) {
		const auto& dir = speakerLayout.speakers[i].direction;
		float mag = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
		if (mag < 0.001f) {
			LogMessage("Speaker[" + std::to_string(i) + "] has zero-length direction!", LogCategory::AudioManager, LogLevel::Error);
		}
	}

	if (speakerLayout.speakers.empty()) {
		LogMessage("ERROR: speakerLayout is empty in TakeSnapshot!", LogCategory::AudioManager, LogLevel::Error);
	}
	for (const auto& s : speakerLayout.speakers) {
		LogMessage("Speaker: " + s.name + " dir=(" +
			std::to_string(s.direction.x) + "," +
			std::to_string(s.direction.y) + "," +
			std::to_string(s.direction.z) + ")",
			LogCategory::AudioManager, LogLevel::Debug);
	}

	uint32_t lastSeen = 0; // block counter

	using Clock = std::chrono::steady_clock;
	auto start = Clock::now();

	while (!shouldQuit) {
		//if (speakerLayout.speakers.empty()) // <- this should not be possible!!
		//	continue;

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
	entities.clear();
	for (auto& snap : gSnapshots) {
		snap = Snapshot::Snapshot();
	}


}

void AudioManager::Update(float /*dt*/)
{
	Command cmd;

	// can we really do this without a lock? :thinking:
	while (inQueue->pop(cmd)) {

		/*LogMessage(
			"ProcessCommands: "
			+ cmd.GetTypeName() + "  (queue length: " + std::to_string(inQueue->Length()) + " / " + std::to_string(commandsToDrain) + ")",
			LogCategory::AudioManager, LogLevel::Debug);*/


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

	// todo: remove children of tags that no longer exist as well
	// eg: SetTag(entity, "Weapon.Model.Pistol.P2020");  should replace all other weapon.model.pistol.* tags

	std::vector<Tag>   finalAdds;           // what we'll actually apply
	std::unordered_map<std::string, std::unordered_set<std::string>> simulated;

	for (const Tag& add : newTags)          // original order matters
	{
		auto& sim = simulated[add.entity];
		for (auto it = sim.begin(); it != sim.end(); ) {
			if (Tag::conflicts(*it, add.tag))
				it = sim.erase(it);
			else
				++it;
		}
		sim.insert(add.tag);                   // pretend we've now added it
		finalAdds.push_back(add);                 // keep the add (may get trimmed later)
	}

	std::vector<Tag> finalRemoves;          // clear & rebuild

	for (const auto& [entity, after] : simulated)                 // one entity at a time
	{
		const auto& current = entities[entity].GetTags();                    // live tag set

		// 2-a  prune obsolete current-tags
		for (const std::string& cur : current.GetAllTags())
			for (const std::string& add : after)
				if (Tag::conflicts(cur, add))
					finalRemoves.push_back({ entity, cur });

	}


	std::vector<Tag> trimmedAdds;
	for (const Tag& add : finalAdds)
	{
		const auto& after = simulated[add.entity];
		if (std::find(after.begin(), after.end(), add.tag) != after.end())
			trimmedAdds.push_back(add);           // still present in the simulated end-state
	}
	//  replace newTags / removedTags
	newTags = std::move(trimmedAdds);
	removedTags = std::move(finalRemoves);










	std::vector<std::string> entitiesWithAddedTags;
	for (auto& e : newTags) {
		entitiesWithAddedTags.push_back(e.entity);
	}

	std::vector<std::string> entitiesWithRemovedTags;
	for (auto& e : removedTags) {
		entitiesWithRemovedTags.push_back(e.entity);
	}



	// TODO: consider using std::vector<TagDelta> tagsForEntity; // Pre-filter the tags for the current entity once
	// so that we only check tags that are relevant for this specific entity.


	//LogMessage("Adding Behaviors", LogCategory::Entity, LogLevel::Debug);
	for (auto& e : entitiesWithAddedTags) {
		///LogMessage("updating entity: " + e, LogCategory::Entity, LogLevel::Debug);

		auto& entity = entities[e];
		TagMap tmp;
		for (auto t : entity.tags.GetAllTags())
			tmp.AddTag(t);
		for (auto& t : newTags) {
			LogMessage("new tag: " + t.tag, LogCategory::Entity, LogLevel::Debug);

			if (t.entity == e)
			{
				LogMessage("new tag for entity: " + t.tag, LogCategory::Entity, LogLevel::Debug);
				tmp.AddTag(t.tag);

			}
		}
		for (auto& t : newTags) {
			if (t.entity != e) continue;
			MatchUtils::FindMatchingBehaviorsForTag(t.tag, exactMatchMap, wildcardMatchers, [&](BehaviorDef* b) {
				//LogMessage("callback", LogCategory::Entity, LogLevel::Debug);
				auto score = MatchUtils::MatchScore(*b, tmp, globalTags, entity.values, globalValues);
				//LogMessage("score: " + b->name + ": " + std::to_string(score), LogCategory::Entity, LogLevel::Debug);

				if (score > 0) {
					entity.activeBehaviors.emplace_back(ActiveBehavior::Create(b, 0)); // TODO get from factory instead of creating
					LogMessage("e: " + entity.id + " // starting new behavior: " + b->name + " // score: " + std::to_string(score), LogCategory::Entity, LogLevel::Debug);
				}
			});
		}
	}


	//LogMessage("Removing Behaviors", LogCategory::Entity, LogLevel::Debug);
	for (auto& e : entitiesWithRemovedTags) {
		auto& entity = entities[e];
		TagMap tmp;
		for (auto& t : entity.tags.GetAllTags())
			tmp.AddTag(t);
		for (auto& t : removedTags)
			if (t.entity == e)
				tmp.RemoveTag(t.tag);

		for (auto& t : removedTags) {
			if (t.entity != e) continue;
			MatchUtils::FindMatchingBehaviorsForTag(t.tag, exactMatchMap, wildcardMatchers, [&](BehaviorDef* b) {
				for (auto& ab : entity.activeBehaviors) {
					if (ab.GetDefinition() == b) {

						int score = MatchUtils::MatchScore(*b, tmp, globalTags, entity.values, globalValues);
						if (score <= 0) {
							if (ab.GetPhase() != ActiveBehavior::Phase::Active)
								continue;
							entity.TransitionToPhase(ab, ActiveBehavior::Phase::Ending, deviceCfg, bufferManager);
							LogMessage("stopping behavior: " + ab.GetDefinition()->name, LogCategory::Entity, LogLevel::Debug);
						}
						// break;
					}
				}
				});
		}
	}


	// update tagmaps
	for (auto& t : newTags) {
		if (t.transient)continue;
		entities[t.entity].tags.AddTag(t.tag);
	}
	for (auto& t : removedTags) {
		entities[t.entity].tags.RemoveTag(t.tag);
	}


	newTags.clear();
	removedTags.clear();


	// update entities
	for (auto& entityValue : entities) {
		entityValue.second.Update(definitions, globalTags, globalValues, deviceCfg, bufferManager);
	}
	// todo: remove entities that don't have any activebehaviors here?


	
}

void AudioManager::Shutdown() {
	shouldQuit.store(true, std::memory_order_release);
	cbBlockIndex.fetch_add(1, std::memory_order_release);     // Force the spin to observe a new block
	// stops the looping thread function
	// what do we actually need to do here?
	// maybe unload buffers / cleanup voices? 
	// we should do most required things already on destruction, but ... can a thing selfdestruct??
}

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
	Quat listenerRot;
	if (!currentListener.empty()) {
		auto it = entities.find(currentListener);
		if (it != entities.end()) {
			it->second.GetValues().TryGetValue("position", listenerPos);
			it->second.GetValues().TryGetValue("rotation", listenerRot);
		}
	}


	// flatten voices
	for (auto& [eid, ed] : entities)
	{
		for (auto& inst : ed.GetBehaviors()) {
			for (auto& v : inst.GetVoices()) {
				if (back.voiceCount >= Snapshot::kMaxVoices)
					continue;
				Vec3 srcPos;
				Quat srcRotation = quaternion::Identity<float>();
				bool isSpatial = ed.GetValues().TryGetValue("position", srcPos);

				float att = 1.f;
				std::vector<float> panMask;

				if (isSpatial) {
					Vec3 d = Vec3::subtract(srcPos, listenerPos);
					float dist = d.magnitude();
					float radius = 20.0f; // fallback if not set
					inst.GetDefinition()->TryFindKey("radius", ed.GetValues(), radius); // use value from definition
					ed.GetValues().TryGetValue("radius", radius); // override if set on entity
					att = std::clamp(1.f - dist / radius, 0.f, 1.f);
					panMask = ComputePanMask(srcPos, listenerPos, listenerRot, speakerLayout);
					for (auto& gain : panMask)
						gain *= att; // apply attenuation per channel
				}
				else {
					// Non-spatial: full volume on all channels
					panMask.resize(speakerLayout.speakers.size(), 1.f);
				}


				back.voices[back.voiceCount++] = Snapshot::VoiceSnap{
					v.buffer,
					v.playhead,
					v.currentVol * att,
					v.loop,
					uint8_t(v.busIndex),
					v.startSample,
					std::move(panMask),
					&v
				};
			}
		}
	}
	// after filling all snap.voices[0..voiceCount-1]
	for (uint32_t v = 0; v < back.voiceCount; ++v) {
		uint8_t B = back.voices[v].bus;          // which bus this voice targets
		uint32_t idx = back.numVoicesInBus[B]++;  // grab next free slot
		back.voicesByBus[B][idx] = v;
	}


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
	int newIndex = (int)buses.size();
	uint32_t frames = device->GetBufferFrames();
	buses.push_back({ std::vector<float>(frames * deviceCfg->channels), {} });
	entityBus.emplace(entityId, newIndex);

	//LogMessage("[BUS] added new bus for entity: " + entityId, LogCategory::AudioManager, LogLevel::Debug);
	return newIndex;
}


void AudioManager::SetTag(Command cmd) {
	auto* tag = std::get_if<std::string>(&cmd.value);
	newTags.push_back(Tag{ cmd.entityId,*tag, false });




	std::string entityId = cmd.entityId;
	//auto* tag = std::get_if<std::string>(&cmd.value);
	auto& entity = entities[entityId]; // create if missing
	entity.SetBus(GetOrCreateBus(entityId));
	//entity.SetTag(*tag);

	//// TODO: rethink listeners!
	if (*tag == "listener")
		currentListener = cmd.entityId;

	//LogMessage("Tag set: e: " + entityId + " t: " + *tag, LogCategory::AudioManager, LogLevel::Debug);
}
void AudioManager::SetTransient(Command cmd) {
	auto* tag = std::get_if<std::string>(&cmd.value);
	newTags.push_back(Tag{ cmd.entityId,*tag,true });

	/*std::string entityId = cmd.entityId;
	auto* tag = std::get_if<std::string>(&cmd.value);
	auto& entity = entities[entityId];
	entity.SetTransientTag(*tag);
	LogMessage("Transient Tag set: e: " + entityId + " t: " + *tag, LogCategory::AudioManager, LogLevel::Debug);*/
}
void AudioManager::ClearTag(Command cmd) {
	auto* tag = std::get_if<std::string>(&cmd.value);
	removedTags.push_back(Tag{ cmd.entityId,*tag,false });




	/*std::string entityId = cmd.entityId;
	auto* tag = std::get_if<std::string>(&cmd.value);
	auto& entity = entities[entityId];
	entity.ClearTag(*tag);

	if (*tag == "listener")
		currentListener = "";*/
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
	
	
	exactMatchMap.clear();
	wildcardMatchers.clear();
	for (auto& b : definitions) {
		for (const auto& tag : b.matchTags) {
			if (tag.ends_with("*")) {
				std::string prefix = tag.substr(0, tag.size() - 1); // strip '*'
				wildcardMatchers.emplace_back(prefix, &b);
			}
			else {
				exactMatchMap[tag].push_back(&b);
			}
		}
	}


}
void AudioManager::SetAssetPath(Command cmd)
{
	const auto* path = std::get_if<std::string>(&cmd.value);
	bufferManager->SetAssetpath(*path);
}
void AudioManager::ClearEntity(Command cmd) {
	auto it = entities.find(cmd.entityId);
	if (it == entities.end()) {
		return;
	}

	it->second.Clear();
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
			float radius = 20.0f; // fallback if not set
			ab.GetDefinition()->TryFindKey("radius", e.second.GetValues(), radius); // use value from definition
			e.second.GetValues().TryGetValue("radius", radius); // override if set on entity
			LogMessage("    - " + ab.Name(), LogCategory::AudioManager, LogLevel::Debug);
			LogMessage("		- radius: " + std::to_string(radius), LogCategory::AudioManager, LogLevel::Debug);


		}
	}
}


void AudioManager::RenderCallback(float* out, int frames) {
	if (shouldQuit)
		return;
	auto t0 = std::chrono::high_resolution_clock::now();
	const auto& s = gSnapshots[currentReadBuffer.load()];
	const int outputSampleCount = frames * deviceCfg->channels;


	// 1) Zero the master once
	std::memset(out, 0, outputSampleCount * sizeof(float));

	// 2) Iterate each sub-bus B = 1..(busCount-1)
	for (uint32_t B = 1; B < s.busCount; ++B) {
		float* busBuf = s.bus[B];
		float  bGain = s.busGain[B];

		// 2a) Zero this bus buffer
		std::memset(busBuf, 0, outputSampleCount * sizeof(float));

		// 2b) Mix each voice in bus B
		uint32_t nVoices = s.numVoicesInBus[B];
		
		for (uint32_t vi = 0; vi < nVoices; ++vi) {
			uint32_t		vIndex = s.voicesByBus[B][vi];        // which entry in s.voices[]
			const auto& vs = s.voices[vIndex];
			const float* pcm = vs.buf->GetData();
			size_t			ph = vs.voice->playhead;               // in frames

			size_t totalFrames = vs.buf->GetFrameCount();   // e.g. 4096
			size_t bufferFrames = frames;                              // how many frames to render now

			int srcChannels = vs.buf->GetChannelCount();
			int dstChannels = deviceCfg->channels;


			// 1) If ph is already out of range, handle that first:
			if (ph >= totalFrames) {
				if (vs.loop) {
					ph %= totalFrames;
				}
				else {
					// Non-looping voice is already done before we start.
					continue;
				}
			}

			size_t samplePosInFloats = ph * srcChannels;  // starting float index into pcm[]
			size_t framesLeft = totalFrames - ph;             // e.g. if ph=20000, totalFrames=22050 => framesLeft=2050
			size_t firstChunkFrames = (std::min)(bufferFrames, framesLeft);
			size_t dstIndex = 0;         // write offset in busBuf (in floats)
			size_t srcIndex = samplePosInFloats; // read offset in pcm[] (in floats)

			float invSrcChannels = 1.0f / float(srcChannels);
			for (size_t f = 0; f < firstChunkFrames; ++f) {
				// Step 1: Read input frame (1 to N floats)
				const float* frame = &pcm[srcIndex];

				// Step 2: For each output channel, accumulate signal
				for (int c = 0; c < dstChannels; ++c) {
					float mix = 0.f;
					for (int sc = 0; sc < srcChannels; ++sc) {
						mix += frame[sc];
					}
					mix *= invSrcChannels;
					busBuf[dstIndex + c] += mix * vs.gain * vs.panMask[c];
				}

				srcIndex += srcChannels;
				dstIndex += dstChannels;
			}

			// 4) Did we fill all bufferFrames already?
			if (firstChunkFrames == bufferFrames) {
				continue;  // on to the next voice
			}

			size_t remainingFrames = bufferFrames - firstChunkFrames;
			if (!vs.loop) {
				continue;
			}


			// Phase 2: wrap to the start of the source buffer and fill the remainder.
			srcIndex = 0;  // now read from the very start of pcm[]
			for (size_t f = 0; f < remainingFrames; ++f) {
				// Step 1: Read input frame (1 to N floats)
				const float* frame = &pcm[srcIndex];

				// Step 2: For each output channel, accumulate signal
				for (int c = 0; c < dstChannels; ++c) {
					float mix = 0.f;
					for (int sc = 0; sc < srcChannels; ++sc) {
						mix += frame[sc];
					}
					mix *= invSrcChannels;
					busBuf[dstIndex + c] += mix * vs.gain * vs.panMask[c];
				}

				srcIndex += srcChannels;
				dstIndex += dstChannels;
			}
		}
		// 2c) Fold sub-bus B into master.
		for (int i = 0; i < outputSampleCount; ++i) {
			out[i] += busBuf[i] * bGain;
		}
	}

	pendingFrames.fetch_add(frames, std::memory_order_relaxed);
	globalSampleCounter += frames;
	cbBlockIndex.fetch_add(1, std::memory_order_release);

	(void)t0;







	AdvancePlayheads();
}



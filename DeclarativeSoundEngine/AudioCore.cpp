// AudioCore.cpp
#include "pch.h"
#include "AudioCore.hpp"
#include "Log.hpp"
#include "IBehaviorDefinition.hpp"
#include <iostream>
#include "AudioDeviceMiniAudio.hpp"
#include <algorithm>
#include <array>  
#include "LeafBuilder.hpp"

const double M_PI = 3.14159265358979323846;
namespace {
	Snapshot gSnapshots[kSnapCount];   // single, file-local definition
}
void BehaviorInstance::Update(const ValueMap& params, AudioBufferManager* bufMgr, uint8_t busIdx, float dt, uint64_t nowSamples)
{
	//if (!onActive)
	//	return;

	constexpr float FADE_SEC = 0.05f;          // 50-ms fade
	float stepFactor = (std::min)(dt / FADE_SEC, 1.0f);  // dt is control-tick

	std::vector<LeafBuilder::Leaf> desired;
	
	LeafBuilder::BuildLeaves(onActive.get(), params, 0, false, busIdx, desired, deviceCfg, bufMgr); // TODO: Startsample!?
	//CollectLeaves(*onActive, params, 1.0f, busIdx, desired, bufMgr);

	// start new
	for (auto& leaf : desired) {
		if (!leaf.buffer) continue; // failed load, skip
		if (!HasVoice(leaf.src))
			StartVoice(leaf, busIdx, nowSamples, params);
	}

	// TODO: Update targetVol of already playing voices (contained in leafs)
	

	// fade-out missing
	for (auto& v : voices)
		if (std::none_of(desired.begin(), desired.end(),
			[&](const LeafBuilder::Leaf& l) {
				return l.src == v.source;
			}))
			v.targetVol = 0.0f;
			

	// 2) ramp
	for (Voice& v : voices) {
		float diff = v.targetVol - v.currentVol;
		v.currentVol += diff * stepFactor;           // 0 → 1 fade-in, 1 → 0 fade-out

		std::cout << "currentVolume: " + std::to_string(v.currentVol) << std::endl;
	}
	// 3) prune
	auto endIt = std::remove_if(voices.begin(), voices.end(),
		[&](const Voice& v)
		{
			bool done = !v.buffer || !v.loop && v.playhead >= v.buffer->GetFrameCount();
			bool silent = (v.currentVol < 0.001f && v.targetVol == 0.f);
			return done || silent;
		});
	voices.erase(endIt, voices.end());
}


// a little helper to overload lambdas
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;
static void PrintValue(const Value& v) {
	std::visit(overloaded{
		[](float f) { std::cerr << f; },
		[](const std::string& s) { std::cerr << '"' << s << '"'; },
		[](const Vec3& w) { std::cerr
							   << "{" << w.x << ", " << w.y << ", " << w.z << "}";
						  }
		}, v);
}
// dumps an entire map
static void DumpValueMap(const ValueMap& m) {
	std::cerr << "-----VALUEMAP------------\n";
	for (auto const& [key, val] : m.GetAllValues()) {
		std::cerr << "  [" << key << "] = ";
		PrintValue(val);
		std::cerr << "\n";
	}
	std::cerr << "-------------------------\n";
}

/*
void BehaviorInstance::CollectLeaves(const Node& n,
	const ValueMap& params,
	float					inGain,
	uint8_t					bus,
	std::vector<SoundLeaf>& out,
	AudioBufferManager* bufMgr,
	bool loop)
{
	std::cout << "CollectLeaves: " + std::to_string(loop) << std::endl;

	float gain = inGain * n.volume.eval(params);

	switch (n.type)
	{
	case NodeType::Sound: {
		const SoundNode& sn = static_cast<const SoundNode&>(n);
		AudioBuffer* buf;

		if (!bufMgr->TryLoad(sn.sound, buf)) {

			return;          // silent fail
		}
		std::cout << "sound node: " + std::to_string(loop) + " - gain: "+std::to_string(gain) << std::endl;
		out.push_back({ &sn, buf, gain, loop, bus });
		return;
	}

	case NodeType::Parallel:
		for (auto& c : n.children)
			CollectLeaves(*c, params, gain, bus, out, bufMgr, loop);
		return;

	case NodeType::Random: {
		auto& rn = static_cast<const RandomNode&>(n);
		size_t idx = rn.pickOnce();               // RandomNode remembers choice
		std::cout << "rng picked: " << idx << std::endl;
		std::cout << "len children: " << rn.children.size() << std::endl;
		CollectLeaves(*rn.children[idx], params, gain, bus, out, bufMgr, loop);
		return;
	}




	case NodeType::Blend: {
		const BlendNode& bn = static_cast<const BlendNode&>(n);

		float val = 0;
		// always get a float (defaults to 0.0 if missing)
		float x = 0.f;
		params.TryGetValue(bn.parameter, x);

		auto w = bn.weights(x);

		std::cout << std::to_string(w[0].second) + " / " + std::to_string(w[1].second) << std::endl;

		if (w[0].first) CollectLeaves(*w[0].first, params, gain * w[0].second, bus, out, bufMgr, loop);
		if (w[1].first) CollectLeaves(*w[1].first, params, gain * w[1].second, bus, out, bufMgr, loop);

		return;
	}

	case NodeType::Select: {
		auto const& sn = static_cast<const SelectNode&>(n);
		std::cerr << "[SelectNode] looking for parameter "
			<< sn.parameter << "\n";

		//DumpValueMap(params);
		float   fv = 0.f;
		std::string sval;
		// first try string (in case someone stored a literal)
		if (params.TryGetValue(sn.parameter, sval)			// otherwise try the float overload
			|| (params.TryGetValue(sn.parameter, fv)
				&& (sval = std::to_string(fv), true)))
		{
			std::cerr << "[SelectNode] matched value " << sval << "\n";
			if (auto child = sn.pick(sval))
			{
				std::cout << child << std::endl;
				CollectLeaves(*child, params, gain, bus, out, bufMgr, loop);
			}
		}
		else {
			std::cerr << "[SelectNode] no entry for "
				<< sn.parameter << "\n";
		}
		return;
	}
	case NodeType::Loop: {
		auto& rn = static_cast<const LoopNode&>(n);
		std::cout << "LOOP: len children: " << rn.children.size() << std::endl;
		CollectLeaves(*rn.children[0], params, gain, bus, out, bufMgr, true); // loopnodes always have exactly one child!
		return;
	}


	default: return;
	}
}
*/


AudioCore::AudioCore(
	IBehaviorDefinition* defsProvider,
	CommandQueue* fromManager,
	CommandQueue* toManager,
	std::unique_ptr<AudioDevice> audioDevice,
	AudioConfig* cfg)
	: defsProvider(defsProvider), inQueue(fromManager), outQueue(toManager), device(std::move(audioDevice))
{
	deviceCfg = cfg;

	audioBufferManager = new AudioBufferManager();

	//device = std::move(audioDevice); //std::make_unique<AudioDeviceMiniaudio>(outputChannels, sampleRate, bufferFrames);

	// size buffers
	size_t bufSamples = size_t(deviceCfg->bufferFrames * deviceCfg->channels);

	for (Snapshot& snap : gSnapshots)
		for (int b = 0; b < kMaxBuses; ++b)
			snap.bus[b].resize(bufSamples, 0.0f);


	device->SetRenderCallback(
		[this](float* output, int frameCount) {RenderCallback(output, frameCount); });




	// Create Master Bus
	buses.clear();
	buses.push_back({ std::vector<float>(deviceCfg->bufferFrames * deviceCfg->channels), {} }); // master

}
AudioCore::~AudioCore() {
	LogMessage("~AudioCore", LogCategory::AudioCore, LogLevel::Debug);
	delete audioBufferManager;
}




void AudioCore::Update() {
	// this function will loop indefinitely in the future! 
	auto now = std::chrono::steady_clock::now();
	float dt = std::chrono::duration<float>(now - lastUpdate).count();
	lastUpdate = now;

	AdvancePlayheads();
	ProcessCommands();
	ProcessActiveSounds(dt);
	TakeSnapshot();

}

void AudioCore::AdvancePlayheads()
{
	uint32_t step = pendingFrames.exchange(0, std::memory_order_acquire);
	if (step == 0) return;

	for (auto& [eid, ed] : entityMap)
		for (auto& inst : ed.instances)
			for (auto& v : inst->voices) {
				if (!v.buffer) continue;
				if (v.buffer->Empty()) continue; // TODO: buffer not ready. but this would offset our start, right? not sure how to properly handle this tbh...?
				size_t len = v.buffer->GetFrameCount();
				if (v.loop)
					v.playhead = (v.playhead + step) % len;
				else
					v.playhead = (std::min)(v.playhead + step, len);
			}
}


// build a fresh read‐only snapshot for render
void AudioCore::TakeSnapshot()
{
	Snapshot& back = gSnapshots[gBuild];

	/* ---- reset counters (no vector clear) ---- */
	back.voiceCount = 0;
	back.busCount = uint32_t(buses.size());

	/* ---- bus gains ---- */

	for (uint32_t i = 0; i < back.busCount; ++i) {
		back.busGain[i] = buses[i].volume.eval(entityMap.empty() ? ValueMap{} : entityMap.begin()->second.params);
		back.busParent.resize(back.busCount); // maybe fixed size in the future (some max bus count)
		back.busParent[i] = buses[i].parent;
	}

	// --- read listener position (fallback to origin) ---
	Vec3 listenerPos{ 0,0,0 };
	if (!currentListener.empty()) {
		auto it = entityMap.find(currentListener);
		if (it != entityMap.end()) {
			it->second.params.TryGetValue("position", listenerPos);
		}
	}

	back.listenerPosition = listenerPos;


	/* ---- flatten voices ---- */
	for (auto& [eid, ed] : entityMap)


		for (auto& inst : ed.instances) {
			for (auto& v : inst->voices) {
				if (back.voiceCount >= kMaxVoices) continue;
				Vec3 srcPos;
				bool isSpatial = ed.params.TryGetValue("position", srcPos);
				float att = 1.f;
				float panL = 1.f, panR = 1.f;
				std::vector<float> pan;
				if (isSpatial) {

					float dx = srcPos.x - listenerPos.x;
					float dy = srcPos.y - listenerPos.y;
					float dz = srcPos.z - listenerPos.z;

					float dist = std::sqrt(dx * dx + dz * dz + dy * dy);
					float radius = 10.0f; // fallthrough default value
					ed.params.TryGetValue("radius", radius); // TODO check if radius>0!
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

				back.voices[back.voiceCount++] = VoiceSnap {
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
	gFront.store(gBuild, std::memory_order_release);
	gBuild = (gBuild + 1) % kSnapCount;


	//LogMessage("Created Snapshot", LogCategory::AudioCore, LogLevel::Debug);
}

void AudioCore::ProcessCommands() {
	Command cmd;
	while (inQueue->pop(cmd)) {
		LogMessage("ProcessCommands: cmd: " + cmd.GetTypeName() + "  (queue length : " + std::to_string(inQueue->Length()) + ")", LogCategory::AudioCore, LogLevel::Debug);

		switch (cmd.type) {
		case CommandType::StopBehavior:			HandleStopBehavior(cmd); break;
		case CommandType::StartBehavior:		HandleStartBehavior(cmd); break;
		case CommandType::ValueUpdate:			HandleValueUpdate(cmd); break;
		case CommandType::RefreshDefinitions:	RefreshDefinitions(); break;
		case CommandType::BusGainUpdate:        HandleBusGain(cmd);break;
		case CommandType::AssetPath:			SetAssetPath(cmd);break;
		case CommandType::SetListener:			currentListener = cmd.entityId; break;
		case CommandType::RemoveListener:
			if (currentListener == cmd.entityId)
				currentListener.clear();
			break;
		default: break;
		}
	}


}

void AudioCore::DebugDumpEntityMap() {
	LogMessage("DEBUG ENTITYMAP:",
		LogCategory::AudioCore, LogLevel::Debug);
	for (auto& [eid, ed] : entityMap) {
		LogMessage("Entity '" + eid + "' — params:" +
			std::to_string(ed.params.GetAllValues().size()) +
			" instances:" + std::to_string(ed.instances.size()),
			LogCategory::AudioCore, LogLevel::Debug);

		for (auto& inst : ed.instances) {
			LogMessage("  inst id=" + std::to_string(inst->id) +
				" phase=" + std::to_string(int(inst->phase)) +
				" voices=" + std::to_string(inst->voices.size()),
				LogCategory::AudioCore, LogLevel::Debug);
		}
	}
	LogMessage("-----", LogCategory::AudioCore, LogLevel::Debug);
}

void AudioCore::RefreshDefinitions() {
	for (auto& p : defsProvider->GetPlayDefs()) {
		prototypes[p.id] = std::move(p);
	}
	LogMessage("Audiocore definitions refreshed!", LogCategory::AudioCore, LogLevel::Debug);
}


void AudioCore::HandleStartBehavior(const Command& cmd)
{

	// 0) locate prototype
	auto p = prototypes.find(cmd.behaviorId);
	if (p == prototypes.end()) {
		LogMessage("StartBehavior: unknown PlayDefinition id "
			+ std::to_string(cmd.behaviorId),
			LogCategory::AudioCore, LogLevel::Warning);
		return;

	}

	LogMessage("AudioCore: StartBehavior id=" + (p->second.name), LogCategory::AudioCore, LogLevel::Info);


	// 1) ensure entity slot
	auto& entity = entityMap[cmd.entityId];   // auto-creates

	// 2) build fresh instance
	auto inst = behaviorFactory.create();
	inst->deviceCfg = deviceCfg;

	inst->id = p->second.id;
	inst->phase = BehaviorInstance::Phase::Start;

	inst->onStart = p->second.onStart ? p->second.onStart->clone() : nullptr;
	inst->onActive = p->second.onActive ? p->second.onActive->clone() : nullptr;
	inst->onEnd = p->second.onEnd ? p->second.onEnd->clone() : nullptr;

	inst->paramExpr = p->second.parameters;    // root-level expressions

	// 3) add to entity
	entity.instances.push_back(std::move(inst));

	// 4) no voice creation here; ProcessActiveSounds() will do it
}
PlayDefinition* AudioCore::GetPrototype(uint32_t behaviorId) {
	// 0) locate prototype
	auto p = prototypes.find(behaviorId);
	if (p == prototypes.end()) {
		LogMessage("StartBehavior: unknown PlayDefinition id "
			+ std::to_string(behaviorId),
			LogCategory::AudioCore, LogLevel::Warning);
		return nullptr;

	}

	return &p->second;
}
std::string AudioCore::GetBehaviorName(uint32_t behaviorId) {
	auto pd = GetPrototype(behaviorId);

	if (pd)return pd->name;
	return "unknown PlayDefinition for " + std::to_string(behaviorId);
}


void AudioCore::HandleStopBehavior(const Command& cmd) {


	LogMessage("AudioCore: StopBehavior called for " + GetBehaviorName(cmd.behaviorId), LogCategory::AudioCore, LogLevel::Info);

	for (auto& inst : entityMap[cmd.entityId].instances) {
		if (inst->id == cmd.behaviorId) {
			inst->phase = BehaviorInstance::Phase::Ending;
		}
	}



}


void AudioCore::HandleValueUpdate(const Command& cmd) {
	std::string val = "";
	if (auto s = std::get_if<std::string>(&cmd.value))
		val = *s;
	else if (auto s = std::get_if<float>(&cmd.value))
		val = std::to_string(*s);
	else if (auto s = std::get_if<Vec3>(&cmd.value))
		val = "( " + std::to_string(s->x) + " / " + std::to_string(s->y) + " / " + std::to_string(s->z) + " )";

	LogMessage("AudioCore: ValueUpdate " + cmd.key + " = " + val, LogCategory::AudioCore, LogLevel::Info);

	auto& entity = entityMap[cmd.entityId];
	entity.params.SetValue(cmd.key, cmd.value);

}

void AudioCore::SetAssetPath(const Command& cmd)
{
	if (auto s = std::get_if<std::string>(&cmd.value)) {
		audioBufferManager->SetAssetpath(*s);
	}
}

void AudioCore::ProcessActiveSounds(float dt)
{
	std::vector<BehaviorInstance*> sheduledDeletion;
	for (auto& [eid, data] : entityMap) {
		for (auto instPtr = data.instances.begin(); instPtr != data.instances.end(); )
		{
			auto inst = *instPtr; // DOES THIS MAKE SENSE?
			switch (inst->phase)
			{
			case  BehaviorInstance::Phase::Start:
			{
				LogMessage("START", LogCategory::AudioCore, LogLevel::Debug);
				//LogMessage("start", LogCategory::AudioCore, LogLevel::Debug);
				//LogMessage(std::to_string(inst->onStart == nullptr), LogCategory::AudioCore, LogLevel::Debug);
				//LogMessage(std::to_string(inst->onActive == nullptr), LogCategory::AudioCore, LogLevel::Debug);
				//LogMessage(std::to_string(inst->onEnd == nullptr), LogCategory::AudioCore, LogLevel::Debug);
				if (inst->voices.empty() && inst->onStart) {

					std::vector<LeafBuilder::Leaf> tmp;
					//inst->CollectLeaves(*inst->onStart, data.params, 1.0f,GetOrCreateBus(eid), tmp, audioBufferManager);
					LeafBuilder::BuildLeaves(inst->onStart.get(), data.params, 0, false, GetOrCreateBus(eid), tmp, deviceCfg, audioBufferManager);	// startsample==0??
					for (auto& leaf : tmp) 
						inst->StartVoice(leaf, leaf.bus, globalSampleCounter, data.params);

					LogMessage("start: started " + std::to_string(tmp.size()) + " leafs", LogCategory::AudioCore, LogLevel::Debug);
				}


				//inst->Update(data.params, audioBufferManager, GetOrCreateBus(eid), dt, globalSampleCounter);


				if (inst->voices.empty() || AllFinished(inst->voices))
				{
					LogMessage("Transition to Active State: " + inst->id, LogCategory::AudioCore, LogLevel::Debug);
					inst->phase = BehaviorInstance::Phase::Active;
				}
				// Active voices will be created next CollectLeaves() pass

			}
			break;

			case  BehaviorInstance::Phase::Active:
				// start OnActiveLeafs
				if (inst->voices.size() > 0)
					LogMessage("ACTIVE: " + inst->voices[0].loop ? "true" : "false", LogCategory::AudioCore, LogLevel::Debug);
				if (inst->voices.empty() && inst->onActive) {

					std::vector<LeafBuilder::Leaf> tmp;
					//inst->CollectLeaves(*inst->onActive, data.params, 1.0f, GetOrCreateBus(eid), tmp, audioBufferManager);
					LeafBuilder::BuildLeaves(inst->onActive.get(), data.params, 0, false, GetOrCreateBus(eid), tmp, deviceCfg, audioBufferManager);
					for (auto& leaf : tmp) 
						inst->StartVoice(leaf, leaf.bus, globalSampleCounter, data.params);

					LogMessage("ACTIVE: started " + std::to_string(tmp.size()) + " leafs", LogCategory::AudioCore, LogLevel::Debug);
				}



				// voices updated/diffed by CollectLeaves() 
				inst->Update(data.params, audioBufferManager, GetOrCreateBus(eid), dt, globalSampleCounter);

				// if active is oneshot, or no active sounds are assigned
				// TODO: is this sufficient?

				if (inst->voices.empty() || AllFinished(inst->voices))
				{
					inst->phase = BehaviorInstance::Phase::Ending;
					LogMessage("Transition to Ended State: " + inst->id, LogCategory::AudioCore, LogLevel::Debug);
				}

				break;

			case  BehaviorInstance::Phase::Ending:
				// TODO:	
				// - stop all sounds that are still playing
				// - start events from onEnd
				// - only after that, wait for allfinished

				LogMessage("ending", LogCategory::AudioCore, LogLevel::Debug);

				//inst->Update(data.params, audioBufferManager, GetOrCreateBus(eid), dt, globalSampleCounter);


				if (AllFinished(inst->voices)) {

					LogMessage("BehaviorInstance Removed:  eid: " + eid + " / instance: " + GetBehaviorName(inst->id) //std::to_string(inst->id)
						, LogCategory::AudioCore, LogLevel::Debug);
					sheduledDeletion.push_back(inst);
					instPtr = data.instances.erase(instPtr);     // remove instance from entity when done

					behaviorFactory.destroy(inst);				// return instance to factory
					continue;									// we removed an entry, so don't increment
				}
				break;
			}
			++instPtr;
		}
	}
}

// Helper: find the first SoundNode in a node-tree
SoundNode* AudioCore::FindFirstSoundNode(Node* node) {
	if (!node)
		return nullptr;
	if (auto* sn = dynamic_cast<SoundNode*>(node)) {
		return sn;
	}
	for (auto& childPtr : node->children) {
		if (auto* found = FindFirstSoundNode(childPtr.get())) {
			return found;
		}
	}
	return nullptr;
}



// MIXING


int AudioCore::GetOrCreateBus(const std::string& entityId) {
	auto it = entityBus.find(entityId);
	if (it != entityBus.end()) return it->second;
	// allocate new sub‐bus
	int newIndex = (int)buses.size();
	buses.push_back({ std::vector<float>(deviceCfg->bufferFrames * deviceCfg->channels), {} });
	entityBus.emplace(entityId, newIndex);
	return newIndex;
}

void AudioCore::ClearBusBuffers() {
	for (auto& bus : buses) {
		std::fill(bus.buffer.begin(), bus.buffer.end(), 0.0f);
	}
}

void AudioCore::HandleBusGain(const Command& cmd)
{
	int idx = GetOrCreateBus(cmd.entityId);
	buses[idx].volume = Expression(std::get<std::string>(cmd.value)); // literal "0.8" or "distance*0.5"
}

void AudioCore::RenderCallback(float* out, int frames)
{
	/* ------- pull immutable snapshot ------- */
	const Snapshot& s = gSnapshots[gFront.load(std::memory_order_acquire)];

	uint64_t blockStart = globalSampleCounter;
	int samples = frames * deviceCfg->channels;      // inside RenderCallback


	/* ------- clear bus buffers in the snapshot ------- */
	for (uint32_t b = 0; b < s.busCount; ++b)
		std::fill(s.bus[b].begin(), s.bus[b].end(), 0.0f);

	/* ------- mix voices ------- */
	for (uint32_t v = 0; v < s.voiceCount; ++v)
	{
		const VoiceSnap& vs = s.voices[v];

		// causes crash with delay node... :thinking:
		if (!vs.buf || vs.buf->Empty())  continue; // buffer not loaded...?

		const float* pcm = vs.buf->GetData();
		int          ch = vs.buf->GetChannelCount();
		size_t       len = vs.buf->GetFrameCount();

		uint64_t basePos = blockStart - vs.startSample;
		float* busBuf = s.bus[vs.bus].data();



		for (int i = 0; i < frames; ++i) {
			uint64_t pos = basePos + i;
			float s0 = 0.f;

			if (pos < len)               s0 = pcm[pos * ch] * vs.gain;
			else if (vs.loop)            s0 = pcm[(pos % len) * ch] * vs.gain;

			for (uint32_t c = 0; c < deviceCfg->channels; ++c)
				busBuf[i * deviceCfg->channels + c] += s0 * vs.pan[c]; // TODO: ChannelMatrix
		}
	}

	/* ------- fold buses (same math, but now inside snapshot) ------- */
	for (int b = int(s.busCount) - 1; b > 0; --b) {
		float gain = s.busGain[b];
		//int    parent = buses[b].parent;           // parent index hasn’t changed
		int parent = parent = s.busParent[b];
		float* src = s.bus[b].data();
		float* dst = s.bus[parent].data();

		for (int i = 0; i < samples; ++i)
			dst[i] += src[i] * gain;
	}

	/* ------- copy master to device ------- */
	std::memcpy(out, s.bus[0].data(), size_t(frames * deviceCfg->channels) * sizeof(float));

	/* ------- advance counters ------- */
	pendingFrames.fetch_add(frames, std::memory_order_relaxed);
	globalSampleCounter += frames;
}
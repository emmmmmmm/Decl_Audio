// activebehavior.hpp
#pragma once
#include "Node.hpp"
#include "BehaviorDef.hpp"
#include "Voice.hpp"
#include "LeafBuilder.hpp"

class ActiveBehavior
{
public:
	enum class Phase { Init, Start, Active, Ending, Finished };
	std::shared_ptr<Node>	currentNodeGraph; // or per-phase, as needed

private:
	std::vector<Voice>		voices;
	uint32_t				instanceId = {};
	Phase					phase{ Phase::Init };
	const BehaviorDef*		definition;
	uint64_t				startSample;

public:
	ActiveBehavior(const BehaviorDef* def, uint64_t startSample)
		: definition(def), phase(Phase::Init), startSample(startSample){ }

	const Phase GetPhase()				const { return phase; }
	const void SetPhase(Phase newPhase)	  { phase=newPhase; }
	const BehaviorDef* GetDefinition()	const { return definition; }
	
	bool HasVoice(const SoundNode* src) const;
	std::vector<Voice>& GetVoices() { return voices; }
	Voice* FindVoiceForLeaf(const LeafBuilder::Leaf& leaf);
	Voice* AddVoice(Voice&& v);
	std::string Name() const				 { return GetDefinition()->name; }

	void StopAllVoices();		// TODO
	void RemoveFinishedVoices();// TODO

	static ActiveBehavior* Create(const BehaviorDef* def, uint64_t startSample)
	{
		return new ActiveBehavior(def, startSample);
	}
	
	// concept from ObjectFactory
	void reset() {
		instanceId = {};
		phase = Phase::Start;
		definition = {};
		voices = {};
	}

	bool AllVoicesFinished() const
	{
		return std::all_of(voices.begin(), voices.end(), VoiceFinished);
	}



private:



	/*Utility*/
	static inline bool VoiceFinished(const Voice& v)
	{
		if (!v.buffer)return true; //!??
		return !v.loop && v.playhead >= v.buffer->GetFrameCount();
	}

	
};
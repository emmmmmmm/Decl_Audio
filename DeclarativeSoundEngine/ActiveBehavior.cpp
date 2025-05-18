#include "pch.h"
#include "ActiveBehavior.hpp"
#include "LeafBuilder.hpp"



bool ActiveBehavior::HasVoice(const SoundNode* src) const
{
	return std::any_of(voices.begin(), voices.end(),
		[&](const Voice& v) { return v.source == src; });
}

Voice* ActiveBehavior::FindVoiceForLeaf(const LeafBuilder::Leaf& leaf) {
	for (auto& v : voices) {
		if (v.buffer == leaf.buffer
			&& v.source == leaf.src
			// this should be sufficient, but could add more fields like busIndex, pitch, etc.
			) {
			return &v;
		}
	}
	return nullptr;
}

Voice* ActiveBehavior::AddVoice(Voice&& v) {
	voices.push_back(std::move(v));
	return &voices.back();
}

void ActiveBehavior::StopAllVoices()
{
	voices.clear(); // actually, I think that's all we need to do here? (unless we want to fade them out somehow, then we should mark them as "ending" somehow I guess?
}

void ActiveBehavior::RemoveFinishedVoices()
{
	// this might work? // TODO
	voices.erase(
		std::remove_if(
			voices.begin(),
			voices.end(),
			[]( Voice& v) { return v.Finished(); }
		),
		voices.end()
	);
}


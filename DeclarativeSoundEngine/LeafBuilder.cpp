// LeafBuilder.cpp
#include "pch.h"
#include "LeafBuilder.hpp"
#include "AudioBuffer.hpp"
#include "Expression.hpp"
#include "ValueMap.hpp"
#include <stdexcept>
#include "AudioBufferManager.hpp"
#include "AudioDevice.hpp"
#include "Log.hpp"

namespace LeafBuilder {

	static uint64_t SamplesFromSeconds(const Expression& expr, const ValueMap& params, uint64_t sampleRate) {
		float seconds = expr.eval(params);
		return (uint64_t) seconds * sampleRate; 
	}

	// Compute total node duration in samples (for Sequence offsetting)
	static uint64_t ComputeDuration(const Node* node, const ValueMap& params, uint64_t sampleRate, AudioBufferManager* bufferManager) {
		switch (node->type) {

		case NodeType::Sound: { 
			auto* sn = static_cast<const SoundNode*>(node);
			AudioBuffer* buf;
			if (bufferManager->TryGet(sn->sound, buf)) {
				/*LogMessage("[ComputeDuration] buffer length: "
					+ std::to_string(static_cast<uint64_t>(buf->GetFrameCount())) + " samples", LogCategory::Leaf, LogLevel::Debug);*/
				return static_cast<uint64_t>(buf->GetFrameCount());
			}
			LogMessage("[ComputeDuration] could not get bufferlength", LogCategory::Leaf, LogLevel::Warning);
			return 0;
			//return buf ? static_cast<double>(buf->GetFrameCount()) : 0.0;
		}
		case NodeType::Delay: {
			auto* dn = static_cast<const DelayNode*>(node);
			return SamplesFromSeconds(dn->delayExpr, params, sampleRate);
		}
		case NodeType::Sequence: {
			uint64_t sum = 0;
			for (auto& child : node->children) {
				sum += ComputeDuration(child.get(), params, sampleRate, bufferManager);
			}
			//LogMessage("[ComputeDuration] sequence duration: " + std::to_string(sum) + " samples", LogCategory::Leaf, LogLevel::Debug);
			return sum;
		}
		case NodeType::Parallel: {
			uint64_t maxDur = 0;
			for (auto& child : node->children) {
				uint64_t d = ComputeDuration(child.get(), params, sampleRate, bufferManager);
				if (d > maxDur) maxDur = d;
			}
			//LogMessage("[ComputeDuration] parallel duration: " + std::to_string(maxDur) +" samples", LogCategory::Leaf, LogLevel::Debug);
			return maxDur;
		}
		case NodeType::Random:
		case NodeType::Blend:
		case NodeType::Select: {
			// worst-case: longest child
			uint64_t maxDur = 0;
			for (auto& child : node->children) {
				uint64_t d = ComputeDuration(child.get(), params, sampleRate, bufferManager);
				if (d > maxDur) maxDur = d;
			}
			return maxDur;
		}
		case NodeType::Loop: {
			// loop is infinite
			return std::numeric_limits<uint64_t>::infinity();
		}
		case NodeType::Reference: {
			auto* rn = static_cast<const ReferenceNode*>(node);
			if (rn->target) return ComputeDuration(rn->target, params, sampleRate, bufferManager);
			return 0;
		}
		default:
			LogMessage("[ComputeDuration]: Unknown node type", LogCategory::Leaf, LogLevel::Warning);
			return 0;
		}
	}


	void BuildLeaves(const Node* node,
		const ValueMap& params,
		uint64_t startSample,
		bool inheritedLoop,
		int bus,
		std::vector<Leaf>& out,
		AudioConfig* audioDeviceCfg,
		AudioBufferManager* bufferManager) {
		BuildLeaves(node, params, startSample, inheritedLoop, {}, {}, bus, out, audioDeviceCfg, bufferManager);
	}

	static void BuildLeaves(const Node* node, const ValueMap& params, uint64_t startSample,
		bool inheritedLoop, std::vector<Expression> inheritedVols, std::vector<Expression> inheritedPitches, int bus,
		std::vector<Leaf>& out, AudioConfig* audioDeviceCfg, AudioBufferManager* bufferManager) {
		if (node == nullptr)
			return; // node not assigned


		inheritedVols.push_back(node->volume);
		inheritedPitches.push_back(node->pitch);



		switch (node->type) {

		case NodeType::Sound: {
			auto* sn = static_cast<const SoundNode*>(node);
			AudioBuffer* buf;
			if (!bufferManager->TryLoad(sn->sound, buf))
			{
				LogMessage("Missing audio buffer: " + sn->sound, LogCategory::Leaf, LogLevel::Warning);
				break;
			}
			uint64_t dur =buf->GetFrameCount();

			float finalVol = 1.0f;
			for (auto& e : inheritedVols)
			{
				finalVol *= e.eval(params);
					}

			float finalPitch = 1.0f;
			for (auto& e : inheritedPitches)
				finalPitch *= e.eval(params);


			out.push_back({ sn, buf, startSample, dur, inheritedLoop, bus , inheritedVols, inheritedPitches });
			break;
		}

		case NodeType::Delay: {
			auto* dn = static_cast<const DelayNode*>(node);
			uint64_t dur = SamplesFromSeconds(dn->delayExpr, params, audioDeviceCfg->sampleRate);
			out.push_back({ nullptr, nullptr, startSample, dur, inheritedLoop, bus , inheritedVols, inheritedPitches });
			break;
		}

		case NodeType::Sequence: {
			uint64_t offset = startSample;
			for (auto& child : node->children) {
				//LogMessage("sequence offset: " + std::to_string(offset), LogCategory::Leaf, LogLevel::Trace);
				BuildLeaves(child.get(), params, offset, inheritedLoop, inheritedVols, inheritedPitches, bus, out, audioDeviceCfg, bufferManager);
				// compute child duration to update offset
				offset += ComputeDuration(child.get(), params, audioDeviceCfg->sampleRate, bufferManager);
			}
			break;
		}

		case NodeType::Parallel:
			for (auto& child : node->children) {
				BuildLeaves(child.get(), params, startSample, inheritedLoop, inheritedVols, inheritedPitches, bus, out, audioDeviceCfg, bufferManager);
			}
			break;

		case NodeType::Random: {
			auto* rn = static_cast<const RandomNode*>(node);
			size_t idx = rn->pickOnce();
			BuildLeaves(rn->children[idx].get(), params, startSample, inheritedLoop, inheritedVols, inheritedPitches, bus, out, audioDeviceCfg, bufferManager);
			break;
		}


		case NodeType::Blend: {
			auto* bn = static_cast<const BlendNode*>(node);
			float x = 0;
			params.TryGetValue(bn->parameter, x);
			auto weights = bn->weights(x);
			for (auto& weight : weights) {
				if (weight.first && weight.second > 0) {
					// capture current leaf count
					size_t baseIndex = out.size();
					// generate leaves for this branch
					BuildLeaves(weight.first, params, startSample, inheritedLoop, inheritedVols, inheritedPitches, bus, out, audioDeviceCfg, bufferManager);

					// scale volume on all newly added leaves
					for (size_t i = baseIndex; i < out.size(); ++i) {
						out[i].volExprs.push_back(Expression(std::to_string(weight.second))); // todo: check if this works as expected

					}
				}
			}
			break;


		}

		case NodeType::Select: {
			auto* sn = static_cast<const SelectNode*>(node);
			std::string val = "";
			params.TryGetValue(sn->parameter, val);
			const Node* sel = sn->pick(val);
			if (sel) BuildLeaves(sel, params, startSample, inheritedLoop, inheritedVols, inheritedPitches, bus, out, audioDeviceCfg, bufferManager);
			break;
		}

		case NodeType::Loop: {
			auto* ln = static_cast<const LoopNode*>(node);
			BuildLeaves(ln->getChild(), params, startSample, true, inheritedVols, inheritedPitches, bus, out, audioDeviceCfg, bufferManager);
			break;
		}

		case NodeType::Reference: {
			auto* rn = static_cast<const ReferenceNode*>(node);
			if (rn->target) BuildLeaves(rn->target, params, startSample, inheritedLoop, inheritedVols, inheritedPitches, bus, out, audioDeviceCfg, bufferManager);
			break;
		}

		default:
			// unknown node type -> ignore
			break;
		}
	}

} // namespace LeafBuilder

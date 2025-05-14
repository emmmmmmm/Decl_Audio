// LeafBuilder.cpp
#include "pch.h"
#include "LeafBuilder.hpp"
#include "AudioBuffer.hpp"
#include "Expression.hpp"
#include "ValueMap.hpp"
#include <stdexcept>
#include "SoundManagerAPI.hpp"
#include "AudioBufferManager.hpp"

namespace LeafBuilder {

	static double SamplesFromSeconds(const Expression& expr, const ValueMap& params, double sampleRate) {
		float seconds = expr.eval(params);
		return seconds * sampleRate;
	}

	// Compute total node duration in samples (for Sequence offsetting)
	static double ComputeDuration(const Node* node, const ValueMap& params, double sampleRate, AudioBufferManager* bufferManager) {
		switch (node->type) {

		case NodeType::Sound: { // TODO: Somethings weird here with the buffers...! 
			auto* sn = static_cast<const SoundNode*>(node);
			AudioBuffer* buf;
			if (bufferManager->TryGet(sn->sound, buf)) {
				std::cout << "[ComputeDuration] buffer length: " << std::to_string(static_cast<double>(buf->GetFrameCount())) << std::endl;
				return static_cast<double>(buf->GetFrameCount());
			}
			std::cout << "[ComputeDuration] could not get bufferlength" << std::endl;
			return 0;
			//return buf ? static_cast<double>(buf->GetFrameCount()) : 0.0;
		}
		case NodeType::Delay: {
			auto* dn = static_cast<const DelayNode*>(node);
			return SamplesFromSeconds(dn->delayExpr, params, sampleRate);
		}
		case NodeType::Sequence: {
			double sum = 0.0;
			for (auto& child : node->children) {
				sum += ComputeDuration(child.get(), params, sampleRate, bufferManager);
			}
			std::cout << "[ComputeDuration] sequence duration: " << std::to_string(sum) << std::endl;
			return sum;
		}
		case NodeType::Parallel: {
			double maxDur = 0.0;
			for (auto& child : node->children) {
				double d = ComputeDuration(child.get(), params, sampleRate, bufferManager);
				if (d > maxDur) maxDur = d;
			}
			std::cout << "[ComputeDuration] parallel duration: " << std::to_string(maxDur) << std::endl;
			return maxDur;
		}
		case NodeType::Random:
		case NodeType::Blend:
		case NodeType::Select: {
			// worst-case: longest child
			double maxDur = 0.0;
			for (auto& child : node->children) {
				double d = ComputeDuration(child.get(), params, sampleRate, bufferManager);
				if (d > maxDur) maxDur = d;
			}
			return maxDur;
		}
		case NodeType::Loop: {
			// loop is infinite
			return std::numeric_limits<double>::infinity();
		}
		case NodeType::Reference: {
			auto* rn = static_cast<const ReferenceNode*>(node);
			if (rn->target) return ComputeDuration(rn->target, params, sampleRate, bufferManager);
			return 0.0;
		}
		default:
			std::cout << "ComputeDuration: Unknown node type" << std::endl;
			return 0.0;
		}
	}



	void BuildLeaves(const Node* node,
		const ValueMap& params,
		double startSample,
		bool inheritedLoop,
		int bus,
		std::vector<Leaf>& out,
		AudioConfig* audioDeviceCfg,
		AudioBufferManager* bufferManager) {
		if (node == nullptr)
			return; // node not assigned

		switch (node->type) {

		case NodeType::Sound: {
			auto* sn = static_cast<const SoundNode*>(node);
			AudioBuffer* buf;
			if (!bufferManager->TryLoad(sn->sound, buf)) 
			//const AudioBuffer* buf = AudioBuffer::Get(sn->sound); // TODO: USE BUFFERMANAGER!! ... *SOMEHOW*
			//if (!buf) 
			{
				std::cerr << "Missing audio buffer: " + sn->sound << std::endl;
				break;
			}
			double dur = static_cast<double>(buf->GetFrameCount());
			float vol = node->volume.eval(params);
			float pitch = node->pitch.eval(params);
			out.push_back({sn, buf, startSample, dur, vol, pitch, inheritedLoop, bus });
			break;
		}

		case NodeType::Delay: {
			auto* dn = static_cast<const DelayNode*>(node);
			double dur = SamplesFromSeconds(dn->delayExpr, params, audioDeviceCfg->sampleRate);
			out.push_back({nullptr, nullptr, startSample, dur, 1.0f, 1.0f, inheritedLoop, bus });
			break;
		}

		case NodeType::Sequence: {
			double offset = startSample;
			for (auto& child : node->children) {
				std::cout << "sequence offset: " + std::to_string(offset) << std::endl;
				BuildLeaves(child.get(), params, offset, inheritedLoop, bus, out, audioDeviceCfg, bufferManager);
				// compute child duration to update offset
				offset += ComputeDuration(child.get(), params, audioDeviceCfg->sampleRate, bufferManager);
			}
			break;
		}

		case NodeType::Parallel:
			for (auto& child : node->children) {
				BuildLeaves(child.get(), params, startSample, inheritedLoop, bus, out, audioDeviceCfg, bufferManager);
			}
			break;

		case NodeType::Random: {
			auto* rn = static_cast<const RandomNode*>(node);
			size_t idx = rn->pickOnce();
			BuildLeaves(rn->children[idx].get(), params, startSample, inheritedLoop, bus, out, audioDeviceCfg, bufferManager);
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
					BuildLeaves(weight.first, params, startSample, inheritedLoop, bus, out, audioDeviceCfg, bufferManager);

					// scale volume on all newly added leaves
					for (size_t i = baseIndex; i < out.size(); ++i) {
						out[i].volume *= weight.second;

						std::cout << "POST WEIGHT: " << std::to_string(weight.second) << " : " << out[i].volume << std::endl;
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
			if (sel) BuildLeaves(sel, params, startSample, inheritedLoop, bus, out, audioDeviceCfg, bufferManager);
			break;
		}

		case NodeType::Loop: {
			auto* ln = static_cast<const LoopNode*>(node);
			BuildLeaves(ln->getChild(), params, startSample, true, bus, out,audioDeviceCfg, bufferManager);
			break;
		}

		case NodeType::Reference: {
			auto* rn = static_cast<const ReferenceNode*>(node);
			if (rn->target) BuildLeaves(rn->target, params, startSample, inheritedLoop, bus, out, audioDeviceCfg, bufferManager);
			break;
		}

		default:
			// unknown node type -> ignore
			break;
		}
	}

} // namespace LeafBuilder

// ParserUtils.cpp
#include "pch.h"
#include "ParserUtils.hpp"
#include "Node.hpp"
#include <stdexcept>

namespace ParserUtils {

	std::string ExtractCoreKey(const YAML::Node& mapNode) {
		for (auto it = mapNode.begin(); it != mapNode.end(); ++it) {
			std::string key = it->first.Scalar();
			if (key == "sound" || key == "delay" || key == "random" || key == "sequence" ||
				key == "blend" || key == "select" || key == "loop" || key == "parallel") {
				return key;
			}
		}
		std::cout << "UNKNOWN NODETYPE: \n" << mapNode << std::endl;
		throw std::runtime_error("Unknown node type in map: ");
	}

	ModifierMap ExtractModifiers(const YAML::Node& mapNode) {
		ModifierMap mods;
		if (mapNode["volume"])
		{
			mods.volume = mapNode["volume"].as<std::string>();

			//std::cout << "MODMAP: " << mods.volume.value() << std::endl;
		}
		if (mapNode["pitch"])    mods.pitch = mapNode["pitch"].as<std::string>();
		if (mapNode["loop"])	 mods.loop = true;



		return mods;
	}

	YAML::Node ExtractChildren(const YAML::Node& mapNode) {
		// 1) if it’s a one-entry map whose value is itself a map, unwrap it
		if (mapNode.IsMap() && mapNode.size() == 1) {
			auto it = mapNode.begin();
			if (it->second.IsMap()) {
				return ExtractChildren(it->second);
			}
		}

		// 2) explicit known container keys
		if (mapNode["nodes"]) return mapNode["nodes"];
		if (mapNode["sounds"]) return mapNode["sounds"];
		if (mapNode["blends"]) return mapNode["blends"];
		if (mapNode["cases"]) return mapNode["cases"];

		// 3) fallback: first sequence child anywhere
		for (auto it = mapNode.begin(); it != mapNode.end(); ++it) {
			if (it->second.IsSequence())
				return it->second;
		}

		return YAML::Node();
	}
	Node* NormalizeLoops(Node* root) {
		if (!root)return nullptr;
		if (auto loopNode = dynamic_cast<LoopNode*>(root)) {
			if (auto inner = dynamic_cast<LoopNode*>(loopNode->getChild())) {
				return NormalizeLoops(inner);
			}
		}
		for (auto& child : root->getChildren())
			NormalizeLoops(child.get());
		return root;
	}


	Node* ParseNode(const YAML::Node& yamlNode, Context& ctx) {
		Node* node = nullptr;
		ModifierMap mods;

		/*std::cerr << "[ParseNode] entry; "
			<< "IsNull=" << yamlNode.IsNull()
			<< ", IsScalar=" << yamlNode.IsScalar()
			<< ", IsSequence=" << yamlNode.IsSequence()
			<< ", IsMap=" << yamlNode.IsMap()
			<< "\n"
			<< yamlNode << "\n";*/

		if (yamlNode.IsScalar()) {
			// simple string → Sound or Reference
			std::string val = yamlNode.as<std::string>();
			auto it = ctx.definitions.find(val);
			if (it != ctx.definitions.end()) {
				auto ref = new ReferenceNode(val);
				ctx.unresolvedRefs.emplace_back(ref, val);
				node = ref;
			}
			else {
				node = new SoundNode(val);
			}
		}
		else if (yamlNode.IsSequence()) {
			auto seq = new SequenceNode();
			for (const auto& child : yamlNode)
				seq->addChild(ParseNode(child, ctx));
			node = seq;
		}
		else if (yamlNode.IsMap()) {
			// Find which kind it is (sound, delay, random, blend, select, …)

			std::string key = ExtractCoreKey(yamlNode);
			// 2) peel off the inner map/sequence
			YAML::Node def = yamlNode[key];

			YAML::Node children;
			if (def.IsMap()) {
				mods = ExtractModifiers(def);
				children = ExtractChildren(def);
			}
			else {
				// def is a sequence or scalar → it *is* the children
				children = def;
			}

			if (key == "sound" || key == "delay") {
				// both store a single string
				auto valNode = yamlNode[key];
				if (!valNode || !valNode.IsScalar())
					throw std::runtime_error(key + " node missing scalar value");
				if (key == "sound")
					node = new SoundNode(valNode.as<std::string>());
				else
					node = new DelayNode(valNode.as<std::string>());

			}
			else if (key == "random") {
				auto rnd = new RandomNode();
				if (children && children.IsSequence())
					for (const auto& c : children)
						rnd->addChild(ParseNode(c, ctx));
				node = rnd;
			}
			else if (key == "sequence") {
				auto seq = new SequenceNode();
				if (children && children.IsSequence())
					for (const auto& c : children)
						seq->addChild(ParseNode(c, ctx));
				node = seq;
			}
			else if (key == "parallel") {
				auto par = new ParallelNode();
				if (children && children.IsSequence())
					for (const auto& c : children)
						par->addChild(ParseNode(c, ctx));
				node = par;
			}
			else if (key == "blend") {
				// 1) grab the 'blend' sub-map
				auto sub = yamlNode["blend"];
				if (!sub || !sub.IsMap())
					throw std::runtime_error("blend node not a map");

				// 2) read parameter from the sub-map
				auto paramN = sub["parameter"];
				if (!paramN || !paramN.IsScalar())
					throw std::runtime_error("blend missing parameter");
				auto bn = new BlendNode();
				bn->parameter = paramN.as<std::string>();

				// 3) iterate cases, but recurse only on the inner sound
				auto listN = sub["blends"];
				if (listN && listN.IsSequence()) {
					for (auto const& item : listN) {
						float at = item["at"].as<float>();
						YAML::Node soundN = item["sound"];
						if (!soundN)
							throw std::runtime_error("blend case missing sound");
						Node* child = ParseNode(soundN, ctx);
						bn->addCase(at, child);
					}
				}
				node = bn;
			}
			else if (key == "select") {
				auto sub = yamlNode["select"];
				if (!sub || !sub.IsMap())
					throw std::runtime_error("select node not a map");

				// read the “parameter”
				auto paramN = sub["parameter"];
				if (!paramN.IsScalar())
					throw std::runtime_error("select missing parameter");
				auto sn = new SelectNode();
				sn->parameter = paramN.as<std::string>();

				// drill into “cases” and only recurse on the sound child
				auto listN = sub["cases"];
				if (listN && listN.IsSequence()) {
					for (auto const& item : listN) {
						// either use item["pattern"] or item["at"] depending on your YAML
						std::string pat = item["at"].as<std::string>();
						auto soundN = item["sound"];
						if (!soundN)
							throw std::runtime_error("select case missing sound");
						Node* child = ParseNode(soundN, ctx);
						sn->addCase(pat, child);
					}
				}
				node = sn;
			}

			else if (key == "loop") {
				auto sub = yamlNode["loop"];
				if (!sub) throw std::runtime_error("loop missing child");
				auto child = std::unique_ptr<Node>(ParseNode(sub, ctx));
				node = new LoopNode(std::move(child));
			}
			else {
				throw std::runtime_error("Unhandled coreKey: " + key);
			}



		}

		else {
			throw std::runtime_error("Invalid YAML node type");
		}
		// apply modifiers
		if (mods.volume)
		{
			node->setVolume(*mods.volume);
		}
		if (mods.pitch)
		{
			node->setPitch(*mods.pitch);
		}
		// insn't this somewhat redundant?
		if (mods.loop) {
			std::unique_ptr<Node> owned(node);
			auto loopNode = std::make_unique<LoopNode>(std::move(owned));
			node = loopNode.release();
		}



		return NormalizeLoops(node);
	}


} // namespace ParserUtils

#pragma once
#include <string>
#include <vector>
#include <memory>
#include "Expression.hpp"
#include <random>
#include "MatchUtils.hpp"
#include <array>    
#include <utility> 


struct Node;
using BlendWeights = std::array<std::pair<const Node*, float>, 2>;


enum class NodeType { Root, Sound, Layer, Random, Blend, Select };

struct Node {
	NodeType type = NodeType::Root;
	std::vector<std::unique_ptr<Node>> children;
	Expression		   volume{ "1.0" };
	float pitch = 1, fadeIn = 0, fadeOut = 0, delay = 0;
	bool loop = false;

	virtual ~Node() = default;

	virtual std::unique_ptr<Node> clone() const {
		auto n = std::make_unique<Node>();
		copyCommonFields(*n);
		for (auto& c : children) {
			n->children.push_back(c->clone());
		}
		return n;
	}

protected:
	void copyCommonFields(Node& dst) const {
		dst.type = type;
		dst.volume = volume;
		dst.pitch = pitch;
		dst.fadeIn = fadeIn;
		dst.fadeOut = fadeOut;
		dst.delay = delay;
		dst.loop = loop;
	}
};


struct SoundNode : Node {
	std::string sound;
	SoundNode() { type = NodeType::Sound; }

	std::unique_ptr<Node> clone() const override {
		auto n = std::make_unique<SoundNode>();
		copyCommonFields(*n);
		n->sound = sound;
		return n;
	}
};

struct LayerNode : Node {
	LayerNode() { type = NodeType::Layer; }
	std::unique_ptr<Node> clone() const override {
		auto n = std::make_unique<LayerNode>();
		copyCommonFields(*n);
		for (auto& c : children) n->children.push_back(c->clone());
		return n;
	}
};



struct RandomNode : Node {
	mutable int choice = -1;
	RandomNode() { type = NodeType::Random; }
	std::unique_ptr<Node> clone() const override {
		auto n = std::make_unique<RandomNode>();
		copyCommonFields(*n);
		for (auto& c : children) n->children.push_back(c->clone());
		n->choice = choice;
		return n;
	}
	static std::mt19937& Rng() {      // one engine per process
		static std::mt19937 rng{ std::random_device{}() };
		return rng;
	}

	size_t pickOnce() const {
		if (choice < 0 && !children.empty()) {
			std::uniform_int_distribution<size_t> d(0, children.size() - 1);
			choice = static_cast<int>(d(Rng()));
		}
		return static_cast<size_t>(choice < 0 ? 0 : choice);
	}
};





struct BlendNode : Node {
	std::string parameter;
	struct Point { float at; std::unique_ptr<Node> node; };
	std::vector<Point> blends;
	BlendNode() { type = NodeType::Blend; }

	std::unique_ptr<Node> clone() const override {
		auto n = std::make_unique<BlendNode>();
		copyCommonFields(*n);
		n->parameter = parameter;
		for (auto& p : blends) {
			Point q{ p.at, p.node->clone() };
			n->blends.push_back(std::move(q));
		}
		for (auto& c : children) {
			n->children.push_back(c->clone());
		}
		return n;
	}

	BlendWeights weights(float x) const
	{
		if (blends.empty())
			return BlendWeights{ { {nullptr, 0.f}, {nullptr, 0.f} } };

		if (x <= blends.front().at)
			return BlendWeights{ { {blends.front().node.get(), 1.f},
								   {nullptr, 0.f} } };

		if (x >= blends.back().at)
			return BlendWeights{ { {blends.back().node.get(), 1.f},
								   {nullptr, 0.f} } };

		for (size_t i = 0; i + 1 < blends.size(); ++i) {
			const auto& a = blends[i];
			const auto& b = blends[i + 1];
			if (x >= a.at && x < b.at) {
				float t = (x - a.at) / (b.at - a.at);
				return BlendWeights{ { {a.node.get(), 1.f - t},
									   {b.node.get(), t} } };
			}
		}
		return BlendWeights{ { {nullptr, 0.f}, {nullptr, 0.f} } };
	}

};






/*
select:
  parameter: "state"        # any key you pass in ValueUpdate or Tag string
  cases:
	idle:        { soundName: idle.wav }
	walk:        { soundName: walk_loop.wav }
	run:         { soundName: run_loop.wav }
	monster_*:   { random: { sounds: [growl1.wav, growl2.wav] } }
  default:       { soundName: default.wav }

  */
struct SelectNode : Node {
	std::string parameter;
	struct Option { std::string pattern; std::unique_ptr<Node> node; };
	std::vector<Option> options;
	std::unique_ptr<Node> defaultNode;

	std::unique_ptr<Node> clone() const override {
		auto n = std::make_unique<SelectNode>();
		copyCommonFields(*n);
		n->parameter = parameter;
		for (auto& o : options)
			n->options.push_back({ o.pattern, o.node->clone() });
		if (defaultNode) n->defaultNode = defaultNode->clone();
		return n;
	}

	const Node* pick(const std::string& value) const {
		for (auto& o : options)
			if (PatternMatch(o.pattern, value))           // glob or regex util
				return o.node.get();
		return defaultNode.get();
	}

};


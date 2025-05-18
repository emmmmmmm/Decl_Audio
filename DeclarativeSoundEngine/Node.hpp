#pragma once
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <array>
#include <random>
#include <typeinfo>
#include <iostream>
#include "Expression.hpp"
#include "Log.hpp"
#include "MatchUtils.hpp"
struct Node;
using BlendWeights = std::array<std::pair<const Node*, float>, 2>;

// Enumeration of all node types
enum class NodeType {
	Root,
	Sound,
	Sequence,
	Parallel,
	Delay,
	Random,
	Blend,
	Select,
	Loop,
	Reference
};

// Base node class
struct Node {
	NodeType type = NodeType::Root;
	std::vector<std::unique_ptr<Node>> children;
	Expression volume{ "1.0" };
	Expression pitch{ "1.0" };
	// fadeIn, fadeOut, delay as modifiers on nodes
	float fadeIn = 0.0f;
	float fadeOut = 0.0f;
	// position/radius omitted for brevity
	virtual ~Node() = default;

	virtual std::unique_ptr<Node> clone() const {
		auto n = std::make_unique<Node>();
		copyCommonFields(*n);
		for (auto& c : children) n->children.push_back(c->clone());
		return n;
	}

	void setVolume(std::string v) {
		volume = Expression(v);
	}
	void setPitch(std::string p) {
		pitch = Expression(p);
	}
	void addChild(Node* child) {
		children.push_back(std::unique_ptr<Node>(child));
	}
	const std::vector<std::unique_ptr<Node>>& getChildren() const {
		return children;
	}

	virtual void PrintChildren() {
		std::cerr << "Node: "<< typeid(*this).name() << std::endl;
		for (auto& c : children)
			c->PrintChildren();
	}

protected:
	void copyCommonFields(Node& dst) const {
		dst.type = type;
		dst.volume = volume;
		dst.pitch = pitch;
		dst.fadeIn = fadeIn;
		dst.fadeOut = fadeOut;
	}
};

// Plays a single sound file
struct SoundNode : Node {
	std::string sound;
	SoundNode(const std::string& file = "") { type = NodeType::Sound; sound = file; }
	std::unique_ptr<Node> clone() const override {
		auto n = std::make_unique<SoundNode>(sound);
		copyCommonFields(*n);
		return n;
	}
	void PrintChildren() override {
		std::cerr << "SoundNode! - " << sound << std::endl;
	}
};

// Plays children in order
struct SequenceNode : Node {
	SequenceNode() { type = NodeType::Sequence; }
	std::unique_ptr<Node> clone() const override {
		auto n = std::make_unique<SequenceNode>();
		copyCommonFields(*n);
		for (auto& c : children) n->children.push_back(c->clone());
		return n;
	}
};

// Plays children in parallel
struct ParallelNode : Node {
	ParallelNode() { type = NodeType::Parallel; }
	std::unique_ptr<Node> clone() const override {
		auto n = std::make_unique<ParallelNode>();
		copyCommonFields(*n);
		for (auto& c : children) n->children.push_back(c->clone());
		return n;
	}
};

// Silent delay node
struct DelayNode : Node {
	Expression delayExpr;
	DelayNode(const std::string& expr) { type = NodeType::Delay; delayExpr = Expression(expr); }
	std::unique_ptr<Node> clone() const override {
		auto n = std::make_unique<DelayNode>(delayExpr.text);
		copyCommonFields(*n);
		return n;
	}
};

// Randomly picks one child
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
	static std::mt19937& Rng() {
		static std::mt19937 rng{ std::random_device{}() };
		return rng;
	}
	size_t pickOnce() const {

		/*std::cerr << "[RandomNode trigger] this=" 
			<< this << " children=" 
			<< children.size() 
			<< " val before random: "
			<< std::to_string(choice)
			<< std::endl;

		for (auto& c : children)
			std::cerr << "  child: " << typeid(*c).name() << std::endl;*/

		if (choice < 0 && !children.empty()) {
			std::uniform_int_distribution<size_t> d(0, children.size() - 1);
			choice = static_cast<int>(d(Rng()));
		}
		//std::cerr << "[RandomNode trigger] choice: " << std::to_string(choice) << std::endl;

		return static_cast<size_t>(choice < 0 ? 0 : choice);
	}
};

// Blend node: interpolates between two or more children based on a parameter
struct BlendNode : Node {
	std::string parameter;

	struct Point {
		float at;
		std::unique_ptr<Node> node;
		Point(float a, std::unique_ptr<Node> n) : at(a), node(std::move(n)) {}
		Point(Point&&) noexcept = default;
		Point& operator=(Point&&) noexcept = default;
	};

	std::vector<Point> blends;
	BlendNode() { type = NodeType::Blend; }
	std::unique_ptr<Node> clone() const override {
		auto bn = std::make_unique<BlendNode>();
		copyCommonFields(*bn);
		bn->parameter = parameter;
		for (auto& p : blends) bn->blends.emplace_back(p.at, p.node->clone());
		return bn;
	}
	BlendWeights weights(float x) const {
		if (blends.empty()) return BlendWeights{ {{nullptr,0.f},{nullptr,0.f}} };
		if (x <= blends.front().at) 
			return BlendWeights{ {{blends.front().node.get(),1.f},{nullptr,0.f}} };
		if (x >= blends.back().at)  
			return BlendWeights{ {{blends.back().node.get(),1.f},{nullptr,0.f}} };
		for (size_t i = 0;i + 1 < blends.size();++i) {
			auto& a = blends[i]; auto& b = blends[i + 1];
			if (x >= a.at && x < b.at) {
				float t = (x - a.at) / (b.at - a.at);
				return BlendWeights{ {{a.node.get(),1.f - t},{b.node.get(),t}} };
			}
		}
		return BlendWeights{ {{nullptr,0.f},{nullptr,0.f}} };
	}

	void addCase(float at, Node* child) {
		blends.emplace_back(at, std::unique_ptr<Node>(child));
	}
};

// Select node: chooses a child based on an exact match or pattern
struct SelectNode : Node {
	std::string parameter;

	struct Option {
		std::string pattern; std::unique_ptr<Node> node;
		Option(std::string a, std::unique_ptr<Node> c)
			: pattern(a), node(std::move(c)) {
		}
		Option(Option&&) noexcept = default;
		Option& operator=(Option&&) noexcept = default;
	};
	std::vector<Option> options;
	std::unique_ptr<Node> defaultNode;
	SelectNode() { type = NodeType::Select; }
	std::unique_ptr<Node> clone() const override {
		auto n = std::make_unique<SelectNode>();
		copyCommonFields(*n);
		n->parameter = parameter;
		for (auto& o : options) n->options.push_back({ o.pattern, o.node->clone() });
		if (defaultNode) n->defaultNode = defaultNode->clone();
		return n;
	}
	const Node* pick(const std::string& value) const {
		for (auto& o : options)
			if (MatchUtils::PatternMatch(o.pattern, value)) return o.node.get();
		return defaultNode.get();
	}

	void addCase(const std::string& pattern, Node* child) {
		options.emplace_back(
			pattern,
			std::unique_ptr<Node>(child)
		);

	}
};

// Loop wrapper: repeats its single child indefinitely until stopped
struct LoopNode : Node {
	// 1) Constructor takes ownership of a child unique_ptr
	explicit LoopNode(std::unique_ptr<Node> child) {
		type = NodeType::Loop;
		children.push_back(std::move(child));
	}

	// 2) clone() clones the child and re-wraps it
	std::unique_ptr<Node> clone() const override {
		// clone the single child subtree
		auto childClone = children.front()->clone();  // unique_ptr<Node>
		// wrap it back into a new LoopNode
		auto n = std::make_unique<LoopNode>(std::move(childClone));
		copyCommonFields(*n);
		return n;
	}

	Node* getChild() const { return children.front().get(); }
};

// Reference node: placeholder that resolves to another node by id
struct ReferenceNode : Node {
	std::string targetId;
	Node* target = nullptr;
	ReferenceNode(const std::string& id) : targetId(id) { type = NodeType::Reference; }
	void resolve(Node* n) { target = n; }
	std::unique_ptr<Node> clone() const override {
		auto n = std::make_unique<ReferenceNode>(targetId);
		copyCommonFields(*n);
		return n;
	}
};

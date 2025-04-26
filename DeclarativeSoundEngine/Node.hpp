// Node.hpp
#pragma once
#include <string>
#include <vector>
#include <memory>

enum class NodeType { Sound, Layer, Random, Blend, Select };

// Base class for all sound-graph nodes
struct Node {
    Node() : type(NodeType::Sound) {}
    NodeType type;
    float volume = 1.0f;
    float pitch = 1.0f;
    float fadeIn = 0.0f;
    float fadeOut = 0.0f;
    float delay = 0.0f;
    int   loop = 0;       // -1: infinite, 0: no loop, >0: count
    virtual ~Node() = default;
};

// A single sound file
struct SoundNode : Node {
    std::string sound;
    SoundNode() { type = NodeType::Sound; }
};

// Plays multiple children simultaneously
struct LayerNode : Node {
    std::vector<std::unique_ptr<Node>> children;
    LayerNode() { type = NodeType::Layer; }
};

// Chooses one child at random each activation
struct RandomNode : Node {
    std::vector<std::unique_ptr<Node>> children;
    RandomNode() { type = NodeType::Random; }
};

// Blends between child nodes based on a parameter
struct BlendNode : Node {
    std::string parameter;
    struct Point { float at; std::unique_ptr<Node> node; };
    std::vector<Point> blends;
    BlendNode() { type = NodeType::Blend; }
};

// Selects a child based on tag or condition
struct SelectNode : Node {
    std::string parameter;
    struct Option { std::string match; std::unique_ptr<Node> node; };
    std::vector<Option> options;
    SelectNode() { type = NodeType::Select; }
};

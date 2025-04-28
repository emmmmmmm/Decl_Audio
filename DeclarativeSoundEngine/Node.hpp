#pragma once
#include <string>
#include <vector>
#include <memory>

enum class NodeType { Root, Sound, Layer, Random, Blend, Select };

struct Node {
    NodeType type = NodeType::Root;
    std::vector<std::unique_ptr<Node>> children;
    float volume = 1, pitch = 1, fadeIn = 0, fadeOut = 0, delay = 0;
    int loop = 0;

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
    RandomNode() { type = NodeType::Random; }
    std::unique_ptr<Node> clone() const override {
        auto n = std::make_unique<RandomNode>();
        copyCommonFields(*n);
        for (auto& c : children) n->children.push_back(c->clone());
        return n;
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
};

struct SelectNode : Node {
    std::string parameter;
    struct Option { std::string match; std::unique_ptr<Node> node; };
    std::vector<Option> options;
    SelectNode() { type = NodeType::Select; }

    std::unique_ptr<Node> clone() const override {
        auto n = std::make_unique<SelectNode>();
        copyCommonFields(*n);
        n->parameter = parameter;
        for (auto& o : options) {
            Option q{ o.match, o.node->clone() };
            n->options.push_back(std::move(q));
        }
        for (auto& c : children) {
            n->children.push_back(c->clone());
        }
        return n;
    }
};

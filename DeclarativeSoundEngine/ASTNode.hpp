#pragma once
#include <string>
#include <vector>
#include <unordered_map>

// Represents a generic AST node mirroring YAML structure.
struct ASTNode {
	enum class Type { Map, Seq, Scalar } type;

	// Only one of these is active based on `type`
	std::string scalar;
	std::vector<ASTNode> seq;
	std::unordered_map<std::string, ASTNode> map;

	ASTNode() : type(Type::Map) {}     // or Type::Scalar, whichever makes sense as default

	explicit ASTNode(const std::string& s)
		: type(Type::Scalar), scalar(s) {
	}

	bool isMap()    const { return type == Type::Map; }
	bool isSeq()    const { return type == Type::Seq; }
	bool isScalar() const { return type == Type::Scalar; }
};


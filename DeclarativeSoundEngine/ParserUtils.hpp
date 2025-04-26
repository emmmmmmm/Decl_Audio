// ParserUtils.hpp
#pragma once

#include "ASTNode.hpp"
#include "Node.hpp"
#include "Condition.hpp"
#include "Expression.hpp"

#include <memory>
#include <string>

// Wraps parsing of conditions from AST scalars
Condition        parseCondition(const std::string& text);
// Wraps parsing of numeric expressions from AST scalars
Expression       parseExpression(const std::string& text);
// Parses an ASTNode subtree into a concrete Node hierarchy
std::unique_ptr<Node> ParseNode(const ASTNode& ast);

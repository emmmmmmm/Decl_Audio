#include "pch.h"
#include "Expression.hpp"
#include "Log.hpp"
#include "ValueMap.hpp"
#include <regex>

float Expression::eval(const ValueMap& params) const {
	static const std::regex varOnly(R"(^\s*([A-Za-z_]\w*)\s*$)");
	static const std::regex numOnly(R"(^\s*([+\-]?[0-9]*\.?[0-9]+)\s*$)");
	static const std::regex binExpr(R"(^\s*([A-Za-z_]\w*)\s*([+\-*/])\s*([0-9]*\.?[0-9]+)\s*$)");
	static const std::regex revBin(R"(^\s*([0-9]*\.?[0-9]+)\s*([+\-*/])\s*([A-Za-z_]\w*)\s*$)");
	std::smatch m;
	
	
		// variable only
		if (std::regex_match(text, m, varOnly)) {
			float value = 0;
			if (params.TryGetValue(m[1], value))
				return value;
			else return 0.0f;
			//return params.HasValue(m[1]) ? params.GetValue(m[1]) : 0.0f;
		}

		// numeric literal
		if (std::regex_match(text, m, numOnly)) {
			return std::stof(m[1]);
		}

		// var OP number
		if (std::regex_match(text, m, binExpr)) {
			float lhs = 0;
			
			params.TryGetValue(m[2], lhs);

			char op = m[2].str()[0];
			float rhs = std::stof(m[3]);
			switch (op) {
			case '+': return lhs + rhs;
			case '-': return lhs - rhs;
			case '*': return lhs * rhs;
			case '/': return rhs != 0.0f ? lhs / rhs : 0.0f;
			}
		}
		// number OP var
		if (std::regex_match(text, m, revBin)) {
			float lhs = std::stof(m[1]);
			char op = m[2].str()[0];
			float rhs = 0.0f;
			params.TryGetValue(m[3], rhs);
			// rhs = params.HasValue(m[3]) ? params.GetValue(m[3]) : 0.0f;
			switch (op) {
			case '+': return lhs + rhs;
			case '-': return lhs - rhs;
			case '*': return lhs * rhs;
			case '/': return rhs != 0.0f ? lhs / rhs : 0.0f;
			}
		}
	
	LogMessage("[Expression] Failed to parse: " + text, LogCategory::General, LogLevel::Warning);
	return 0.0f;
}
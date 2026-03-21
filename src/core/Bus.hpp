#pragma once
#include "Vec3.hpp"
#include "Expression.hpp"

struct Bus {
	std::vector<float> buffer;
	Vec3               position = {};
	Expression		   volume{ "1.0" };

	struct Routing { int dst; float gain; };

	std::vector<Routing> sends;     // extra fan-outs // TBD
	int parent = 0;					// 0 = master, or another sub-mix
};
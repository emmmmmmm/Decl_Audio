#pragma once

#include <compare>

struct Vec3 {
	float x, y, z;
	auto operator<=>(const Vec3&) const = default;  // three-way compare
};
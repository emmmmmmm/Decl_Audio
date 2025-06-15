#pragma once

#include <compare>

struct Vec3 {
	float x, y, z;
	auto operator<=>(const Vec3&) const = default;  // three-way compare
	float magnitude() const {
		return std::sqrt(x * x + y * y + z * z);
	}
	static Vec3 subtract(const Vec3& a, const Vec3& b)  {
		Vec3 ret{};
		ret.x = a.x - b.x;
		ret.y = a.y - b.y;
		ret.z = a.z - b.z;
		return ret;
	}
	static Vec3 add(const Vec3& a, const Vec3& b)  {
		Vec3 ret{};
		ret.x = a.x + b.x;
		ret.y = a.y + b.y;
		ret.z = a.z + b.z;
		return ret;
	}
	static Vec3 scale(const Vec3& a, const float b) {
		Vec3 ret{};
		ret.x = a.x * b;
		ret.y = a.y * b;
		ret.z = a.z * b;
		return ret;
	}

	static Vec3 cross(const Vec3& a, const Vec3& b) {
		Vec3 ret{};
		ret.x = a.y * b.z - a.z * b.y;
		ret.y = a.z * b.x - a.x * b.z;
		ret.z = a.x * b.y - a.y * b.x;
		return ret;
	}

	Vec3 Normalized() const {
		Vec3 ret{};
		float mag = this->magnitude();
		if (mag == 0.f) return { 0, 0, 0 }; // or handle div-by-zero differently
		return { x / mag, y / mag, z / mag };
		return ret;
	}
	static float Dot(const Vec3& a, const Vec3& b) {
		return a.x * b.x 
				+ a.y * b.y 
				+ a.z * b.z;
	}
};
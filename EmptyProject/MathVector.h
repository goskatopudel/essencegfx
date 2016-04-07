#pragma once
#include "Essence.h"
#include <initializer_list>
#include <math.h>

template <typename T, i32 n>
struct Vector {
	T data[n];

	constexpr Vector() = default;
	~Vector() = default;

	Vector(T v) {
		for (i32 i = 0; i < n; ++i) {
			data[i] = v;
		}
	}

	T& operator[](i32 index) { return data[index] };
	const T& operator[](i32 index) const { return data[index]; };
};


template <typename T, i32 rows, i32 cols>
struct Matrix { T data[rows][cols]; };

template <typename T> struct Vector<T, 2> {
	union {
		T data[2];
		struct { T x, y; };
	};

	constexpr Vector() = default;
	~Vector() = default;

	template<typename K>
	explicit Vector(K v) {
		T _v = (T)v;
		x = _v;
		y = _v;
	}

	Vector(T v) {
		x = v;
		y = v;
	}

	Vector(T vx, T vy) {
		x = vx;
		y = vy;
	}

	Vector(const T* va) {
		x = va[0];
		y = va[1];
	}

	T& operator[](i32 index) { return data[index]; };
	const T& operator[](i32 index) const { return data[index]; };
};

template <typename T> struct Vector<T, 3> {
	union {
		T data[3];
		struct { T x, y, z; };
		Vector<T, 2> xy;
	};

	constexpr Vector() = default;
	~Vector() = default;

	template<typename K>
	explicit Vector(K v) {
		T _v = (T)v;
		x = _v;
		y = _v;
		z = _v;
	}

	Vector(T v) {
		x = v;
		y = v;
		z = v;
	}

	Vector(Vector<T, 2> vxy, T vz) {
		x = vxy.x;
		y = vxy.y;
		z = vz;
	}

	Vector(T vx, T vy, T vz) {
		x = vx;
		y = vy;
		z = vz;
	}

	Vector(const T* va) {
		x = va[0];
		y = va[1];
		z = va[2];
	}

	Vector(std::initializer_list<T> il) {
		auto iter = il.begin();
		data[0] = *iter; iter++;
		data[1] = *iter; iter++;
		data[2] = *iter; iter++;
	}

	T& operator[](i32 index) { return data[index]; };
	const T& operator[](i32 index) const { return data[index]; };
};

template <typename T> struct Vector<T, 4> {
	union {
		T data[4];
		struct { T x, y, z, w; };
		struct { float r, g, b, a; };
		Vector<T, 2> xy;
		Vector<T, 3> xyz;
		u32 packed_u32;
	};

	constexpr Vector() = default;
	~Vector() = default;

	template<typename K>
	explicit Vector(K v) {
		T _v = (T)v;
		x = _v;
		y = _v;
		z = _v;
		w = _v;
	}

	Vector(T v) {
		x = v;
		y = v;
		z = v;
		w = v;
	}

	Vector(T vx, T vy, T vz) {
		x = vx;
		y = vy;
		z = vz;
		w = T(1);
	}

	Vector(T vx, T vy, T vz, T vw) {
		x = vx;
		y = vy;
		z = vz;
		w = vw;
	}

	Vector(Vector<T, 3> vxyz, T vw) {
		x = vxyz.x;
		y = vxyz.y;
		z = vxyz.z;
		w = vw;
	}

	Vector(const T* va) {
		x = va[0];
		y = va[1];
		z = va[2];
		w = va[3];
	}

	Vector(std::initializer_list<T> il) {
		auto iter = il.begin();
		data[0] = *iter; iter++;
		data[1] = *iter; iter++;
		data[2] = *iter; iter++;
		data[3] = *iter; iter++;
	}

	T& operator[](i32 index) { return data[index]; };
};

typedef Vector<float, 2> Vec2f;
typedef Vector<float, 3> Vec3f;
typedef Vector<float, 4> Vec4f;
typedef Vector<float, 2> float2;
typedef Vector<float, 3> float3;
typedef Vector<float, 4> float4;
typedef Vector<u32, 2> Vec2u;
typedef Vector<u32, 3> Vec3u;
typedef Vector<u32, 4> Vec4u;
typedef Vector<i32, 2> Vec2i;
typedef Vector<i32, 3> Vec3i;
typedef Vector<i32, 4> Vec4i;
typedef Vector<u8, 4> Color4b;

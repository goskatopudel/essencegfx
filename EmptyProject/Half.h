#pragma once
#include "Essence.h"

u32 half_to_float(u16 h);
u16 half_from_float(u32 f);
u16 half_add(u16 arg0, u16 arg1);
u16 half_mul(u16 arg0, u16 arg1);
u16 half_div(u16 arg0, u16 arg1);

static inline u16 half_sub(u16 ha, u16 hb)
{
	// (a-b) is the same as (a+(-b))
	return half_add(ha, hb ^ 0x8000);
}

struct float16 {
	u16 h;

	float16() = default;
	explicit float16(float f) : h(half_from_float(f)) {}
	explicit float16(u16 a) : h(a) {}
};

inline bool operator == (float16 a, float16 b) {
	return a.h == b.h;
}

inline bool operator != (float16 a, float16 b) {
	return a.h != b.h;
}

inline float16 operator + (float16 a, float16 b) {
	return float16(half_add(a.h, b.h));
}

inline float16 operator - (float16 a, float16 b) {
	return float16(half_sub(a.h, b.h));
}

inline float16 operator * (float16 a, float16 b) {
	return float16(half_mul(a.h, b.h));
}

inline float16 operator / (float16 a, float16 b) {
	return float16(half_div(a.h, b.h));
}

typedef float16		half;
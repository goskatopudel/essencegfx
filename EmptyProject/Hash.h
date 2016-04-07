#pragma once
#include "Essence.h"

struct hash128__ {
	u64 h;
	u64 l;
};

void MurmurHash3_x86_32(const void * key, int len, u32 seed, void * out);
void MurmurHash3_x86_128(const void * key, int len, u32 seed, void * out);
void MurmurHash3_x64_128(const void * key, u64 len, hash128__ seed, void * out);
u64 MurmurHash2_64(const void * key, u64 len, u64 seed);

inline hash128__ MurmurHash3_x64_128(const void * key, u64 len, hash128__ seed) {
	hash128__ v;
	MurmurHash3_x64_128(key, len, seed, &v);
	return v;
}

u32 HashCombine32(u32 h1, u32 h2);
u64 HashCombine64(u64 h1, u64 h2);

#pragma once
#include "Essence.h"


inline void *align_forward(const void *p, size_t alignment) {
	uintptr_t pi = uintptr_t(p);
	pi = (pi + alignment - 1) & ~(alignment - 1);
	return (void *)pi;
}

inline void *pointer_add(const void *p, size_t bytes) {
	return (void*)((const char *)p + bytes);
}

inline void *pointer_sub(const void *p, size_t bytes) {
	return (void*)((const char *)p - bytes);
}

inline size_t padded_size(size_t size, size_t alignment) {
	return (size + alignment - 1) & ~(alignment - 1);
}

inline size_t pointer_sub(const void *a, const void *b) {
	return uintptr_t(a) - uintptr_t(b);
}

inline u64 Kilobytes(u64 bytes) {
	return bytes / 1024;
}

inline u64 Megabytes(u64 bytes) {
	return Kilobytes(bytes) / 1024;
}
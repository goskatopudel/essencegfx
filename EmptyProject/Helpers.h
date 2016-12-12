#pragma once

template<typename T>
T& GetInstance() {
	static T Instance;
	return Instance;
}

template <typename F>
struct ScopeExit {
	ScopeExit(F f) : f(f) {}
	~ScopeExit() { f(); }
	F f;
};

template <typename F>
ScopeExit<F> MakeScopeExit(F f) {
	return ScopeExit<F>(f);
};

#define STRING_JOIN2(arg1, arg2) DO_STRING_JOIN2(arg1, arg2)
#define DO_STRING_JOIN2(arg1, arg2) arg1 ## arg2
#define SCOPE_EXIT(code) \
    auto STRING_JOIN2(scope_exit_, __LINE__) = MakeScopeExit([&](){code;})

#include <EASTL/vector.h>

template<typename T>
void RemoveSwap(eastl::vector<T> Container, u64 Index) {
	if (Index != Container.size() - 1) {
		eastl::swap(Container[Container.size() - 1], Container[Index]);
	}
	Container.pop_back();
}
#pragma once

#include <assert.h>

#define check(expression) assert(expression)

#ifdef NDEBUG

template<typename T>
void __verify(T _Arg) {}
#define verify(expression) { __verify(expression); }

#else

#define verify(expression) check(expression)

#endif

#define VERIFYDX12(expression)  verify(SUCCEEDED(expression))
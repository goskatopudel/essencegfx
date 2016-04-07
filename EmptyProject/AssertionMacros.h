#pragma once

#include <assert.h>

#define check(expression) assert(expression)

#ifdef NDEBUG

#define verify(expression) { expression; }

#else

#define verify(expression) check(expression)

#endif

#define VERIFYDX12(expression)  verify(SUCCEEDED(expression))
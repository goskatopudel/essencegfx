#pragma once
#include "Essence.h"

inline void Print(const wchar_t* wstr) {
	OutputDebugString(wstr);
}

#include <EASTL\string.h>

void PrintFormated(const wchar_t* pFormat, ...);

inline void PrintFormatedVA(const wchar_t* pFormat, va_list args) {
	PrintFormated(pFormat, args);
}

eastl::wstring ConvertToWString(const char* src, u64 len);
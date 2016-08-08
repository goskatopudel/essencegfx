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
eastl::string ConvertToString(const wchar_t* src, u64 len);
eastl::wstring ConvertToWString(eastl::string const& str);

eastl::string Format(const char* pFormat, ...);
eastl::wstring Format(const wchar_t* pFormat, ...);
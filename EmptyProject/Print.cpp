#include "Print.h"

void PrintFormated(const wchar_t* pFormat, ...) {
	va_list args;
	eastl::wstring w;
	va_start(args, pFormat);
	w.sprintf_va_list(pFormat, args);
	Print(w.c_str());
	va_end(args);
}

eastl::wstring ConvertToWString(const char* src, u64 len) {
	eastl::wstring wstr;
	wstr.resize(len);

	auto srcBegin = src;
	auto srcEnd = src + len;
	auto dstBegin = &wstr[0];
	auto dstEnd = dstBegin + len;
	eastl::DecodePart(srcBegin, srcEnd, dstBegin, dstEnd);

	return std::move(wstr);
}

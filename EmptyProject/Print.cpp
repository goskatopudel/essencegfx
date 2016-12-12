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
	if (len == -1) {
		len = strlen(src);
	}
	eastl::wstring wstr;
	wstr.resize(len);

	auto srcBegin = src;
	auto srcEnd = src + len;
	auto dstBegin = &wstr[0];
	auto dstEnd = dstBegin + len;
	eastl::DecodePart(srcBegin, srcEnd, dstBegin, dstEnd);

	return std::move(wstr);
}

eastl::string ConvertToString(const wchar_t* src, u64 len) {
	if (len == -1) {
		len = wcslen(src);
	}
	eastl::string str;
	// dunno why easstl::DecodePart needs more space
	str.resize(len + 64);

	auto srcBegin = src;
	auto srcEnd = src + len;
	auto dstBegin = &str[0];
	auto dstEnd = dstBegin + len + 64;
	eastl::DecodePart(srcBegin, srcEnd, dstBegin, dstEnd);

	return std::move(str);
}

eastl::wstring ConvertToWString(eastl::string const& str) {
	return std::move(ConvertToWString(str.c_str(), str.size()));
}

eastl::string Format(const char* pFormat, ...) {
	va_list args;
	eastl::string w;
	va_start(args, pFormat);
	w.sprintf_va_list(pFormat, args);
	va_end(args);
	return std::move(w);
}

eastl::wstring Format(const wchar_t* pFormat, ...) {
	va_list args;
	eastl::wstring w;
	va_start(args, pFormat);
	w.sprintf_va_list(pFormat, args);
	va_end(args);
	return std::move(w);
}
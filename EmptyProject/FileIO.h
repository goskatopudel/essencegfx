#pragma once
#include "Essence.h"
#include <EASTL\unique_ptr.h>
#include <stdio.h>

struct FileReadResult {
	u8 *					Data;
	u64						Bytesize;

	eastl::unique_ptr<u8[]>	OwnedData;

	FileReadResult() = default;

	inline operator bool () const { return Bytesize > 0; };
};

FileReadResult ReadEntireFile(const char * filename);
FileReadResult ReadEntireFile(const wchar_t * filename);
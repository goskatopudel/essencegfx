#include "FileIO.h"

FileReadResult ReadEntrieFileInternal(FILE * f) {
	FileReadResult result = {};

	u8 * buffer = nullptr;
	long length = 0;

	if (f)
	{
		fseek(f, 0, SEEK_END);
		length = ftell(f);
		fseek(f, 0, SEEK_SET);
		buffer = new u8[length + 1];
		if (buffer)
		{
			fread(buffer, 1, length, f);
		}
		buffer[length] = 0;
		fclose(f);
		length += 1;

		result.Bytesize = length;
	}

	result.Data = buffer;
	result.OwnedData.reset(buffer);
	return std::move(result);
}

FileReadResult ReadEntireFile(const char* filename) {
	FILE * f;
	if (fopen_s(&f, filename, "rb") != 0) {
		return{};
	}

	return ReadEntrieFileInternal(f);
}

FileReadResult ReadEntireFile(const wchar_t* filename) {
	FILE * f;
	if (_wfopen_s(&f, filename, L"rb") != 0) {
		return{};
	}

	return ReadEntrieFileInternal(f);
}
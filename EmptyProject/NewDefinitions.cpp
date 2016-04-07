#include "Essence.h"
#include <malloc.h>

void* operator new[](size_t size, const char* /*name*/, int /*flags*/,
	unsigned /*debugFlags*/, const char* /*file*/, int /*line*/)
{
	return _aligned_offset_malloc(size, 1, 0);
}

void* operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char* /*name*/,
	int flags, unsigned /*debugFlags*/, const char* /*file*/, int /*line*/)
{
	// Substitute your aligned malloc. 
	return _aligned_offset_malloc(size, alignment, alignmentOffset);
}

void* operator new(size_t size)
{
	return _aligned_offset_malloc(size, 1, 0);
}

void* operator new[](size_t size)
{
	return _aligned_offset_malloc(size, 1, 0);
}

void operator delete(void* p)
{
	if (p) // The standard specifies that 'delete NULL' is a valid operation.
		_aligned_free(p);
}

void operator delete[](void* p)
{
	if (p)
		_aligned_free(p);
}
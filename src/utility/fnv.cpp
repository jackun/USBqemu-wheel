#include "fnv.h"
#include <cassert>

namespace FNV {

inline uint32_t fnv1a(unsigned char b, uint32_t hash)
{
	return (b ^ hash) * PRIME;
}

inline uint32_t fnv1a(wchar_t c, uint32_t hash)
{
	hash = fnv1a( (unsigned char)(c & 0xFF), hash);
	return fnv1a( (unsigned char)((c >> 8) & 0xFF), hash);
}

uint32_t fnv1a(const void* data, size_t len, uint32_t hash)
{
	assert(data);
	const unsigned char* ptr = (const unsigned char*)data;
	while (len--)
		hash = fnv1a(*ptr++, hash);
	return hash;
}

uint32_t fnv1a(const std::string& str, uint32_t hash)
{
	for (auto& c : str)
		hash = fnv1a((unsigned char)c, hash);
	return hash;
}

uint32_t fnv1a(const std::wstring& wstr, uint32_t hash)
{
	for (auto& c : wstr)
		hash = fnv1a(c, hash);
	return hash;
}

};
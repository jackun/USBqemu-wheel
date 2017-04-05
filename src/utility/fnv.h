#pragma once

#ifndef FNV_H
#define FNV_H

#include <string>
#include <cstdint>

namespace FNV {

const uint32_t PRIME = 0x01000193; // 16777619
const uint32_t SEED  = 0x811C9DC5; // 2166136261

inline uint32_t fnv1a(unsigned char b, uint32_t hash = SEED);
inline uint32_t fnv1a(wchar_t c, uint32_t hash = SEED);
uint32_t fnv1a(const void* data, size_t numBytes, uint32_t hash = SEED);
uint32_t fnv1a(const std::string& str, uint32_t hash = SEED);
uint32_t fnv1a(const std::wstring& wstr, uint32_t hash = SEED);

};
#endif
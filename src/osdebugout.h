#pragma once

#include <cstdio>
#include <iostream>

#ifdef _WIN32
#include "platcompat.h"

#include <vector>
static int rateLimit = 0;
static void _OSDebugOut(const TCHAR *psz_fmt, ...)
{
	if(rateLimit > 0 && rateLimit < 100)
	{
		rateLimit++;
		return;
	}
	else
	{
		//rateLimit = 1;
	}

	va_list args;
	va_start(args, psz_fmt);

#ifdef UNICODE
	int bufsize = _vscwprintf(psz_fmt, args) + 1;
	std::vector<TCHAR> msg(bufsize);
	vswprintf_s(&msg[0], bufsize, psz_fmt, args);
#else
	int bufsize = _vscprintf(psz_fmt, args) + 1;
	std::vector<TCHAR> msg(bufsize);
	vsprintf_s(&msg[0], bufsize, psz_fmt, args);
#endif

	//_vsnwprintf_s(&msg[0], bufsize, bufsize-1, psz_fmt, args);
	va_end(args);

	OutputDebugString(&msg[0]);
}

#ifdef _DEBUG
#define OSDebugOut(psz_fmt, ...) _OSDebugOut(TEXT("[USBqemu] [%s]:%d\t") psz_fmt, TEXT(__FUNCTION__), __LINE__, ##__VA_ARGS__)
#define OSDebugOut_noprfx(psz_fmt, ...) _OSDebugOut(psz_fmt, ##__VA_ARGS__)
#else
#define OSDebugOut(...) do{}while(0)
#endif

#else //_WIN32

#ifdef _DEBUG
#define OSDebugOut(psz_fmt, ...) do{ fprintf(stderr, "[USBqemu] [%s]:%d\t" psz_fmt, __func__, __LINE__, ##__VA_ARGS__); }while(0)
#define OSDebugOut_noprfx(psz_fmt, ...) do{ fprintf(stderr, psz_fmt, ##__VA_ARGS__); }while(0)
#define OSDebugOutStream_noprfx(str) do{ std::cerr << str << std::endl; }while(0)
#else
#define OSDebugOut(psz_fmt, ...) do{}while(0)
#define OSDebugOut_noprfx(psz_fmt, ...) do{}while(0)
#define OSDebugOutStream_noprfx(str) do{}while(0)
#endif

#endif //_WIN32

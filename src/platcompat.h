#ifndef PLATCOMPAT_H
#define PLATCOMPAT_H

// Annoying defines
// ---------------------------------------------------------------------
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#define wfopen _wfopen
#define fseeko64 _fseeki64
#define ftello64 _ftelli64
#define TSTDSTRING std::wstring

//FIXME narrow string fmt
#ifdef UNICODE
#define SFMTs "S"
#else
#define SFMTs "s"
#endif

#define __builtin_constant_p(p) false

#else //_WIN32

#define MAX_PATH PATH_MAX
#define __inline inline

//#ifndef TEXT
//#define TEXT(x) L##x
//#endif
//FIXME narrow string fmt
#define SFMTs "s"
#define TEXT(val) val
#define TCHAR char
#define wfopen fopen
#define TSTDSTRING std::string

#endif //_WIN32

#if __MINGW32__
#define DBL_EPSILON 2.2204460492503131e-16
template <size_t size>  
errno_t mbstowcs_s(  
	size_t *pReturnValue,  
	wchar_t (&wcstr)[size],  
	const char *mbstr,  
	size_t count
)
{
	return mbstowcs_s(pReturnValue, wcstr, size, mbstr, count);
}

template <size_t size>  
errno_t wcstombs_s(  
	size_t *pReturnValue,  
	char (&mbstr)[size],  
	const wchar_t *wcstr,  
	size_t count
)
{
	return wcstombs_s(pReturnValue, mbstr, size, wcstr, count);
}

#if 0 //newer mingw has it defined
template <size_t size>  
errno_t wcsncpy_s(  
	wchar_t (&strDest)[size],  
	const wchar_t *strSource,  
	size_t count
)
{
	return wcsncpy_s(strDest, size, strSource, count);
}
#endif

#endif //__MINGW32__

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) ((sizeof(x) / sizeof((x)[0])))
#endif

//TODO Idk, used only in desc.h and struct USBDescriptor should be already packed anyway
#if defined(_WIN32) && !defined(__MINGW32__)
#define PACK(def,name) __pragma( pack(push, 1) ) def name __pragma( pack(pop) )
#else
#define PACK(def,name) def __attribute__((gcc_struct, packed)) name
#endif

#ifdef _WIN64
typedef __int64 ssize_t;
#else
typedef int     ssize_t;
#endif
#endif

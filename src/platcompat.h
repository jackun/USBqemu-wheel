#ifndef PLATCOMPAT_H
#define PLATCOMPAT_H

// Annoying defines
// ---------------------------------------------------------------------
#ifdef _WIN32
#include <Windows.h>
#define wfopen _wfopen
#define TSTDSTRING std::wstring

//FIXME narrow string fmt
#ifdef UNICODE
#define SFMTs "S"
#else
#define SFMTs "s"
#endif

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

#endif

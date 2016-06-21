/*  USBlinuz
 *  Copyright (C) 2002-2004  USBlinuz Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#pragma once

#ifndef __PS2USB_H__
#define __PS2USB_H__

#include <stdio.h>

#ifndef EXPORT_C_
#ifdef _MSC_VER
#define EXPORT_C_(type) extern "C" type CALLBACK
#else
#define EXPORT_C_(type) extern "C" __attribute__((stdcall,externally_visible,visibility("default"))) type
#endif
#endif

// Annoying defines
// ---------------------------------------------------------------------
#ifdef _WIN32

#define usleep(x)	Sleep(x / 1000)
#include <windows.h>
#include <windowsx.h>

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
#define OSDebugOut(psz_fmt, ...) _OSDebugOut( TEXT("[") TEXT(__FUNCTION__) TEXT("] ") psz_fmt, ##__VA_ARGS__)
#else
#define OSDebugOut(...) do{}while(0)
#endif

#define wfopen _wfopen
#define STDSTR std::wstring

//FIXME narrow string fmt
#ifdef UNICODE
#define SFMTs "S"
#else
#define SFMTs "s"
#endif

#else //_WIN32

//#include <gtk/gtk.h>
#include <limits.h>
#include <string>
#define MAX_PATH PATH_MAX
#define __inline inline

#ifdef _DEBUG
#define OSDebugOut(psz_fmt, ...) fprintf(stderr, psz_fmt, ##__VA_ARGS__)
#else
#define OSDebugOut(psz_fmt, ...)
#endif

//#ifndef TEXT
//#define TEXT(x) L##x
//#endif
//FIXME narrow string fmt
#define SFMTs "s"
#define TEXT(val) val
#define TCHAR char
#define wfopen fopen
#define STDSTR std::string

#endif //_WIN32

// ---------------------------------------------------------------------
#define USBdefs
#include "PS2Edefs.h"

#define USB_LOG __Log

typedef struct _Config {
  int Log;
  int Port0; //player2
  int Port1; //player1
  int DFPPass; //[2]; //TODO per player
  int WheelType[2];
  TCHAR usb_img[MAX_PATH+1];

  STDSTR mics[2];
  int MicBuffering; //ms
  int LogitechIDs;

} Config;

extern Config conf;
extern u8 *ram;

// ---------------------------------------------------------------------
#include "qemu-usb/USBinternal.h"

#define PSXCLK	36864000	/* 36.864 Mhz */

extern USBcallback _USBirq;
void USBirq(int);

void SaveConfig();
void LoadConfig();
void DestroyDevices();
void CreateDevices();

extern FILE *usbLog;
void __Log(const char *fmt, ...);
// Hah, for l10n that will not happen anyway probably
#ifndef _WIN32
void SysMessage(const char *fmt, ...);
#else
#if _UNICODE
void SysMessageW(const wchar_t *fmt, ...);
#define SysMessage SysMessageW
#else
void SysMessageA(const char *fmt, ...);
#define SysMessage SysMessageA
#endif
#endif
s64 get_clock();

USBDevice *usb_hub_init(int nb_ports);
USBDevice *usb_msd_init(const TCHAR *filename);
USBDevice *eyetoy_init(void);
USBDevice *usb_mouse_init(void);

/* usb-pad.cpp */
USBDevice *pad_init(int port, int type);

/* usb-mic-singstar.cpp */
USBDevice *singstar_mic_init(int port, STDSTR *devs);

/* usb-pad-raw.cpp */
#if _WIN32
extern HWND gsWnd;
extern HWND msgWindow;
int InitWindow(HWND);
void UninitWindow();
#endif

#endif

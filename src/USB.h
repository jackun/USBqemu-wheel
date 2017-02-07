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

#include <cstdio>
#include <cstring>
#include <string>

#ifndef EXPORT_C_
#ifdef _MSC_VER
#define EXPORT_C_(type) extern "C" type CALLBACK
#else
#define EXPORT_C_(type) extern "C" __attribute__((stdcall,externally_visible,visibility("default"))) type
#endif
#endif

#include "platcompat.h"

#ifdef _WIN32

#include <windows.h>
#include <windowsx.h>

#else //_WIN32

//#include <gtk/gtk.h>
#include <limits.h>
#include <string>

#endif //_WIN32

#include "osdebugout.h"

// ---------------------------------------------------------------------
#define USBdefs
#include "PS2Edefs.h"

#define USB_LOG __Log
#define PLAYER_TWO_PORT 0
#define PLAYER_ONE_PORT 1
#define USB_PORT PLAYER_ONE_PORT

typedef struct _Config {
  int Log;
  std::string Port0; //player2
  std::string Port1; //player1
  int DFPPass; //[2]; //TODO per player
  int WheelType[2];
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

/* usb-pad-raw.cpp */
#if _WIN32
extern HWND gsWnd;
extern HWND msgWindow;
int InitWindow(HWND);
void UninitWindow();
#endif

#endif

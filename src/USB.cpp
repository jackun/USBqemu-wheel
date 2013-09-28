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

#include <stdlib.h>
#include <string>
#include <errno.h>

#include "qemu-usb/vl.h"
#include "USB.h"
#include "usb-pad/config.h"

const unsigned char version  = PS2E_USB_VERSION;
const unsigned char revision = 0;
const unsigned char build    = 1;    // increase that with each version
const unsigned char fix      = 1;

static char *libraryName     = "Qemu USB Driver (Wheel Mod)"
#ifdef _DEBUG
" (debug)"
#endif
;

OHCIState *qemu_ohci;
USBDevice *usb_device1 = NULL;
USBDevice *usb_device2 = NULL;

Config conf;
char USBfreezeID[] = "USB STv0";
typedef struct {
	OHCIState t;
} USBfreezeData;

u8 *usbR;
u8 *ram;
usbStruct usb;
USBcallback _USBirq;
FILE *usbLog;
int64_t usb_frame_time;
int64_t usb_bit_time;

s64 clocks;

#if _WIN32
HWND gsWnd=NULL;
#endif


void USBirq(int cycles)
{
	USB_LOG("USBirq.\n");

	_USBirq(cycles);
}

void __Log(char *fmt, ...) {
	va_list list;

	if (!conf.Log) return;

	va_start(list, fmt);
	vfprintf(usbLog, fmt, list);
	va_end(list);
}

#if __cplusplus
extern "C" {
#endif

u32 CALLBACK PS2EgetLibType() {
	return PS2E_LT_USB;
}

char* CALLBACK PS2EgetLibName() {
	return libraryName;
}

u32 CALLBACK PS2EgetLibVersion2(u32 type) {
	return (version<<16) | (revision<<8) | build | (fix << 24);
}

s32 CALLBACK USBinit() {
	LoadConfig();
	if (conf.Log)
	{
		usbLog = fopen("logs/usbLog.txt", "w");
		setvbuf(usbLog, NULL,  _IONBF, 0);
		USB_LOG("usblinuz plugin version %d,%d\n",revision,build);
		USB_LOG("USBinit\n");
	}

	qemu_ohci = ohci_create(0x1f801600,2);

	//if(!player_joys[0].empty())
	{
		usb_device1 = pad_init(PLAYER_ONE_PORT);
		qemu_ohci->rhport[PLAYER_ONE_PORT].port.attach(&(qemu_ohci->rhport[PLAYER_ONE_PORT].port), usb_device1);
	}

	if(!player_joys[1].empty())
	{
		usb_device2 = pad_init(PLAYER_TWO_PORT);
		qemu_ohci->rhport[PLAYER_TWO_PORT].port.attach(&(qemu_ohci->rhport[PLAYER_TWO_PORT].port), usb_device2);
	}

	return 0;
}

void CALLBACK USBshutdown() {

	if(qemu_ohci->rhport[PLAYER_ONE_PORT].port.dev)
	qemu_ohci->rhport[PLAYER_ONE_PORT].port.dev->handle_destroy(qemu_ohci->rhport[PLAYER_ONE_PORT].port.dev);
	
	if(qemu_ohci->rhport[PLAYER_TWO_PORT].port.dev)
	qemu_ohci->rhport[PLAYER_TWO_PORT].port.dev->handle_destroy(qemu_ohci->rhport[PLAYER_TWO_PORT].port.dev);

	free(qemu_ohci);

//#ifdef _DEBUG
	if (conf.Log && usbLog) fclose(usbLog);
//#endif
}

s32 CALLBACK USBopen(void *pDsp) {
	USB_LOG("USBopen\n");

#if _WIN32
	HWND hWnd=(HWND)pDsp;

	if (!IsWindow (hWnd) && !IsBadReadPtr ((u32*)hWnd, 4))
		hWnd = *(HWND*)hWnd;
	if (!IsWindow (hWnd))
		hWnd = NULL;
	else
	{
		while (GetWindowLong (hWnd, GWL_STYLE) & WS_CHILD)
			hWnd = GetParent (hWnd);
	}
	gsWnd = hWnd;
#endif

	return 0;
}

void CALLBACK USBclose() {
}

u8   CALLBACK USBread8(u32 addr) {
	USB_LOG("* Invalid 8bit read at address %lx\n", addr);
	return 0;
}

u16  CALLBACK USBread16(u32 addr) {
	USB_LOG("* Invalid 16bit read at address %lx\n", addr);
	return 0;
}

u32  CALLBACK USBread32(u32 addr) {
	u32 hard;

	hard=ohci_mem_read(qemu_ohci,addr);

	USB_LOG("* Known 32bit read at address %lx: %lx\n", addr, hard);

	return hard;
}

void CALLBACK USBwrite8(u32 addr,  u8 value) {
	USB_LOG("* Invalid 8bit write at address %lx value %x\n", addr, value);
}

void CALLBACK USBwrite16(u32 addr, u16 value) {
	USB_LOG("* Invalid 16bit write at address %lx value %x\n", addr, value);
}

void CALLBACK USBwrite32(u32 addr, u32 value) {
	USB_LOG("* Known 32bit write at address %lx value %lx\n", addr, value);
	ohci_mem_write(qemu_ohci,addr,value);
}

void CALLBACK USBirqCallback(USBcallback callback) {
	_USBirq = callback;
}

extern u32 bits;

int CALLBACK _USBirqHandler(void) 
{
	//fprintf(stderr," * USB: IRQ Acknowledged.\n");
	//qemu_ohci->intr_status&=~bits;
	return 1;
}

USBhandler CALLBACK USBirqHandler(void) {
	return (USBhandler)_USBirqHandler;
}

void CALLBACK USBsetRAM(void *mem) {
	ram = (u8*)mem;
}

//TODO update VID/PID and various buffer lengths with changes in usb-pad.c?
s32 CALLBACK USBfreeze(int mode, freezeData *data) {
	USBfreezeData *usbd;

	if (mode == FREEZE_LOAD) {
		usbd = (USBfreezeData*)data->data;
		if (data->size != sizeof(USBfreezeData)) return -1;
		
		memcpy(qemu_ohci, usbd, sizeof(OHCIState));
		//qemu_ohci->rhport[0].port.dev->handle_reset(usb_device);

	} else
	if (mode == FREEZE_SAVE) {
		data->size = sizeof(USBfreezeData);
		data->data = (s8*)malloc(data->size);
		if (data->data == NULL) return -1;
		usbd = (USBfreezeData*)data->data;
		memcpy(usbd, qemu_ohci, sizeof(OHCIState));
	}

	return 0;
}

void CALLBACK USBasync(u32 cycles)
{
	clocks+=cycles;
	if(qemu_ohci->eof_timer>0)
	{
		while(cycles>=qemu_ohci->eof_timer)
		{
			cycles-=qemu_ohci->eof_timer;
			qemu_ohci->eof_timer=0;
			ohci_frame_boundary(qemu_ohci);
		}
		if((cycles>0)&&(qemu_ohci->eof_timer>0))
			qemu_ohci->eof_timer-=cycles;
	}
}

s32  CALLBACK USBtest() {
	return 0;
}

#if __cplusplus
} //extern "C" {
#endif

void cpu_physical_memory_rw(u32 addr, u8 *buf,
							int len, int is_write)
{
	if(is_write)
		memcpy(&(ram[addr]),buf,len);
	else
		memcpy(buf,&(ram[addr]),len);
}


s64 get_clock()
{
	return clocks;
}

void *qemu_mallocz(uint32_t size)
{
	void *m=malloc(size);
	if(!m) return NULL;
	memset(m,0,size);
	return m;
}

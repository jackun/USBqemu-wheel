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

#if BUILD_DX
	#include "usb-pad/dx/global.h"
	#include "usb-pad/dx/dialog.h"
#endif

#include "qemu-usb/vl.h"
#include "USB.h"
#include "usb-pad/config.h"

const unsigned char version  = PS2E_USB_VERSION;
const unsigned char revision = 0;
const unsigned char build    = 5;    // increase that with each version
const unsigned char fix      = 2;

static char libraryName[256];

OHCIState *qemu_ohci = NULL;
USBDevice *usb_device1 = NULL;
USBDevice *usb_device2 = NULL;
bool configChanged = false;

Config conf;
char USBfreezeID[] = "USBqemuW01";
typedef struct {
	char freezeID[11];
	s64 cycles;
	s64 remaining;
	OHCIState t;
	int extraData; // for future expansion with the device state
} USBfreezeData;

u8 *ram = 0;
USBcallback _USBirq;
FILE *usbLog;
int64_t usb_frame_time;
int64_t usb_bit_time;

s64 clocks = 0;
s64 remaining = 0;

#if _WIN32
HWND gsWnd=NULL;
#endif


void USBirq(int cycles)
{
	USB_LOG("USBirq.\n");

	_USBirq(cycles);
}

void __Log(const char *fmt, ...) {
	va_list list;

	if (!conf.Log ||!usbLog) return;

	va_start(list, fmt);
	vfprintf(usbLog, fmt, list);
	va_end(list);
}

//Simpler to reset and reattach after USBclose/USBopen
void Reset()
{
	if(qemu_ohci)
		ohci_reset(qemu_ohci);
}

void DestroyDevices()
{
	//FIXME something throws an null ptr exception?
	if(qemu_ohci && qemu_ohci->rhport[PLAYER_ONE_PORT].port.dev) {
		qemu_ohci->rhport[PLAYER_ONE_PORT].port.dev->handle_destroy(qemu_ohci->rhport[PLAYER_ONE_PORT].port.dev);
		qemu_ohci->rhport[PLAYER_ONE_PORT].port.dev = NULL;
	}
	else if(usb_device1) //maybe redundant
		usb_device1->handle_destroy(usb_device1);
	
	if(qemu_ohci && qemu_ohci->rhport[PLAYER_TWO_PORT].port.dev) {
		qemu_ohci->rhport[PLAYER_TWO_PORT].port.dev->handle_destroy(qemu_ohci->rhport[PLAYER_TWO_PORT].port.dev);
		qemu_ohci->rhport[PLAYER_TWO_PORT].port.dev = NULL;
	}
	else if(usb_device2)
		usb_device2->handle_destroy(usb_device2);

	usb_device1 = NULL;
	usb_device2 = NULL;
}

void CreateDevices()
{
	if(!qemu_ohci) return; //No USBinit yet ie. called from config. dialog
	DestroyDevices();
	switch(conf.Port1)
	{
	case 1:
		usb_device1 = pad_init(PLAYER_ONE_PORT, 0);
		break;
	case 2:
		usb_device1 = pad_init(PLAYER_ONE_PORT, 1);
		break;
	case 3:
		usb_device1 = usb_msd_init(conf.usb_img);
		break;
	case 4:
		//TODO only one at a time?
		usb_device1 = singstar_mic_init(PLAYER_ONE_PORT, conf.mics);
		break;
	default:
		break;
	}
	
	switch(conf.Port0)
	{
	case 1:
		usb_device2 = pad_init(PLAYER_TWO_PORT, 0);
		break;
	case 2:
		usb_device2 = pad_init(PLAYER_TWO_PORT, 1);
		break;
	case 3:
		usb_device2 = usb_msd_init(conf.usb_img);
		break;
	case 4:
		//TODO only one at a time?
		usb_device2 = singstar_mic_init(PLAYER_TWO_PORT, conf.mics);
		break;
	default:
		break;
	}

	//No need for NULL check. NULL device means detach port.
	qemu_ohci->rhport[PLAYER_ONE_PORT].port.attach(&(qemu_ohci->rhport[PLAYER_ONE_PORT].port), usb_device1);
	qemu_ohci->rhport[PLAYER_TWO_PORT].port.attach(&(qemu_ohci->rhport[PLAYER_TWO_PORT].port), usb_device2);
}

EXPORT_C_(u32) PS2EgetLibType() {
	return PS2E_LT_USB;
}

EXPORT_C_(char*) PS2EgetLibName() {

#ifdef _MSC_VER
	sprintf_s
#else
	sprintf
#endif
	( libraryName, "Qemu USB Driver (Wheel) (" __DATE__ ")" 
#ifdef _DEBUG
		" (debug)"
#endif
	);

	return libraryName;
}

EXPORT_C_(u32) PS2EgetLibVersion2(u32 type) {
	return (version<<16) | (revision<<8) | build | (fix << 24);
}

EXPORT_C_(s32) USBinit() {
	OSDebugOut(TEXT("USBinit\n"));
	LoadConfig();

	if (conf.Log)
	{
		usbLog = fopen("logs/usbLog.txt", "w");
		if(usbLog) setvbuf(usbLog, NULL,  _IONBF, 0);
		USB_LOG("usbqemu wheel mod plugin version %d,%d\n",revision,build);
		USB_LOG("USBinit\n");
	}

	qemu_ohci = ohci_create(0x1f801600,2);
	if(!qemu_ohci) return 1;

	return 0;
}

EXPORT_C_(void) USBshutdown() {

	OSDebugOut(TEXT("USBshutdown\n"));
	DestroyDevices();

	free(qemu_ohci);

	ram = 0;

//#ifdef _DEBUG
	if (conf.Log && usbLog) fclose(usbLog);
//#endif
}

EXPORT_C_(s32) USBopen(void *pDsp) {
	USB_LOG("USBopen\n");
	OSDebugOut(TEXT("USBopen\n"));

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
	if(msgWindow == NULL)
		InitWindow(hWnd);
#endif

	if ((configChanged && (usb_device1 || usb_device2)) ||
		(!usb_device1 && !usb_device2))
	{
		configChanged = false;
		CreateDevices(); //TODO Pass pDsp to init?
	}

	//TODO Pass pDsp to open probably so dinput can bind to this HWND
	if(usb_device1 && usb_device1->open) usb_device1->open(usb_device1);
	if(usb_device2 && usb_device2->open) usb_device2->open(usb_device2);
	return 0;
}

EXPORT_C_(void) USBclose() {
	OSDebugOut(TEXT("USBclose\n"));
	if(usb_device1 && usb_device1->close) usb_device1->close(usb_device1);
	if(usb_device2 && usb_device2->close) usb_device2->close(usb_device2);
#if _WIN32
	UninitWindow();
#endif
}

EXPORT_C_(u8) USBread8(u32 addr) {
	USB_LOG("* Invalid 8bit read at address %lx\n", addr);
	return 0;
}

EXPORT_C_(u16) USBread16(u32 addr) {
	USB_LOG("* Invalid 16bit read at address %lx\n", addr);
	return 0;
}

EXPORT_C_(u32) USBread32(u32 addr) {
	u32 hard;

	hard=ohci_mem_read(qemu_ohci,addr);

	USB_LOG("* Known 32bit read at address %lx: %lx\n", addr, hard);

	return hard;
}

EXPORT_C_(void) USBwrite8(u32 addr,  u8 value) {
	USB_LOG("* Invalid 8bit write at address %lx value %x\n", addr, value);
}

EXPORT_C_(void) USBwrite16(u32 addr, u16 value) {
	USB_LOG("* Invalid 16bit write at address %lx value %x\n", addr, value);
}

EXPORT_C_(void) USBwrite32(u32 addr, u32 value) {
	USB_LOG("* Known 32bit write at address %lx value %lx\n", addr, value);
	ohci_mem_write(qemu_ohci,addr,value);
}

EXPORT_C_(void) USBirqCallback(USBcallback callback) {
	_USBirq = callback;
}

extern u32 bits;

EXPORT_C_(int) _USBirqHandler(void) 
{
	//fprintf(stderr," * USB: IRQ Acknowledged.\n");
	//qemu_ohci->intr_status&=~bits;
	//TODO Instead of USBasync(), do like nuuvem plugin and
	// do ohci_frame_boundary() here?
#if 0
	// load state - restart interrupts
	if( freeze_load == 1 ) {
		USB_LOG( "_USBirqHandler: Freeze - restart IRQ\n" );


		USBirq(PSXCLK / 1000);


		freeze_load = 0;
		return 0;
	}

	// generate SF packet every 1ms ticks
	if( (qemu_ohci->ctl & OHCI_CTL_HCFS) == OHCI_USB_OPERATIONAL ) {
		USBirq(PSXCLK / 1000);
		qemu_ohci->intr_status |= OHCI_INTR_SF;
	}
	ohci_frame_boundary(qemu_ohci);
#endif

	return 1;
}

EXPORT_C_(USBhandler) USBirqHandler(void) {
	return (USBhandler)_USBirqHandler;
}

EXPORT_C_(void) USBsetRAM(void *mem) {
	ram = (u8*)mem;
	Reset();
}

//TODO update VID/PID and various buffer lengths with changes in usb-pad.c?
EXPORT_C_(s32) USBfreeze(int mode, freezeData *data) {
	USBfreezeData usbd;

	if (mode == FREEZE_LOAD) 
	{
		if(data->size < sizeof(USBfreezeData))
		{
			SysMessage(TEXT("ERROR: Unable to load freeze data! Got %d bytes, expected >= %d."), data->size, sizeof(USBfreezeData));
			return -1;
		}

		usbd = *(USBfreezeData*)data->data;
		usbd.freezeID[10] = 0;

		if( strcmp(usbd.freezeID, USBfreezeID) != 0)
		{
			SysMessage(TEXT("ERROR: Unable to load freeze data! Found ID '%s', expected ID '%s'."), usbd.freezeID, USBfreezeID);
			return -1;
		}

		if (data->size != sizeof(USBfreezeData))
			return -1;

		clocks = usbd.cycles;
		remaining = usbd.remaining;
		for(int i=0; i< qemu_ohci->num_ports; i++)
		{
			usbd.t.rhport[i].port.opaque = qemu_ohci;
			//usbd.t.rhport[i].port.ops = qemu_ohci->rhport[i].port.ops;
			usbd.t.rhport[i].port.dev = qemu_ohci->rhport[i].port.dev; // pointers
		}
		*qemu_ohci = usbd.t;

		// WARNING: TODO: Load the state of the attached devices!

	}
	else if (mode == FREEZE_SAVE) 
	{
		data->size = sizeof(USBfreezeData);
		data->data = (s8*)malloc(data->size);
		if (data->data == NULL)
			return -1;

		strncpy(usbd.freezeID,  USBfreezeID, strlen(USBfreezeID));
		usbd.t = *qemu_ohci;
		for(int i=0; i< qemu_ohci->num_ports; i++)
		{
			//usbd.t.rhport[i].port.ops = NULL; // pointers
			usbd.t.rhport[i].port.opaque = NULL; // pointers
			usbd.t.rhport[i].port.dev = NULL; // pointers
		}

		usbd.cycles = clocks;
		usbd.remaining = remaining;
		memcpy(data->data, &usbd, data->size);
		//*(USBfreezeData*)data->data = usbd;

		// WARNING: TODO: Save the state of the attached devices!
	}

	return 0;
}

EXPORT_C_(void) USBasync(u32 cycles)
{
	remaining += cycles;
	clocks += remaining;
	if(qemu_ohci->eof_timer>0)	{		while(remaining>=qemu_ohci->eof_timer)
		{
			remaining-=qemu_ohci->eof_timer;
			qemu_ohci->eof_timer=0;
			ohci_frame_boundary(qemu_ohci);
		}
		if((remaining>0)&&(qemu_ohci->eof_timer>0))
		{
			s64 m = qemu_ohci->eof_timer;
			if(remaining < m)
				m = remaining;
			qemu_ohci->eof_timer -= m;
			remaining -= m;
		}
	}
	//if(qemu_ohci->eof_timer <= 0)
	//{
	//ohci_frame_boundary(qemu_ohci);
	//}
}

EXPORT_C_(s32) USBtest() {
	return 0;
}

void cpu_physical_memory_rw(u32 addr, u8 *buf, int len, int is_write)
{
	//OSDebugOut(TEXT("%s addr %08X, len %d\n"), is_write ? TEXT("write") : TEXT("read "), addr, len);
	if(is_write)
		memcpy(&(ram[addr]),buf,len);
	else
		memcpy(buf,&(ram[addr]),len);
}

s64 get_clock()
{
	return clocks;
}

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

#include <cstdlib>
#include <string>
#include <cerrno>
#include <cassert>

#include "USB.h"
#include "platcompat.h"
#include "osdebugout.h"
#include "deviceproxy.h"
#include "version.h" //CMake generated

#define PSXCLK	36864000	/* 36.864 Mhz */

const unsigned char version  = PS2E_USB_VERSION;

static char libraryName[256];

OHCIState *qemu_ohci = NULL;
USBDevice *usb_device[2] = { NULL };
bool configChanged = false;

Config conf;
char USBfreezeID[] = "USBqemuW01";
typedef struct {
	char freezeID[11];
	s64 cycles;
	s64 remaining;
	OHCIState t;
	struct {
		DeviceType index;
		u32 size;
		USBDevice dev;
	} device[2];
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
		ohci_hard_reset(qemu_ohci);
}

void DestroyDevices()
{
	for (int i=0; i<2; i++)
	{
		if(qemu_ohci && qemu_ohci->rhport[i].port.dev) {
			qemu_ohci->rhport[i].port.dev->handle_destroy(qemu_ohci->rhport[i].port.dev);
			qemu_ohci->rhport[i].port.dev = NULL;
		}
		else if(usb_device[i])
			usb_device[i]->handle_destroy(usb_device[i]);

		usb_device[i] = NULL;
	}
}

USBDevice* CreateDevice(DeviceType index, int port)
{
	DeviceProxyBase *devProxy;
	USBDevice* device = nullptr;

	if (index == DEVTYPE_NONE)
		return nullptr;

	devProxy = RegisterDevice::instance().Device(index);
	if (devProxy)
		device = devProxy->CreateDevice(port);
	else
		SysMessage(TEXT("Device %d: Unknown device type"), 1 - port);

	return device;
}

void USBAttach(int port, USBDevice *dev, bool sneaky = false)
{
	if (!qemu_ohci) return;

	if (sneaky)
	{
		USBDevice *tmp = qemu_ohci->rhport[port].port.dev;
		if (tmp)
			tmp->handle_destroy(tmp);
		qemu_ohci->rhport[port].port.dev = dev;
	}

	qemu_ohci->rhport[port].port.ops->attach(&(qemu_ohci->rhport[port].port), dev);
}

USBDevice* CreateDevice(const std::string& name, int port)
{
	DeviceProxyBase *devProxy;
	USBDevice* device = nullptr;

	if (!name.empty())
	{
		devProxy = RegisterDevice::instance().Device(name);
		if (devProxy)
			device = devProxy->CreateDevice(port);
		else
			SysMessage(TEXT("Device %d: Unknown device type"), 1 - port);
	}

	return device;
}

void CreateDevices()
{
	if(!qemu_ohci) return; //No USBinit yet ie. called from config. dialog
	DestroyDevices();

	for (int i=0; i<2; i++)
	{
		usb_device[i] = CreateDevice(conf.Port[i], i);
		USBAttach(i, usb_device[i]);
	}
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
	return (version<<16) | ((char)VER_REV<<8) | (char)VER_BLD | ((char)VER_FIX << 24);
}

EXPORT_C_(s32) USBinit() {
	OSDebugOut(TEXT("USBinit\n"));
	LoadConfig();

	if (conf.Log)
	{
		usbLog = fopen("logs/usbLog.txt", "w");
		if(usbLog) setvbuf(usbLog, NULL,  _IONBF, 0);
		USB_LOG("usbqemu wheel mod plugin version %d.%d.%d\n", VER_REV, VER_BLD, VER_FIX);
		USB_LOG("USBinit\n");
	}

	qemu_ohci = ohci_create(0x1f801600,2);
	if(!qemu_ohci) return 1;

	clocks = 0;
	remaining = 0;

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

	if (configChanged || (!usb_device[0] && !usb_device[1]))
	{
		configChanged = false;
		CreateDevices(); //TODO Pass pDsp to init?
	}

	//TODO Pass pDsp to open probably so dinput can bind to this HWND
	if(usb_device[0] && usb_device[0]->open)
		usb_device[0]->open(usb_device[0]);

	if(usb_device[1] && usb_device[1]->open)
		usb_device[1]->open(usb_device[1]);

	return 0;
}

EXPORT_C_(void) USBclose() {
	OSDebugOut(TEXT("USBclose\n"));

	if(usb_device[0] && usb_device[0]->close)
		usb_device[0]->close(usb_device[0]);

	if(usb_device[1] && usb_device[1]->close)
		usb_device[1]->close(usb_device[1]);

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
	return 1;
}

EXPORT_C_(USBhandler) USBirqHandler(void) {
	return (USBhandler)_USBirqHandler;
}

EXPORT_C_(void) USBsetRAM(void *mem) {
	ram = (u8*)mem;
	Reset();
}

EXPORT_C_(s32) USBfreeze(int mode, freezeData *data) {
	USBfreezeData usbd = { 0 };

	//TODO FREEZE_SIZE mismatch causes loading to fail in PCSX2 beforehand
	if (mode == FREEZE_LOAD) 
	{
		if(data->size < sizeof(USBfreezeData))
		{
			SysMessage(TEXT("ERROR: Unable to load freeze data! Got %d bytes, expected >= %d.\n"), data->size, sizeof(USBfreezeData));
			return -1;
		}

		usbd = *(USBfreezeData*)data->data;
		usbd.freezeID[10] = 0;

		if( strcmp(usbd.freezeID, USBfreezeID) != 0)
		{
			SysMessage(TEXT("ERROR: Unable to load freeze data! Found ID '%") TEXT(SFMTs) TEXT("', expected ID '%") TEXT(SFMTs) TEXT("'.\n"), usbd.freezeID, USBfreezeID);
			return -1;
		}

		//TODO Subsequent save state loadings make USB "stall" for n seconds since previous load
		//clocks = usbd.cycles;
		//remaining = usbd.remaining;

		for(int i=0; i< qemu_ohci->num_ports; i++)
		{
			usbd.t.rhport[i].port.opaque = qemu_ohci;
			usbd.t.rhport[i].port.ops = qemu_ohci->rhport[i].port.ops;
			usbd.t.rhport[i].port.dev = qemu_ohci->rhport[i].port.dev;
		}
		*qemu_ohci = usbd.t;

		s8 *ptr = data->data + sizeof(USBfreezeData);
		// Load the state of the attached devices
		if (data->size != sizeof(USBfreezeData) + usbd.device[0].size + usbd.device[1].size)
			return -1;

		RegisterDevice& regInst = RegisterDevice::instance();
		for (int i=0; i<2; i++)
		{
			auto index = regInst.Index(conf.Port[i]);
			auto proxy = regInst.Device(index);

			//TODO FREEZE_SIZE mismatch causes loading to fail in PCSX2 beforehand
			// but just in case, recreate the same device type as was saved
			if (usbd.device[i].index != index)
			{
				index = usbd.device[i].index;
				USBDevice *dev = qemu_ohci->rhport[i].port.dev;
				qemu_ohci->rhport[i].port.dev = nullptr;

				if (dev)
				{
					assert(usb_device[i] == dev);
					dev->handle_destroy(dev);
				}

				proxy = regInst.Device(index);
				usb_device[i] = CreateDevice(index, i);
				USBAttach(i, usb_device[i], index != DEVTYPE_MSD);
			}

			if (proxy)
			{
				if (proxy->Freeze(FREEZE_SIZE, usb_device[i], nullptr) != usbd.device[i].size)
				{
					SysMessage(TEXT("Port %d: device's freeze size doesn't match.\n"), 1+(1-i));
					return -1;
				}

				USBDevice tmp  = *usb_device[i];
				*usb_device[i] = usbd.device[i].dev;

				usb_device[i]->port           = tmp.port;
				usb_device[i]->handle_packet  = tmp.handle_packet;
				usb_device[i]->handle_destroy = tmp.handle_destroy;
				usb_device[i]->open           = tmp.open;
				usb_device[i]->close          = tmp.close;
				usb_device[i]->handle_reset   = tmp.handle_reset;
				usb_device[i]->handle_control = tmp.handle_control;
				usb_device[i]->handle_data    = tmp.handle_data;

				proxy->Freeze(FREEZE_LOAD, usb_device[i], ptr);
			}
			else if (index != DEVTYPE_NONE)
			{
				SysMessage(TEXT("Port %d: unknown device.\nPlugin is probably too old for this save.\n"), 1+(1-i));
				return -1;
			}

			ptr += usbd.device[i].size;
		}

	}
	//TODO straight copying of structs can break cross-platform/cross-compiler save states 'cause padding 'n' stuff
	else if (mode == FREEZE_SAVE) 
	{
		RegisterDevice& regInst = RegisterDevice::instance();

		for (int i=0; i<2; i++)
		{
			//TODO check that current created usb device and conf.Port[n] are the same
			auto index = regInst.Index(conf.Port[i]);
			auto proxy = regInst.Device(index);
			usbd.device[i].index = index;

			if (proxy)
				usbd.device[i].size = proxy->Freeze(FREEZE_SIZE, usb_device[i], nullptr);
			else
				usbd.device[i].size = 0;
		}

		strncpy(usbd.freezeID,  USBfreezeID, strlen(USBfreezeID));
		usbd.t = *qemu_ohci;
		for(int i=0; i< qemu_ohci->num_ports; i++)
		{
			usbd.t.rhport[i].port.opaque = nullptr;
			usbd.t.rhport[i].port.ops = nullptr;
			usbd.t.rhport[i].port.dev = nullptr;
		}

		usbd.cycles = clocks;
		usbd.remaining = remaining;

		s8 *ptr = data->data + sizeof(USBfreezeData);

		// Save the state of the attached devices
		for (int i=0; i<2; i++)
		{
			auto proxy = regInst.Device(conf.Port[i]);
			if (proxy && usbd.device[i].size)
			{
				proxy->Freeze(FREEZE_SAVE, usb_device[i], ptr);
				usbd.device[i].dev = *usb_device[i];

				usbd.device[i].dev.port           = nullptr;
				usbd.device[i].dev.handle_packet  = nullptr;
				usbd.device[i].dev.handle_destroy = nullptr;
				usbd.device[i].dev.open           = nullptr;
				usbd.device[i].dev.close          = nullptr;
				usbd.device[i].dev.handle_reset   = nullptr;
				usbd.device[i].dev.handle_control = nullptr;
				usbd.device[i].dev.handle_data    = nullptr;
			}

			ptr += usbd.device[i].size;
		}

		*(USBfreezeData*)data->data = usbd;
	}
	else if (mode == FREEZE_SIZE) 
	{
		RegisterDevice& regInst = RegisterDevice::instance();
		data->size = sizeof(USBfreezeData);
		for (int i=0; i<2; i++)
		{
			//TODO check that current created usb device and conf.Port[n] are the same
			auto index = regInst.Index(conf.Port[i]);
			auto proxy = regInst.Device(index);

			if (proxy)
				data->size += proxy->Freeze(FREEZE_SIZE, usb_device[i], nullptr);
		}
	}

	return 0;
}

EXPORT_C_(void) USBasync(u32 cycles)
{
	remaining += cycles;
	clocks += remaining;
	if(qemu_ohci->eof_timer>0)
	{
		while(remaining>=qemu_ohci->eof_timer)
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

int cpu_physical_memory_rw(u32 addr, u8 *buf, size_t len, int is_write)
{
	//OSDebugOut(TEXT("%s addr %08X, len %d\n"), is_write ? TEXT("write") : TEXT("read "), addr, len);
	// invalid address, reset and try again
	if (addr > 0x200000)
	{
		OSDebugOut(TEXT("invalid address, soft resetting qemu.\n"));
		if (qemu_ohci)
			ohci_soft_reset(qemu_ohci);
		return 1;
	}

	if(is_write)
		memcpy(&(ram[addr]),buf,len);
	else
		memcpy(buf,&(ram[addr]),len);
	return 0;
}

int get_ticks_per_second()
{
	return PSXCLK;
}

s64 get_clock()
{
	return clocks;
}

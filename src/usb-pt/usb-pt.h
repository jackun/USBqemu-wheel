#ifndef USB_PT_H
#define USB_PT_H

#include <libusb-1.0/libusb.h>
#include "USB.h"
#include "osdebugout.h"
#include "deviceproxy.h"
//#include <unordered_map>

#define DEVICENAME "usbpt"
#define APINAME "libusb"

#define N_IGNORE_BUSPORT TEXT("ignore_busport")

struct ConfigUSBDevice
{
	std::string name;
	int vid;
	int pid;
	int bus;
	int port;
	bool ignore_busport;

	ConfigUSBDevice(): vid(0), pid(0), bus(0), port(0), ignore_busport(false) {}
	ConfigUSBDevice(bool b): vid(0), pid(0), bus(0), port(0), ignore_busport(b) {}

	bool operator==(const ConfigUSBDevice& r) const
	{
		return vid == r.vid && pid == r.pid &&
			(bus == r.bus && port == r.port || ignore_busport);
	}
};

typedef struct PTState {
	USBDevice	dev;
	uint8_t		port;

	libusb_device_handle *usb_handle;
	libusb_context *usb_ctx;

	int config;
	int intf;
	int altset;
	int ep;
	int transfer_type_ep;
	int transfer_type;
	//std::unordered_map<int, int> ep_ttype;
} PTState;

class PTDevice : public Device
{
public:
	virtual ~PTDevice() {}
	static USBDevice* CreateDevice(int port);
	static const TCHAR* Name()
	{
		return TEXT("USB pass-through device");
	}
	static const char* TypeName()
	{
		return DEVICENAME;
	}
	static std::list<std::string> ListAPIs()
	{
		return std::list<std::string> {APINAME};
	}
	static const TCHAR* LongAPIName(const std::string& name)
	{
		return TEXT(APINAME);
	}
	static int Configure(int port, const std::string& api, void *data);
	static int Freeze(int mode, USBDevice *dev, void *data);
};

#endif
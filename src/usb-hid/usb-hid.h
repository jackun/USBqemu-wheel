#pragma once
#include "../configuration.h"
#include "qemu-usb/hid.h"

enum HIDType
{
	HIDTYPE_KBD,
	HIDTYPE_MOUSE,
};

class UsbHID
{
public:
	UsbHID(int port) : mPort(port) {}
	virtual ~UsbHID() {}
	virtual int Open() = 0;
	virtual int Close() = 0;
//	virtual int TokenIn(uint8_t *buf, int len) = 0;
	virtual int TokenOut(const uint8_t *data, int len) = 0;
	virtual int Reset() = 0;

	virtual int Port() { return mPort; }
	virtual void Port(int port) { mPort = port; }
	virtual void SetHIDState(HIDState *hs) { mHIDState = hs; }
	virtual void SetHIDType(HIDType t) { mHIDType = t; }

protected:
	int mPort;
	HIDState *mHIDState;
	HIDType mHIDType;
};
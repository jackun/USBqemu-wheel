#ifndef USBMICUX16_H
#define USBMICUX16_H
#include "../deviceproxy.h"
#include "midideviceproxy.h"

struct USBDevice;

namespace usb_midi {
class MidiUx16Device
{
public:
	virtual ~MidiUx16Device() {}
	static USBDevice* CreateDevice(int port);
	static USBDevice* CreateDevice(int port, const std::string& api);
	static const TCHAR* Name()
	{
		return TEXT("Yamaha UX16 USB-MIDI Interface (Drummania)");
	}
	static const char* TypeName()
	{
		return "midikbd_ux16";
	}
	static std::list<std::string> ListAPIs()
	{
		return RegisterMidiDevice::instance().Names();
	}
	static const TCHAR* LongAPIName(const std::string& name)
	{
		auto proxy = RegisterMidiDevice::instance().Proxy(name);
		if (proxy)
			return proxy->Name();
		return nullptr;
	}
	static int Configure(int port, const std::string& api, void* data);
	static int Freeze(int mode, USBDevice* dev, void* data);
};

class MidiPc300Device : public MidiUx16Device
{
public:
	virtual ~MidiPc300Device() {}
	static USBDevice* CreateDevice(int port);
	static const char* TypeName()
	{
		return "midikbd_pc300";
	}
	static const TCHAR* Name()
	{
		return TEXT("Roland PC-300 MIDI Keyboard (Keyboardmania)");
	}
};

}
#endif

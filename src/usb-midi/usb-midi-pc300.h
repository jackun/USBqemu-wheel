#ifndef USBMICPC300_H
#define USBMICPC300_H
#include "../deviceproxy.h"
#include "midideviceproxy.h"

struct USBDevice;

namespace usb_midi_pc300 {
class MidiPc300Device
{
public:
	virtual ~MidiPc300Device() {}
	static USBDevice* CreateDevice(int port);
	static USBDevice* CreateDevice(int port, const std::string& api);
	static const TCHAR* Name()
	{
		return TEXT("Roland PC-300 MIDI Keyboard");
	}
	static const char* TypeName()
	{
		return "midikbd_pc300";
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
	static int Configure(int port, const std::string& api, void *data);
	static int Freeze(int mode, USBDevice *dev, void *data);
	static void Initialize()
	{
		RegisterMidiDevice::Initialize();
	}
};

}
#endif

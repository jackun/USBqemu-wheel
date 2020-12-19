#ifndef USBMICUX16_H
#define USBMICUX16_H
#include "../deviceproxy.h"
#include "midideviceproxy.h"

struct USBDevice;

namespace usb_midi {
	namespace usb_midi_ux16 {
		class MidiUx16Device
		{
		public:
			virtual ~MidiUx16Device() {}
			static USBDevice* CreateDevice(int port);
			static USBDevice* CreateDevice(int port, const std::string& api);
			static const TCHAR* Name()
			{
				return TEXT("Yamaha UX16 USB-MIDI Interface");
			}
			static const char* TypeName()
			{
				return "midikbd_ux16";
			}
			static std::list<std::string> ListAPIs()
			{
				return usb_midi::RegisterMidiDevice::instance().Names();
			}
			static const TCHAR* LongAPIName(const std::string& name)
			{
				auto proxy = usb_midi::RegisterMidiDevice::instance().Proxy(name);
				if (proxy)
					return proxy->Name();
				return nullptr;
			}
			static int Configure(int port, const std::string& api, void* data);
			static int Freeze(int mode, USBDevice* dev, void* data);
		};

	}
}
#endif

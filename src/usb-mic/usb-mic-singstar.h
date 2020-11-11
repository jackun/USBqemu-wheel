#ifndef USBMICSINGSTAR_H
#define USBMICSINGSTAR_H
#include "../deviceproxy.h"
#include "audiodeviceproxy.h"

struct USBDevice;

namespace usb_mic
{
	class SingstarDevice
	{
	public:
		virtual ~SingstarDevice() {}
		static USBDevice* CreateDevice(int port);
		static USBDevice* CreateDevice(int port, const std::string& api,
									   bool only_mono = false);
		static const TCHAR* Name() { return TEXT("Singstar"); }
		static const char* TypeName() { return "singstar"; }
		static std::list<std::string> ListAPIs()
		{
			return RegisterAudioDevice::instance().Names();
		}
		static const TCHAR* LongAPIName(const std::string& name)
		{
			auto proxy = RegisterAudioDevice::instance().Proxy(name);
			if (proxy)
				return proxy->Name();
			return nullptr;
		}
		static int Configure(int port, const std::string& api, void* data);
		static int Freeze(int mode, USBDevice* dev, void* data);
		static std::vector<std::string> SubTypes()
		{
			return {};
		}
	};

	class LogitechMicDevice : public SingstarDevice
	{
	public:
		virtual ~LogitechMicDevice() {}
		static USBDevice* CreateDevice(int port);
		static const char* TypeName() { return "logitech_usbmic"; }
		static const TCHAR* Name() { return TEXT("Logitech USB Mic"); }
	};

	// Konami Karaoke Revolution (NTSC-J) (RU042)
	class AK5370MicDevice : public SingstarDevice
	{
	public:
		virtual ~AK5370MicDevice() {}
		static USBDevice* CreateDevice(int port);
		static const char* TypeName() { return "ak5370_mic"; }
		static const TCHAR* Name() { return TEXT("Karaoke Revolution Mic"); }
	};

} // namespace usb_mic
#endif

#ifndef USBMICSINGSTAR_H
#define USBMICSINGSTAR_H
#include "../deviceproxy.h"
#include "audiodeviceproxy.h"

struct USBDevice;

namespace usb_mic_singstar {
class SingstarDevice : public Device
{
public:
	virtual ~SingstarDevice() {}
	static USBDevice* CreateDevice(int port);
	static USBDevice* CreateDevice(int port, const std::string& api);
	static const TCHAR* Name()
	{
		return TEXT("Singstar");
	}
	static const char* TypeName()
	{
		return "singstar";
	}
	static std::list<std::string> ListAPIs()
	{
		return RegisterAudioDevice::instance().Names();
	}
	static const TCHAR* LongAPIName(const std::string& name)
	{
		return RegisterAudioDevice::instance().Proxy(name)->Name();
	}
	static int Configure(int port, const std::string& api, void *data);
	static std::vector<CONFIGVARIANT> GetSettings(const std::string &api);
};
};
#endif

#ifndef USBMICSINGSTAR_H
#define USBMICSINGSTAR_H
#include "../deviceproxy.h"
#include "audiodeviceproxy.h"

struct USBDevice;

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
	static std::list<std::string> APIs()
	{
		return RegisterAudioDevice::instance().Names();
	}
	static const TCHAR* LongAPIName(const std::string& name)
	{
		return RegisterAudioDevice::instance().Proxy(name)->Name();
	}
	static int Configure(int port, std::string api, void *data);
	static std::vector<CONFIGVARIANT> GetSettings(const std::string &api);
};

#endif

#ifndef USBMICSINGSTAR_H
#define USBMICSINGSTAR_H
#include "../deviceproxy.h"
#include "audiosourceproxy.h"

struct USBDevice;

class SingstarDevice : public Device
{
public:
	virtual ~SingstarDevice() {}
	static USBDevice* CreateDevice(int port);
	static const TCHAR* Name()
	{
		return TEXT("Singstar");
	}
	static std::list<std::string> APIs()
	{
		return RegisterAudioSource::instance().Names();
	}
	static const TCHAR* LongAPIName(const std::string& name)
	{
		return RegisterAudioSource::instance().Proxy(name)->Name();
	}
	static int Configure(int port, std::string api, void *data);
	static std::vector<CONFIGVARIANT> GetSettings(const std::string &api);
};

#endif
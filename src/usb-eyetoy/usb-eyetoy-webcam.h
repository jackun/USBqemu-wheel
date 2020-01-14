#ifndef USBEYETOYWEBCAM_H
#define USBEYETOYWEBCAM_H

#include "../qemu-usb/vl.h"
#include "../configuration.h"
#include "../deviceproxy.h"

namespace usb_eyetoy {

class EyeToyWebCamDevice
{
public:
	virtual ~EyeToyWebCamDevice() {}
	static USBDevice* CreateDevice(int port);
	static const TCHAR* Name()
	{
		return TEXT("EyeToy (Webcam Mode)"); //TODO better name
	}
	static const char* TypeName()
	{
		return "eyetoy_webcam";
	}
	static std::list<std::string> ListAPIs();
	static const TCHAR* LongAPIName(const std::string& name);
	static int Configure(int port, const std::string& api, void *data);
	static int Freeze(int mode, USBDevice *dev, void *data);
};

} //namespace
#endif
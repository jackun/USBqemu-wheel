#include "videodeviceproxy.h"

void usb_eyetoy::RegisterVideoDevice::Register()
{
	auto& inst = RegisterVideoDevice::instance();
	//inst.Add(some_win32::APINAME, new VideoDeviceProxy<some_win32::SomeWin32WebCam>());
}
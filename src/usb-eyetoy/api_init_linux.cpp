#include "videodeviceproxy.h"

void usb_eyetoy::RegisterVideoDevice::Register()
{
	auto& inst = RegisterVideoDevice::instance();
	//inst.Add(v4l::APINAME, new VideoDeviceProxy<v4l::V4LWebCam>());
}
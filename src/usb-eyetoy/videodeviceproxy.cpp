#include "videodeviceproxy.h"

usb_eyetoy::VideoDeviceProxyBase::VideoDeviceProxyBase(const std::string& name)
{
	RegisterVideoDevice::instance().Add(name, this);
}

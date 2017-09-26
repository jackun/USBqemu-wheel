#include "videodeviceproxy.h"

VideoDeviceProxyBase::VideoDeviceProxyBase(const std::string& name)
{
	RegisterVideoDevice::instance().Add(name, this);
}

#include "deviceproxy.h"

RegisterDevice *RegisterDevice::registerDevice = nullptr;

DeviceProxyBase::DeviceProxyBase(DeviceType key)
{
	OSDebugOut("ctor (type %d) @ %p\n", key, this);
	//RegisterDevice::instance().Add(key, this);
}

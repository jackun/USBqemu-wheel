#include "deviceproxy.h"

DeviceProxyBase::DeviceProxyBase(DeviceType key)
{
	OSDebugOut("ctor (type %d)\n", key);
	RegisterDevice::instance().Add(key, this);
}

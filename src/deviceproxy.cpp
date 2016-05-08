#include "deviceproxy.h"

DeviceProxyBase::DeviceProxyBase(DeviceKey key)
{
	RegisterDevice::instance().Add(key, this);
}

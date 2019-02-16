#include "hidproxy.h"

UsbHIDProxyBase::UsbHIDProxyBase(const std::string& name)
{
	RegisterUsbHID::instance().Add(name, this);
}

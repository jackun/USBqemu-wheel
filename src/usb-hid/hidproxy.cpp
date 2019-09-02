#include "hidproxy.h"

usb_hid::UsbHIDProxyBase::UsbHIDProxyBase(const std::string& name)
{
	RegisterUsbHID::instance().Add(name, this);
}

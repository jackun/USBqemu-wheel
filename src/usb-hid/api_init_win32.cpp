#include "hidproxy.h"
#include "raw/rawinput.h"

void usb_hid::RegisterUsbHID::Initialize()
{
	auto& inst = RegisterUsbHID::instance();
	inst.Add(usb_hid::raw::APINAME, new UsbHIDProxy<usb_hid::raw::RawInput>());
}
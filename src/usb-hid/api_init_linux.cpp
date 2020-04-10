#include "hidproxy.h"
#include "evdev/evdev.h"

void usb_hid::RegisterUsbHID::Initialize()
{
	auto& inst = RegisterUsbHID::instance();
	inst.Add(usb_hid::evdev::APINAME, new UsbHIDProxy<usb_hid::evdev::EvDev>());
}
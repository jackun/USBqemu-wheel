#include "deviceproxy.h"
#include "usb-pad/usb-pad.h"
#include "usb-msd/usb-msd.h"
#include "usb-mic/usb-mic-singstar.h"
#include "usb-mic/usb-headset.h"
#include "usb-hid/usb-hid.h"
#include "usb-eyetoy/usb-eyetoy-webcam.h"

void RegisterDevice::Initialize()
{
	auto& inst = RegisterDevice::instance();
	inst.Add(DEVTYPE_PAD, new DeviceProxy<usb_pad::PadDevice>());
	inst.Add(DEVTYPE_MSD, new DeviceProxy<usb_msd::MsdDevice>());
	inst.Add(DEVTYPE_SINGSTAR, new DeviceProxy<usb_mic_singstar::SingstarDevice>());
	inst.Add(DEVTYPE_LOGITECH_MIC, new DeviceProxy<usb_mic_singstar::LogitechMicDevice>());
	inst.Add(DEVTYPE_LOGITECH_HEADSET, new DeviceProxy<usb_headset::HeadsetDevice>());
	inst.Add(DEVTYPE_HIDKBD, new DeviceProxy<usb_hid::HIDKbdDevice>());
	inst.Add(DEVTYPE_HIDMOUSE, new DeviceProxy<usb_hid::HIDMouseDevice>());
	inst.Add(DEVTYPE_RBKIT, new DeviceProxy<usb_pad::RBDrumKitDevice>());
	//inst.Add(DEVTYPE_EYETOY_WEBCAM, new DeviceProxy<usb_eyetoy::EyeToyWebCamDevice>()); // not ready
}
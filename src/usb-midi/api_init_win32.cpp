#include "midideviceproxy.h"
#include "mididev-keyboards.h"

void usb_midi::RegisterMidiDevice::Register()
{
	auto& inst = RegisterMidiDevice::instance();
	inst.Add(mididev_keyboards::APINAME, new MidiDeviceProxy<mididev_keyboards::KeyboardMidiDevice>());
}
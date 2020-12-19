#include "midideviceproxy.h"
#include "win32/win32midi.h"

void usb_midi::RegisterMidiDevice::Register()
{
	auto& inst = RegisterMidiDevice::instance();
	inst.Add(mididev_keyboards::APINAME, new MidiDeviceProxy<mididev_keyboards::Win32MidiDevice>());
}
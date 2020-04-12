#include "midideviceproxy.h"
#include "mididev-keyboards.h"

void RegisterMidiDevice::Initialize()
{
	auto& inst = RegisterMidiDevice::instance();
	inst.Add(mididev_keyboards::APINAME, new MidiDeviceProxy<mididev_keyboards::KeyboardMidiDevice>());
}
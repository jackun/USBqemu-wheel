#include "midideviceproxy.h"
#include "mididev-noop.h"

void RegisterMidiDevice::Initialize()
{
	auto& inst = RegisterMidiDevice::instance();
	inst.Add(mididev_noop::APINAME, new MidiDeviceProxy<mididev_noop::NoopMidiDevice>());
}
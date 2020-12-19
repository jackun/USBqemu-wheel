#include "midideviceproxy.h"

usb_midi::MidiDeviceProxyBase::MidiDeviceProxyBase(const std::string& name)
{
	RegisterMidiDevice::instance().Add(name, this);
}

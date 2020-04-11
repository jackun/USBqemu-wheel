#include "midideviceproxy.h"

MidiDeviceProxyBase::MidiDeviceProxyBase(const std::string& name)
{
	RegisterMidiDevice::instance().Add(name, this);
}

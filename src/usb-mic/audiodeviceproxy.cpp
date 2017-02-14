#include "audiodeviceproxy.h"

AudioDeviceProxyBase::AudioDeviceProxyBase(const std::string& name)
{
	RegisterAudioDevice::instance().Add(name, this);
}

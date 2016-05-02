#include "audiosourceproxy.h"
#include <iostream>

AudioSourceProxyBase::AudioSourceProxyBase(std::string name)
{
	std::cout << "AudioSourceProxyBase ctor: " << name << std::endl;
	RegisterAudioSource::instance().Add(name, this);
}

#include "audiosourceproxy.h"

AudioSourceProxyBase::AudioSourceProxyBase(std::string name)
{
	RegisterAudioSource::instance().Add(name, this);
}

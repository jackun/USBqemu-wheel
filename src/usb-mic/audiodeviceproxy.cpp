#include "audiosourceproxy.h"

AudioSourceProxyBase::AudioSourceProxyBase(const std::string& name)
{
	RegisterAudioSource::instance().Add(name, this);
}

#ifndef AUDIOSOURCEPROXY_H
#define AUDIOSOURCEPROXY_H
#include "audiosrc.h"
#include <string>
#include <map>
#include <list>
#include <algorithm>
#include <iterator>

class AudioSourceProxyBase
{
	public:
	AudioSourceProxyBase(std::string name);
	virtual AudioSource* CreateObject(AudioDeviceInfo& dev) const = 0; //Can be generalized? Probably not
	virtual const wchar_t* Name() const = 0;
	virtual void AudioDevices(std::vector<AudioDeviceInfo> &devices) const = 0;
	virtual bool AudioInit() = 0;
	virtual void AudioDeinit() = 0;
};

template <class T>
class AudioSourceProxy : public AudioSourceProxyBase
{
	public:
	AudioSourceProxy(std::string name): AudioSourceProxyBase(name) {} //Why can't it automagically, ugh
	AudioSource* CreateObject(AudioDeviceInfo& dev) const { return new T(dev); }
	virtual const wchar_t* Name() const
	{
		return T::Name();
	}
	virtual void AudioDevices(std::vector<AudioDeviceInfo> &devices) const
	{
		T::AudioDevices(devices);
	}
	virtual bool AudioInit()
	{
		return T::AudioInit();
	}
	virtual void AudioDeinit()
	{
		T::AudioDeinit();
	}
};

struct SelectKey {
	template <typename F, typename S>
	F operator()(const std::pair<const F, S> &x) const { return x.first; }
};

class RegisterAudioSource
{
	public:
	typedef std::map<std::string, AudioSourceProxyBase* > RegisterAudioSourceMap;
	static RegisterAudioSource& instance() {
		static RegisterAudioSource registerAudioSource;
		return registerAudioSource;
	}

	void Add(const std::string name, AudioSourceProxyBase* creator)
	{
		registerAudioSourceMap[name] = creator;
	}

	AudioSourceProxyBase* AudioSource(std::string name)
	{
		return registerAudioSourceMap[name];
	}
	
	std::list<std::string> Names() const
	{
		std::list<std::string> nameList;
		std::transform(
			registerAudioSourceMap.begin(), registerAudioSourceMap.end(),
			std::back_inserter(nameList),
			SelectKey());
		return nameList;
	}

	const RegisterAudioSourceMap& Map() const
	{
		return registerAudioSourceMap;
	}
	
private:
	RegisterAudioSourceMap registerAudioSourceMap;
};

#define REGISTER_AUDIOSRC(name,cls) AudioSourceProxy<cls> g##cls##Proxy(#name)
#endif

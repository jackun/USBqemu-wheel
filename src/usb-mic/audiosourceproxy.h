#ifndef AUDIOSOURCEPROXY_H
#define AUDIOSOURCEPROXY_H
#include "audiosrc.h"
#include <string>
#include <map>
#include <list>
#include <algorithm>
#include <iterator>
#include "../helpers.h"
#include "../configuration.h"
#include "../proxybase.h"

class AudioSourceError : public std::runtime_error
{
public:
	AudioSourceError(const char* msg) : std::runtime_error(msg) {}
	virtual ~AudioSourceError() throw () {}
};

class AudioSourceProxyBase : public ProxyBase
{
	public:
	AudioSourceProxyBase(std::string name);
	virtual AudioSource* CreateObject(int port, int mic) const = 0; //Can be generalized? Probably not
	virtual void AudioDevices(std::vector<AudioDeviceInfo> &devices) const = 0;
	virtual bool AudioInit() = 0;
	virtual void AudioDeinit() = 0;
};

template <class T>
class AudioSourceProxy : public AudioSourceProxyBase
{
	public:
	AudioSourceProxy(std::string name): AudioSourceProxyBase(name) {} //Why can't it automagically, ugh
	AudioSource* CreateObject(int port, int mic) const
	{
		try
		{
			return new T(port, mic);
		}
		catch(AudioSourceError& err)
		{
			(void)err;
			return nullptr;
		}
	}
	virtual const wchar_t* Name() const
	{
		return T::Name();
	}
	virtual int Configure(int port, void *data)
	{
		return T::Configure(port, data);
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
	virtual std::vector<CONFIGVARIANT> GetSettings()
	{
		return T::GetSettings();
	}
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

	AudioSourceProxyBase* Proxy(std::string name)
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

	std::string Name(int idx) const
	{
		auto it = registerAudioSourceMap.begin();
		std::advance(it, idx);
		if (it != registerAudioSourceMap.end())
			return std::string(it->first);
		return std::string();
	}

	const RegisterAudioSourceMap& Map() const
	{
		return registerAudioSourceMap;
	}
	
private:
	RegisterAudioSourceMap registerAudioSourceMap;
};

#define REGISTER_AUDIOSRC(name,cls) AudioSourceProxy<cls> g##cls##Proxy(name)
#endif

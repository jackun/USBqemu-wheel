#ifndef AUDIODEVICEPROXY_H
#define AUDIODEVICEPROXY_H
#include "audiodev.h"
#include <string>
#include <map>
#include <list>
#include <algorithm>
#include <iterator>
#include "../helpers.h"
#include "../configuration.h"
#include "../proxybase.h"
#include "../osdebugout.h"

class AudioDeviceError : public std::runtime_error
{
public:
	AudioDeviceError(const char* msg) : std::runtime_error(msg) {}
	virtual ~AudioDeviceError() throw () {}
};

class AudioDeviceProxyBase : public ProxyBase
{
	AudioDeviceProxyBase(const AudioDeviceProxyBase&) = delete;

	public:
	AudioDeviceProxyBase(const std::string& name);
	virtual AudioDevice* CreateObject(int port, int mic, AudioDir dir) const = 0; //Can be generalized? Probably not
	virtual void AudioDevices(std::vector<AudioDeviceInfo> &devices, AudioDir) const = 0;
	virtual bool AudioInit() = 0;
	virtual void AudioDeinit() = 0;
};

template <class T>
class AudioDeviceProxy : public AudioDeviceProxyBase
{
	AudioDeviceProxy(const AudioDeviceProxy&) = delete;

	public:
	AudioDeviceProxy(const std::string& name): AudioDeviceProxyBase(name) {} //Why can't it automagically, ugh
	AudioDevice* CreateObject(int port, int mic, AudioDir dir) const
	{
		try
		{
			return new T(port, mic, dir);
		}
		catch(AudioDeviceError& err)
		{
			OSDebugOut(TEXT("AudioDevice port %d mic %d: %") TEXT(SFMTs) TEXT("\n"), port, mic, err.what());
			(void)err;
			return nullptr;
		}
	}
	virtual const TCHAR* Name() const
	{
		return T::Name();
	}
	virtual int Configure(int port, void *data)
	{
		return T::Configure(port, data);
	}
	virtual void AudioDevices(std::vector<AudioDeviceInfo> &devices, AudioDir dir) const
	{
		T::AudioDevices(devices, dir);
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

class RegisterAudioDevice
{
	RegisterAudioDevice(const RegisterAudioDevice&) = delete;
	RegisterAudioDevice() {}

	public:
	typedef std::map<std::string, AudioDeviceProxyBase* > RegisterAudioDeviceMap;
	static RegisterAudioDevice& instance() {
		static RegisterAudioDevice registerAudioDevice;
		return registerAudioDevice;
	}

	~RegisterAudioDevice() {}

	void Add(const std::string& name, AudioDeviceProxyBase* creator)
	{
		registerAudioDeviceMap[name] = creator;
	}

	AudioDeviceProxyBase* Proxy(const std::string& name)
	{
		return registerAudioDeviceMap[name];
	}
	
	std::list<std::string> Names() const
	{
		std::list<std::string> nameList;
		std::transform(
			registerAudioDeviceMap.begin(), registerAudioDeviceMap.end(),
			std::back_inserter(nameList),
			SelectKey());
		return nameList;
	}

	std::string Name(int idx) const
	{
		auto it = registerAudioDeviceMap.begin();
		std::advance(it, idx);
		if (it != registerAudioDeviceMap.end())
			return std::string(it->first);
		return std::string();
	}

	const RegisterAudioDeviceMap& Map() const
	{
		return registerAudioDeviceMap;
	}
	
private:
	RegisterAudioDeviceMap registerAudioDeviceMap;
};

#define REGISTER_AUDIODEV(name,cls) AudioDeviceProxy<cls> g##cls##Proxy(name)
#endif

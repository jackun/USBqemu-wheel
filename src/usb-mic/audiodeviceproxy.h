#ifndef AUDIODEVICEPROXY_H
#define AUDIODEVICEPROXY_H
#include <memory>
#include <string>
#include <map>
#include <list>
#include <algorithm>
#include <iterator>
#include "../helpers.h"
#include "../configuration.h"
#include "../proxybase.h"
#include "../osdebugout.h"
#include "audiodev.h"

class AudioDeviceError : public std::runtime_error
{
public:
	AudioDeviceError(const char* msg) : std::runtime_error(msg) {}
	virtual ~AudioDeviceError() throw () {}
};

class AudioDeviceProxyBase : public ProxyBase
{
	AudioDeviceProxyBase(const AudioDeviceProxyBase&) = delete;
	AudioDeviceProxyBase& operator=(const AudioDeviceProxyBase&) = delete;

	public:
	AudioDeviceProxyBase() {};
	AudioDeviceProxyBase(const std::string& name);
	virtual AudioDevice* CreateObject(int port, const char* dev_type, int mic, AudioDir dir) const = 0; //Can be generalized? Probably not
	virtual void AudioDevices(std::vector<AudioDeviceInfo> &devices, AudioDir) const = 0;
	virtual bool AudioInit() = 0;
	virtual void AudioDeinit() = 0;
};

template <class T>
class AudioDeviceProxy : public AudioDeviceProxyBase
{
	AudioDeviceProxy(const AudioDeviceProxy&) = delete;

	public:
	AudioDeviceProxy() {}
	AudioDeviceProxy(const std::string& name): AudioDeviceProxyBase(name) {} //Why can't it automagically, ugh
	~AudioDeviceProxy() { OSDebugOut(TEXT("%p\n"), this); }

	AudioDevice* CreateObject(int port, const char* dev_type, int mic, AudioDir dir) const
	{
		try
		{
			return new T(port, dev_type, mic, dir);
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
	virtual int Configure(int port, const char* dev_type, void *data)
	{
		return T::Configure(port, dev_type, data);
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
};

class RegisterAudioDevice
{
	RegisterAudioDevice(const RegisterAudioDevice&) = delete;
	RegisterAudioDevice() {}

	public:
	typedef std::map<std::string, std::unique_ptr<AudioDeviceProxyBase> > RegisterAudioDeviceMap;
	static RegisterAudioDevice& instance() {
		static RegisterAudioDevice registerAudioDevice;
		return registerAudioDevice;
	}

	~RegisterAudioDevice() { Clear(); OSDebugOut("%p\n", this); }

	static void Initialize();

	void Clear()
	{
		registerAudioDeviceMap.clear();
	}

	void Add(const std::string& name, AudioDeviceProxyBase* creator)
	{
		registerAudioDeviceMap[name] = std::unique_ptr<AudioDeviceProxyBase>(creator);
	}

	AudioDeviceProxyBase* Proxy(const std::string& name)
	{
		return registerAudioDeviceMap[name].get();
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

#define REGISTER_AUDIODEV(name,cls) //AudioDeviceProxy<cls> g##cls##Proxy(name)
#endif

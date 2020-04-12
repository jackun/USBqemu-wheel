#ifndef MIDIDEVICEPROXY_H
#define MIDIDEVICEPROXY_H
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
#include "mididev.h"

class MidiDeviceError : public std::runtime_error
{
public:
	MidiDeviceError(const char* msg) : std::runtime_error(msg) {}
	virtual ~MidiDeviceError() throw () {}
};

class MidiDeviceProxyBase : public ProxyBase
{
	MidiDeviceProxyBase(const MidiDeviceProxyBase&) = delete;
	MidiDeviceProxyBase& operator=(const MidiDeviceProxyBase&) = delete;

	public:
	MidiDeviceProxyBase() {};
	MidiDeviceProxyBase(const std::string& name);
	virtual MidiDevice* CreateObject(int port, const char* dev_type) const = 0; //Can be generalized? Probably not
	virtual void MidiDevices(std::vector<MidiDeviceInfo> &devices) const = 0;
	virtual bool AudioInit() = 0;
	virtual void AudioDeinit() = 0;
};

template <class T>
class MidiDeviceProxy : public MidiDeviceProxyBase
{
	MidiDeviceProxy(const MidiDeviceProxy&) = delete;

	public:
	MidiDeviceProxy() {}
	MidiDeviceProxy(const std::string& name): MidiDeviceProxyBase(name) {} //Why can't it automagically, ugh
	~MidiDeviceProxy() { OSDebugOut(TEXT("%p\n"), this); }

	MidiDevice* CreateObject(int port, const char* dev_type) const
	{
		try
		{
			return new T(port, dev_type);
		}
		catch(MidiDeviceError& err)
		{
			OSDebugOut(TEXT("MidiDevice port %d: %") TEXT(SFMTs) TEXT("\n"), port, err.what());
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
	virtual void MidiDevices(std::vector<MidiDeviceInfo> &devices) const
	{
		T::MidiDevices(devices);
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

class RegisterMidiDevice
{
	RegisterMidiDevice(const RegisterMidiDevice&) = delete;
	RegisterMidiDevice() {}

	public:
	typedef std::map<std::string, std::unique_ptr<MidiDeviceProxyBase> > RegisterMidiDeviceMap;
	static RegisterMidiDevice& instance() {
		static RegisterMidiDevice registerMidiDevice;
		return registerMidiDevice;
	}

	~RegisterMidiDevice() { Clear(); OSDebugOut("%p\n", this); }

	static void Initialize();

	void Clear()
	{
		registerMidiDeviceMap.clear();
	}

	void Add(const std::string& name, MidiDeviceProxyBase* creator)
	{
		registerMidiDeviceMap[name] = std::unique_ptr<MidiDeviceProxyBase>(creator);
	}

	MidiDeviceProxyBase* Proxy(const std::string& name)
	{
		return registerMidiDeviceMap[name].get();
	}

	std::list<std::string> Names() const
	{
		std::list<std::string> nameList;
		std::transform(
			registerMidiDeviceMap.begin(), registerMidiDeviceMap.end(),
			std::back_inserter(nameList),
			SelectKey());
		return nameList;
	}

	std::string Name(int idx) const
	{
		auto it = registerMidiDeviceMap.begin();
		std::advance(it, idx);
		if (it != registerMidiDeviceMap.end())
			return std::string(it->first);
		return std::string();
	}

	const RegisterMidiDeviceMap& Map() const
	{
		return registerMidiDeviceMap;
	}

private:
	RegisterMidiDeviceMap registerMidiDeviceMap;
};

#define REGISTER_MIDIDEV(name,cls) //MidiDeviceProxy<cls> g##cls##Proxy(name)
#endif

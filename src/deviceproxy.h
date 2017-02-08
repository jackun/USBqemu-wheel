#ifndef DEVICEPROXY_H
#define DEVICEPROXY_H
#include "USB.h"
#include "configuration.h"
#include <string>
#include <map>
#include <list>
#include <algorithm>
#include <iterator>
//#include <memory>
#include "helpers.h"
#include "proxybase.h"

struct DeviceKey
{
	DeviceKey(int i, std::string name): index(i), name(name) {}
	bool operator<(const DeviceKey& a) const
	{
		return index < a.index;
	}
	bool operator==(const DeviceKey& a) const
	{
		return name == a.name;
	}
	int index;
	std::string name;
};

struct SelectDeviceKey {
	template <typename S>
	std::string operator()(const std::pair<const DeviceKey, S> &x) const { return x.first.name; }
};

class DeviceError : public std::runtime_error
{
	public:
	DeviceError(const char* msg) : std::runtime_error(msg) {}
	virtual ~DeviceError() throw () {}
};

class Device
{
	public:
	virtual ~Device() {}
};

class DeviceProxyBase
{
	public:
	DeviceProxyBase(DeviceKey key);
	virtual ~DeviceProxyBase() {}
	virtual USBDevice* CreateDevice(int port) = 0;
	virtual const TCHAR* Name() const = 0;
	virtual int Configure(int port, std::string api, void *data) = 0;
	virtual std::list<std::string> APIs() = 0;
	virtual const TCHAR* LongAPIName(const std::string& name) = 0;
	virtual std::vector<CONFIGVARIANT> GetSettings(const std::string &api) = 0;

	virtual bool IsValidAPI(const std::string& api)
	{
		std::list<std::string> apis = APIs();
		auto it = std::find(apis.begin(), apis.end(), api);
		if (it != apis.end())
			return true;
		return false;
	}
};

template <class T>
class DeviceProxy : public DeviceProxyBase
{
	public:
	DeviceProxy(DeviceKey key): DeviceProxyBase(key) {}
	virtual ~DeviceProxy() {}
	virtual USBDevice* CreateDevice(int port)
	{
		return T::CreateDevice(port);
	}
	virtual const TCHAR* Name() const
	{
		return T::Name();
	}
	virtual int Configure(int port, std::string api, void *data)
	{
		return T::Configure(port, api, data);
	}
	virtual std::list<std::string> APIs()
	{
		return T::APIs();
	}
	virtual const TCHAR* LongAPIName(const std::string& name)
	{
		return T::LongAPIName(name);
	}
	virtual std::vector<CONFIGVARIANT> GetSettings(const std::string &api)
	{
		return T::GetSettings(api);
	}
};

class RegisterDevice
{
	RegisterDevice(const RegisterDevice&) = delete;
	RegisterDevice() {}

	public:
	typedef std::map<DeviceKey, DeviceProxyBase* > RegisterDeviceMap;
	static RegisterDevice& instance() {
		static RegisterDevice registerDevice;
		return registerDevice;
	}

	~RegisterDevice() {}

	void Add(const DeviceKey key, DeviceProxyBase* creator)
	{
		registerDeviceMap[key] = creator;
	}

	DeviceProxyBase* Device(const std::string& name)
	{
		//return registerDeviceMap[name];
		/*for (auto& k : registerDeviceMap)
			if(k.first.name == name)
				return k.second;
		return nullptr;*/
		auto proxy = std::find_if(registerDeviceMap.begin(),
			registerDeviceMap.end(),
			[&name](RegisterDeviceMap::value_type val) -> bool
		{
			return val.first.name == name;
		});
		if (proxy != registerDeviceMap.end())
			return proxy->second;
		return nullptr;
	}

	DeviceProxyBase* Device(int index)
	{
		auto it = registerDeviceMap.begin();
		std::advance(it, index);
		if (it != registerDeviceMap.end())
			return it->second;
		return nullptr;
	}

	std::list<std::string> Names() const
	{
		std::list<std::string> nameList;
		std::transform(
			registerDeviceMap.begin(), registerDeviceMap.end(),
			std::back_inserter(nameList),
			SelectDeviceKey());
		return nameList;
	}

	std::string Name(int index) const
	{
		auto it = registerDeviceMap.begin();
		std::advance(it, index);
		if (it != registerDeviceMap.end())
			return std::string(it->first.name);
		return std::string();
	}

	const RegisterDeviceMap& Map() const
	{
		return registerDeviceMap;
	}

	private:
	RegisterDeviceMap registerDeviceMap;
};

#define REGISTER_DEVICE(idx,name,cls) DeviceProxy<cls> g##cls##Proxy(DeviceKey(idx, name))
//#define REGISTER_DEVICE(idx,name,cls) static std::unique_ptr< DeviceProxy<cls> > g##cls##Proxy(new DeviceProxy<cls>(DeviceKey(idx, name)))
#endif

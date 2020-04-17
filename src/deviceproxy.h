#ifndef DEVICEPROXY_H
#define DEVICEPROXY_H
#include "configuration.h"
#include <memory>
#include <string>
#include <map>
#include <list>
#include <algorithm>
#include <iterator>
//#include <memory>
#include "helpers.h"
#include "proxybase.h"
#include "qemu-usb/USBinternal.h"

void RegisterAPIs();
void UnregisterAPIs();

// also map key/array index
enum DeviceType
{
	DEVTYPE_NONE = -1,
	DEVTYPE_PAD = 0,
	DEVTYPE_MSD,
	DEVTYPE_SINGSTAR,
	DEVTYPE_LOGITECH_MIC,
	DEVTYPE_LOGITECH_HEADSET,
	DEVTYPE_HIDKBD,
	DEVTYPE_HIDMOUSE,
	DEVTYPE_RBKIT,
	DEVTYPE_EYETOY,
	DEVTYPE_EYETOY_WEBCAM,
};

struct SelectDeviceName {
	template <typename S>
	std::string operator()(const std::pair<const DeviceType, S> &x) const { return x.second->TypeName(); }
};

class DeviceError : public std::runtime_error
{
	public:
	DeviceError(const char* msg) : std::runtime_error(msg) {}
	virtual ~DeviceError() {}
};

class DeviceProxyBase
{
	public:
	DeviceProxyBase() {};
	virtual ~DeviceProxyBase() {}
	virtual USBDevice* CreateDevice(int port) = 0;
	virtual const TCHAR* Name() const = 0;
	virtual const char* TypeName() const = 0;
	virtual int Configure(int port, const std::string& api, void *data) = 0;
	virtual std::list<std::string> ListAPIs() = 0;
	virtual const TCHAR* LongAPIName(const std::string& name) = 0;
	virtual int Freeze(int mode, USBDevice *dev, void *data) = 0;

	virtual bool IsValidAPI(const std::string& api)
	{
		const std::list<std::string>& apis = ListAPIs();
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
	DeviceProxy() {}
	virtual ~DeviceProxy()
	{
		OSDebugOut(TEXT("%p\n"), this);
	}
	virtual USBDevice* CreateDevice(int port)
	{
		return T::CreateDevice(port);
	}
	virtual const TCHAR* Name() const
	{
		return T::Name();
	}
	virtual const char* TypeName() const
	{
		return T::TypeName();
	}
	virtual int Configure(int port, const std::string& api, void *data)
	{
		return T::Configure(port, api, data);
	}
	virtual std::list<std::string> ListAPIs()
	{
		return T::ListAPIs();
	}
	virtual const TCHAR* LongAPIName(const std::string& name)
	{
		return T::LongAPIName(name);
	}
	virtual int Freeze(int mode, USBDevice *dev, void *data)
	{
		return T::Freeze(mode, dev, data);
	}
};

template <class T>
class RegisterProxy
{
	RegisterProxy(const RegisterProxy&) = delete;
	RegisterProxy() {}

	public:
	typedef std::map<std::string, std::unique_ptr<T> > RegisterProxyMap;
	static RegisterProxy& instance() {
		static RegisterProxy registerProxy;
		return registerProxy;
	}

	virtual ~RegisterProxy() { Clear(); OSDebugOut("%p\n", this); }

	void Clear()
	{
		registerProxyMap.clear();
	}

	void Add(const std::string& name, T* creator)
	{
		registerProxyMap[name] = std::unique_ptr<T>(creator);
	}

	T* Proxy(const std::string& name)
	{
		return registerProxyMap[name].get();
	}

	std::list<std::string> Names() const
	{
		std::list<std::string> nameList;
		std::transform(
			registerProxyMap.begin(), registerProxyMap.end(),
			std::back_inserter(nameList),
			SelectKey());
		return nameList;
	}

	std::string Name(int idx) const
	{
		auto it = registerProxyMap.begin();
		std::advance(it, idx);
		if (it != registerProxyMap.end())
			return std::string(it->first);
		return std::string();
	}

	const RegisterProxyMap& Map() const
	{
		return registerProxyMap;
	}

private:
	RegisterProxyMap registerProxyMap;
};

class RegisterDevice
{
	RegisterDevice(const RegisterDevice&) = delete;
	RegisterDevice() {}
	static RegisterDevice *registerDevice;

	public:
	typedef std::map<DeviceType, std::unique_ptr<DeviceProxyBase> > RegisterDeviceMap;
	static RegisterDevice& instance() {
		if (!registerDevice)
			registerDevice = new RegisterDevice();
		return *registerDevice;
	}

	~RegisterDevice() { OSDebugOut("%p\n", this); }

	static void Register();
	void Unregister();

	void Add(DeviceType key, DeviceProxyBase* creator)
	{
		registerDeviceMap[key] = std::unique_ptr<DeviceProxyBase>(creator);
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
			[&name](const RegisterDeviceMap::value_type& val) -> bool
		{
			return val.second->TypeName() == name;
		});
		if (proxy != registerDeviceMap.end())
			return proxy->second.get();
		return nullptr;
	}

	DeviceProxyBase* Device(int index)
	{
		auto it = registerDeviceMap.begin();
		std::advance(it, index);
		if (it != registerDeviceMap.end())
			return it->second.get();
		return nullptr;
	}

	DeviceType Index(const std::string& name)
	{
		auto proxy = std::find_if(registerDeviceMap.begin(),
			registerDeviceMap.end(),
			[&name](RegisterDeviceMap::value_type& val) -> bool
		{
			return val.second->TypeName() == name;
		});
		if (proxy != registerDeviceMap.end())
			return proxy->first;
		return DEVTYPE_NONE;
	}

	std::list<std::string> Names() const
	{
		std::list<std::string> nameList;
		std::transform(
			registerDeviceMap.begin(), registerDeviceMap.end(),
			std::back_inserter(nameList),
			SelectDeviceName());
		return nameList;
	}

	std::string Name(int index) const
	{
		auto it = registerDeviceMap.begin();
		std::advance(it, index);
		if (it != registerDeviceMap.end())
			return it->second->TypeName();
		return std::string();
	}

	const RegisterDeviceMap& Map() const
	{
		return registerDeviceMap;
	}

	private:
	RegisterDeviceMap registerDeviceMap;
};

#endif

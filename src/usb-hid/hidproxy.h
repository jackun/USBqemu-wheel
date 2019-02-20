#ifndef USBHIDPROXY_H
#define USBHIDPROXY_H
#include <string>
#include <map>
#include <list>
#include <algorithm>
#include <iterator>
#include "usb-hid.h"
#include "../helpers.h"
#include "../proxybase.h"

class UsbHIDError : public std::runtime_error
{
public:
	UsbHIDError(const char* msg) : std::runtime_error(msg) {}
	virtual ~UsbHIDError() throw () {}
};

class UsbHIDProxyBase : public ProxyBase
{
	UsbHIDProxyBase(const UsbHIDProxyBase&) = delete;

	public:
	UsbHIDProxyBase(const std::string& name);
	virtual UsbHID* CreateObject(int port, const char* dev_type) const = 0;
	// ProxyBase::Configure is ignored
	virtual int Configure(int port, const char* dev_type, HIDType hid_type, void *data) = 0;
};

template <class T>
class UsbHIDProxy : public UsbHIDProxyBase
{
	UsbHIDProxy(const UsbHIDProxy&) = delete;

	public:
	UsbHIDProxy(const std::string& name): UsbHIDProxyBase(name) {}
	UsbHID* CreateObject(int port, const char* dev_type) const
	{
		try
		{
			return new T(port, dev_type);
		}
		catch(UsbHIDError& err)
		{
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
		return RESULT_CANCELED;
	}
	virtual int Configure(int port, const char* dev_type, HIDType hid_type, void *data)
	{
		return T::Configure(port, dev_type, hid_type, data);
	}
};

class RegisterUsbHID
{
	RegisterUsbHID(const RegisterUsbHID&) = delete;
	RegisterUsbHID() {}

	public:
	typedef std::map<std::string, UsbHIDProxyBase* > RegisterUsbHIDMap;
	static RegisterUsbHID& instance() {
		static RegisterUsbHID registerUsbHID;
		return registerUsbHID;
	}

	void Add(const std::string& name, UsbHIDProxyBase* creator)
	{
		registerUsbHIDMap[name] = creator;
	}

	UsbHIDProxyBase* Proxy(const std::string& name)
	{
		return registerUsbHIDMap[name];
	}

	std::list<std::string> Names() const
	{
		std::list<std::string> nameList;
		std::transform(
			registerUsbHIDMap.begin(), registerUsbHIDMap.end(),
			std::back_inserter(nameList),
			SelectKey());
		return nameList;
	}

	std::string Name(int idx) const
	{
		auto it = registerUsbHIDMap.begin();
		std::advance(it, idx);
		if (it != registerUsbHIDMap.end())
			return std::string(it->first);
		return std::string();
	}

	const RegisterUsbHIDMap& Map() const
	{
		return registerUsbHIDMap;
	}

private:
	RegisterUsbHIDMap registerUsbHIDMap;
};

#define REGISTER_USBHID(name,cls) UsbHIDProxy<cls> g##cls##Proxy(name)
#endif

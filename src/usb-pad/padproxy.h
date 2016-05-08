#ifndef PADPROXY_H
#define PADPROXY_H
#include <string>
#include <map>
#include <list>
#include <algorithm>
#include <iterator>
#include "usb-pad.h"
#include "../helpers.h"

class PadError : public std::runtime_error
{
public:
	PadError(const char* msg) : std::runtime_error(msg) {}
	virtual ~PadError() throw () {}
};

class PadProxyBase
{
	public:
	PadProxyBase(std::string name);
	virtual Pad* CreateObject() const = 0;
	virtual const wchar_t* Name() const = 0;
	virtual bool Configure(int port, void *data) = 0;
	virtual std::vector<CONFIGVARIANT> GetSettings() = 0;
};

template <class T>
class PadProxy : public PadProxyBase
{
	public:
	PadProxy(std::string name): PadProxyBase(name) {}
	Pad* CreateObject(/*std::string device*/) const
	{
		try
		{
			return new T;
		}
		catch(PadError& err)
		{
			(void)err;
			return nullptr;
		}
	}
	virtual const wchar_t* Name() const
	{
		return T::Name();
	}
	virtual bool Configure(int port, void *data)
	{
		return T::Configure(port, data);
	}
	virtual std::vector<CONFIGVARIANT> GetSettings()
	{
		return T::GetSettings();
	}
};

class RegisterPad
{
	public:
	typedef std::map<std::string, PadProxyBase* > RegisterPadMap;
	static RegisterPad& instance() {
		static RegisterPad registerPad;
		return registerPad;
	}

	void Add(const std::string name, PadProxyBase* creator)
	{
		registerPadMap[name] = creator;
	}

	PadProxyBase* Proxy(std::string name)
	{
		return registerPadMap[name];
	}
	
	std::list<std::string> Names() const
	{
		std::list<std::string> nameList;
		std::transform(
			registerPadMap.begin(), registerPadMap.end(),
			std::back_inserter(nameList),
			SelectKey());
		return nameList;
	}

	std::string Name(int idx) const
	{
		auto it = registerPadMap.begin();
		std::advance(it, idx);
		if (it != registerPadMap.end())
			return std::string(it->first);
		return std::string();
	}

	const RegisterPadMap& Map() const
	{
		return registerPadMap;
	}
	
private:
	RegisterPadMap registerPadMap;
};

#define REGISTER_PAD(name,cls) PadProxy<cls> g##cls##Proxy(#name)
#endif

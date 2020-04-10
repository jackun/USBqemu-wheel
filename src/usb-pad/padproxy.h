#ifndef PADPROXY_H
#define PADPROXY_H
#include <memory>
#include <string>
#include <map>
#include <list>
#include <algorithm>
#include <iterator>
#include "usb-pad.h"
#include "../helpers.h"
#include "../proxybase.h"

namespace usb_pad {

class PadError : public std::runtime_error
{
public:
	PadError(const char* msg) : std::runtime_error(msg) {}
	virtual ~PadError() throw () {}
};

class PadProxyBase : public ProxyBase
{
	PadProxyBase(const PadProxyBase&) = delete;

	public:
	PadProxyBase() {}
	PadProxyBase(const std::string& name);
	virtual Pad* CreateObject(int port, const char* dev_type) const = 0;
};

template <class T>
class PadProxy : public PadProxyBase
{
	PadProxy(const PadProxy&) = delete;

	public:
	PadProxy() { OSDebugOut(TEXT("%" SFMTs "\n"), T::Name()); }
	~PadProxy() { OSDebugOut(TEXT("%p\n"), this); }
	PadProxy(const std::string& name): PadProxyBase(name) {}
	Pad* CreateObject(int port, const char *dev_type) const
	{
		try
		{
			return new T(port, dev_type);
		}
		catch(PadError& err)
		{
			(void)err;
			return nullptr;
		}
	}
	virtual const TCHAR* Name() const
	{
		return T::Name();
	}
	virtual int Configure(int port, const char *dev_type, void *data)
	{
		return T::Configure(port, dev_type, data);
	}
};

class RegisterPad
{
	RegisterPad(const RegisterPad&) = delete;
	RegisterPad() {}

	public:
	typedef std::map<std::string, std::unique_ptr<PadProxyBase> > RegisterPadMap;
	static RegisterPad& instance() {
		static RegisterPad registerPad;
		return registerPad;
	}

	~RegisterPad() { Clear(); OSDebugOut("~RegisterPad()\n"); }

	static void Initialize();

	void Clear()
	{
		printf("registerPadMap.size: %d\n", registerPadMap.size());
		registerPadMap.clear();
	}

	void Add(const std::string& name, PadProxyBase* creator)
	{
		registerPadMap[name] = std::unique_ptr<PadProxyBase>(creator);
	}

	PadProxyBase* Proxy(const std::string& name)
	{
		return registerPadMap[name].get();
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

#ifndef REGISTER_PAD
#define REGISTER_PAD(name,cls) //PadProxy<cls> g##cls##Proxy(name)
#endif
} //namespace
#endif

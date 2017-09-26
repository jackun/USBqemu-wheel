#ifndef VIDEODEVICEPROXY_H
#define VIDEODEVICEPROXY_H
#include <string>
#include <map>
#include <list>
#include <algorithm>
#include <iterator>
#include "videodev.h"
#include "../helpers.h"
#include "../proxybase.h"

namespace usb_eyetoy {

class VideoDeviceError : public std::runtime_error
{
public:
	VideoDeviceError(const char* msg) : std::runtime_error(msg) {}
	virtual ~VideoDeviceError() throw () {}
};

class VideoDeviceProxyBase : public ProxyBase
{
	VideoDeviceProxyBase(const VideoDeviceProxyBase&) = delete;

	public:
	VideoDeviceProxyBase(const std::string& name);
	virtual VideoDevice* CreateObject(int port) const = 0;
};

template <class T>
class VideoDeviceProxy : public VideoDeviceProxyBase
{
	VideoDeviceProxy(const VideoDeviceProxy&) = delete;

	public:
	VideoDeviceProxy(const std::string& name): VideoDeviceProxyBase(name) {}
	VideoDevice* CreateObject(int port) const
	{
		try
		{
			return new T(port);
		}
		catch(VideoDeviceError& err)
		{
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
};

class RegisterVideoDevice
{
	RegisterVideoDevice(const RegisterVideoDevice&) = delete;
	RegisterVideoDevice() {}

	public:
	typedef std::map<std::string, VideoDeviceProxyBase* > RegisterVideoDeviceMap;
	static RegisterVideoDevice& instance() {
		static RegisterVideoDevice registerCam;
		return registerCam;
	}

	static void Initialize();

	void Add(const std::string& name, VideoDeviceProxyBase* creator)
	{
		registerMap[name] = creator;
	}

	VideoDeviceProxyBase* Proxy(const std::string& name)
	{
		return registerMap[name];
	}
	
	std::list<std::string> Names() const
	{
		std::list<std::string> nameList;
		std::transform(
			registerMap.begin(), registerMap.end(),
			std::back_inserter(nameList),
			SelectKey());
		return nameList;
	}

	std::string Name(int idx) const
	{
		auto it = registerMap.begin();
		std::advance(it, idx);
		if (it != registerMap.end())
			return std::string(it->first);
		return std::string();
	}

	const RegisterVideoDeviceMap& Map() const
	{
		return registerMap;
	}
	
private:
	RegisterVideoDeviceMap registerMap;
};

#define REGISTER_VIDEODEV(name,cls) VideoDeviceProxy<cls> g##cls##Proxy(name)
} //namespace
#endif

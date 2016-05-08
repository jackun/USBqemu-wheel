#ifndef USBMSD_H
#define USBMSD_H
#include "../deviceproxy.h"

// Catch typos at compile time
#define S_CONFIG_PATH TEXT("Image path")
#define N_CONFIG_PATH TEXT("path")

class MsdDevice : public Device
{
public:
	virtual ~MsdDevice() {}
	static USBDevice* CreateDevice(int port);
	static const wchar_t* Name()
	{
		return L"Mass storage device";
	}
	static std::list<std::string> APIs()
	{
		return std::list<std::string> { "cstdio" };
	}
	static const wchar_t* APIName(const std::string& name)
	{
		return L"cstdio";
	}

//	static bool LoadSettings(int port, std::vector<CONFIGVARIANT>& params);
//	static bool SaveSettings(int port, std::vector<CONFIGVARIANT>& params);

	static int Configure(int port, std::string api, void *data);
	static std::vector<CONFIGVARIANT> GetSettings(const std::string &api)
	{
		(void)api;
		std::vector<CONFIGVARIANT> params;
		params.push_back(CONFIGVARIANT(S_CONFIG_PATH, N_CONFIG_PATH, CONFIG_TYPE_TCHAR));
		return params;
	}
};
#endif

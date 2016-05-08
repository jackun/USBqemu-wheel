#include "usb-msd.h"
#include "../linux/ini.h"
#include "../configuration.h"

int MsdDevice::Configure(int port, std::string api, void *data)
{
	return RESULT_CANCELED;
}

/*std::vector<CONFIGVARIANT> MsdDevice::GetSettings(const std::string &api)
{
	(void)api;
	std::vector<CONFIGVARIANT> params;
	params.push_back(CONFIGVARIANT(S_CONFIG_PATH, N_CONFIG_PATH, CONFIG_TYPE_CHAR));
	return params;
}*/

/*
bool MsdDevice::LoadSettings(int port, std::vector<CONFIGVARIANT>& params)
{
	for(auto& p : params)
		if(!LoadSetting(port, "msd", "Default", p))
			return false;
	return true;
}

bool MsdDevice::SaveSettings(int port, std::vector<CONFIGVARIANT>& params)
{
	for(auto& p : params)
		if(!SaveSetting(port, "msd", "Default", p))
			return false;
	return true;
}
*/

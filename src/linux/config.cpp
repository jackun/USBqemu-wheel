#include "../osdebugout.h"
#include "../configuration.h"
#include "../deviceproxy.h"
#include "../usb-pad/padproxy.h"
#include "../usb-mic/audiodeviceproxy.h"

#include <map>
#include <vector>

#include "ini.h"
#include "config.h"

//Hopefully PCSX2 has inited all the GTK stuff already
using namespace std;

static std::string usb_path;
std::string IniDir;
std::string LogDir;
const char* iniFile = "USBqemu-wheel.ini";

void SysMessage_stderr(const char *fmt, ...)
{
	va_list arglist;

	va_start(arglist, fmt);
	vfprintf(stderr, fmt, arglist);
	va_end(arglist);
}

EXPORT_C_(void) USBsetSettingsDir( const char* dir )
{
	IniDir = dir;
}

EXPORT_C_(void) USBsetLogDir( const char* dir )
{
	LogDir = dir;
}

bool LoadSettingValue(const std::string& ini, const std::string& section, const char* param, std::string& value)
{
	char tmp[4096] = {0};
	if (INILoadString(ini.c_str(), section.c_str(), param, tmp) != 0)
		return false;

	value = tmp;
	return true;
}

bool LoadSettingValue(const std::string& ini, const std::string& section, const char* param, int32_t& value)
{
	return INILoadUInt(ini.c_str(), section.c_str(), param, (unsigned int *)&value) == 0;
}

bool LoadSettingValue(const std::string& ini, const std::string& section, const char* param, bool& value)
{
	unsigned int intv;
	bool ret = INILoadUInt(ini.c_str(), section.c_str(), param, &intv) == 0;
	value = ret && (intv ? true : false);
	return ret;
}

bool SaveSettingValue(const std::string& ini, const std::string& section, const char* param, const std::string& value)
{
	return INISaveString(ini.c_str(), section.c_str(), param, value.c_str()) == 0;
}

bool SaveSettingValue(const std::string& ini, const std::string& section, const char* param, const int32_t value)
{
	return INISaveUInt(ini.c_str(), section.c_str(), param, (unsigned int)value) == 0;
}

bool SaveSettingValue(const std::string& ini, const std::string& section, const char* param, const bool value)
{
	return INISaveUInt(ini.c_str(), section.c_str(), param, value ? 1 : 0) == 0;
}

void SaveConfig() {
	OSDebugOut("\n");
	//char* envptr = getenv("HOME");
	//if(envptr == NULL)
	//	return;
	//char path[1024];
	//snprintf(path, sizeof(path), "%s/.config/PCSX2/inis/USBqemu-wheel.ini", envptr);
	std::string iniPath(IniDir);
	iniPath.append(iniFile);
	const char *path = iniPath.c_str();

	//OSDebugOut("%s\n", path);

	OSDebugOut("[%s %d] '%s'='%s'\n", N_DEVICES, 0, N_DEVICE_PORT0, conf.Port[0].c_str());
	INISaveString(path, N_DEVICES, N_DEVICE_PORT0, conf.Port[0].c_str());

	OSDebugOut("[%s %d] '%s'='%s'\n", N_DEVICES, 1, N_DEVICE_PORT1, conf.Port[1].c_str());
	INISaveString(path, N_DEVICES, N_DEVICE_PORT1, conf.Port[1].c_str());

	OSDebugOut("[%s %d] '%s'=%d\n", N_DEVICES, 0, N_WHEEL_TYPE0, conf.WheelType[0]);
	INISaveUInt(path, N_DEVICES, N_WHEEL_TYPE0, conf.WheelType[0]);

	OSDebugOut("[%s %d] '%s'=%d\n", N_DEVICES, 1, N_WHEEL_TYPE1, conf.WheelType[1]);
	INISaveUInt(path, N_DEVICES, N_WHEEL_TYPE1, conf.WheelType[1]);

	for (auto& k : changedAPIs)
	{
		SaveSetting(nullptr, k.first.first, k.first.second, N_DEVICE_API, k.second);
	}
}

void LoadConfig() {
	char tmp[1024] = {0};
	OSDebugOut("USB load config\n");
	//char* envptr = getenv("HOME");
	//if(envptr == NULL)
	//	return;
	//char path[1024];
	//sprintf(path, "%s/.config/PCSX2/inis/USBqemu-wheel.ini", envptr);
	std::string iniPath(IniDir);
	iniPath.append(iniFile);
	const char *path = iniPath.c_str();

	INILoadString(path, N_DEVICES, N_DEVICE_PORT0, tmp);
	conf.Port[0] = tmp;
	INILoadString(path, N_DEVICES, N_DEVICE_PORT1, tmp);
	conf.Port[1] = tmp;
	INILoadUInt(path, N_DEVICES, N_WHEEL_TYPE0, (uint32_t*)&conf.WheelType[0]);
	INILoadUInt(path, N_DEVICES, N_WHEEL_TYPE1, (uint32_t*)&conf.WheelType[1]);

	for (int i=0; i<2; i++)
	{
		auto& instance = RegisterDevice::instance();
		std::string api;
		LoadSetting(nullptr, i, conf.Port[i], N_DEVICE_API, api);
		auto dev = instance.Device(conf.Port[i]);

		if (dev)
		{
			OSDebugOut("Checking device '%s' api: '%s'...\n", conf.Port[i].c_str(), api.c_str());
			if (!dev->IsValidAPI(api))
			{
				const auto& apis = dev->ListAPIs();
				if (!apis.empty())
					api = *apis.begin();

				OSDebugOut("Invalid! Defaulting to '%s'\n", api.c_str());
			}
			else
				OSDebugOut("API OK\n");
		}

		if(api.size())
			changedAPIs[std::make_pair(i, conf.Port[i])] = api;
	}
}

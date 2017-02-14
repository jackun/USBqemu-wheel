#include "../USB.h"
#include "../osdebugout.h"
#include "../configuration.h"
#include "../deviceproxy.h"
#include "../usb-pad/padproxy.h"
#include "../usb-mic/audiodeviceproxy.h"

#include <sstream>
#include <map>
#include <vector>
#include <string>

#include "ini.h"
#include "config.h"

//libjoyrumble used as an example
//Hopefully PCSX2 has inited all the GTK stuff already
using namespace std;

static std::string usb_path;
std::string IniDir;
std::string LogDir;
const char* iniFile = "USBqemu-wheel.ini";

void SysMessage(const char *fmt, ...)
{
	va_list arglist;

	va_start(arglist, fmt);
	vfprintf(stderr, fmt, arglist);
	va_end(arglist);
}

void CALLBACK USBsetSettingsDir( const char* dir )
{
	IniDir = dir;
}

void CALLBACK USBsetLogDir( const char* dir )
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

bool SaveSettingValue(const std::string& ini, const std::string& section, const char* param, std::string& value)
{
	return INISaveString(ini.c_str(), section.c_str(), param, value.c_str()) == 0;
}

bool SaveSettingValue(const std::string& ini, const std::string& section, const char* param, int32_t& value)
{
	return INISaveUInt(ini.c_str(), section.c_str(), param, (unsigned int)value) == 0;
}

bool LoadSetting(int port, const std::string& key, CONFIGVARIANT& var)
{
	bool ret = false;
	if (key.empty())
	{
		OSDebugOut("Key is empty for '%s' on port %d\n", var.name, port);
		return false;
	}

	std::stringstream section;
	section << key << " " << port;

	std::string ini(IniDir);
	ini.append(iniFile);

	OSDebugOut("[%s %d] '%s'=", key.c_str(), port, var.name);
	switch(var.type)
	{
		case CONFIG_TYPE_INT:
			ret = LoadSettingValue(ini, section.str(), var.name, var.intValue);
			OSDebugOut_noprfx("%d\n", var.intValue);
			break;
		//case CONFIG_TYPE_DOUBLE:
		//	return LoadSettingValue(ini, section.str(), var.name, var.doubleValue);
		case CONFIG_TYPE_TCHAR:
			ret = LoadSettingValue(ini, section.str(), var.name, var.tstrValue);
			OSDebugOut_noprfx("'%s'\n", var.tstrValue.c_str());
			break;
		case CONFIG_TYPE_CHAR:
			ret = LoadSettingValue(ini, section.str(), var.name, var.strValue);
			OSDebugOut_noprfx("'%s'\n", var.strValue.c_str());
			break;
		//case CONFIG_TYPE_WCHAR:
		//	return LoadSettingValue(ini, section.str(), var.name, var.wstrValue);
		break;
		default:
			OSDebugOut("\nInvalid config type %d for %s\n", var.type, var.name);
			break;
	};
	return ret;
}

/**
 * 
 * [devices]
 * portX = pad
 * 
 * [pad X]
 * api = joydev
 * 
 * [joydev X]
 * button0 = 1
 * button1 = 2
 * ...
 * 
 * */
bool SaveSetting(int port, const std::string& key, CONFIGVARIANT& var)
{
	bool ret = false;
	if (key.empty())
	{
		OSDebugOut("Key is empty for '%s' on port %d\n", var.name, port);
		return false;
	}

	std::stringstream section;
	section << key << " " << port;

	std::string ini(IniDir);
	ini.append(iniFile);

	OSDebugOut("[%s %d] '%s'=", key.c_str(), port, var.name);
	switch(var.type)
	{
		case CONFIG_TYPE_INT:
			ret = SaveSettingValue(ini, section.str(), var.name, var.intValue);
			OSDebugOut_noprfx("%d\n", var.intValue);
			break;
		case CONFIG_TYPE_TCHAR:
			ret = LoadSettingValue(ini, section.str(), var.name, var.tstrValue);
			OSDebugOut_noprfx("'%s'\n", var.tstrValue.c_str());
			break;
		case CONFIG_TYPE_CHAR:
			ret = SaveSettingValue(ini, section.str(), var.name, var.strValue);
			OSDebugOut_noprfx("'%s'\n", var.strValue.c_str());
			break;
		break;
		default:
			OSDebugOut("\nInvalid config type %d for %s\n", var.type, var.name);
			break;
	};
	return ret;
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

	OSDebugOut("[%s %d] '%s'='%s'\n", N_DEVICES, 0, N_DEVICE_PORT0, conf.Port0.c_str());
	INISaveString(path, N_DEVICES, N_DEVICE_PORT0, conf.Port0.c_str());

	OSDebugOut("[%s %d] '%s'='%s'\n", N_DEVICES, 1, N_DEVICE_PORT1, conf.Port1.c_str());
	INISaveString(path, N_DEVICES, N_DEVICE_PORT1, conf.Port1.c_str());

	OSDebugOut("[%s %d] '%s'=%d\n", N_DEVICES, 0, N_WHEEL_TYPE0, conf.WheelType[0]);
	INISaveUInt(path, N_DEVICES, N_WHEEL_TYPE0, conf.WheelType[0]);

	OSDebugOut("[%s %d] '%s'=%d\n", N_DEVICES, 1, N_WHEEL_TYPE1, conf.WheelType[1]);
	INISaveUInt(path, N_DEVICES, N_WHEEL_TYPE1, conf.WheelType[1]);

	for (auto& k : changedAPIs)
	{
		CONFIGVARIANT var(N_DEVICE_API, k.second);
		SaveSetting(k.first.first, k.first.second, var);
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
	conf.Port0 = tmp;
	INILoadString(path, N_DEVICES, N_DEVICE_PORT1, tmp);
	conf.Port1 = tmp;
	INILoadUInt(path, N_DEVICES, N_WHEEL_TYPE0, (u32*)&conf.WheelType[0]);
	INILoadUInt(path, N_DEVICES, N_WHEEL_TYPE1, (u32*)&conf.WheelType[1]);

	for (auto& pair: {std::make_pair(0, conf.Port0), std::make_pair(1, conf.Port1)})
	{
		auto& instance = RegisterDevice::instance();
		CONFIGVARIANT tmpVar(N_DEVICE_API, CONFIG_TYPE_CHAR);
		LoadSetting(pair.first, pair.second, tmpVar);
		std::string api = tmpVar.strValue;
		auto dev = instance.Device(pair.second);

		if (dev)
		{
			OSDebugOut("Checking device '%s' api: '%s'...\n", pair.second.c_str(), api.c_str());
			if (!dev->IsValidAPI(api))
			{
				const auto& apis = dev->APIs();
				if (!apis.empty())
					api = *apis.begin();

				OSDebugOut("Invalid! Defaulting to '%s'\n", api.c_str());
			}
			else
				OSDebugOut("API OK\n");
		}

		if(api.size())
			changedAPIs[pair] = api;
	}
}

#include "../USB.h"
#include "../configuration.h"
#include "../deviceproxy.h"
#include "../usb-pad/padproxy.h"
#include "../usb-mic/audiosourceproxy.h"

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
	fprintf(stderr, "USBsetSettingsDir: %s\n", dir);
	IniDir = dir;
}

void CALLBACK USBsetLogDir( const char* dir )
{
	printf("USBsetLogDir: %s\n", dir);
	LogDir = dir;
}

template<typename T>
bool LoadSettingValue(const std::string& ini, const std::string& section, const char* param, T& value);
template<typename T>
bool SaveSettingValue(const std::string& ini, const std::string& section, const char* param, T& value);

template<>
bool LoadSettingValue(const std::string& ini, const std::string& section, const char* param, std::string& value)
{
	char tmp[4096] = {0};
	INILoadString(ini.c_str(), section.c_str(), param, tmp);
	value = tmp;
	return true;
}

template<>
bool LoadSettingValue(const std::string& ini, const std::string& section, const char* param, int64_t& value)
{
	INILoadUInt(ini.c_str(), section.c_str(), param, (unsigned int *)value);
	return true;
}

template<>
bool SaveSettingValue(const std::string& ini, const std::string& section, const char* param, std::string& value)
{
	INISaveString(ini.c_str(), section.c_str(), param, value.c_str());
	return true;
}

template<>
bool SaveSettingValue(const std::string& ini, const std::string& section, const char* param, int64_t& value)
{
	INISaveUInt(ini.c_str(), section.c_str(), param, (unsigned int)value);
	return true;
}

bool LoadSetting(int port, const std::string& key, CONFIGVARIANT& var)
{
	fprintf(stderr, "USBqemu load \"%s\" from [%s %d]\n", var.name, key.c_str(), port);

	if (key.empty())
		return false;

	std::stringstream section;
	section << key << " " << port;

	std::string ini(IniDir);
	ini.append(iniFile);

	switch(var.type)
	{
		case CONFIG_TYPE_INT:
			return LoadSettingValue(ini, section.str(), var.name, var.intValue);
		//case CONFIG_TYPE_DOUBLE:
		//	return LoadSettingValue(ini, section.str(), var.name, var.doubleValue);
		case CONFIG_TYPE_TCHAR:
			return LoadSettingValue(ini, section.str(), var.name, var.tstrValue);
		case CONFIG_TYPE_CHAR:
			return LoadSettingValue(ini, section.str(), var.name, var.strValue);
		//case CONFIG_TYPE_WCHAR:
		//	return LoadSettingValue(ini, section.str(), var.name, var.wstrValue);
		break;
		default:
			fprintf(stderr, "Invalid config type %d for %s\n", var.type, var.name);
			break;
	};
	return false;
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
	fprintf(stderr, "USBqemu save \"%s\" to [%s %d]\n", var.name, key.c_str(), port);

	if (key.empty())
		return false;

	std::stringstream section;
	section << key << " " << port;

	std::string ini(IniDir);
	ini.append(iniFile);

	switch(var.type)
	{
		case CONFIG_TYPE_INT:
			return SaveSettingValue(ini, section.str(), var.name, var.intValue);
		case CONFIG_TYPE_TCHAR:
			return LoadSettingValue(ini, section.str(), var.name, var.tstrValue);
		case CONFIG_TYPE_CHAR:
			return SaveSettingValue(ini, section.str(), var.name, var.strValue);
		break;
		default:
			fprintf(stderr, "Invalid config type %d for %s\n", var.type, var.name);
			break;
	};
	return false;
}

void SaveConfig() {
	fprintf(stderr, "USB save config\n");
	//char* envptr = getenv("HOME");
	//if(envptr == NULL)
	//	return;
	//char path[1024];
	//snprintf(path, sizeof(path), "%s/.config/PCSX2/inis/USBqemu-wheel.ini", envptr);
	std::string iniPath(IniDir);
	iniPath.append(iniFile);
	const char *path = iniPath.c_str();

	//fprintf(stderr, "%s\n", path);

	INISaveString(path, N_DEVICES, N_DEVICE_PORT0, conf.Port0.c_str());
	INISaveString(path, N_DEVICES, N_DEVICE_PORT1, conf.Port1.c_str());
	INISaveUInt(path, N_DEVICES, N_WHEEL_TYPE0, conf.WheelType[0]);
	INISaveUInt(path, N_DEVICES, N_WHEEL_TYPE1, conf.WheelType[1]);

	for (auto& k : changedAPIs)
	{
		CONFIGVARIANT var(N_DEVICE_API, CONFIG_TYPE_CHAR);
		var.strValue = k.second;
		fprintf(stderr, "Save apis: %s %s\n", k.first.second.c_str(), k.second.c_str());
		SaveSetting(k.first.first, k.first.second, var);
	}
}

void LoadConfig() {
	char tmp[1024] = {0};
	fprintf(stderr, "USB load config\n");
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

	{
		auto instance = RegisterDevice::instance();
		CONFIGVARIANT tmpVar(N_DEVICE_API, CONFIG_TYPE_CHAR);
		LoadSetting(0, conf.Port0, tmpVar);
		std::string api = tmpVar.strValue;
		auto dev = instance.Device(conf.Port0);

		if (dev && !dev->IsValidAPI(api))
		{
			auto apis = dev->APIs();
			if (!apis.empty())
				api = *apis.begin();
		}

		if(api.size())
			changedAPIs[std::make_pair(0, conf.Port0)] = api;

		LoadSetting(1, conf.Port1, tmpVar);
		api = tmpVar.strValue;

		dev = instance.Device(conf.Port1);
		if (dev && !dev->IsValidAPI(api))
		{
			auto apis = dev->APIs();
			if (!apis.empty())
				api = *apis.begin();
		}

		if(api.size())
			changedAPIs[std::make_pair(1, conf.Port1)] = api;
	}
}

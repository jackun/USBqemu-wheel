#ifndef LINUXCONFIG_H
#define LINUXCONFIG_H
#include <sstream>
#include <string>
#include "osdebugout.h"

extern std::string IniPath;
extern std::string LogDir;
extern const char* iniFile;

bool LoadSettingValue(const std::string& ini, const std::string& section, const char* param, std::string& value);
bool LoadSettingValue(const std::string& ini, const std::string& section, const char* param, int32_t& value);
bool LoadSettingValue(const std::string& ini, const std::string& section, const char* param, bool& value);

bool SaveSettingValue(const std::string& ini, const std::string& section, const char* param, const std::string& value);
bool SaveSettingValue(const std::string& ini, const std::string& section, const char* param, const int32_t value);
bool SaveSettingValue(const std::string& ini, const std::string& section, const char* param, const bool value);

template<typename Type>
bool LoadSetting(const char* dev_type, int port, const std::string& key, const char* name, Type& var)
{
	bool ret = false;
	if (key.empty())
	{
		OSDebugOut("Key is empty for '%s' on port %d\n", name, port);
		return false;
	}

	std::stringstream section;
	if (dev_type)
		section << dev_type << " ";
	section << key << " " << port;
	std::string str = section.str();

	OSDebugOut("[%s] '%s'=", str.c_str(), name);
	ret = LoadSettingValue(IniPath, str, name, var);
	if (ret)
		OSDebugOutStream_noprfx(var);
	else
		OSDebugOut_noprfx("<failed>\n");
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
template<typename Type>
bool SaveSetting(const char* dev_type, int port, const std::string& key, const char* name, const Type var)
{
	bool ret = false;
	if (key.empty())
	{
		OSDebugOut("Key is empty for '%s' on port %d\n", name, port);
		return false;
	}

	std::stringstream section;
	if (dev_type)
		section << dev_type << " ";
	section << key << " " << port;
	std::string str = section.str();

	OSDebugOut("[%s] '%s'=", str.c_str(), name);

	ret = SaveSettingValue(IniPath, str, name, var);
	OSDebugOutStream_noprfx(var);
	return ret;
}

#endif

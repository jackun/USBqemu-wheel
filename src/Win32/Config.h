#ifndef WIN32CONFIG_H
#define WIN32CONFIG_H
#include <string>
#include <sstream>
#include "osdebugout.h"

void GetIniFile(std::wstring &iniFile);
bool LoadSettingValue(const std::wstring& ini, const std::wstring& section, const wchar_t* param, std::string& value);
bool LoadSettingValue(const std::wstring& ini, const std::wstring& section, const wchar_t* param, std::wstring& value);
bool LoadSettingValue(const std::wstring& ini, const std::wstring& section, const wchar_t* param, bool& value);
bool LoadSettingValue(const std::wstring& ini, const std::wstring& section, const wchar_t* param, int32_t& value);

bool SaveSettingValue(const std::wstring& ini, const std::wstring& section, const wchar_t* param, const std::string& value);
bool SaveSettingValue(const std::wstring& ini, const std::wstring& section, const wchar_t* param, const std::wstring& value);
bool SaveSettingValue(const std::wstring& ini, const std::wstring& section, const wchar_t* param, const wchar_t* value);
bool SaveSettingValue(const std::wstring& ini, const std::wstring& section, const wchar_t* param, const bool value);
bool SaveSettingValue(const std::wstring& ini, const std::wstring& section, const wchar_t* param, const int32_t value);

#include <sstream>
template<typename Type>
bool LoadSetting(const char* dev_type, int port, const std::string& key, const TCHAR* name, Type& var)
{
	OSDebugOut(L"USBqemu load \"%s\" from [%S %d]\n", name, key.c_str(), port);

	std::wstringstream section;
	std::wstring wkey;
	wkey.assign(key.begin(), key.end());
	if (dev_type)
		section << dev_type << " ";
	section << wkey << " " << port;
	std::wstring str = section.str();

	std::wstring ini;
	GetIniFile(ini);

	return LoadSettingValue(ini, str, name, var);
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
bool SaveSetting(const char* dev_type, int port, const std::string& key, const TCHAR* name, const Type var)
{
	OSDebugOut(L"USBqemu save \"%s\" to [%S %d]\n", name, key.c_str(), port);

	std::wstringstream section;
	std::wstring wkey;
	wkey.assign(key.begin(), key.end());
	if (dev_type)
		section << dev_type << " ";
	section << wkey << " " << port;
	std::wstring str = section.str();

	std::wstring ini;
	GetIniFile(ini);

	return SaveSettingValue(ini, str, name, var);
}

#endif
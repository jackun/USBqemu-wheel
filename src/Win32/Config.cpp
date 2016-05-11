#include <string>
#include <stdlib.h>
#include <sstream>

#include "../USB.h"
#include "../configuration.h"

extern HINSTANCE hInst;
std::wstring IniDir;
std::wstring LogDir;

void CALLBACK USBsetSettingsDir( const char* dir )
{
	OSDebugOut(L"USBsetSettingsDir: %S\n", dir);
	wchar_t dst[4096] = {0};
	size_t num = 0;
	mbstowcs_s(&num, dst, dir, ARRAYSIZE(dst));
	IniDir = dst;
}

void CALLBACK USBsetLogDir( const char* dir )
{
	OSDebugOut(L"USBsetLogDir: %S\n", dir);
	wchar_t dst[4096] = {0};
	size_t num = 0;
	mbstowcs_s(&num, dst, dir, ARRAYSIZE(dst));
	LogDir = dst;
}

//TODO Use \\?\ to go past 260 char limit
void GetIniFile(std::wstring &iniFile)
{
	iniFile.clear();
	if(!IniDir.length()) {
		WCHAR tmp[MAX_PATH] = {0};
		GetModuleFileName(GetModuleHandle((LPWSTR)hInst), tmp, MAX_PATH);

		std::wstring path(tmp);
		unsigned last = path.find_last_of(L"\\");
		iniFile = path.substr(0, last);
		iniFile.append(L"\\inis");
		CreateDirectory(iniFile.c_str(), NULL);
		iniFile.append(L"\\USBqemu-wheel.ini");
	} else {
		iniFile.append(IniDir);
		iniFile.append(L"USBqemu-wheel.ini");
	}
}


template<typename T>
bool LoadSettingValue(const std::wstring& ini, const std::wstring& section, const wchar_t* param, T& value);
template<typename T>
bool SaveSettingValue(const std::wstring& ini, const std::wstring& section, const wchar_t* param, T& value);

template<>
bool LoadSettingValue(const std::wstring& ini, const std::wstring& section, const wchar_t* param, std::string& value)
{
	wchar_t tmp[4096] = { 0 };
	if (!GetPrivateProfileStringW(section.c_str(), param, NULL, tmp, sizeof(tmp) / sizeof(*tmp), ini.c_str()))
		return false;
	if (GetLastError() == ERROR_FILE_NOT_FOUND)
		return false;

	char tmpA[4096] = { 0 };
	size_t num = 0;
	wcstombs_s(&num, tmpA, tmp, sizeof(tmpA)); //TODO error-check
	value = tmpA;
	return true;
}

template<>
bool LoadSettingValue(const std::wstring& ini, const std::wstring& section, const wchar_t* param, std::wstring& value)
{
	wchar_t tmp[4096] = { 0 };
	if (!GetPrivateProfileStringW(section.c_str(), param, NULL, tmp, sizeof(tmp) / sizeof(*tmp), ini.c_str()))
		return false;
	if (GetLastError() == ERROR_FILE_NOT_FOUND)
		return false;
	value = tmp;
	return true;
}

template<>
bool LoadSettingValue(const std::wstring& ini, const std::wstring& section, const wchar_t* param, int64_t& value)
{
	//value = GetPrivateProfileIntW(section.c_str(), param, 0, ini.c_str());
	wchar_t tmp[4096] = { 0 };
	if (!GetPrivateProfileStringW(section.c_str(), param, NULL, tmp, sizeof(tmp) / sizeof(*tmp), ini.c_str()))
		return false;
	if (GetLastError() == ERROR_FILE_NOT_FOUND)
		return false;
	value = wcstoul(tmp, NULL, 10);
	return true;
}

template<>
bool SaveSettingValue(const std::wstring& ini, const std::wstring& section, const wchar_t* param, std::string& value)
{
	std::wstring wstr;
	wstr.assign(value.begin(), value.end());
	return !!WritePrivateProfileStringW(section.c_str(), param, wstr.c_str(), ini.c_str());
}

template<>
bool SaveSettingValue(const std::wstring& ini, const std::wstring& section, const wchar_t* param, std::wstring& value)
{
	return !!WritePrivateProfileStringW(section.c_str(), param, value.c_str(), ini.c_str());
}

template<>
bool SaveSettingValue(const std::wstring& ini, const std::wstring& section, const wchar_t* param, int64_t& value)
{
	wchar_t tmp[32] = { 0 };
	swprintf_s(tmp, L"%I64u", value);
	return !!WritePrivateProfileStringW(section.c_str(), param, tmp, ini.c_str());
}

bool LoadSetting(int port, const std::string& key, CONFIGVARIANT& var)
{
	OSDebugOut(L"USBqemu load \"%s\" from %S\n", var.name, key.c_str());

	std::wstringstream section;
	std::wstring wkey;
	wkey.assign(key.begin(), key.end());
	section << wkey << " " << port;

	std::wstring ini;
	GetIniFile(ini);

	switch (var.type)
	{
	case CONFIG_TYPE_INT:
		return LoadSettingValue(ini, section.str(), var.name, var.intValue);
		//case CONFIG_TYPE_DOUBLE:
		//	return LoadSettingValue(ini, section.str(), var.name, var.doubleValue);
	case CONFIG_TYPE_TCHAR:
		return LoadSettingValue(ini, section.str(), var.name, var.wstrValue);
	case CONFIG_TYPE_CHAR:
		return LoadSettingValue(ini, section.str(), var.name, var.strValue);
	case CONFIG_TYPE_WCHAR:
		return LoadSettingValue(ini, section.str(), var.name, var.wstrValue);
	default:
		OSDebugOut(L"Invalid config type %d for %s\n", var.type, var.name);
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
	OSDebugOut(L"USBqemu save \"%s\" to %S\n", var.name, key.c_str());

	std::wstringstream section;
	std::wstring wkey;
	wkey.assign(key.begin(), key.end());
	section << wkey << " " << port;

	std::wstring ini;
	GetIniFile(ini);

	switch (var.type)
	{
	case CONFIG_TYPE_INT:
		return SaveSettingValue(ini, section.str(), var.name, var.intValue);
	case CONFIG_TYPE_TCHAR:
		return LoadSettingValue(ini, section.str(), var.name, var.wstrValue);
	case CONFIG_TYPE_WCHAR:
		return SaveSettingValue(ini, section.str(), var.name, var.wstrValue);
	case CONFIG_TYPE_CHAR:
		return SaveSettingValue(ini, section.str(), var.name, var.strValue);
	default:
		OSDebugOut(L"Invalid config type %d for %s\n", var.type, var.name);
		break;
	};
	return false;
}

void SaveConfig()
{
	Config *Conf1 = &conf;
	std::wstring szIniFile;
	TCHAR szValue[256];

	GetIniFile(szIniFile);

	FILE *f = nullptr;
	auto err = _wfopen_s(&f, szIniFile.c_str(), L"a+");
	if (!f) {
		MessageBoxW(NULL, L"Cannot save to ini!", L"USBqemu", MB_ICONERROR);
	} else
		fclose(f);

	swprintf_s(szValue,L"%u",Conf1->Log);
	WritePrivateProfileStringW(L"Interface", L"Logging", szValue, szIniFile.c_str());

	swprintf_s(szValue,L"%u",Conf1->DFPPass);
	WritePrivateProfileStringW(N_DEVICES, L"DFP Passthrough", szValue, szIniFile.c_str());

	swprintf_s(szValue,L"%S",Conf1->Port0.c_str());
	WritePrivateProfileStringW(N_DEVICES, N_DEVICE_PORT0, szValue, szIniFile.c_str());

	swprintf_s(szValue,L"%S",Conf1->Port1.c_str());
	WritePrivateProfileStringW(N_DEVICES, N_DEVICE_PORT1, szValue, szIniFile.c_str());

	swprintf_s(szValue,L"%u",Conf1->WheelType[0]);
	WritePrivateProfileStringW(N_DEVICES, N_WHEEL_TYPE0, szValue, szIniFile.c_str());

	swprintf_s(szValue,L"%u",Conf1->WheelType[1]);
	WritePrivateProfileStringW(N_DEVICES, N_WHEEL_TYPE1, szValue, szIniFile.c_str());

	for (auto &kp : changedAPIs)
	{
		CONFIGVARIANT var(N_DEVICE_API, CONFIG_TYPE_CHAR);
		var.strValue = kp.second;
		SaveSetting(kp.first.first, kp.first.second, var);
	}
	changedAPIs.clear();
}

void LoadConfig() {

	Config *Conf1 = &conf;
	std::wstring szIniFile;
	wchar_t szValue[MAX_PATH+1];
	char tmpA[MAX_PATH + 1] = { 0 };
	size_t num = 0;

	GetIniFile(szIniFile);

	FILE *fp = nullptr;
	auto err = _wfopen_s(&fp, szIniFile.c_str(), L"rt");//check if ini really exists
	if (!fp)
	{
		memset(&conf, 0, sizeof(conf));
		conf.Log = 0;//default value
		SaveConfig();//save and return
		return ;
	}
	fclose(fp);

	GetPrivateProfileStringW(L"Interface", L"Logging", NULL, szValue, 20, szIniFile.c_str());
	Conf1->Log = wcstoul(szValue, NULL, 10);

	GetPrivateProfileStringW(N_DEVICES, TEXT("DFP Passthrough"), NULL, szValue, 20, szIniFile.c_str());
	Conf1->DFPPass = wcstoul(szValue, NULL, 10);

	GetPrivateProfileStringW(N_DEVICES, N_DEVICE_PORT0, NULL, szValue, ARRAYSIZE(szValue), szIniFile.c_str());
	wcstombs_s(&num, tmpA, szValue, sizeof(tmpA));//TODO error-check
	Conf1->Port0 = tmpA;

	GetPrivateProfileStringW(N_DEVICES, N_DEVICE_PORT1, NULL, szValue, ARRAYSIZE(szValue), szIniFile.c_str());
	wcstombs_s(&num, tmpA, szValue, sizeof(tmpA));//TODO error-check
	Conf1->Port1 = tmpA;

	GetPrivateProfileStringW(N_DEVICES, N_WHEEL_TYPE0, NULL, szValue, 20, szIniFile.c_str());
	Conf1->WheelType[0] = wcstoul(szValue, NULL, 10);

	GetPrivateProfileStringW(N_DEVICES, N_WHEEL_TYPE1, NULL, szValue, 20, szIniFile.c_str());
	Conf1->WheelType[1] = wcstoul(szValue, NULL, 10);

	return ;

}


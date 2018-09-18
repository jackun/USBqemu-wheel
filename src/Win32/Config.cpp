#include <string>
#include <stdlib.h>
#include "Config.h"
#include "../configuration.h"

extern HINSTANCE hInst;
std::wstring IniDir;
std::wstring LogDir;

EXPORT_C_(void) USBsetSettingsDir( const char* dir )
{
	OSDebugOut(L"USBsetSettingsDir: %S\n", dir);
	wchar_t dst[4096] = {0};
	size_t num = 0;
	mbstowcs_s(&num, dst, dir, countof(dst));
	IniDir = dst;
}

EXPORT_C_(void) USBsetLogDir( const char* dir )
{
	OSDebugOut(L"USBsetLogDir: %S\n", dir);
	wchar_t dst[4096] = {0};
	size_t num = 0;
	mbstowcs_s(&num, dst, dir, countof(dst));
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
		size_t last = path.find_last_of(L'\\');
		iniFile = path.substr(0, last);
		iniFile.append(L"\\inis");
		CreateDirectory(iniFile.c_str(), NULL);
		iniFile.append(L"\\USBqemu-wheel.ini");
	} else {
		iniFile.append(IniDir);
		iniFile.append(L"USBqemu-wheel.ini");
	}
}

bool LoadSettingValue(const std::wstring& ini, const std::wstring& section, const wchar_t* param, std::string& value)
{
	wchar_t tmp[4096] = { 0 };
	GetPrivateProfileStringW(section.c_str(), param, NULL, tmp, sizeof(tmp) / sizeof(*tmp), ini.c_str());
	if (GetLastError() == ERROR_FILE_NOT_FOUND)
		return false;

	char tmpA[4096] = { 0 };
	size_t num = 0;
	wcstombs_s(&num, tmpA, tmp, sizeof(tmpA)); //TODO error-check
	value = tmpA;
	return true;
}

bool LoadSettingValue(const std::wstring& ini, const std::wstring& section, const wchar_t* param, std::wstring& value)
{
	wchar_t tmp[4096] = { 0 };
	GetPrivateProfileStringW(section.c_str(), param, NULL, tmp, sizeof(tmp) / sizeof(*tmp), ini.c_str());
	if (GetLastError() == ERROR_FILE_NOT_FOUND)
		return false;
	value = tmp;
	return true;
}

bool LoadSettingValue(const std::wstring& ini, const std::wstring& section, const wchar_t* param, bool& value)
{
	//value = GetPrivateProfileIntW(section.c_str(), param, 0, ini.c_str());
	wchar_t tmp[4096] = { 0 };
	if (!GetPrivateProfileStringW(section.c_str(), param, NULL, tmp, sizeof(tmp) / sizeof(*tmp), ini.c_str()))
		return false;
	if (GetLastError() == ERROR_FILE_NOT_FOUND)
		return false;
	value = !!wcstoul(tmp, NULL, 10);
	return true;
}

bool LoadSettingValue(const std::wstring& ini, const std::wstring& section, const wchar_t* param, int32_t& value)
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

bool SaveSettingValue(const std::wstring& ini, const std::wstring& section, const wchar_t* param, const std::string& value)
{
	std::wstring wstr;
	wstr.assign(value.begin(), value.end());
	return !!WritePrivateProfileStringW(section.c_str(), param, wstr.c_str(), ini.c_str());
}

bool SaveSettingValue(const std::wstring& ini, const std::wstring& section, const wchar_t* param, const std::wstring& value)
{
	return !!WritePrivateProfileStringW(section.c_str(), param, value.c_str(), ini.c_str());
}

bool SaveSettingValue(const std::wstring& ini, const std::wstring& section, const wchar_t* param, const wchar_t* value)
{
	return !!WritePrivateProfileStringW(section.c_str(), param, value, ini.c_str());
}

bool SaveSettingValue(const std::wstring& ini, const std::wstring& section, const wchar_t* param, const bool value)
{
	wchar_t tmp[2] = { 0 };
	swprintf_s(tmp, L"%d", value ? 1 : 0);
	return !!WritePrivateProfileStringW(section.c_str(), param, tmp, ini.c_str());
}

bool SaveSettingValue(const std::wstring& ini, const std::wstring& section, const wchar_t* param, const int32_t value)
{
	wchar_t tmp[32] = { 0 };
	swprintf_s(tmp, L"%d", value);
	return !!WritePrivateProfileStringW(section.c_str(), param, tmp, ini.c_str());
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

	swprintf_s(szValue,L"%d",Conf1->Log);
	WritePrivateProfileStringW(L"Interface", L"Logging", szValue, szIniFile.c_str());

	swprintf_s(szValue,L"%d",Conf1->DFPPass);
	WritePrivateProfileStringW(N_DEVICES, L"DFP Passthrough", szValue, szIniFile.c_str());

	swprintf_s(szValue,L"%S",Conf1->Port[0].c_str());
	WritePrivateProfileStringW(N_DEVICES, N_DEVICE_PORT0, szValue, szIniFile.c_str());

	swprintf_s(szValue,L"%S",Conf1->Port[1].c_str());
	WritePrivateProfileStringW(N_DEVICES, N_DEVICE_PORT1, szValue, szIniFile.c_str());

	swprintf_s(szValue,L"%d",Conf1->WheelType[0]);
	WritePrivateProfileStringW(N_DEVICES, N_WHEEL_TYPE0, szValue, szIniFile.c_str());

	swprintf_s(szValue,L"%d",Conf1->WheelType[1]);
	WritePrivateProfileStringW(N_DEVICES, N_WHEEL_TYPE1, szValue, szIniFile.c_str());

	for (auto &kp : changedAPIs)
	{
		SaveSetting(kp.first.first, kp.first.second, N_DEVICE_API, kp.second);
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
		conf.Log = 0;//default value
		SaveConfig();//save and return
		return ;
	}
	fclose(fp);

	GetPrivateProfileStringW(L"Interface", L"Logging", NULL, szValue, 20, szIniFile.c_str());
	Conf1->Log = wcstoul(szValue, NULL, 10);

	GetPrivateProfileStringW(N_DEVICES, TEXT("DFP Passthrough"), NULL, szValue, 20, szIniFile.c_str());
	Conf1->DFPPass = wcstoul(szValue, NULL, 10);

	GetPrivateProfileStringW(N_DEVICES, N_DEVICE_PORT0, NULL, szValue, countof(szValue), szIniFile.c_str());
	wcstombs_s(&num, tmpA, szValue, sizeof(tmpA));//TODO error-check
	Conf1->Port[0] = tmpA;

	GetPrivateProfileStringW(N_DEVICES, N_DEVICE_PORT1, NULL, szValue, countof(szValue), szIniFile.c_str());
	wcstombs_s(&num, tmpA, szValue, sizeof(tmpA));//TODO error-check
	Conf1->Port[1] = tmpA;

	GetPrivateProfileStringW(N_DEVICES, N_WHEEL_TYPE0, NULL, szValue, 20, szIniFile.c_str());
	Conf1->WheelType[0] = wcstoul(szValue, NULL, 10);

	GetPrivateProfileStringW(N_DEVICES, N_WHEEL_TYPE1, NULL, szValue, 20, szIniFile.c_str());
	Conf1->WheelType[1] = wcstoul(szValue, NULL, 10);

	return ;

}


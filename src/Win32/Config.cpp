#include <string>
#include <stdlib.h>

#include "../USB.h"

extern HINSTANCE hInst;
std::string szIniDir;

void CALLBACK USBsetSettingsDir( const char* dir )
{
	printf("USBsetSettingsDir: %s\n", dir);
	szIniDir = dir;
}

void CALLBACK USBsetLogDir( const char* dir )
{
	printf("USBsetLogDir: %s\n", dir);
}

void GetIniFile(std::string &iniFile)
{
	if(!szIniDir.length()) {
		char *szTemp = NULL, tmp[MAX_PATH];
		GetModuleFileName(GetModuleHandle((LPCSTR)hInst), tmp, MAX_PATH);
		szTemp = strrchr(tmp, '\\');
		if(!szTemp) return;
		strcpy(szTemp, "\\inis\\USBqemu-wheel.ini");
		iniFile.append(tmp);
	} else {
		iniFile.append(szIniDir);
		iniFile.append("USBqemu-wheel.ini");
	}
}

void SaveConfig()
{
	Config *Conf1 = &conf;
	std::string szIniFile;
	char szValue[256];

	GetIniFile(szIniFile);

	FILE *f = fopen(szIniFile.c_str(), "w+");
	if(!f) {
		MessageBoxA(NULL, "Cannot save to ini!", "USBqemu", MB_ICONERROR);
	} else
		fclose(f);

	sprintf_s(szValue,"%u",Conf1->Log);
	WritePrivateProfileString("Interface", "Logging",szValue,szIniFile.c_str());

	sprintf_s(szValue,"%u",Conf1->DFPPass);
	WritePrivateProfileString("Devices", "DFP Passthrough",szValue,szIniFile.c_str());

	sprintf_s(szValue,"%u",Conf1->Port0);
	WritePrivateProfileString("Devices", "Port 0",szValue,szIniFile.c_str());

	sprintf_s(szValue,"%u",Conf1->Port1);
	WritePrivateProfileString("Devices", "Port 1",szValue,szIniFile.c_str());

	sprintf_s(szValue,"%u",Conf1->WheelType1);
	WritePrivateProfileString("Devices", "Wheel Type 1",szValue,szIniFile.c_str());

	sprintf_s(szValue,"%u",Conf1->WheelType2);
	WritePrivateProfileString("Devices", "Wheel Type 2",szValue,szIniFile.c_str());

	WritePrivateProfileString("Devices", "USB Image",Conf1->usb_img,szIniFile.c_str());

	//WritePrivateProfileString("Joystick", "Player1", player_joys[0].c_str(), szIniFile);
	//WritePrivateProfileString("Joystick", "Player2", player_joys[1].c_str(), szIniFile);

}

void LoadConfig() {
	FILE *fp;

	Config *Conf1 = &conf;
	std::string szIniFile;
	char szValue[MAX_PATH+1];

	GetIniFile(szIniFile);

	fp=fopen(szIniFile.c_str(), "rt");//check if ini really exists
	if (!fp)
	{
		CreateDirectory("inis",NULL);
		memset(&conf, 0, sizeof(conf));
		conf.Log = 0;//default value
		SaveConfig();//save and return
		return ;
	}
	fclose(fp);

	GetPrivateProfileString("Interface", "Logging", NULL, szValue, 20, szIniFile.c_str());
	Conf1->Log = strtoul(szValue, NULL, 10);

	GetPrivateProfileString("Devices", "DFP Passthrough", NULL, szValue, 20, szIniFile.c_str());
	Conf1->DFPPass = strtoul(szValue, NULL, 10);

	GetPrivateProfileString("Devices", "Port 0", NULL, szValue, 20, szIniFile.c_str());
	Conf1->Port0 = strtoul(szValue, NULL, 10);

	GetPrivateProfileString("Devices", "Port 1", NULL, szValue, 20, szIniFile.c_str());
	Conf1->Port1 = strtoul(szValue, NULL, 10);

	GetPrivateProfileString("Devices", "Wheel Type 1", NULL, szValue, 20, szIniFile.c_str());
	Conf1->WheelType1 = strtoul(szValue, NULL, 10);

	GetPrivateProfileString("Devices", "Wheel Type 2", NULL, szValue, 20, szIniFile.c_str());
	Conf1->WheelType2 = strtoul(szValue, NULL, 10);

	GetPrivateProfileString("Devices", "USB Image", NULL, Conf1->usb_img, sizeof(Conf1->usb_img), szIniFile.c_str());

	//GetPrivateProfileString("Joystick", "Player1", NULL, szValue, MAX_PATH, szIniFile);
	//player_joys[0] = szValue;

	//GetPrivateProfileString("Joystick", "Player2", NULL, szValue, MAX_PATH, szIniFile);
	//player_joys[1] = szValue;
	
	return ;

}


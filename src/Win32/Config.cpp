#include <string>
#include <stdlib.h>

#include "../USB.h"

extern HINSTANCE hInst;
std::string IniDir;
std::string LogDir;

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

void GetIniFile(std::string &iniFile)
{
	iniFile.clear();
	if(!IniDir.length()) {
		char tmp[MAX_PATH] = {0};
		GetModuleFileName(GetModuleHandle((LPCSTR)hInst), tmp, MAX_PATH);

		std::string path(tmp);
		unsigned last = path.find_last_of("\\");
		iniFile = path.substr(0, last);
		iniFile.append("\\inis\\USBqemu-wheel.ini");
	} else {
		iniFile.append(IniDir);
		iniFile.append("USBqemu-wheel.ini");
	}
}

void SaveConfig()
{
	Config *Conf1 = &conf;
	std::string szIniFile;
	char szValue[256];

	GetIniFile(szIniFile);

	FILE *f = fopen(szIniFile.c_str(), "a+");
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

	sprintf_s(szValue,"%u",Conf1->WheelType[0]);
	WritePrivateProfileString("Devices", "Wheel Type 1",szValue,szIniFile.c_str());

	sprintf_s(szValue,"%u",Conf1->WheelType[1]);
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
	Conf1->WheelType[0] = strtoul(szValue, NULL, 10);

	GetPrivateProfileString("Devices", "Wheel Type 2", NULL, szValue, 20, szIniFile.c_str());
	Conf1->WheelType[1] = strtoul(szValue, NULL, 10);

	GetPrivateProfileString("Devices", "USB Image", NULL, Conf1->usb_img, sizeof(Conf1->usb_img), szIniFile.c_str());

	//GetPrivateProfileString("Joystick", "Player1", NULL, szValue, MAX_PATH, szIniFile);
	//player_joys[0] = szValue;

	//GetPrivateProfileString("Joystick", "Player2", NULL, szValue, MAX_PATH, szIniFile);
	//player_joys[1] = szValue;
	
	return ;

}


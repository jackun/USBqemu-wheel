#include <string>
#include <stdlib.h>

#include "../USB.h"

extern HINSTANCE hInst;
char *szIni = "\\inis\\USBqemu-wheel.ini";

void SaveConfig()
{
	Config *Conf1 = &conf;
	char *szTemp;
	char szIniFile[MAX_PATH+1], szValue[256];

	GetModuleFileName(GetModuleHandle((LPCSTR)hInst), szIniFile, MAX_PATH);
	szTemp = strrchr(szIniFile, '\\');

	if(!szTemp) return;
	strcpy(szTemp, szIni);

	//FILE *f = fopen(szIniFile, "w+");
	//if(f) fclose(f);

	sprintf_s(szValue,"%u",Conf1->Log);
	WritePrivateProfileString("Interface", "Logging",szValue,szIniFile);

	sprintf_s(szValue,"%u",Conf1->DFPPass);
	WritePrivateProfileString("Devices", "DFP Passthrough",szValue,szIniFile);

	sprintf_s(szValue,"%u",Conf1->Port0);
	WritePrivateProfileString("Devices", "Port 0",szValue,szIniFile);

	sprintf_s(szValue,"%u",Conf1->Port1);
	WritePrivateProfileString("Devices", "Port 1",szValue,szIniFile);

	sprintf_s(szValue,"%u",Conf1->WheelType1);
	WritePrivateProfileString("Devices", "Wheel Type 1",szValue,szIniFile);

	sprintf_s(szValue,"%u",Conf1->WheelType2);
	WritePrivateProfileString("Devices", "Wheel Type 2",szValue,szIniFile);
	
	//WritePrivateProfileString("Joystick", "Player1", player_joys[0].c_str(), szIniFile);
	//WritePrivateProfileString("Joystick", "Player2", player_joys[1].c_str(), szIniFile);

}

void LoadConfig() {
	FILE *fp;

	Config *Conf1 = &conf;
	char *szTemp;
	char szIniFile[MAX_PATH+1], szValue[MAX_PATH+1];

	GetModuleFileName(GetModuleHandle((LPCSTR)hInst), szIniFile, MAX_PATH);
	szTemp = strrchr(szIniFile, '\\');

	if(!szTemp) return;
	strcpy(szTemp, szIni);
	fp=fopen(szIniFile, "rt");//check if ini really exists
	if (!fp)
	{
		CreateDirectory("inis",NULL);
		memset(&conf, 0, sizeof(conf));
		conf.Log = 0;//default value
		SaveConfig();//save and return
		return ;
	}
	fclose(fp);

	GetPrivateProfileString("Interface", "Logging", NULL, szValue, 20, szIniFile);
	Conf1->Log = strtoul(szValue, NULL, 10);

	GetPrivateProfileString("Devices", "DFP Passthrough", NULL, szValue, 20, szIniFile);
	Conf1->DFPPass = strtoul(szValue, NULL, 10);

	GetPrivateProfileString("Devices", "Port 0", NULL, szValue, 20, szIniFile);
	Conf1->Port0 = strtoul(szValue, NULL, 10);

	GetPrivateProfileString("Devices", "Port 1", NULL, szValue, 20, szIniFile);
	Conf1->Port1 = strtoul(szValue, NULL, 10);

	GetPrivateProfileString("Devices", "Wheel Type 1", NULL, szValue, 20, szIniFile);
	Conf1->WheelType1 = strtoul(szValue, NULL, 10);

	GetPrivateProfileString("Devices", "Wheel Type 2", NULL, szValue, 20, szIniFile);
	Conf1->WheelType2 = strtoul(szValue, NULL, 10);

	//GetPrivateProfileString("Joystick", "Player1", NULL, szValue, MAX_PATH, szIniFile);
	//player_joys[0] = szValue;

	//GetPrivateProfileString("Joystick", "Player2", NULL, szValue, MAX_PATH, szIniFile);
	//player_joys[1] = szValue;
	
	return ;

}


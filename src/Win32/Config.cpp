#include <stdlib.h>

#include "../USB.h"
#include "Config.h"

extern HINSTANCE hInst;
char *szIni = "\\inis\\USBqemu-wheel.ini";

void LoadMappings(int vid, int pid, uint8_t *maps)
{
	char *szTemp;
	char szIniFile[MAX_PATH+1], szValue[256];

	GetModuleFileName(GetModuleHandle((LPCSTR)hInst), szIniFile, MAX_PATH);
	szTemp = strrchr(szIniFile, '\\');

	if(!szTemp) return;
	strcpy(szTemp, szIni);

	// Default 1-to-1
	for(int i = 0; i < 16 /*MAX_BUTTONS*/; i++)
		maps[i] = i;

	char dev[128];
	sprintf(dev, "%X_%X", vid, pid);
	if(GetPrivateProfileString("Mappings", dev, NULL, szValue, 256, szIniFile))
	{
		char *tok = strtok(szValue, ",");
		while(tok != NULL)
		{
			sscanf(tok, "%X,", maps++);
			tok = strtok(NULL, ",");
		}
	}

	return;
}

void SaveMappings(int vid, int pid, uint8_t *maps)
{
	char *szTemp;
	char szIniFile[MAX_PATH+1], szValue[256] = {0};

	GetModuleFileName(GetModuleHandle((LPCSTR)hInst), szIniFile, MAX_PATH);
	szTemp = strrchr(szIniFile, '\\');

	if(!szTemp) return;
	strcpy(szTemp, szIni);

	char dev[128], tmp[8];
	sprintf(dev, "%X_%X", vid, pid);
	for(int i = 0; i<16 /*MAX_BUTTONS*/; i++)
	{
		sprintf(tmp, "%X,", maps[i]);
		strcat(szValue, tmp);
	}

	WritePrivateProfileString("Mappings", dev, szValue, szIniFile);

	return;
}

void SaveConfig()
{
	Config *Conf1 = &conf;
	char *szTemp;
	char szIniFile[MAX_PATH+1], szValue[256];

	GetModuleFileName(GetModuleHandle((LPCSTR)hInst), szIniFile, MAX_PATH);
	szTemp = strrchr(szIniFile, '\\');

	if(!szTemp) return;
	strcpy(szTemp, szIni);
	sprintf(szValue,"%u",Conf1->Log);
	WritePrivateProfileString("Interface", "Logging",szValue,szIniFile);
	WritePrivateProfileString("Joystick", "Player1", player_joys[0].c_str(), szIniFile);
	WritePrivateProfileString("Joystick", "Player2", player_joys[1].c_str(), szIniFile);

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
	GetPrivateProfileString("Joystick", "Player1", NULL, szValue, MAX_PATH, szIniFile);
	player_joys[0] = szValue;
	GetPrivateProfileString("Joystick", "Player2", NULL, szValue, MAX_PATH, szIniFile);
	player_joys[1] = szValue;
	Conf1->Log = strtoul(szValue, NULL, 10);
	return ;

}


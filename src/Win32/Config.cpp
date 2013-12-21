#include <stdlib.h>

#include "../USB.h"
#include "Config.h"

extern HINSTANCE hInst;
char *szIni = "\\inis\\USBqemu-wheel.ini";

//XXX Allowing PLayer One and Two have different mappings with "ply".
void LoadMappings(int ply, int vid, int pid, PS2Buttons *btns, PS2Axis *axes)
{
	char *szTemp;
	char szIniFile[MAX_PATH+1], szValue[256];

	GetModuleFileName(GetModuleHandle((LPCSTR)hInst), szIniFile, MAX_PATH);
	szTemp = strrchr(szIniFile, '\\');

	if(!szTemp) return;
	strcpy(szTemp, szIni);

	memset(btns, PAD_BUTTON_COUNT, MAX_BUTTONS);
	memset(axes, PAD_AXIS_COUNT, MAX_AXES);

	// Default 1-to-1
	for(int i = 0; i < PAD_BUTTON_COUNT /*MAX_BUTTONS*/; i++) //FIXME max buttons
		btns[i] = (PS2Buttons)i;
	for(int i = 0; i < PAD_AXIS_COUNT /*MAX_AXES*/; i++) //FIXME max axes
		axes[i] = (PS2Axis)i;

	char dev[128];
	sprintf(dev, "%X_%X_%d", vid, pid, ply);
	if(GetPrivateProfileString("Mappings", dev, NULL, szValue, 256, szIniFile))
	{
		char *tok = strtok(szValue, ",");
		while(tok != NULL)
		{
			sscanf(tok, "%X,", btns++);
			tok = strtok(NULL, ",");
		}
	}

	ZeroMemory(szValue, sizeof(szValue));
	if(GetPrivateProfileString("Axes", dev, NULL, szValue, 256, szIniFile))
	{
		char *tok = strtok(szValue, ",");
		while(tok != NULL)
		{
			sscanf(tok, "%X,", axes++);
			tok = strtok(NULL, ",");
		}
	}

	return;
}

void SaveMappings(int ply, int vid, int pid, PS2Buttons *btns, PS2Axis *axes)
{
	char *szTemp;
	char szIniFile[MAX_PATH+1], szValue[256] = {0};

	GetModuleFileName(GetModuleHandle((LPCSTR)hInst), szIniFile, MAX_PATH);
	szTemp = strrchr(szIniFile, '\\');

	if(!szTemp) return;
	strcpy(szTemp, szIni);

	char dev[128], tmp[8];
	sprintf(dev, "%X_%X_%d", vid, pid, ply);
	for(int i = 0; i<16 /*MAX_BUTTONS*/; i++)
	{
		sprintf(tmp, "%X,", btns[i]);
		strcat(szValue, tmp);
	}

	WritePrivateProfileString("Mappings", dev, szValue, szIniFile);

	ZeroMemory(szValue, sizeof(szValue));
	for(int i = 0; i<6 /*MAX_AXES*/; i++)
	{
		sprintf(tmp, "%X,", axes[i]);
		strcat(szValue, tmp);
	}

	WritePrivateProfileString("Axes", dev, szValue, szIniFile);

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


#include <stdlib.h>

#include "../USB.h"
#include "Config.h"

extern HINSTANCE hInst;
char *szIni = "\\inis\\USBqemu-wheel.ini";

inline bool MapExists(MapVector *maps, char* hid)
{
	MapVector::iterator it;
	for(it = maps->begin(); it != maps->end(); it++)
		if(!(*it)->hidPath.compare(hid))
			return true;
	return false;
}

void LoadMappings(MapVector *maps)
{
	char *szTemp;
	char szIniFile[MAX_PATH+1], szValue[256];

	GetModuleFileName(GetModuleHandle((LPCSTR)hInst), szIniFile, MAX_PATH);
	szTemp = strrchr(szIniFile, '\\');

	if(!szTemp) return;
	strcpy(szTemp, szIni);
	maps->clear();
	
	char sec[32] = {0}, tmp[16] = {0}, bind[32] = {0}, hid[MAX_PATH+1];
	int j = 0, v = 0;
	while(j < 25)
	{
		sprintf(sec, "DEVICE %d", j++);
		if(GetPrivateProfileString(sec, "HID", NULL, hid, MAX_PATH, szIniFile)
			&& hid && !MapExists(maps, hid))
		{
			Mappings *m = new Mappings;
			//ZeroMemory(m, sizeof(Mappings));
			maps->push_back(m);
			Mappings *ptr = maps->back();

			ptr->hidPath = std::string(hid);
			ptr->devName = std::string(hid);
			//ResetData(&ptr->data[0]);
			//ResetData(&ptr->data[1]);
			ZeroMemory(&ptr->data[0], sizeof(wheel_data_t));
			ZeroMemory(&ptr->data[1], sizeof(wheel_data_t));

			for(int i = 0; i<MAX_BUTTONS; i++)
			{
				sprintf(bind, "Button %d", i);
				GetPrivateProfileString(sec, bind, NULL, tmp, 16, szIniFile);
				sscanf(tmp, "%08X", &(ptr->btnMap[i]));
			}

			for(int i = 0; i<MAX_AXES; i++)
			{
				sprintf(bind, "Axis %d", i);
				GetPrivateProfileString(sec, bind, NULL, tmp, 16, szIniFile);
				sscanf(tmp, "%08X", &ptr->axisMap[i]);
			}

			for(int i = 0; i<4; i++)
			{
				sprintf(bind, "Hat %d", i);
				GetPrivateProfileString(sec, bind, NULL, tmp, 16, szIniFile);
				sscanf(tmp, "%08X", &ptr->hatMap[i]);
			}
			ptr = NULL;
		}
	}

	return;
}

void SaveMappings(MapVector *maps)
{
	char *szTemp;
	char szIniFile[MAX_PATH+1], szValue[256] = {0};

	GetModuleFileName(GetModuleHandle((LPCSTR)hInst), szIniFile, MAX_PATH);
	szTemp = strrchr(szIniFile, '\\');

	if(!szTemp) return;
	strcpy(szTemp, szIni);

	MapVector::iterator it;
	uint32_t numDevice = 0;
	for(it = maps->begin(); it != maps->end(); it++)
	{
		char dev[32] = {0}, tmp[16] = {0}, bind[32] = {0};

		sprintf(dev, "DEVICE %d", numDevice++);
		WritePrivateProfileString(dev, "HID", (*it)->hidPath.c_str(), szIniFile);

		//writing everything separately, then string lengths are more predictable
		for(int i = 0; i<MAX_BUTTONS; i++)
		{
			sprintf(bind, "Button %d", i);
			sprintf(tmp, "%08X", (*it)->btnMap[i]);
			WritePrivateProfileString(dev, bind, tmp, szIniFile);
		}

		for(int i = 0; i<MAX_AXES; i++)
		{
			sprintf(bind, "Axis %d", i);
			sprintf(tmp, "%08X", (*it)->axisMap[i]);
			WritePrivateProfileString(dev, bind, tmp, szIniFile);
		}

		for(int i = 0; i<4; i++)
		{
			sprintf(bind, "Hat %d", i);
			sprintf(tmp, "%08X", (*it)->hatMap[i]);
			WritePrivateProfileString(dev, bind, tmp, szIniFile);
		}
	}

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

	//FILE *f = fopen(szIniFile, "w+");
	//if(f) fclose(f);

	sprintf(szValue,"%u",Conf1->Log);
	WritePrivateProfileString("Interface", "Logging",szValue,szIniFile);

	sprintf(szValue,"%u",Conf1->DFPPass);
	WritePrivateProfileString("Devices", "DFP Passthrough",szValue,szIniFile);

	sprintf(szValue,"%u",Conf1->Port0);
	WritePrivateProfileString("Devices", "Port 0",szValue,szIniFile);

	sprintf(szValue,"%u",Conf1->Port1);
	WritePrivateProfileString("Devices", "Port 1",szValue,szIniFile);

	sprintf(szValue,"%u",Conf1->WheelType1);
	WritePrivateProfileString("Devices", "Wheel Type 1",szValue,szIniFile);

	sprintf(szValue,"%u",Conf1->WheelType2);
	WritePrivateProfileString("Devices", "Wheel Type 2",szValue,szIniFile);
	
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

	GetPrivateProfileString("Joystick", "Player1", NULL, szValue, MAX_PATH, szIniFile);
	player_joys[0] = szValue;

	GetPrivateProfileString("Joystick", "Player2", NULL, szValue, MAX_PATH, szIniFile);
	player_joys[1] = szValue;
	
	return ;

}


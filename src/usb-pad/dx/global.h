#define pushAll __asm push eax __asm push ebx __asm push ecx __asm push edx __asm push esi __asm push edi 
#define popAll  __asm pop  edi __asm pop  esi __asm pop  edx __asm pop  ecx __asm pop  ebx __asm pop  eax
#define SAFE_FREE(p)	{ if(p) { free(p); (p) = NULL; } }


#include <windows.h>
#include <CommCtrl.h>
#include <stdlib.h>
#include <Shellapi.h>
#include <stdint.h>

#include <algorithm>
#include <vector>
#include <map>

#include "stdio.h"
#include "d3dx9.h"
#include "d3dx9math.h"
//local code
#include "versionproxy.h"
#include "ini.h"
#include "math.h"
#include "stdio.h"

DWORD LOG = 0;
DWORD INVERTFORCES = 0;
DWORD BYPASSCAL = 0;
 //logfile
FILE* fl = NULL;
char logstring[255];
char	*pStr, strPath[255], strTemp[255];

char key[255]={0};

//dialog window stuff
//HINSTANCE hInstance = NULL;
extern HINSTANCE hInst;

bool dialogOpen = false;
extern HWND gsWnd;
HWND hWin = NULL;
DWORD pid = NULL;
DWORD old = NULL;

bool tw = false;
HWND hKey;
HWND hWnd;
char text[1024];
char CID = 0;

HFONT hFont;
HDC hDC;
PAINTSTRUCT Ps;
RECT rect;
static WNDPROC pFnPrevFunc;
LONG filtercontrol = 0;
float TESTV=0;
float TESTVF=0;
DWORD m_dwScalingTime;
DWORD m_dwDrawingTime;
DWORD m_dwCreationTime;
DWORD m_dwMemory;
DWORD m_dwOption;
DWORD m_dwTime;
HBITMAP m_hOldAABitmap;
HBITMAP m_hAABitmap;
HDC m_hAADC;
HBITMAP m_hOldMemBitmap;
HBITMAP m_hMemBitmap;
HDC m_hMemDC;




//dinput control mappings

const DWORD numc = 20; //total control maps

LONG AXISID[numc] = {0};
LONG INVERT[numc] = {0};
LONG HALF[numc] = {0};
LONG BUTTON[numc] = {0};
LONG LINEAR[numc] = {0};
LONG OFFSET[numc] = {0};
LONG DEADZONE[numc] = {0};


//label enum
DWORD LABELS[numc] = {
	IDC_LABEL0,
	IDC_LABEL1,
	IDC_LABEL2,
	IDC_LABEL3,
	IDC_LABEL4,
	IDC_LABEL5,
	IDC_LABEL6,
	IDC_LABEL7,
	IDC_LABEL8,
	IDC_LABEL9,
	IDC_LABEL10,
	IDC_LABEL11,
	IDC_LABEL12,
	IDC_LABEL13,
	IDC_LABEL14,
	IDC_LABEL15,
	IDC_LABEL16,
	IDC_LABEL17,
	IDC_LABEL18,
	IDC_LABEL19,
};


HWND GetWindowHandle(DWORD tPID)
{
	//Get first window handle
	HWND res = FindWindow(NULL,NULL);
	DWORD mPID = NULL;
	while(res != 0)
	{
		if(!GetParent(res))
		{
			GetWindowThreadProcessId(res,&mPID);
			if (mPID == tPID)
				return res;
		}
		res = GetWindow(res, GW_HWNDNEXT);
	}
	return NULL;
}

void GetID(char * name)
{
	hWin = ::FindWindow(name, NULL);
	::GetWindowThreadProcessId(hWin, &pid);
}
void WriteLogFile(const char* szString)
{
	if (!LOG) return;
#if _DEBUG
	fprintf(stderr, "%s\n", szString);
#endif

	FILE* pFile = NULL;
	errno_t err = fopen_s(&pFile, "DIwheellog.txt", "a");
	if(err == 0)
	{
		fprintf(pFile, "%s\n",szString);
		fclose(pFile);
	}

}
void SaveMain()
{
	GetIniFile(strMySystemFile);

	sprintf_s(strTemp, "%i", LOG);WriteToFile("MAIN", "LOG", strTemp);
	sprintf_s(strTemp, "%i", INVERTFORCES);WriteToFile("MAIN", "INVERTFORCES", strTemp);
	sprintf_s(strTemp, "%i", BYPASSCAL);WriteToFile("MAIN", "BYPASSCAL", strTemp);

	for(int i=0; i<numc;i++){
		sprintf_s(text, "AXISID%i", i);sprintf_s(strTemp, "%i", AXISID[i]);WriteToFile("CONTROLS", text, strTemp);
		sprintf_s(text, "INVERT%i", i);sprintf_s(strTemp, "%i", INVERT[i]);WriteToFile("CONTROLS", text, strTemp);
		sprintf_s(text, "HALF%i", i);sprintf_s(strTemp, "%i", HALF[i]);WriteToFile("CONTROLS", text, strTemp);
		sprintf_s(text, "BUTTON%i", i);sprintf_s(strTemp, "%i", BUTTON[i]);WriteToFile("CONTROLS", text, strTemp);
		sprintf_s(text, "LINEAR%i", i);sprintf_s(strTemp, "%i", LINEAR[i]);WriteToFile("CONTROLS", text, strTemp);
		sprintf_s(text, "OFFSET%i", i);sprintf_s(strTemp, "%i", OFFSET[i]);WriteToFile("CONTROLS", text, strTemp);
		sprintf_s(text, "DEADZONE%i", i);sprintf_s(strTemp, "%i", DEADZONE[i]);WriteToFile("CONTROLS", text, strTemp);
	}
}

void LoadMain()
{
	memset(AXISID, 0xFF, sizeof(LONG)*numc);
	memset(INVERT, 0xFF, sizeof(LONG)*numc);
	memset(HALF, 0xFF, sizeof(LONG)*numc);
	memset(BUTTON, 0xFF, sizeof(LONG)*numc);

	GetIniFile(strMySystemFile);

	FILE * fp = NULL;
	errno_t err = fopen_s(&fp, strMySystemFile.c_str(), "r");//check if ini really exists
	if (!fp)
	{
		CreateDirectory("inis",NULL);
		SaveMain();//save
	}
	else
		fclose(fp);

	char szText[260];
	//if (ReadFromFile("MAIN", "FFBDEVICE1")) strcpy(szText, ReadFromFile("MAIN", "FFBDEVICE1"));
	//player_joys[0] = szText;
	if (ReadFromFile("MAIN", "LOG", szText)) LOG = atoi(szText);
	if (ReadFromFile("MAIN", "INVERTFORCES", szText)) INVERTFORCES = atoi(szText);
	if (ReadFromFile("MAIN", "BYPASSCAL", szText)) BYPASSCAL = atoi(szText);

	for(int i=0; i<numc;i++){
		sprintf_s(text, "AXISID%i", i); if (ReadFromFile("CONTROLS", text, szText)) AXISID[i] = atoi(szText);
		sprintf_s(text, "INVERT%i", i); if (ReadFromFile("CONTROLS", text, szText)) INVERT[i] = atoi(szText);
		sprintf_s(text, "HALF%i", i); if (ReadFromFile("CONTROLS", text, szText)) HALF[i] = atoi(szText);
		sprintf_s(text, "BUTTON%i", i); if (ReadFromFile("CONTROLS", text, szText)) BUTTON[i] = atoi(szText);
		sprintf_s(text, "LINEAR%i", i); if (ReadFromFile("CONTROLS", text, szText)) LINEAR[i] = atoi(szText);
		sprintf_s(text, "OFFSET%i", i); if (ReadFromFile("CONTROLS", text, szText)) OFFSET[i] = atoi(szText);
		sprintf_s(text, "DEADZONE%i", i); if (ReadFromFile("CONTROLS", text, szText)) DEADZONE[i] = atoi(szText);
	}

}

//use direct input
#include "di.h"
void InitDI()
{

	LoadMain();
	if(gsWnd) {
		hWin = gsWnd;
	} else {
		pid = GetCurrentProcessId();
		while(hWin == 0){ hWin = GetWindowHandle(pid);}
	}
	
	
	InitDirectInput(hWin, 0);
}

float GetControl(int id,  bool axisbutton=false)
{
	if(id==0) //steering uses two inputs
	{
		//apply steering
		if(AXISID[0] > -1 && AXISID[1] > -1){
			if(ReadAxisFiltered(0) > 0.0){
				return -ReadAxisFiltered(0);
			}else{
				if(ReadAxisFiltered(1) > 0.0){
					return ReadAxisFiltered(1);
				}else{
					return  0;
				}
			}
		}
	}
	else
	{
		//apply
		if(axisbutton){
			if(AXISID[id+1] > -1){
				if(ReadAxisFiltered(id+1)>0.5){
					return 1.0;
				}else{
					return 0.0;}
			}
		}else{
			if(AXISID[id+1] > -1){return ReadAxisFiltered(id+1);}
		}


		if(BUTTON[id+1] > -1){
			if(KeyDown(BUTTON[id+1]))
				return 1.0;
			else
				return 0.0;
		}
	}
	return 0.f;
}

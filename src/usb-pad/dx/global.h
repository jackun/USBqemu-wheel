#define pushAll __asm push eax __asm push ebx __asm push ecx __asm push edx __asm push esi __asm push edi 
#define popAll  __asm pop  edi __asm pop  esi __asm pop  edx __asm pop  ecx __asm pop  ebx __asm pop  eax
#define SAFE_FREE(p)	{ if(p) { free(p); (p) = NULL; } }


#include <windows.h>
#include <commctrl.h>
#include <stdlib.h>
#include <shellapi.h>
#include <stdint.h>

#include <algorithm>
#include <vector>
#include <map>
#include <atomic>

#include "stdio.h"
//local code
#include "versionproxy.h"
#include "ini.h"
#include "math.h"
#include "stdio.h"
#include "../../configuration.h"
#include "../../osdebugout.h"

static std::atomic<int> refCount (0);
DWORD LOG = 0;
DWORD INVERTFORCES[2] = { 0 };
DWORD BYPASSCAL = 0;

TCHAR	*pStr, strPath[255], strTemp[255];
static bool useRamp = false;

char key[255]={0};

//dialog window stuff
//HINSTANCE hInstance = NULL;
extern HINSTANCE hInst;

bool dialogOpen = false;
extern HWND gsWnd;
HWND hWin = NULL;
DWORD pid = 0;
DWORD old = 0;

HWND hKey;
HWND hWnd;
TCHAR text[1024];
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

LONG AXISID[2][numc] = { { 0 } };
LONG INVERT[2][numc] = { { 0 } };
LONG HALF[2][numc] = { { 0 } };
LONG BUTTON[2][numc] = { { 0 } };
LONG LINEAR[2][numc] = { { 0 } };
LONG OFFSET[2][numc] = { { 0 } };
LONG DEADZONE[2][numc] = { { 0 } };
LONG GAINZ[2][1] = { { 0 } };
LONG FFMULTI[2][1] = { { 0 } };


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
	DWORD mPID = 0;
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

void GetID(TCHAR * name)
{
	hWin = ::FindWindow(name, NULL);
	::GetWindowThreadProcessId(hWin, &pid);
}

void SaveMain(int port)
{
	GetIniFile(strMySystemFile);

	swprintf_s(strTemp, L"%u", LOG);WriteToFile(L"MAIN", L"LOG", strTemp);
	swprintf_s(strTemp, L"%u", BYPASSCAL);WriteToFile(L"MAIN", L"BYPASSCAL", strTemp);

	wchar_t section[128];
	swprintf_s(section, L"CONTROLS %d", port);

	swprintf_s(strTemp, L"%u", INVERTFORCES[port]); WriteToFile(section, L"INVERTFORCES", strTemp);

	for(int i=0; i<numc;i++){
		swprintf_s(text, L"AXISID%i", i);swprintf_s(strTemp, L"%i", AXISID[port][i]);WriteToFile(section, text, strTemp);
		swprintf_s(text, L"INVERT%i", i);swprintf_s(strTemp, L"%i", INVERT[port][i]);WriteToFile(section, text, strTemp);
		swprintf_s(text, L"HALF%i", i);swprintf_s(strTemp, L"%i", HALF[port][i]);WriteToFile(section, text, strTemp);
		swprintf_s(text, L"BUTTON%i", i);swprintf_s(strTemp, L"%i", BUTTON[port][i]);WriteToFile(section, text, strTemp);
		swprintf_s(text, L"LINEAR%i", i);swprintf_s(strTemp, L"%i", LINEAR[port][i]);WriteToFile(section, text, strTemp);
		swprintf_s(text, L"OFFSET%i", i);swprintf_s(strTemp, L"%i", OFFSET[port][i]);WriteToFile(section, text, strTemp);
		swprintf_s(text, L"DEADZONE%i", i);swprintf_s(strTemp, L"%i", DEADZONE[port][i]);WriteToFile(section, text, strTemp);
		//swprintf_s(text, L"GAINZ%i", i); swprintf_s(strTemp, L"%i", GAINZ[port][i]); WriteToFile(section, text, strTemp);
	}
	swprintf_s(strTemp, L"%i", GAINZ[port][0]); WriteToFile(section, TEXT("GAINZ"), strTemp);
	swprintf_s(strTemp, L"%i", FFMULTI[port][0]); WriteToFile(section, TEXT("FFMULTI"), strTemp);
	//only for config dialog
	CONFIGVARIANT var(L"UseRamp", useRamp);
	SaveSetting(port, "dinput", var);
}

void LoadMain(int port)
{
	memset(AXISID[port], 0xFF, sizeof(LONG)*numc);
	memset(INVERT[port], 0xFF, sizeof(LONG)*numc);
	memset(HALF[port], 0xFF, sizeof(LONG)*numc);
	memset(BUTTON[port], 0xFF, sizeof(LONG)*numc);

	GetIniFile(strMySystemFile);

	FILE * fp = NULL;
	errno_t err = _wfopen_s(&fp, strMySystemFile.c_str(), L"r");//check if ini really exists
	if (!fp)
	{
		CreateDirectory(L"inis",NULL);
		SaveMain(port);//save
	}
	else
		fclose(fp);

	TCHAR szText[260];
	//if (ReadFromFile("MAIN", "FFBDEVICE1")) strcpy(szText, ReadFromFile("MAIN", "FFBDEVICE1"));
	//player_joys[0] = szText;
	if (ReadFromFile(L"MAIN", L"LOG", szText)) LOG = wcstol(szText, NULL, 10);
	if (ReadFromFile(L"MAIN", L"BYPASSCAL", szText)) BYPASSCAL = wcstol(szText, NULL, 10);

	wchar_t section[128];
	swprintf_s(section, L"CONTROLS %d", port);

	if (ReadFromFile(section, L"INVERTFORCES", szText)) INVERTFORCES[port] = wcstol(szText, NULL, 10);
	for(int i=0; i<numc;i++){
		swprintf_s(text, TEXT("AXISID%i"), i); if (ReadFromFile(section, text, szText)) AXISID[port][i] = wcstol(szText, NULL, 10);
		swprintf_s(text, TEXT("INVERT%i"), i); if (ReadFromFile(section, text, szText)) INVERT[port][i] = wcstol(szText, NULL, 10);
		swprintf_s(text, TEXT("HALF%i"), i); if (ReadFromFile(section, text, szText)) HALF[port][i] = wcstol(szText, NULL, 10);
		swprintf_s(text, TEXT("BUTTON%i"), i); if (ReadFromFile(section, text, szText)) BUTTON[port][i] = wcstol(szText, NULL, 10);
		swprintf_s(text, TEXT("LINEAR%i"), i); if (ReadFromFile(section, text, szText)) LINEAR[port][i] = wcstol(szText, NULL, 10);
		swprintf_s(text, TEXT("OFFSET%i"), i); if (ReadFromFile(section, text, szText)) OFFSET[port][i] = wcstol(szText, NULL, 10);
		swprintf_s(text, TEXT("DEADZONE%i"), i); if (ReadFromFile(section, text, szText)) DEADZONE[port][i] = wcstol(szText, NULL, 10);
	}

	if (ReadFromFile(section, TEXT("GAINZ"), szText))
		GAINZ[port][0] = wcstol(szText, NULL, 10);
	else
		GAINZ[port][0] = 10000;

	if (ReadFromFile(section, TEXT("FFMULTI"), szText))
		FFMULTI[port][0] = wcstol(szText, NULL, 10);
	else
		FFMULTI[port][0] = 0;

	//only for config dialog
	CONFIGVARIANT var(L"UseRamp", CONFIG_TYPE_BOOL);
	if (LoadSetting(port, "dinput", var))
		useRamp = var.boolValue;
}

//use direct input
#include "di.h"
void InitDI(int port)
{

	LoadMain(port);
	if(gsWnd) {
		hWin = gsWnd;
	} else {
		pid = GetCurrentProcessId();
		while(hWin == 0){ hWin = GetWindowHandle(pid);}
	}
	
	InitDirectInput(hWin, port);
}

float GetControl(int port, int id,  bool axisbutton)
{
	if(id==0) //steering uses two inputs
	{
		//apply steering
		if(AXISID[port][0] > -1 && AXISID[port][1] > -1){
			if(ReadAxisFiltered(port, 0) > 0.0){
				return -ReadAxisFiltered(port, 0);
			}else{
				if(ReadAxisFiltered(port, 1) > 0.0){
					return ReadAxisFiltered(port, 1);
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
			if(AXISID[port][id+1] > -1){
				if(ReadAxisFiltered(port, id+1)>0.5){
					return 1.0;
				}else{
					return 0.0;}
			}
		}else{
			if(AXISID[port][id+1] > -1){return ReadAxisFiltered(port, id+1);}
		}
	}
	return 0.f;
}

bool GetControl(int port, int id)
{
	if (BUTTON[port][id + 1] > -1) {
		if (KeyDown(BUTTON[port][id + 1]))
			return true;
		else
			return false;
	}
	return false;
}

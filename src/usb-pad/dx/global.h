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
TCHAR logstring[255];
TCHAR	*pStr, strPath[255], strTemp[255];

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

void GetID(TCHAR * name)
{
	hWin = ::FindWindow(name, NULL);
	::GetWindowThreadProcessId(hWin, &pid);
}
void WriteLogFile(const TCHAR* szString)
{
	if (!LOG) return;
#if _DEBUG
	fwprintf(stderr, L"%s\n", szString);
#endif

	FILE* pFile = NULL;
	errno_t err = _wfopen_s(&pFile, L"DIwheellog.txt", L"a");
	if(err == 0)
	{
		fwprintf(pFile, L"%s\n",szString);
		fclose(pFile);
	}

}
void SaveMain()
{
	GetIniFile(strMySystemFile);

	swprintf_s(strTemp, L"%i", LOG);WriteToFile(L"MAIN", L"LOG", strTemp);
	swprintf_s(strTemp, L"%i", INVERTFORCES);WriteToFile(L"MAIN", L"INVERTFORCES", strTemp);
	swprintf_s(strTemp, L"%i", BYPASSCAL);WriteToFile(L"MAIN", L"BYPASSCAL", strTemp);

	for(int i=0; i<numc;i++){
		swprintf_s(text, L"AXISID%i", i);swprintf_s(strTemp, L"%i", AXISID[i]);WriteToFile(L"CONTROLS", text, strTemp);
		swprintf_s(text, L"INVERT%i", i);swprintf_s(strTemp, L"%i", INVERT[i]);WriteToFile(L"CONTROLS", text, strTemp);
		swprintf_s(text, L"HALF%i", i);swprintf_s(strTemp, L"%i", HALF[i]);WriteToFile(L"CONTROLS", text, strTemp);
		swprintf_s(text, L"BUTTON%i", i);swprintf_s(strTemp, L"%i", BUTTON[i]);WriteToFile(L"CONTROLS", text, strTemp);
		swprintf_s(text, L"LINEAR%i", i);swprintf_s(strTemp, L"%i", LINEAR[i]);WriteToFile(L"CONTROLS", text, strTemp);
		swprintf_s(text, L"OFFSET%i", i);swprintf_s(strTemp, L"%i", OFFSET[i]);WriteToFile(L"CONTROLS", text, strTemp);
		swprintf_s(text, L"DEADZONE%i", i);swprintf_s(strTemp, L"%i", DEADZONE[i]);WriteToFile(L"CONTROLS", text, strTemp);
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
	errno_t err = _wfopen_s(&fp, strMySystemFile.c_str(), L"r");//check if ini really exists
	if (!fp)
	{
		CreateDirectory(L"inis",NULL);
		SaveMain();//save
	}
	else
		fclose(fp);

	TCHAR szText[260];
	//if (ReadFromFile("MAIN", "FFBDEVICE1")) strcpy(szText, ReadFromFile("MAIN", "FFBDEVICE1"));
	//player_joys[0] = szText;
	if (ReadFromFile(L"MAIN", L"LOG", szText)) LOG = wcstol(szText, NULL, 10);
	if (ReadFromFile(L"MAIN", L"INVERTFORCES", szText)) INVERTFORCES = wcstol(szText, NULL, 10);
	if (ReadFromFile(L"MAIN", L"BYPASSCAL", szText)) BYPASSCAL = wcstol(szText, NULL, 10);

	for(int i=0; i<numc;i++){
		swprintf_s(text, TEXT("AXISID%i"), i); if (ReadFromFile(L"CONTROLS", text, szText)) AXISID[i] = wcstol(szText, NULL, 10);
		swprintf_s(text, TEXT("INVERT%i"), i); if (ReadFromFile(L"CONTROLS", text, szText)) INVERT[i] = wcstol(szText, NULL, 10);
		swprintf_s(text, TEXT("HALF%i"), i); if (ReadFromFile(L"CONTROLS", text, szText)) HALF[i] = wcstol(szText, NULL, 10);
		swprintf_s(text, TEXT("BUTTON%i"), i); if (ReadFromFile(L"CONTROLS", text, szText)) BUTTON[i] = wcstol(szText, NULL, 10);
		swprintf_s(text, TEXT("LINEAR%i"), i); if (ReadFromFile(L"CONTROLS", text, szText)) LINEAR[i] = wcstol(szText, NULL, 10);
		swprintf_s(text, TEXT("OFFSET%i"), i); if (ReadFromFile(L"CONTROLS", text, szText)) OFFSET[i] = wcstol(szText, NULL, 10);
		swprintf_s(text, TEXT("DEADZONE%i"), i); if (ReadFromFile(L"CONTROLS", text, szText)) DEADZONE[i] = wcstol(szText, NULL, 10);
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

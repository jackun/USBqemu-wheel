#include <stdio.h>
#include <windows.h>
#include <windowsx.h>
#include <vector>

#include "../USB.h"
#include "resource.h"
#include "../usb-pad/config.h"

#include <setupapi.h>
extern "C" {
	#include "../ddk/hidsdi.h"
}

HINSTANCE hInst;
std::vector<std::string> joys;

void SysMessage(char *fmt, ...) {
	va_list list;
	char tmp[512];

	va_start(list,fmt);
	vsprintf(tmp,fmt,list);
	va_end(list);
	MessageBox(0, tmp, "USBlinuz Msg", 0);
}

void populate(HWND hW)
{
	joys.clear();
	joys.push_back("");
	int i=0, sel_idx=1;
	HANDLE usbHandle = INVALID_HANDLE_VALUE;
	DWORD needed=0;
	HDEVINFO devInfo;
	GUID guid;
	SP_DEVICE_INTERFACE_DATA diData;
	PSP_DEVICE_INTERFACE_DETAIL_DATA didData;
	HIDD_ATTRIBUTES attr;
	PHIDP_PREPARSED_DATA pPreparsedData;
	HIDP_CAPS caps;
	OVERLAPPED ovl;

	memset(&ovl, 0, sizeof(OVERLAPPED));
	ovl.hEvent = CreateEvent(0, 0, 0, 0);
	ovl.Offset = 0;
	ovl.OffsetHigh = 0;

	HidD_GetHidGuid(&guid);

	devInfo = SetupDiGetClassDevs(&guid, 0, 0, DIGCF_DEVICEINTERFACE);
	if(!devInfo) return;

	diData.cbSize = sizeof(diData);

	SendDlgItemMessageW(hW, IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)L"None");
	SendDlgItemMessageW(hW, IDC_COMBO2, CB_ADDSTRING, 0, (LPARAM)L"None");
	SendDlgItemMessageW(hW, IDC_COMBO1, CB_SETCURSEL, 0, 0);
	SendDlgItemMessageW(hW, IDC_COMBO2, CB_SETCURSEL, 0, 0);

	while(SetupDiEnumDeviceInterfaces(devInfo, 0, &guid, i, &diData)){
		if(usbHandle != INVALID_HANDLE_VALUE)
			CloseHandle(usbHandle);

		SetupDiGetDeviceInterfaceDetail(devInfo, &diData, 0, 0, &needed, 0);

		didData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(needed);
		didData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
		if(!SetupDiGetDeviceInterfaceDetail(devInfo, &diData, didData, needed, 0, 0)){
			free(didData);
			break;
		}

		usbHandle = CreateFile(didData->DevicePath, GENERIC_READ|GENERIC_WRITE,
			FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);

		if(usbHandle == INVALID_HANDLE_VALUE){
			fprintf(stderr,"Could not open device %i\n", i);
			free(didData);
			i++;
			continue;
		}

		HidD_GetAttributes(usbHandle, &attr);
		HidD_GetPreparsedData(usbHandle, &pPreparsedData);

		HidP_GetCaps(pPreparsedData, &caps);

		if(caps.UsagePage == HID_USAGE_PAGE_GENERIC && caps.Usage == HID_USAGE_GENERIC_JOYSTICK)
		{
			fprintf(stderr, "Joystick found %04X:%04X\n", attr.VendorID, attr.ProductID);
			joys.push_back(std::string(didData->DevicePath));

			wchar_t str[MAX_PATH];
			HidD_GetProductString(usbHandle, str, sizeof(str));//TODO HidD_GetProductString returns unicode always?
			SendDlgItemMessageW(hW, IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)str);
			SendDlgItemMessageW(hW, IDC_COMBO2, CB_ADDSTRING, 0, (LPARAM)str);

			if(strcmp(player_joys[0].c_str(), didData->DevicePath) == 0)
				SendDlgItemMessageW(hW, IDC_COMBO1, CB_SETCURSEL, sel_idx, 0);
			if(strcmp(player_joys[1].c_str(), didData->DevicePath) == 0)
				SendDlgItemMessageW(hW, IDC_COMBO2, CB_SETCURSEL, sel_idx, 0);
			sel_idx++;
		}
		free(didData);
		HidD_FreePreparsedData(pPreparsedData);
		i++;
	}
}

BOOL CALLBACK ConfigureDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	switch(uMsg) {
		case WM_INITDIALOG:
			LoadConfig();
			if (conf.Log) CheckDlgButton(hW, IDC_LOGGING, TRUE);
			populate(hW);
			return TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
				case IDCANCEL:
					EndDialog(hW, TRUE);
					return TRUE;
				case IDOK:
					if (IsDlgButtonChecked(hW, IDC_LOGGING))
						 conf.Log = 1;
					else conf.Log = 0;
					
					if(joys.size() > 0)
					{
						player_joys[0] = *(joys.begin() + SendDlgItemMessage(hW, IDC_COMBO1, CB_GETCURSEL, 0, 0));
						player_joys[1] = *(joys.begin() + SendDlgItemMessage(hW, IDC_COMBO2, CB_GETCURSEL, 0, 0));
					}
					SaveConfig();
					EndDialog(hW, FALSE);
					return TRUE;
			}
	}
	return FALSE;
}


BOOL CALLBACK AboutDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch(uMsg) {
		case WM_INITDIALOG:
			return TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
				case IDOK:
					EndDialog(hW, FALSE);
					return TRUE;
			}
	}
	return FALSE;
}

void CALLBACK USBconfigure() {
    DialogBox(hInst,
              MAKEINTRESOURCE(IDD_CONFIG),
              GetActiveWindow(),  
              (DLGPROC)ConfigureDlgProc); 
}

void CALLBACK USBabout() {
    DialogBox(hInst,
              MAKEINTRESOURCE(IDD_ABOUT),
              GetActiveWindow(),  
              (DLGPROC)AboutDlgProc);
}

BOOL APIENTRY DllMain(HANDLE hModule,                  // DLL INIT
                      DWORD  dwReason, 
                      LPVOID lpReserved) {
	hInst = (HINSTANCE)hModule;
	return TRUE;                                          // very quick :)
}

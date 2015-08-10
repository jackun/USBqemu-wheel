#include "../USB.h"
#include "../usb-mic/mic-audiodefs.h"
#include "resource.h"
#include <CommCtrl.h>

extern HINSTANCE hInst;
//TODO unicode
static OPENFILENAME ofn;
static AudioDeviceInfoList audioDevs;

#if BUILD_RAW
	extern BOOL CALLBACK ConfigureRawDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam);
#endif
#if BUILD_DX
	extern BOOL CALLBACK DxDialogProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam);
#endif

#define CHECKED_SET_MAX_INT(var, hDlg, nIDDlgItem, bSigned, min, max)\
do {\
	/*CheckControlTextIsNumber(GetDlgItem(hDlg, nIDDlgItem), bSigned, 0);*/\
	var = GetDlgItemInt(hDlg, nIDDlgItem, NULL, bSigned);\
	if (var < min)\
		var = min;\
	else if (var > max)\
	{\
		var = max;\
		SetDlgItemInt(hDlg, nIDDlgItem, var, bSigned);\
		SendMessage(GetDlgItem(hDlg, nIDDlgItem), EM_SETSEL, -2, -2);\
	}\
} while (0)

void SysMessage(const char *fmt, ...) {
	va_list list;
	char tmp[512];

	va_start(list, fmt);
	vsprintf_s(tmp, 512, fmt, list);
	va_end(list);
	MessageBoxA(0, tmp, "Qemu USB Msg", 0);
}

BOOL CALLBACK MsdDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch(uMsg) {
		case WM_INITDIALOG:
			SetWindowText(GetDlgItem(hW, IDC_EDIT1), conf.usb_img);
			return TRUE;

		case WM_COMMAND:
			if (HIWORD(wParam) == BN_CLICKED) {
				switch(LOWORD(wParam)) {
				case IDC_BUTTON1:
					ZeroMemory(&ofn, sizeof(ofn));
					ofn.lStructSize = sizeof(ofn);
					ofn.hwndOwner = hW;
					ofn.lpstrTitle = TEXT("USB image file");
					ofn.lpstrFile = conf.usb_img;
					ofn.nMaxFile = sizeof(conf.usb_img);
					ofn.lpstrFilter = TEXT("All\0*.*\0");
					ofn.nFilterIndex = 1;
					ofn.lpstrFileTitle = NULL;
					ofn.nMaxFileTitle = 0;
					ofn.lpstrInitialDir = NULL;
					ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

					if(GetOpenFileName(&ofn)==TRUE) {
						SetWindowText(GetDlgItem(hW, IDC_EDIT1), ofn.lpstrFile);
					}
					break;
				case IDOK:
					GetWindowText(GetDlgItem(hW, IDC_EDIT1), conf.usb_img, sizeof(conf.usb_img));
					//strcpy_s(conf.usb_img, ofn.lpstrFile);
					SaveConfig();
				case IDCANCEL:
					EndDialog(hW, FALSE);
					return TRUE;
				}
			}
	}
	return FALSE;
}

BOOL CALLBACK MicDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	int tmp = 0;
	switch(uMsg) {
		case WM_INITDIALOG:
			SendDlgItemMessage(hW, IDC_MICSLIDER, TBM_SETRANGEMIN, TRUE, 1);
			SendDlgItemMessage(hW, IDC_MICSLIDER, TBM_SETRANGEMAX, TRUE, 1000);
			SendDlgItemMessage(hW, IDC_MICSLIDER, TBM_SETPOS, TRUE, conf.MicBuffering);
			SetDlgItemInt(hW, IDC_MICBUF, conf.MicBuffering, FALSE);

			SendDlgItemMessageW(hW, IDC_COMBOMIC1, CB_ADDSTRING, 0, (LPARAM)L"None");
			SendDlgItemMessageW(hW, IDC_COMBOMIC2, CB_ADDSTRING, 0, (LPARAM)L"None");

			SendDlgItemMessage(hW, IDC_COMBOMIC1, CB_SETCURSEL, 0, 0);
			SendDlgItemMessage(hW, IDC_COMBOMIC2, CB_SETCURSEL, 0, 0);

			if(AudioInit())
			{
				audioDevs.clear();
				GetAudioDevices(audioDevs);
				AudioDeviceInfoList::iterator it;
				int i = 0;
				for(it = audioDevs.begin(); it != audioDevs.end(); it++)
				{
					SendDlgItemMessageW(hW, IDC_COMBOMIC1, CB_ADDSTRING, 0, (LPARAM)it->strName.c_str());
					SendDlgItemMessageW(hW, IDC_COMBOMIC2, CB_ADDSTRING, 0, (LPARAM)it->strName.c_str());

					i++;
					if(it->strID == conf.mics[0])
						SendDlgItemMessage(hW, IDC_COMBOMIC1, CB_SETCURSEL, i, i);
					if(it->strID == conf.mics[1])
						SendDlgItemMessage(hW, IDC_COMBOMIC2, CB_SETCURSEL, i, i);
				}
				AudioDeinit();
			}
			return TRUE;

		case WM_HSCROLL:
		if ((HWND)lParam == GetDlgItem(hW, IDC_MICSLIDER))
		{
			int pos = SendDlgItemMessage(hW, IDC_MICSLIDER, TBM_GETPOS, 0, 0);
			SetDlgItemInt(hW, IDC_MICBUF, pos, FALSE);
			break;
		}
		break;

		case WM_COMMAND:
		switch(HIWORD(wParam))
		{
			case EN_CHANGE:
			{
				switch (LOWORD(wParam))
				{
				case IDC_MICBUF:
					CHECKED_SET_MAX_INT(tmp, hW, IDC_MICBUF, FALSE, 1, 1000);
					SendDlgItemMessage(hW, IDC_MICSLIDER, TBM_SETPOS, TRUE, tmp);
					break;
				}
			}
			break;

			case BN_CLICKED:
			{
				switch(LOWORD(wParam)) {
				case IDOK:
					int p1, p2;
					p1 = SendDlgItemMessage(hW, IDC_COMBOMIC1, CB_GETCURSEL, 0, 0);
					p2 = SendDlgItemMessage(hW, IDC_COMBOMIC2, CB_GETCURSEL, 0, 0);

					if(p1 > 0)
						conf.mics[0] = (audioDevs.begin() + p1 - 1)->strID;
					else
						conf.mics[0] = L"";
					if(p2 > 0)
						conf.mics[1] = (audioDevs.begin() + p2 - 1)->strID;
					else
						conf.mics[1] = L"";

					conf.MicBuffering = SendDlgItemMessage(hW, IDC_MICSLIDER, TBM_GETPOS, 0, 0);

					SaveConfig();
				case IDCANCEL:
					EndDialog(hW, FALSE);
					return TRUE;
				}
			}
			break;
		}
	}
	return FALSE;
}

BOOL CALLBACK ConfigureDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch(uMsg) {
		case WM_INITDIALOG:
			SendDlgItemMessageA(hW, IDC_BUILD_DATE, WM_SETTEXT, 0, (LPARAM)__DATE__ " " __TIME__);
			LoadConfig();
			CheckDlgButton(hW, IDC_LOGGING, conf.Log);
			CheckDlgButton(hW, IDC_DFP_PASS, conf.DFPPass);
			//Selected emulated devices.
			SendDlgItemMessageA(hW, IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)"None");
			SendDlgItemMessageA(hW, IDC_COMBO2, CB_ADDSTRING, 0, (LPARAM)"None");
#if BUILD_RAW
			SendDlgItemMessageA(hW, IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)"Wheel (raw input api)");
			SendDlgItemMessageA(hW, IDC_COMBO2, CB_ADDSTRING, 0, (LPARAM)"Wheel (raw input api)");
#else
			SendDlgItemMessageA(hW, IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)"Wheel (raw input api) (disabled)");
			SendDlgItemMessageA(hW, IDC_COMBO2, CB_ADDSTRING, 0, (LPARAM)"Wheel (raw input api) (disabled)");
#endif
#if BUILD_DX
			SendDlgItemMessageA(hW, IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)"Wheel (DX input)");
			SendDlgItemMessageA(hW, IDC_COMBO2, CB_ADDSTRING, 0, (LPARAM)"Wheel (DX input)");
#else
			SendDlgItemMessageA(hW, IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)"Wheel (DX input) (disabled)");
			SendDlgItemMessageA(hW, IDC_COMBO2, CB_ADDSTRING, 0, (LPARAM)"Wheel (DX input) (disabled)");
#endif
			SendDlgItemMessageA(hW, IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)"Mass-storage");
			SendDlgItemMessageA(hW, IDC_COMBO2, CB_ADDSTRING, 0, (LPARAM)"Mass-storage");

			//TODO Only one at a time?
			SendDlgItemMessageA(hW, IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)"Singstar (2 players) (experimental!)");

			//Port 1 aka device/player 1
			SendDlgItemMessage(hW, IDC_COMBO1, CB_SETCURSEL, conf.Port1, 0);
			//Port 0 aka device/player 2
			SendDlgItemMessage(hW, IDC_COMBO2, CB_SETCURSEL, conf.Port0, 0);

			SendDlgItemMessageA(hW, IDC_COMBO_WHEEL_TYPE1, CB_ADDSTRING, 0, (LPARAM)"Driving Force / Generic Logitech Wheel");
			SendDlgItemMessageA(hW, IDC_COMBO_WHEEL_TYPE1, CB_ADDSTRING, 0, (LPARAM)"Driving Force Pro");
			SendDlgItemMessageA(hW, IDC_COMBO_WHEEL_TYPE1, CB_ADDSTRING, 0, (LPARAM)"GT Force");
			SendDlgItemMessage(hW, IDC_COMBO_WHEEL_TYPE1, CB_SETCURSEL, conf.WheelType[0], 0);

			SendDlgItemMessageA(hW, IDC_COMBO_WHEEL_TYPE2, CB_ADDSTRING, 0, (LPARAM)"Driving Force / Generic Logitech Wheel");
			SendDlgItemMessageA(hW, IDC_COMBO_WHEEL_TYPE2, CB_ADDSTRING, 0, (LPARAM)"Driving Force Pro");
			SendDlgItemMessageA(hW, IDC_COMBO_WHEEL_TYPE2, CB_ADDSTRING, 0, (LPARAM)"GT Force");
			SendDlgItemMessage(hW, IDC_COMBO_WHEEL_TYPE2, CB_SETCURSEL, conf.WheelType[1], 0);

			return TRUE;
			break;
		case WM_COMMAND:
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				switch(LOWORD(wParam)) {
				case IDC_CONFIGURE1:
				case IDC_CONFIGURE2:
					switch(SendDlgItemMessage(hW, LOWORD(wParam) == IDC_CONFIGURE1 ? IDC_COMBO1 : IDC_COMBO2, CB_GETCURSEL, 0, 0))
					{
#if BUILD_RAW
					case 1:
						DialogBoxParam(hInst,
							MAKEINTRESOURCE(201),
							hW,
							(DLGPROC)ConfigureRawDlgProc, 0);
						return FALSE;
#endif
#if BUILD_DX
					case 2:
						DialogBoxParam(hInst,
							MAKEINTRESOURCE(202),
							hW,
							(DLGPROC)DxDialogProc, 0);
						return FALSE;
#endif
					case 3:
						DialogBoxParam(hInst,
							MAKEINTRESOURCE(IDD_DLGMSD),
							hW,
							(DLGPROC)MsdDlgProc, 0);
						return FALSE;
					case 4:
						DialogBoxParam(hInst,
							MAKEINTRESOURCE(IDD_DLGMIC),
							hW,
							(DLGPROC)MicDlgProc, 0);
						return FALSE;
					default:
						break;
					}
					break;
				case IDCANCEL:
					EndDialog(hW, TRUE);
					return TRUE;
				case IDOK:
					conf.Log = IsDlgButtonChecked(hW, IDC_LOGGING);
					conf.DFPPass = IsDlgButtonChecked(hW, IDC_DFP_PASS);
					//device type
					conf.Port1 = SendDlgItemMessage(hW, IDC_COMBO1, CB_GETCURSEL, 0, 0);
					conf.Port0 = SendDlgItemMessage(hW, IDC_COMBO2, CB_GETCURSEL, 0, 0);
					//wheel type
					conf.WheelType[0] = SendDlgItemMessage(hW, IDC_COMBO_WHEEL_TYPE1, CB_GETCURSEL, 0, 0);
					conf.WheelType[1] = SendDlgItemMessage(hW, IDC_COMBO_WHEEL_TYPE2, CB_GETCURSEL, 0, 0);
					if(conf.Port1 == 2 && conf.Port0 == 2) {
						MessageBoxExA(hW, "Currently only one DX wheel\n at a time is supported!", "Warning", MB_ICONEXCLAMATION, 0);
						return FALSE;
					} else if(conf.Port1 == 3 && conf.Port0 == 3) {
						// at a time? sounds weird
						MessageBoxExA(hW, "Currently only one USB storage device\n at a time is supported!", "Warning", MB_ICONEXCLAMATION, 0);
						return FALSE;
					} else if(conf.Port1 == 4 && conf.Port0 == 4) {
						MessageBoxExA(hW, "Currently only one Singstar device\n at a time is supported!", "Warning", MB_ICONEXCLAMATION, 0);
						return FALSE;
					}
					SaveConfig();
					CreateDevices();
					EndDialog(hW, FALSE);
					return TRUE;
				}
			}
	}

	return FALSE;
}


EXPORT_C_(BOOL) AboutDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam) {
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

EXPORT_C_(void) USBconfigure() {
    DialogBox(hInst,
              MAKEINTRESOURCE(IDD_CONFIG),
              GetActiveWindow(),
              (DLGPROC)ConfigureDlgProc);
}

EXPORT_C_(void) USBabout() {
    DialogBox(hInst,
              MAKEINTRESOURCE(IDD_ABOUT),
              GetActiveWindow(),
              (DLGPROC)AboutDlgProc);
}

BOOL APIENTRY DllMain(HANDLE hModule,
                      DWORD  dwReason,
                      LPVOID lpReserved) {
	hInst = (HINSTANCE)hModule;
	return TRUE;
}

#include "../USB.h"
#include "resource.h"

extern HINSTANCE hInst;

#if BUILD_RAW
	extern BOOL CALLBACK ConfigureRawDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam);
#endif
#if BUILD_DX
	extern BOOL CALLBACK DxDialogProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam);
#endif

OPENFILENAMEA ofn;

BOOL CALLBACK MsdDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch(uMsg) {
		case WM_INITDIALOG:
			SetWindowTextA(GetDlgItem(hW, IDC_EDIT1), conf.usb_img);
			return TRUE;

		case WM_COMMAND:
			if (HIWORD(wParam) == BN_CLICKED) {
				switch(LOWORD(wParam)) {
				case IDC_BUTTON1:
					ZeroMemory(&ofn, sizeof(ofn));
					ofn.lStructSize = sizeof(ofn);
					ofn.hwndOwner = hW;
					ofn.lpstrTitle = "USB image file";
					ofn.lpstrFile = conf.usb_img;
					ofn.nMaxFile = sizeof(conf.usb_img);
					ofn.lpstrFilter = "All\0*.*\0";
					ofn.nFilterIndex = 1;
					ofn.lpstrFileTitle = NULL;
					ofn.nMaxFileTitle = 0;
					ofn.lpstrInitialDir = NULL;
					ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

					if(GetOpenFileNameA(&ofn)==TRUE) {
						SetWindowTextA(GetDlgItem(hW, IDC_EDIT1), ofn.lpstrFile);
					}
					break;
				case IDOK:
					GetWindowTextA(GetDlgItem(hW, IDC_EDIT1), conf.usb_img, sizeof(conf.usb_img));
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

BOOL CALLBACK ConfigureDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch(uMsg) {
		case WM_INITDIALOG:
			SendDlgItemMessage(hW, IDC_BUILD_DATE, WM_SETTEXT, 0, (LPARAM)__DATE__ " " __TIME__);
			LoadConfig();
			CheckDlgButton(hW, IDC_LOGGING, conf.Log);
			CheckDlgButton(hW, IDC_DFP_PASS, conf.DFPPass);
			//Selected emulated devices. Maybe add virtual USB stick, keyboard and mouse
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
			//Port 1 aka device/player 1
			SendDlgItemMessage(hW, IDC_COMBO1, CB_SETCURSEL, conf.Port1, 0);
			//Port 0 aka device/player 2
			SendDlgItemMessage(hW, IDC_COMBO2, CB_SETCURSEL, conf.Port0, 0);

			SendDlgItemMessageA(hW, IDC_COMBO_WHEEL_TYPE1, CB_ADDSTRING, 0, (LPARAM)"DF / Generic Logitech Wheel");
			SendDlgItemMessageA(hW, IDC_COMBO_WHEEL_TYPE1, CB_ADDSTRING, 0, (LPARAM)"Driving Force Pro");
			SendDlgItemMessage(hW, IDC_COMBO_WHEEL_TYPE1, CB_SETCURSEL, conf.WheelType[0], 0);

			SendDlgItemMessageA(hW, IDC_COMBO_WHEEL_TYPE2, CB_ADDSTRING, 0, (LPARAM)"DF / Generic Logitech Wheel");
			SendDlgItemMessageA(hW, IDC_COMBO_WHEEL_TYPE2, CB_ADDSTRING, 0, (LPARAM)"Driving Force Pro");
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

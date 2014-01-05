#include "../USB.h"
#include "resource.h"

extern HINSTANCE hInst;

#if BUILD_RAW
	extern BOOL CALLBACK ConfigureRawDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam);
#endif
#if BUILD_DX
	extern BOOL CALLBACK DxDialogProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam);
#endif

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
			//Port 1 aka device/player 1
			SendDlgItemMessage(hW, IDC_COMBO1, CB_SETCURSEL, conf.Port1, 0);
			//Port 0 aka device/player 2
			SendDlgItemMessage(hW, IDC_COMBO2, CB_SETCURSEL, conf.Port0, 0);
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
					conf.Port1 = SendDlgItemMessage(hW, IDC_COMBO1, CB_GETCURSEL, 0, 0);
					conf.Port0 = SendDlgItemMessage(hW, IDC_COMBO2, CB_GETCURSEL, 0, 0);
					if(conf.Port1 == 2 && conf.Port0 == 2)
					{
						MessageBoxExA(hW, "Currently only one DX wheel is supported!", "Warning", MB_ICONEXCLAMATION, 0);
						return FALSE;
					}

					SaveConfig();
					EndDialog(hW, FALSE);
					return TRUE;
				}
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

BOOL APIENTRY DllMain(HANDLE hModule,
                      DWORD  dwReason,
                      LPVOID lpReserved) {
	hInst = (HINSTANCE)hModule;
	return TRUE;
}

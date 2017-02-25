#include "usb-msd.h"
#include "../Win32/Config-win32.h"
#include "../Win32/resource.h"

#define APINAME "cstdio"
static OPENFILENAMEW ofn;

BOOL CALLBACK MsdDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	int port;
	static wchar_t buff[4096] = { 0 };

	switch (uMsg) {
	case WM_INITDIALOG:
	{
		memset(buff, 0, sizeof(buff));
		port = (int)lParam;
		SetWindowLong(hW, GWL_USERDATA, (LONG)lParam);
		CONFIGVARIANT var(N_CONFIG_PATH, CONFIG_TYPE_WCHAR);
		if (LoadSetting(port, APINAME, var))
			wcsncpy_s(buff, var.wstrValue.c_str(), ARRAYSIZE(buff));
		SetWindowTextW(GetDlgItem(hW, IDC_EDIT1), buff);
		return TRUE;
	}
	case WM_CREATE:
		SetWindowLong(hW, GWL_USERDATA, (LONG)lParam);
		break;
	case WM_COMMAND:

		if (HIWORD(wParam) == BN_CLICKED) {
			switch (LOWORD(wParam)) {
			case IDC_BUTTON1:
				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = hW;
				ofn.lpstrTitle = L"USB image file";
				ofn.lpstrFile = buff;
				ofn.nMaxFile = ARRAYSIZE(buff);
				ofn.lpstrFilter = L"All\0*.*\0";
				ofn.nFilterIndex = 1;
				ofn.lpstrFileTitle = NULL;
				ofn.nMaxFileTitle = 0;
				ofn.lpstrInitialDir = NULL;
				ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

				if (GetOpenFileName(&ofn)) {
					SetWindowText(GetDlgItem(hW, IDC_EDIT1), ofn.lpstrFile);
				}
				break;
			case IDOK:
			{
				INT_PTR res = RESULT_OK;
				GetWindowTextW(GetDlgItem(hW, IDC_EDIT1), buff, ARRAYSIZE(buff));
				port = (int)GetWindowLong(hW, GWL_USERDATA);
				CONFIGVARIANT var(N_CONFIG_PATH, buff);
				if (!SaveSetting(port, APINAME, var))
					res = RESULT_FAILED;
				//strcpy_s(conf.usb_img, ofn.lpstrFile);
				EndDialog(hW, res);
				return TRUE;
			}
			case IDCANCEL:
				EndDialog(hW, FALSE);
				return TRUE;
			}
		}
	}
	return FALSE;
}

int MsdDevice::Configure(int port, std::string api, void *data)
{
	Win32Handles handles = *(Win32Handles*)data;
	return DialogBoxParam(handles.hInst,
		MAKEINTRESOURCE(IDD_DLGMSD),
		handles.hWnd,
		(DLGPROC)MsdDlgProc, port);
}

/*
bool MsdDevice::LoadSettings(int port, TSTDSTRING& path)
{
}

bool MsdDevice::SaveSettings(int port, TSTDSTRING& path)
{
}*/
#undef APINAME

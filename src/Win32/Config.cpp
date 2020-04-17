#include "../USB.h"
#include "resource.h"
#include "Config.h"
#include "../deviceproxy.h"
#include "../usb-pad/padproxy.h"
#include "../usb-mic/audiodeviceproxy.h"
#include "../configuration.h"

HINSTANCE hInst;
extern bool configChanged;

void SysMessageA(const char *fmt, ...) {
	va_list list;
	char tmp[512];

	va_start(list, fmt);
	vsprintf_s(tmp, 512, fmt, list);
	va_end(list);
	MessageBoxA(0, tmp, "Qemu USB Msg", 0);
}

void SysMessageW(const wchar_t *fmt, ...) {
	va_list list;
	wchar_t tmp[512];

	va_start(list, fmt);
	vswprintf_s(tmp, 512, fmt, list);
	va_end(list);
	MessageBoxW(0, tmp, L"Qemu USB Msg", 0);
}

void SelChangedAPI(HWND hW, int port)
{
	int sel = SendDlgItemMessage(hW, port ? IDC_COMBO_API1 : IDC_COMBO_API2, CB_GETCURSEL, 0, 0);
	int devtype = SendDlgItemMessage(hW, port ? IDC_COMBO1 : IDC_COMBO2, CB_GETCURSEL, 0, 0);
	if (devtype == 0)
		return;
	devtype--;
	auto& rd = RegisterDevice::instance();
	auto devName = rd.Name(devtype);
	auto apis = rd.Device(devtype)->ListAPIs();
	auto it = apis.begin();
	std::advance(it, sel);
	changedAPIs[std::make_pair(port, devName)] = *it;
}

void PopulateAPIs(HWND hW, int port)
{
	OSDebugOut(TEXT("Populate api %d\n"), port);
	SendDlgItemMessage(hW, port ? IDC_COMBO_API1 : IDC_COMBO_API2, CB_RESETCONTENT, 0, 0);
	int devtype = SendDlgItemMessage(hW, port ? IDC_COMBO1 : IDC_COMBO2, CB_GETCURSEL, 0, 0);
	if (devtype == 0)
		return;
	devtype--;
	auto& rd = RegisterDevice::instance();
	auto dev = rd.Device(devtype);
	auto devName = rd.Name(devtype);
	auto apis = dev->ListAPIs();

	std::string selApi = GetSelectedAPI(std::make_pair(port, devName));

	std::string var;
	if (LoadSetting(nullptr, port, rd.Name(devtype), N_DEVICE_API, var))
		OSDebugOut(L"Current API: %S\n", var.c_str());
	else
	{
		if (apis.begin() != apis.end())
		{
			selApi = *apis.begin();
			changedAPIs[std::make_pair(port, devName)] = selApi;
		}
	}

	int i = 0, sel = 0;
	for (auto& api : apis)
	{
		auto name = dev->LongAPIName(api);
		if (!name)
			continue;
		SendDlgItemMessageW(hW, port ? IDC_COMBO_API1 : IDC_COMBO_API2, CB_ADDSTRING, 0, (LPARAM)name);
		if (api == var)
			sel = i;
		i++;
	}
	SendDlgItemMessage(hW, port ? IDC_COMBO_API1 : IDC_COMBO_API2, CB_SETCURSEL, sel, 0);
}

BOOL CALLBACK ConfigureDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	int port;
	switch(uMsg) {
		case WM_INITDIALOG:
			SendDlgItemMessageA(hW, IDC_BUILD_DATE, WM_SETTEXT, 0, (LPARAM)__DATE__ " " __TIME__);
			LoadConfig();
			CheckDlgButton(hW, IDC_LOGGING, conf.Log);
			//Selected emulated devices.
			SendDlgItemMessageA(hW, IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)"None");
			SendDlgItemMessageA(hW, IDC_COMBO2, CB_ADDSTRING, 0, (LPARAM)"None");

			{
				auto& rd = RegisterDevice::instance();
				int i = 0, p1 = 0, p2 = 0;
				for (auto& name : rd.Names())
				{
					i++; //jump over "None"
					auto dev = rd.Device(name);
					SendDlgItemMessageW(hW, IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)dev->Name());
					SendDlgItemMessageW(hW, IDC_COMBO2, CB_ADDSTRING, 0, (LPARAM)dev->Name());

					//Port 1 aka device/player 1
					if (conf.Port[1] == name)
						p1 = i;
					//Port 0 aka device/player 2
					if (conf.Port[0] == name)
						p2 = i;
				}
				SendDlgItemMessage(hW, IDC_COMBO1, CB_SETCURSEL, p1, 0);
				SendDlgItemMessage(hW, IDC_COMBO2, CB_SETCURSEL, p2, 0);
				PopulateAPIs(hW, 0);
				PopulateAPIs(hW, 1);
			}

			SendDlgItemMessageA(hW, IDC_COMBO_WHEEL_TYPE1, CB_ADDSTRING, 0, (LPARAM)"Driving Force");
			SendDlgItemMessageA(hW, IDC_COMBO_WHEEL_TYPE1, CB_ADDSTRING, 0, (LPARAM)"Driving Force Pro");
			SendDlgItemMessageA(hW, IDC_COMBO_WHEEL_TYPE1, CB_ADDSTRING, 0, (LPARAM)"Driving Force Pro (rev11.02)");
			SendDlgItemMessageA(hW, IDC_COMBO_WHEEL_TYPE1, CB_ADDSTRING, 0, (LPARAM)"GT Force");
			SendDlgItemMessage(hW, IDC_COMBO_WHEEL_TYPE1, CB_SETCURSEL, conf.WheelType[PLAYER_ONE_PORT], 0);

			SendDlgItemMessageA(hW, IDC_COMBO_WHEEL_TYPE2, CB_ADDSTRING, 0, (LPARAM)"Driving Force");
			SendDlgItemMessageA(hW, IDC_COMBO_WHEEL_TYPE2, CB_ADDSTRING, 0, (LPARAM)"Driving Force Pro");
			SendDlgItemMessageA(hW, IDC_COMBO_WHEEL_TYPE2, CB_ADDSTRING, 0, (LPARAM)"Driving Force Pro (rev11.02)");
			SendDlgItemMessageA(hW, IDC_COMBO_WHEEL_TYPE2, CB_ADDSTRING, 0, (LPARAM)"GT Force");
			SendDlgItemMessage(hW, IDC_COMBO_WHEEL_TYPE2, CB_SETCURSEL, conf.WheelType[PLAYER_TWO_PORT], 0);

			return TRUE;
			break;
		case WM_COMMAND:
			switch (HIWORD(wParam))
			{
			case CBN_SELCHANGE:
				switch (LOWORD(wParam)) {
				case IDC_COMBO_API1:
				case IDC_COMBO_API2:
					port = (LOWORD(wParam) == IDC_COMBO_API1) ? 1 : 0;
					SelChangedAPI(hW, port);
					break;
				case IDC_COMBO1:
				case IDC_COMBO2:
					port = (LOWORD(wParam) == IDC_COMBO1) ? 1 : 0;
					PopulateAPIs(hW, port);
					break;
				}
			break;
			case BN_CLICKED:
				switch(LOWORD(wParam)) {
				case IDC_CONFIGURE1:
				case IDC_CONFIGURE2:
				{
					LRESULT devtype, apitype;
					port = (LOWORD(wParam) == IDC_CONFIGURE1) ? 1 : 0;
					devtype = SendDlgItemMessage(hW, port ? IDC_COMBO1 : IDC_COMBO2, CB_GETCURSEL, 0, 0);
					apitype = SendDlgItemMessage(hW, port ? IDC_COMBO_API1 : IDC_COMBO_API2, CB_GETCURSEL, 0, 0);

					if (devtype > 0)
					{
						devtype--;
						auto device = RegisterDevice::instance().Device(devtype);
						if (device)
						{
							auto list = device->ListAPIs();
							auto it = list.begin();
							std::advance(it, apitype);
							if (it == list.end())
								break;
							std::string api = *it;
							Win32Handles handles(hInst, hW);
							if (device->Configure(port, api, &handles) == RESULT_FAILED)
								SysMessage(TEXT("Some settings may not have been saved!\n"));
						}
					}
				}
				break;
				case IDCANCEL:
					EndDialog(hW, TRUE);
					return TRUE;
				case IDOK:
					conf.Log = IsDlgButtonChecked(hW, IDC_LOGGING);
					{
						auto& regInst = RegisterDevice::instance();
						int i;
						//device type
						i = SendDlgItemMessage(hW, IDC_COMBO1, CB_GETCURSEL, 0, 0);
						conf.Port[1] = regInst.Name(i - 1);
						i = SendDlgItemMessage(hW, IDC_COMBO2, CB_GETCURSEL, 0, 0);
						conf.Port[0] = regInst.Name(i - 1);
					}
					//wheel type
					conf.WheelType[PLAYER_ONE_PORT] = SendDlgItemMessage(hW, IDC_COMBO_WHEEL_TYPE1, CB_GETCURSEL, 0, 0);
					conf.WheelType[PLAYER_TWO_PORT] = SendDlgItemMessage(hW, IDC_COMBO_WHEEL_TYPE2, CB_GETCURSEL, 0, 0);

					SaveConfig();
					CreateDevices();
					EndDialog(hW, RESULT_OK);
					configChanged = true;
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
    RegisterAPIs();
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

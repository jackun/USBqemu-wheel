#include "usb-pt.h"
#include "../Win32/Config-win32.h"
#include "../Win32/resource.h"
#include "configuration.h"
#include <sstream>
#include <iomanip>


BOOL CALLBACK USBPTDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam);

struct Win32USBPTSettings
{
	int port;
	int ignore;
	ConfigUSBDevice current;
	std::vector<ConfigUSBDevice> devs;
};

int PTDevice::Configure(int port, const std::string& api, void *data)
{
	Win32Handles h = *(Win32Handles *)data;
	Win32USBPTSettings s {};
	s.port = port;

	{
		CONFIGVARIANT var(N_DEVICE, CONFIG_TYPE_CHAR);
		if (LoadSetting(port, APINAME, var))
		{
			sscanf(var.strValue.c_str(), "%d:%d:%x:%x:",
				&s.current.bus, &s.current.port, &s.current.vid, &s.current.pid);
		}
	}

	{
		CONFIGVARIANT var(N_IGNORE_BUSPORT, CONFIG_TYPE_INT);
		if (LoadSetting(port, APINAME, var))
			s.ignore = var.intValue;
	}

	get_usb_devices(s.devs);

	return DialogBoxParam(h.hInst,
		MAKEINTRESOURCE(IDD_DLGUSBPT),
		h.hWnd,
		(DLGPROC)USBPTDlgProc, (LPARAM)&s);
}

static BOOL CALLBACK USBPTDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	int tmp = 0;
	Win32USBPTSettings *s;

	switch (uMsg) {
	case WM_CREATE:
		SetWindowLong(hW, GWL_USERDATA, (LONG)lParam);
		break;
	case WM_INITDIALOG:
	{
		s = (Win32USBPTSettings *)lParam;
		SetWindowLong(hW, GWL_USERDATA, (LONG)lParam);

		if (!s)
			return FALSE;

		SendDlgItemMessageA(hW, IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)"None");
		SendDlgItemMessage(hW, IDC_COMBO1, CB_SETCURSEL, 0, 0);
		CheckDlgButton(hW, IDC_CHECK1, s->ignore);

		int i = 1;
		for (auto& dev : s->devs)
		{
			std::stringstream str;
			str << dev.name
				<< " [" << std::hex << std::setw(4) << std::setfill('0')
				<< dev.vid
				<< ":" << std::hex << std::setw(4) << std::setfill('0')
				<< dev.pid
				<< "]";
			str << " [bus" << dev.bus << " port" << dev.port << "]";

			SendDlgItemMessageA(hW, IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)str.str().c_str());
			if (s->current == dev)
				SendDlgItemMessage(hW, IDC_COMBO1, CB_SETCURSEL, i, i);
			i++;
		}
		return TRUE;
	}

	case WM_COMMAND:
		switch (HIWORD(wParam))
		{
		case BN_CLICKED:
		{
			switch (LOWORD(wParam)) {
			case IDOK:
			{
				int p, ret, ignore_busport;
				s = (Win32USBPTSettings *)GetWindowLong(hW, GWL_USERDATA);
				if (!s)
					return FALSE;

				INT_PTR res = RESULT_OK;
				p = SendDlgItemMessage(hW, IDC_COMBO1, CB_GETCURSEL, 0, 0);
				ignore_busport = IsDlgButtonChecked(hW, IDC_CHECK1);

				std::stringstream str;

				if (p > 0)
				{
					ConfigUSBDevice &dev = s->devs[p - 1];
					str << dev.bus << ":" << dev.port << ":"
						<< std::hex << std::setw(4) << std::setfill('0')
						<< dev.vid << ":"
						<< std::hex << std::setw(4) << std::setfill('0')
						<< dev.pid;
					str << ":" << dev.name;
				}

				CONFIGVARIANT var0(N_DEVICE, str.str());
				if (!SaveSetting(s->port, APINAME, var0))
					res = RESULT_FAILED;

				CONFIGVARIANT var1(N_IGNORE_BUSPORT, ignore_busport);
				if (!SaveSetting(s->port, APINAME, var1))
					res = RESULT_FAILED;

				EndDialog(hW, res);
				return TRUE;
			}
			case IDCANCEL:
				EndDialog(hW, RESULT_CANCELED);
				return TRUE;
			}
		}
		break;
		}
	}
	return FALSE;
}
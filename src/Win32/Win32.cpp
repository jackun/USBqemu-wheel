#define _WIN32_WINNT  0x0501
#include <stdio.h>
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>

#include <algorithm>
#include <vector>
#include <map>

#include "../USB.h"
#include "resource.h"
#include "Config.h"
#include "../usb-pad/config.h"

#define TXT(x) (#x)
char *BTN2TXT[] = { 
	"Cross",
	"Square",
	"Circle",
	"Triangle",
	"L1",
	"L2",
	"R1",
	"R2",
	"Select",
	"Start"
};

char *AXIS2TXT[] = { 
	"Axis X",
	"Axis Y",
	"Axis Z",
	//"Axis RX",
	//"Axis RY",
	"Axis RZ"
};

HINSTANCE hInst;
std::vector<std::string> joys;
DWORD selectedJoy[2];
//std::vector<std::string>::iterator* tmpIter;
typedef struct _Mappings {
	PS2Buttons	btnMap[MAX_BUTTONS];
	PS2Axis		axisMap[MAX_AXES];
} Mappings_t;

typedef struct _DevInfo
{
	int ply;
	RID_DEVICE_INFO_HID hid;

	bool operator==(const _DevInfo &t) const{
		if(ply == t.ply && hid.dwProductId == t.hid.dwProductId &&
			hid.dwVendorId == t.hid.dwVendorId &&
			hid.dwVersionNumber == t.hid.dwVersionNumber &&
			hid.usUsage == t.hid.usUsage &&
			hid.usUsagePage == t.hid.usUsagePage)
			return true;
		return false;
	}

	bool operator<(const _DevInfo &t) const{
		if(ply < t.ply) return true;
		if(hid.dwProductId < t.hid.dwProductId) return true;
		if(hid.dwVendorId < t.hid.dwVendorId) return true;
		if(hid.dwVersionNumber < t.hid.dwVersionNumber) return true;
		return false;
	}

} DevInfo_t;

typedef std::map<const DevInfo_t, Mappings_t> MappingsMap;
MappingsMap mappings;

PS2Buttons	*pbtnMap;
PS2Axis		*paxisMap;

uint32_t  axisDiff[MAX_AXES]; //previous axes values
bool      axisPass2 = false;

PS2Buttons btnCapturing  = PAD_BUTTON_COUNT;
PS2Axis    axisCapturing = PAD_AXIS_COUNT;

#define ARRAY_SIZE(x)	(sizeof(x) / sizeof((x)[0]))
#define CHECK(exp)		{ if(!(exp)) goto Error; }
#define SAFE_FREE(p)	{ if(p) { free(p); (p) = NULL; } }

void SysMessage(char *fmt, ...) {
	va_list list;
	char tmp[512];

	va_start(list,fmt);
	vsprintf_s(tmp,512,fmt,list);
	va_end(list);
	MessageBox(0, tmp, "USBlinuz Msg", 0);
}

void populate(HWND hW)
{
	mappings.clear();
	joys.clear();
	joys.push_back("");
	int i=0, sel_idx=1;
	HANDLE usbHandle = INVALID_HANDLE_VALUE;
	DWORD needed=0;
	HDEVINFO devInfo;
	GUID guid;
	SP_DEVICE_INTERFACE_DATA diData;
	PSP_DEVICE_INTERFACE_DETAIL_DATA didData = NULL;
	HIDD_ATTRIBUTES attr;
	PHIDP_PREPARSED_DATA pPreparsedData = NULL;
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

	LVCOLUMN LvCol;
	memset(&LvCol,0,sizeof(LvCol));
	LvCol.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM;
	LvCol.pszText="PC";
	LvCol.cx=0x4F;
	SendDlgItemMessage(hW, IDC_LIST1, LVM_SETEXTENDEDLISTVIEWSTYLE,
		0,LVS_EX_FULLROWSELECT); // Set style
	ListView_InsertColumn(GetDlgItem(hW, IDC_LIST1), 1, &LvCol);

	LvCol.pszText="PS2";
	ListView_InsertColumn(GetDlgItem(hW, IDC_LIST1), 1, &LvCol);

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
			std::string strPath(didData->DevicePath);
			std::transform(strPath.begin(), strPath.end(), strPath.begin(), ::toupper);
			joys.push_back(strPath);

			wchar_t str[MAX_PATH];
			HidD_GetProductString(usbHandle, str, sizeof(str));//TODO HidD_GetProductString returns unicode always?
			SendDlgItemMessageW(hW, IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)str);
			SendDlgItemMessageW(hW, IDC_COMBO2, CB_ADDSTRING, 0, (LPARAM)str);

			if(player_joys[0] == strPath)
			{
				SendDlgItemMessage(hW, IDC_COMBO1, CB_SETCURSEL, sel_idx, 0);
				selectedJoy[0] = sel_idx;
			}
			else if(player_joys[1] == strPath)
			{
				SendDlgItemMessage(hW, IDC_COMBO2, CB_SETCURSEL, sel_idx, 0);
				selectedJoy[1] = sel_idx;
			}
			sel_idx++;
		}
		SAFE_FREE(didData);
		HidD_FreePreparsedData(pPreparsedData);
		i++;
	}
}

void populateMappings(HWND hW, int ply)
{
	HANDLE usbHandle = INVALID_HANDLE_VALUE;
	OVERLAPPED ovl;
	HIDD_ATTRIBUTES attr;
	PHIDP_PREPARSED_DATA pPreparsedData = NULL;
	HIDP_CAPS caps;
	PS2Buttons btns[MAX_BUTTONS], *pbtns;
	PS2Axis axes[MAX_AXES], *paxes;
	DevInfo_t mapDevInfo;
	MappingsMap::iterator iter;

	pbtns = btns;
	paxes = axes;

	memset(&ovl, 0, sizeof(OVERLAPPED));
	ovl.hEvent = CreateEvent(0, 0, 0, 0);
	ovl.Offset = 0;
	ovl.OffsetHigh = 0;
	LVITEM lvItem;
	HWND lv = GetDlgItem(hW, IDC_LIST1);

	std::string joy = *(joys.begin() + selectedJoy[ply]);

	usbHandle = CreateFile(joy.c_str(), GENERIC_READ|GENERIC_WRITE,
		FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);

	if(usbHandle == INVALID_HANDLE_VALUE){
		fprintf(stderr,"Could not open device %s\n", joy.c_str());
		goto Error;
	}

	HidD_GetAttributes(usbHandle, &attr);
	HidD_GetPreparsedData(usbHandle, &pPreparsedData);
	HidP_GetCaps(pPreparsedData, &caps);
	
	mapDevInfo.ply = ply;
	mapDevInfo.hid.dwProductId = attr.ProductID;
	mapDevInfo.hid.dwVendorId = attr.VendorID;
	mapDevInfo.hid.dwVersionNumber = attr.VersionNumber;
	mapDevInfo.hid.usUsagePage = 1;
	mapDevInfo.hid.usUsage = 4;
	iter = mappings.find(mapDevInfo);

	if(iter != mappings.end())
	{
		pbtns = iter->second.btnMap;
		paxes = iter->second.axisMap;
	}
	else
		LoadMappings(ply, attr.VendorID, attr.ProductID, pbtns, paxes);

	memset(&lvItem,0,sizeof(lvItem));
	
	lvItem.mask = LVIF_TEXT;
	lvItem.cchTextMax = 256;
	lvItem.iItem = 0;
	lvItem.iSubItem = 0;

	for(int i = 0; i<MAX_BUTTONS; i++)
	{
		if(pbtns[i] >= PAD_BUTTON_COUNT)
			continue;
		
		lvItem.iItem = i;
		char tmp[256];
		sprintf_s(tmp, 256, "%d: Button %d", ply, i);
		lvItem.pszText = tmp;
		ListView_InsertItem(lv, &lvItem);

		sprintf_s(tmp, 256, "%s", BTN2TXT[pbtns[i]]);
		ListView_SetItemText(lv, lvItem.iItem, 1, tmp);
	}

	for(int i = 0; i<MAX_AXES; i++)
	{
		if(paxes[i] >= PAD_AXIS_COUNT)
			continue;
		
		lvItem.iItem = ListView_GetItemCount(lv);
		char tmp[256];
		sprintf_s(tmp, 256, "%d: Axis %d", ply, i);
		lvItem.pszText = tmp;
		ListView_InsertItem(lv, &lvItem);

		sprintf_s(tmp, 256, "%s", AXIS2TXT[paxes[i]]);
		ListView_SetItemText(lv, lvItem.iItem, 1, tmp);
	}

Error:
	if(pPreparsedData)
		HidD_FreePreparsedData(pPreparsedData);
}

#define CHECKDIFF(x) \
	if(axisPass2) {\
		if((uint32_t)abs((int)(axisDiff[(int)x] - value)) > (logical >> 2)){\
			paxisMap[x] = axisCapturing;\
			axisPass2 = false;\
			axisCapturing = PAD_AXIS_COUNT;\
			goto Error;\
		}\
	break;}

void ParseRawInput(PRAWINPUT pRawInput)
{
	PHIDP_PREPARSED_DATA pPreparsedData = NULL;
	HIDP_CAPS            Caps;
	PHIDP_BUTTON_CAPS    pButtonCaps = NULL;
	PHIDP_VALUE_CAPS     pValueCaps = NULL;
	USHORT               capsLength;
	UINT                 bufferSize;
	USAGE                usage[MAX_BUTTONS];
	ULONG                i, usageLength, value;
	char                 name[1024];
	UINT                 nameSize = 1024;
	UINT                 pSize;
	RID_DEVICE_INFO      devInfo;
	std::string          devName;
	DevInfo_t            mapDevInfo;
	Mappings_t           maps;
	std::pair<MappingsMap::iterator, bool> iter;

	//
	// Get the preparsed data block
	//
	
	CHECK( GetRawInputDeviceInfo(pRawInput->header.hDevice, RIDI_PREPARSEDDATA, NULL, &bufferSize) == 0 );
	CHECK( pPreparsedData = (PHIDP_PREPARSED_DATA)malloc(bufferSize) );
	CHECK( (int)GetRawInputDeviceInfo(pRawInput->header.hDevice, RIDI_PREPARSEDDATA, pPreparsedData, &bufferSize) >= 0 );
	CHECK( HidP_GetCaps(pPreparsedData, &Caps) == HIDP_STATUS_SUCCESS );
	GetRawInputDeviceInfo(pRawInput->header.hDevice, RIDI_DEVICENAME, name, &nameSize);
	pSize = sizeof(devInfo);
	GetRawInputDeviceInfo(pRawInput->header.hDevice, RIDI_DEVICEINFO, &devInfo, &pSize);

	devName = name;
	std::transform(devName.begin(), devName.end(), devName.begin(), ::toupper);
	uint8_t idx = -1;
	if(*(joys.begin() + selectedJoy[0]) == devName)
		idx = 0;
	else if(*(joys.begin() + selectedJoy[1]) == devName)
		idx = 1;
	else
		goto Error;

	mapDevInfo.ply = idx;
	mapDevInfo.hid = devInfo.hid;
	iter = mappings.insert(std::make_pair(mapDevInfo, maps));

	pbtnMap = mappings[mapDevInfo].btnMap;
	paxisMap = mappings[mapDevInfo].axisMap;

	if(iter.second) //false -> already loaded
		LoadMappings(idx, devInfo.hid.dwVendorId, devInfo.hid.dwProductId, pbtnMap, paxisMap);

	//Capture buttons
	if(btnCapturing < PAD_BUTTON_COUNT)
	{
		// Button caps
		CHECK( pButtonCaps = (PHIDP_BUTTON_CAPS)malloc(sizeof(HIDP_BUTTON_CAPS) * Caps.NumberInputButtonCaps) );

		capsLength = Caps.NumberInputButtonCaps;
		CHECK( HidP_GetButtonCaps(HidP_Input, pButtonCaps, &capsLength, pPreparsedData) == HIDP_STATUS_SUCCESS )
		int numberOfButtons = pButtonCaps->Range.UsageMax - pButtonCaps->Range.UsageMin + 1;

		usageLength = numberOfButtons;
		CHECK(
			HidP_GetUsages(
				HidP_Input, pButtonCaps->UsagePage, 0, usage, &usageLength, pPreparsedData,
				(PCHAR)pRawInput->data.hid.bRawData, pRawInput->data.hid.dwSizeHid
			) == HIDP_STATUS_SUCCESS );

		if(usageLength > 0)
		{
			pbtnMap[usage[0] - pButtonCaps->Range.UsageMin] = btnCapturing;
			btnCapturing = PAD_BUTTON_COUNT;
		}
	}
	else if(axisCapturing < PAD_AXIS_COUNT)
	{
		// Value caps
		CHECK( pValueCaps = (PHIDP_VALUE_CAPS)malloc(sizeof(HIDP_VALUE_CAPS) * Caps.NumberInputValueCaps) );
		capsLength = Caps.NumberInputValueCaps;
		CHECK( HidP_GetValueCaps(HidP_Input, pValueCaps, &capsLength, pPreparsedData) == HIDP_STATUS_SUCCESS )

		for(i = 0; i < Caps.NumberInputValueCaps && axisCapturing < PAD_AXIS_COUNT; i++)
		{
			CHECK(
				HidP_GetUsageValue(
					HidP_Input, pValueCaps[i].UsagePage, 0, pValueCaps[i].Range.UsageMin, &value, pPreparsedData,
					(PCHAR)pRawInput->data.hid.bRawData, pRawInput->data.hid.dwSizeHid
				) == HIDP_STATUS_SUCCESS );

			uint32_t logical = pValueCaps[i].LogicalMax - pValueCaps[i].LogicalMin;

			switch(pValueCaps[i].Range.UsageMin)
			{
			case 0x30:	// X-axis
				CHECKDIFF(0);
				axisDiff[0] = value;
				break;

			case 0x31:	// Y-axis
				CHECKDIFF(1);
				axisDiff[1] = value;
				break;

			case 0x32: // Z-axis
				CHECKDIFF(2);
				axisDiff[2] = value;
				break;

			case 0x33: // Rotate-X
				CHECKDIFF(3);
				axisDiff[3] = value;
				break;

			case 0x34: // Rotate-Y
				CHECKDIFF(4);
				axisDiff[4] = value;
				break;

			case 0x35: // Rotate-Z
				CHECKDIFF(5);
				axisDiff[5] = value;
				break;

			case 0x39:	// Hat Switch
				break;
			}
		}

		axisPass2 = true;
	}

Error:
	SAFE_FREE(pPreparsedData);
	SAFE_FREE(pButtonCaps);
	SAFE_FREE(pValueCaps);
}

void resetState(HWND hW)
{
	SendDlgItemMessageW(hW, IDC_STATIC_CAP, WM_SETTEXT, 0, (LPARAM)L"");
	btnCapturing = PAD_BUTTON_COUNT;
	axisCapturing = PAD_AXIS_COUNT;
	paxisMap = NULL;
	pbtnMap = NULL;
	ListView_DeleteAllItems(GetDlgItem(hW, IDC_LIST1));
	populateMappings(hW, 0);
	populateMappings(hW, 1);
}

BOOL CALLBACK ConfigureDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	BOOL ret;

	switch(uMsg) {
		case WM_INITDIALOG:
			SendDlgItemMessage(hW, IDC_BUILD_DATE, WM_SETTEXT, 0, (LPARAM)__DATE__ " " __TIME__);
			RAWINPUTDEVICE rid;
			rid.usUsagePage = 1;
			rid.usUsage     = 4;	// Joystick
			rid.dwFlags     = RIDEV_INPUTSINK;
			rid.hwndTarget  = hW;
			ret = RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE));

			LoadConfig();
			if (conf.Log) CheckDlgButton(hW, IDC_LOGGING, TRUE);
			populate(hW);
			ListView_DeleteAllItems(GetDlgItem(hW, IDC_LIST1));
			populateMappings(hW, 0);
			return TRUE;

		case WM_INPUT:
			if(axisCapturing == PAD_AXIS_COUNT &&
					btnCapturing == PAD_BUTTON_COUNT)
				return 0;

			PRAWINPUT pRawInput;
			UINT      bufferSize;
			GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &bufferSize, sizeof(RAWINPUTHEADER));
			pRawInput = (PRAWINPUT)malloc(bufferSize);
			if(!pRawInput)
				return 0;
			GetRawInputData((HRAWINPUT)lParam, RID_INPUT, pRawInput, &bufferSize, sizeof(RAWINPUTHEADER));
			ParseRawInput(pRawInput);
			free(pRawInput);

			if(axisCapturing == PAD_AXIS_COUNT && btnCapturing == PAD_BUTTON_COUNT){
				SendDlgItemMessageW(hW, IDC_STATIC_CAP, WM_SETTEXT, 0, (LPARAM)L"");
				ListView_DeleteAllItems(GetDlgItem(hW, IDC_LIST1));
				populateMappings(hW, 0);
				populateMappings(hW, 1);
			}
			return 0;

		case WM_KEYDOWN:
			if(LOWORD(lParam) == VK_ESCAPE)
			{
				btnCapturing = PAD_BUTTON_COUNT;
				axisCapturing = PAD_AXIS_COUNT;
				SendDlgItemMessageW(hW, IDC_STATIC_CAP, WM_SETTEXT, 0, (LPARAM)L"");
				return TRUE;
			}
			break;
		case WM_COMMAND:
			switch (HIWORD(wParam))
			{
			case LBN_SELCHANGE:
				switch (LOWORD(wParam))
				{
				case IDC_COMBO1:
					//tmpIter = &selectedJoy[0];
					selectedJoy[0] = SendDlgItemMessage(hW, IDC_COMBO1, CB_GETCURSEL, 0, 0);
					resetState(hW);
					if(selectedJoy[0] > 0 && selectedJoy[0] == selectedJoy[1])
					{
						//selectedJoy[0] = *tmpIter;
						selectedJoy[0] = 0;
						resetState(hW);
						//SendDlgItemMessage(hW, IDC_COMBO1, CB_SETCURSEL, std::distance(joys.begin(), *tmpIter), 0);
						SendDlgItemMessage(hW, IDC_COMBO1, CB_SETCURSEL, selectedJoy[0], 0);
						SendDlgItemMessageW(hW, IDC_STATIC_CAP, WM_SETTEXT, 0, (LPARAM)L"Both players can't have the same controller."); //Actually, whatever, but config logics are limited ;P
					}
					break;
				case IDC_COMBO2:
					//tmpIter = &selectedJoy[1];
					selectedJoy[1] = SendDlgItemMessage(hW, IDC_COMBO2, CB_GETCURSEL, 0, 0);
					resetState(hW);
					if(selectedJoy[1] > 0  && selectedJoy[0] == selectedJoy[1])
					{
						//selectedJoy[1] = *tmpIter;
						selectedJoy[1] = 0;
						resetState(hW);
						//SendDlgItemMessage(hW, IDC_COMBO2, CB_SETCURSEL, std::distance(joys.begin(), *tmpIter), 0);
						SendDlgItemMessage(hW, IDC_COMBO2, CB_SETCURSEL, selectedJoy[1], 0);
						SendDlgItemMessageW(hW, IDC_STATIC_CAP, WM_SETTEXT, 0, (LPARAM)L"Both players can't have the same controller."); //Actually, whatever, but config logics are limited ;P
					}
					break;
				default:
					return FALSE;
				}
				break;
			case BN_CLICKED:
				switch(LOWORD(wParam)) {
				case IDC_BUTTON1://cross
				case IDC_BUTTON2://square
				case IDC_BUTTON3://circle
				case IDC_BUTTON4://triangle
				case IDC_BUTTON5://L1
				case IDC_BUTTON6://L2
				case IDC_BUTTON7://R1
				case IDC_BUTTON8://R2
				case IDC_BUTTON9://select
				case IDC_BUTTON10://start
					btnCapturing = (PS2Buttons) (LOWORD(wParam) - IDC_BUTTON1);
					SendDlgItemMessageW(hW, IDC_STATIC_CAP, WM_SETTEXT, 0, (LPARAM)L"Capturing a button, press ESC to cancel");
					return TRUE;
				case IDC_BUTTON17://x
				case IDC_BUTTON18://y
				case IDC_BUTTON19://z
				case IDC_BUTTON20://rz
					axisCapturing = (PS2Axis) (LOWORD(wParam) - IDC_BUTTON17);
					SendDlgItemMessageW(hW, IDC_STATIC_CAP, WM_SETTEXT, 0, (LPARAM)L"Capturing an axis, press ESC to cancel");
					return TRUE;
				case IDCANCEL:
					if(btnCapturing < PAD_BUTTON_COUNT || axisCapturing < PAD_AXIS_COUNT)
						return FALSE;
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
					for(MappingsMap::iterator it = mappings.begin(); it != mappings.end(); it++)
					{
						SaveMappings(it->first.ply, it->first.hid.dwVendorId, it->first.hid.dwProductId, it->second.btnMap, it->second.axisMap);
					}
					EndDialog(hW, FALSE);
					return TRUE;
			}
			}
			
	}

	return DefWindowProc(hW, uMsg, wParam, lParam);
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
              NULL,//GetActiveWindow(),  
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

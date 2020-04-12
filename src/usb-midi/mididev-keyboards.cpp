// Used OBS as example

#include <assert.h>
#include <propsys.h>
#include <typeinfo>
#include <functiondiscoverykeys_devpkey.h>
#include <process.h>
#include <stdlib.h>
#include <windows.h>
#include "mididev-keyboards.h"
#include "../Win32/Config-win32.h"
#include "../Win32/resource.h"

#pragma comment(lib, "winmm.lib")

namespace mididev_keyboards {
//Config dlg temporaries
struct KeyboardsSettings
{
	int port;
	const char* dev_type;
	MidiDeviceInfoList sourceDevs;
	std::wstring selectedDev;
	int32_t midiOffset;
};

struct MidiInfo {
	std::queue<unsigned int> midiBuffer[2];
	int32_t midiOffset;
};

MidiInfo midiInfo;

static BOOL CALLBACK KeyboardsDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam);

void CALLBACK midiCallback(HMIDIIN hMidiIn, UINT wMsg, const DWORD_PTR dwInstance, const DWORD dwParam1, const DWORD dwParam2)
{
	switch (wMsg) {
		case MIM_DATA:
		{
			DWORD dwParam = dwParam1;

			//OSDebugOut(TEXT("cmd: %02x"), dwParam & 0xff);

			if (dwParam & 0x80) {
				const int note = dwParam >> 8 & 0x7f;
				dwParam = dwParam & ~(0x7f << 8);
				dwParam |= ((note + midiInfo.midiOffset) % 0x7f) << 8;
				midiInfo.midiBuffer[dwInstance].push(dwParam);
			}
		}
	}
}

KeyboardMidiDevice::~KeyboardMidiDevice()
{
}

static HMIDIIN hMidiDevice;
bool KeyboardMidiDevice::Init()
{
	if (!LoadSetting(mDevType, mPort, APINAME, N_AUDIO_SOURCE0, mDevID))
	{
		throw MidiDeviceError("KeyboardMidiDevice:: failed to load source from ini!");
	}

	if (!mDevID.length())
		return false;

	return true;
}

bool KeyboardMidiDevice::Reinitialize()
{
	return true;
}

void KeyboardMidiDevice::Start()
{
	hMidiDevice = nullptr;

	LoadSetting(mDevType, mPort, APINAME, KEY_OFFSET, midiInfo.midiOffset);

	int ret = midiInOpen(&hMidiDevice, _wtoi(mDevID.c_str()), reinterpret_cast<DWORD_PTR>(midiCallback), mPort, CALLBACK_FUNCTION);
	if (ret != MMSYSERR_NOERROR) {
		throw MidiDeviceError("KeyboardMidiDevice:: failed to open MIDI device!");
	}

	midiInStart(hMidiDevice);
}

void KeyboardMidiDevice::Stop()
{
	if (hMidiDevice) {
		midiInStop(hMidiDevice);
		midiInClose(hMidiDevice);
	}

	hMidiDevice = nullptr;
}


bool KeyboardMidiDevice::AudioInit()
{
	return true;
}

uint32_t KeyboardMidiDevice::PopMidiCommand() {
	if (midiInfo.midiBuffer[mPort].size() <= 0) {
		return 0xffffffff;
	}

	uint32_t val = midiInfo.midiBuffer[mPort].front();
	midiInfo.midiBuffer[mPort].pop();
	return val;
}

void KeyboardMidiDevice::MidiDevices(std::vector<MidiDeviceInfo> &devices)
{
	const int nMidiDeviceNum = midiInGetNumDevs();

	OSDebugOut(TEXT("Found %d MIDI devices\n"), nMidiDeviceNum);

	for (int i = 0; i < nMidiDeviceNum; i++) {
        MIDIINCAPS caps;
		midiInGetDevCaps(i, &caps, sizeof(MIDIINCAPS));

        OSDebugOut(TEXT("Connecting MIDI device %s\n"), caps.szPname);

		MidiDeviceInfo info;

		wchar_t wstrID[4096] = {0};
		wsprintf(wstrID, L"%d", i);

		wchar_t wszPname[4096] = {0};
		wcscpy_s(wszPname, countof(wszPname), caps.szPname);

		info.strID = wstrID;
		info.strName = caps.szPname;
		devices.push_back(info);
	}
}

int KeyboardMidiDevice::Configure(int port, const char* dev_type, void *data)
{
	Win32Handles h = *(Win32Handles*)data;
	KeyboardsSettings settings;

	settings.port = port == 0;
	settings.dev_type = dev_type;

	return DialogBoxParam(
		h.hInst,
		MAKEINTRESOURCE(IDD_DLGMIDIKBD),
		h.hWnd,
		(DLGPROC)KeyboardsDlgProc,
		(LPARAM)&settings
	);
}

static void RefreshInputKeyboardList(HWND hW, LRESULT idx, KeyboardsSettings *settings)
{
	settings->sourceDevs.clear();

	SendDlgItemMessage(hW, IDC_COMBO1, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessageW(hW, IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)L"None");
	SendDlgItemMessage(hW, IDC_COMBO1, CB_SETCURSEL, 0, 0);

	KeyboardMidiDevice::MidiDevices(settings->sourceDevs);
	MidiDeviceInfoList::iterator it;
	int i = 0;
	for (it = settings->sourceDevs.begin(); it != settings->sourceDevs.end(); ++it)
	{
		SendDlgItemMessageW(hW, IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)it->strName.c_str());

		i++;
		if (it->strID == settings->selectedDev) {
			SendDlgItemMessage(hW, IDC_COMBO1, CB_SETCURSEL, i, i);
		}
	}
}

static BOOL CALLBACK KeyboardsDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	int tmp = 0;
	KeyboardsSettings *s;

	switch (uMsg) {
	case WM_CREATE:
		SetWindowLongPtr(hW, GWLP_USERDATA, (LONG)lParam);
		break;
	case WM_INITDIALOG:
	{
		s = (KeyboardsSettings *)lParam;
		SetWindowLongPtr(hW, GWLP_USERDATA, (LONG)lParam);

		s->midiOffset = 0;
		LoadSetting(s->dev_type, s->port, APINAME, KEY_OFFSET, s->midiOffset);
		SetDlgItemInt(hW, IDC_BUFFER1, s->midiOffset, FALSE);

		LoadSetting(s->dev_type, s->port, APINAME, N_AUDIO_SOURCE0, s->selectedDev);

		RefreshInputKeyboardList(hW, -1, s);
		return TRUE;
	}
	case WM_COMMAND:
		switch (HIWORD(wParam))
		{
		case EN_CHANGE:
		{
			switch (LOWORD(wParam))
			{
			case IDC_BUFFER1:
				CHECKED_SET_MAX_INT(tmp, hW, IDC_BUFFER1, FALSE, -127, 127);
				break;
			}
		}
		break;
		case BN_CLICKED:
		{
			switch (LOWORD(wParam)) {
			case IDOK:
			{
				int p;
				s = (KeyboardsSettings *)GetWindowLongPtr(hW, GWLP_USERDATA);
				INT_PTR res = RESULT_OK;

				p = SendDlgItemMessage(hW, IDC_COMBO1, CB_GETCURSEL, 0, 0);
				s->selectedDev.clear();

				if (p > 0) {
					s->selectedDev = (s->sourceDevs.begin() + p - 1)->strID;
				}

				if (!SaveSetting(s->dev_type, s->port, APINAME, N_AUDIO_SOURCE0, s->selectedDev))
					res = RESULT_FAILED;

				static wchar_t buff[4096] = { 0 };
				GetWindowTextW(GetDlgItem(hW, IDC_BUFFER1), buff, countof(buff));
				if (!SaveSetting(s->dev_type, s->port, APINAME, KEY_OFFSET, _wtoi(buff)))
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

REGISTER_MIDIDEV(APINAME, KeyboardMidiDevice);
} // namespace
#include "rawinput.h"
#include <cstdio>
#include <vector>
#include <algorithm>
#include "platcompat.h"
#include "osdebugout.h"

extern HINSTANCE hInst;

namespace shared{ namespace rawinput{

static std::vector<ParseRawInputCB *> callbacks;

HWND msgWindow = nullptr;
WNDPROC eatenWndProc = nullptr;
HWND eatenWnd = nullptr;
HHOOK hHook = nullptr, hHookWnd = nullptr, hHookKB = nullptr;
bool skipInput = false;

void RegisterCallback(ParseRawInputCB *cb)
{
	if (cb && std::find(callbacks.begin(), callbacks.end(), cb) == callbacks.end())
		callbacks.push_back(cb);
}

void UnregisterCallback(ParseRawInputCB *cb)
{
	auto it = std::find(callbacks.begin(), callbacks.end(), cb);
	if (it != callbacks.end())
		callbacks.erase(it);
}

static int RegisterRaw(HWND hWnd)
{
	msgWindow = hWnd;
	RAWINPUTDEVICE Rid[4];
	Rid[0].usUsagePage = 0x01; 
	Rid[0].usUsage = HID_USAGE_GENERIC_GAMEPAD; 
	Rid[0].dwFlags = hWnd ? RIDEV_INPUTSINK : RIDEV_REMOVE; // adds game pad
	Rid[0].hwndTarget = hWnd;

	Rid[1].usUsagePage = 0x01; 
	Rid[1].usUsage = HID_USAGE_GENERIC_JOYSTICK; 
	Rid[1].dwFlags = hWnd ? RIDEV_INPUTSINK : RIDEV_REMOVE; // adds joystick
	Rid[1].hwndTarget = hWnd;

	Rid[2].usUsagePage = 0x01; 
	Rid[2].usUsage = HID_USAGE_GENERIC_KEYBOARD; 
	Rid[2].dwFlags = hWnd ? RIDEV_INPUTSINK : RIDEV_REMOVE;// | RIDEV_NOLEGACY;   // adds HID keyboard //and also !ignores legacy keyboard messages
	Rid[2].hwndTarget = hWnd;

	Rid[3].usUsagePage = 0x01;
	Rid[3].usUsage = HID_USAGE_GENERIC_MOUSE;
	Rid[3].dwFlags = hWnd ? RIDEV_INPUTSINK : RIDEV_REMOVE;
	Rid[3].hwndTarget = hWnd;

	if (RegisterRawInputDevices(Rid, countof(Rid), sizeof(Rid[0])) == FALSE) {
		//registration failed. Call GetLastError for the cause of the error.
		fprintf(stderr, "Could not (de)register raw input devices.\n");
		return 0;
	}
	return 1;
}

static LRESULT CALLBACK MyWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_ACTIVATE:
		OSDebugOut(TEXT("******      WM_ACTIVATE        ****** %p %d\n"), hWnd, LOWORD(wParam) != WA_INACTIVE);
		break;
	case WM_SETFOCUS:
		OSDebugOut(TEXT("******      WM_SETFOCUS        ****** %p\n"), hWnd);
		break;
	case WM_KILLFOCUS:
		OSDebugOut(TEXT("******      WM_KILLFOCUS        ****** %p\n"), hWnd);
		break;
	}
	return 0;
}

static LRESULT CALLBACK RawInputProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	MyWndProc(hWnd, uMsg, wParam, lParam);

	switch(uMsg) {
	case WM_CREATE:
		if (eatenWnd == nullptr)
			RegisterRaw(hWnd);
		break;
	case WM_INPUT:
		{
		//if(skipInput) return;
		PRAWINPUT pRawInput;
		UINT      bufferSize=0;
		GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &bufferSize, sizeof(RAWINPUTHEADER));
		pRawInput = (PRAWINPUT)malloc(bufferSize);
		if(!pRawInput)
			break;
		if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, pRawInput, &bufferSize, sizeof(RAWINPUTHEADER)) > 0) {
			for (auto cb : callbacks)
				cb->ParseRawInput(pRawInput);
		}
		free(pRawInput);
		break;
		}
	case WM_ACTIVATE:
		OSDebugOut(TEXT("******      WM_ACTIVATE        ******\n"));
		break;
	case WM_DESTROY:
		if (eatenWnd == nullptr)
			RegisterRaw(nullptr);
		Uninitialize();
		break;
	}

	if(eatenWndProc)
		return CallWindowProc(eatenWndProc, hWnd, uMsg, wParam, lParam);
	//else
	//	return DefWindowProc(hWnd, uMsg, wParam, lParam);
	return 0;
}

static LRESULT CALLBACK HookProc(INT code, WPARAM wParam, LPARAM lParam)
{
	MSG *msg = reinterpret_cast<MSG*> (lParam);

	//fprintf(stderr, "hook: %d, %d, %d\n", code, wParam, lParam);
	if(code == HC_ACTION)
		RawInputProc(msg->hwnd, msg->message, msg->wParam, msg->lParam);
	return CallNextHookEx(hHook, code, wParam, lParam);
}

static LRESULT CALLBACK HookWndProc(INT code, WPARAM wParam, LPARAM lParam)
{
	MSG *msg = reinterpret_cast<MSG*> (lParam);

	//fprintf(stderr, "hook: %d, %d, %d\n", code, wParam, lParam);
	if (code == HC_ACTION)
		MyWndProc(msg->hwnd, msg->message, msg->wParam, msg->lParam);
	return CallNextHookEx(hHookWnd, code, wParam, lParam);
}

static LRESULT CALLBACK KBHookProc(INT code, WPARAM wParam, LPARAM lParam)
{
	fprintf(stderr, "kb hook: %d, %u, %d\n", code, wParam, lParam);
	KBDLLHOOKSTRUCT *kb = reinterpret_cast<KBDLLHOOKSTRUCT*> (lParam);
	//if(code == HC_ACTION)
	//	RawInputProc(msg->hwnd, msg->message, msg->wParam, msg->lParam);
	return CallNextHookEx(0, code, wParam, lParam);
}

int Initialize(void *ptr)
{
	HWND hWnd = reinterpret_cast<HWND> (ptr);
	if (!InitHid())
		return 0;

#if 0
	if (!RegisterRaw(hWnd))
		return 0;
	hHook = SetWindowsHookEx(WH_GETMESSAGE, HookProc, hInst, 0);
	//hHookWnd = SetWindowsHookEx(WH_CALLWNDPROC, HookWndProc, hInst, 0);
	//hHookKB = SetWindowsHookEx(WH_KEYBOARD_LL, KBHookProc, hInst, 0);
	int err = GetLastError();
#else
	eatenWnd = hWnd;
	eatenWndProc = (WNDPROC) SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)RawInputProc);
	RegisterRaw(hWnd);
#endif
	return 1;
}

void Uninitialize()
{
	if(hHook)
	{
		UnhookWindowsHookEx(hHook);
		//UnhookWindowsHookEx(hHookKB);
		hHook = 0;
	}
	if(eatenWnd)
		RegisterRaw(nullptr);
	if(eatenWnd && eatenWndProc)
		SetWindowLongPtr(eatenWnd, GWLP_WNDPROC, (LONG_PTR)eatenWndProc);
	eatenWndProc = nullptr;
	eatenWnd = nullptr;
}

}} //namespace
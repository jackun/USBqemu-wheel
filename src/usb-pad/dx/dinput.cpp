#include <math.h>
#include "dx.h"
#include <VersionHelpers.h>
#include <Xinput.h>
#if 0
#include <wbemidl.h>
#include <oleauto.h>
//#include <wmsstd.h>
#endif

#define SAFE_DELETE(p)  { if(p) { delete (p);     (p)=NULL; } }
#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=NULL; } }

//dialog window stuff
extern HWND gsWnd;

namespace usb_pad { namespace dx {

static std::atomic<int> refCount (0);
static bool useRamp = false;

HWND hWin = NULL;
DWORD pid = 0;
DWORD old = 0;

static LPDIRECTINPUT8       g_pDI       = NULL;

std::vector<JoystickDevice *> g_pJoysticks;
std::map<int, InputMapped> g_Controls[2];

static DWORD rgdwAxes[1] = { DIJOFS_X }; //FIXME if steering uses two axes, then this needs DIJOFS_Y too?
static LONG  rglDirection[1] = { 0 };

//only two effect (constant force, spring)
static bool FFB[2] = { false, false };
static LPDIRECTINPUTEFFECT  g_pEffectConstant[2] = { 0, 0 };
static LPDIRECTINPUTEFFECT  g_pEffectSpring[2] = { 0, 0 };
static LPDIRECTINPUTEFFECT  g_pEffectFriction[2] = { 0, 0 }; //DFP mode only
static LPDIRECTINPUTEFFECT  g_pEffectRamp[2] = { 0, 0 };
static LPDIRECTINPUTEFFECT  g_pEffectDamper[2] = { 0, 0 };
static DWORD g_dwNumForceFeedbackAxis[4] = {0};
static DIEFFECT eff;
static DIEFFECT effSpring;
static DIEFFECT effFriction;
static DIEFFECT effRamp;
static DIEFFECT effDamper;
static DICONSTANTFORCE cfw;
static DICONDITION cSpring;
static DICONDITION cFriction;
static DIRAMPFORCE cRamp;
static DICONDITION cDamper;

std::ostream& operator<<(std::ostream& os, REFGUID guid) {
	std::ios_base::fmtflags f(os.flags());
	os << std::uppercase;
	os.width(8);
	os << std::hex << guid.Data1 << '-';

	os.width(4);
	os << std::hex << guid.Data2 << '-';

	os.width(4);
	os << std::hex << guid.Data3 << '-';

	os.width(2);
	os << std::hex
		<< static_cast<short>(guid.Data4[0])
		<< static_cast<short>(guid.Data4[1])
		<< '-'
		<< static_cast<short>(guid.Data4[2])
		<< static_cast<short>(guid.Data4[3])
		<< static_cast<short>(guid.Data4[4])
		<< static_cast<short>(guid.Data4[5])
		<< static_cast<short>(guid.Data4[6])
		<< static_cast<short>(guid.Data4[7]);
	os.flags(f);
	return os;
}

// Extra enum
#define XINPUT_GAMEPAD_GUIDE 0x0400

typedef struct
{
	float SCP_UP;
	float SCP_RIGHT;
	float SCP_DOWN;
	float SCP_LEFT;

	float SCP_LX;
	float SCP_LY;

	float SCP_L1;
	float SCP_L2;
	float SCP_L3;

	float SCP_RX;
	float SCP_RY;

	float SCP_R1;
	float SCP_R2;
	float SCP_R3;

	float SCP_T;
	float SCP_C;
	float SCP_X;
	float SCP_S;

	float SCP_SELECT;
	float SCP_START;

	float SCP_PS;

} SCP_EXTN;


// This way, I don't require that XInput junk be installed.
typedef void(CALLBACK* _XInputEnable)(BOOL enable);
typedef DWORD(CALLBACK* _XInputGetStateEx)(DWORD dwUserIndex, XINPUT_STATE* pState);
typedef DWORD(CALLBACK* _XInputGetExtended)(DWORD dwUserIndex, SCP_EXTN* pPressure);
typedef DWORD(CALLBACK* _XInputSetState)(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration);

_XInputEnable pXInputEnable = 0;
_XInputGetStateEx pXInputGetStateEx = 0;
_XInputGetExtended pXInputGetExtended = 0;
_XInputSetState pXInputSetState = 0;
static bool xinputNotInstalled = false;

static uint32_t xInputActiveCount = 0;

static void LoadXInput()
{
	wchar_t temp[30];
	if (!pXInputSetState) {
		// XInput not installed, so don't repeatedly try to load it.
		if (xinputNotInstalled)
			return;

		// Prefer XInput 1.3 since SCP only has an XInput 1.3 wrapper right now.
		// Also use LoadLibrary and not LoadLibraryEx for XInput 1.3, since some
		// Windows 7 systems have issues with it.
		// FIXME: Missing FreeLibrary call.
		HMODULE hMod = LoadLibrary(L"xinput1_3.dll");
		if (hMod == nullptr && IsWindows8OrGreater()) {
			hMod = LoadLibraryEx(L"XInput1_4.dll", nullptr, LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
		}

		if (hMod) {
			if ((pXInputEnable = (_XInputEnable)GetProcAddress(hMod, "XInputEnable")) &&
				((pXInputGetStateEx = (_XInputGetStateEx)GetProcAddress(hMod, (LPCSTR)100)) || // Try Ex version first
					(pXInputGetStateEx = (_XInputGetStateEx)GetProcAddress(hMod, "XInputGetState")))) {
				pXInputGetExtended = (_XInputGetExtended)GetProcAddress(hMod, "XInputGetExtended");
				pXInputSetState = (_XInputSetState)GetProcAddress(hMod, "XInputSetState");
			}
		}
		if (!pXInputSetState) {
			xinputNotInstalled = true;
			return;
		}
	}
	pXInputEnable(1);
}

LONG GetAxisValueFromOffset(int axis, const DIJOYSTATE2& j)
{
	#define LVX_OFFSET 8 // count POVs or not?
	switch(axis)
	{
	case 0: return j.lX; break;
	case 1: return j.lY; break;
	case 2: return j.lZ; break;
	case 3: return j.lRx; break;
	case 4: return j.lRy; break;
	case 5: return j.lRz; break;
	case 6: return j.rglSlider[0]; break;
	case 7: return j.rglSlider[1]; break;
	//case 8: return j.rgdwPOV[0]; break;
	//case 9: return j.rgdwPOV[1]; break;
	//case 10: return j.rgdwPOV[2]; break;
	//case 11: return j.rgdwPOV[3]; break;
	case LVX_OFFSET + 0: return j.lVX; break;		/* 'v' as in velocity */
	case LVX_OFFSET + 1: return j.lVY; break;
	case LVX_OFFSET + 2: return j.lVZ; break;
	case LVX_OFFSET + 3: return j.lVRx; break;
	case LVX_OFFSET + 4: return j.lVRy; break;
	case LVX_OFFSET + 5: return j.lVRz; break;
	case LVX_OFFSET + 6: return j.rglVSlider[0]; break;
	case LVX_OFFSET + 7: return j.rglVSlider[1]; break;
	case LVX_OFFSET + 8: return j.lAX; break;		/* 'a' as in acceleration */
	case LVX_OFFSET + 9: return j.lAY; break;
	case LVX_OFFSET + 10: return j.lAZ; break;
	case LVX_OFFSET + 11: return j.lARx; break;
	case LVX_OFFSET + 12: return j.lARy; break;
	case LVX_OFFSET + 13: return j.lARz; break;
	case LVX_OFFSET + 14: return j.rglASlider[0]; break;
	case LVX_OFFSET + 15: return j.rglASlider[1]; break;
	case LVX_OFFSET + 16: return j.lFX; break;		/* 'f' as in force */
	case LVX_OFFSET + 17: return j.lFY; break;
	case LVX_OFFSET + 18: return j.lFZ; break;
	case LVX_OFFSET + 19: return j.lFRx; break;		/* 'fr' as in rotational force aka torque */
	case LVX_OFFSET + 20: return j.lFRy; break;
	case LVX_OFFSET + 21: return j.lFRz; break;
	case LVX_OFFSET + 22: return j.rglFSlider[0]; break;
	case LVX_OFFSET + 23: return j.rglFSlider[1]; break;
	}
	#undef LVX_OFFSET
	return 0;
}

bool DInputDevice::Poll()
{
	HRESULT hr = 0;
	if (m_device)
	{
		hr = m_device->Poll();
		if (FAILED(hr))
		{
			OSDebugOut(TEXT("poll failed: %08x\n"), hr);
			hr = m_device->Acquire();
			OSDebugOut(TEXT("acquire failed: %08x\n"), hr);
			//return SUCCEEDED(hr);
		}
		else
		{
			if (m_type == CT_JOYSTICK) {
				m_device->GetDeviceState(sizeof(DIJOYSTATE2), &m_controls);
			}
			else if (m_type == CT_MOUSE) {
				m_device->GetDeviceState(sizeof(DIMOUSESTATE2), &m_controls);
			}
			else if (m_type == CT_KEYBOARD) {
				m_device->GetDeviceState(sizeof(m_controls.kbd), &m_controls);
			}
			else if (m_type == CT_XINPUT) {
				hr = m_device->GetDeviceState(sizeof(DIJOYSTATE2), &m_controls);
				OSDebugOut("%S: xinput GetDeviceState %08x\n", __func__, hr);
				if (pXInputGetStateEx && m_xinput < XUSER_MAX_COUNT) {
					XINPUT_STATE xstate{};
					if (pXInputGetStateEx(m_xinput, &xstate) == ERROR_SUCCESS) {
						m_controls.js2.lZ = 0;
						m_controls.js2.rglSlider[0] = xstate.Gamepad.bLeftTrigger * USHRT_MAX / UCHAR_MAX;
						m_controls.js2.rglSlider[1] = xstate.Gamepad.bRightTrigger * USHRT_MAX / UCHAR_MAX;
						OSDebugOut(L"%S: %d %d %d %d\n", __func__,
							m_controls.js2.lX, m_controls.js2.lY,
							m_controls.js2.rglSlider[0], m_controls.js2.rglSlider[1]);
					}
				}
			}
			return true;
		}
	}
	return false;
}

bool DInputDevice::GetButton(int b)
{
	if (m_type == CT_KEYBOARD) {
		return (b < ARRAY_SIZE(m_controls.kbd) && m_controls.kbd[b] & 0x80);
	} else if (m_type == CT_MOUSE) {
		return (b < ARRAY_SIZE(DIMOUSESTATE2::rgbButtons) && m_controls.ms2.rgbButtons[b] & 0x80);
	}
	else
	{
		if (b < ARRAY_SIZE(DIJOYSTATE2::rgbButtons) && m_controls.js2.rgbButtons[b] & 0x80)
			return true;

		if (b >= ARRAY_SIZE(DIJOYSTATE2::rgbButtons))
		{
			b -= ARRAY_SIZE(DIJOYSTATE2::rgbButtons);
			int i = b / 4;
			//hat switch cases (4 hat switches with 4 directions possible)  surely a better way... but this would allow funky joysticks to work.
			switch (b % 4)
			{
			case 0:
				if ((m_controls.js2.rgdwPOV[i] <= 4500 || m_controls.js2.rgdwPOV[i] >= 31500) && m_controls.js2.rgdwPOV[i] != -1) {
					return true;
				}
				break;
			case 1:
				if (m_controls.js2.rgdwPOV[i] >= 4500 && m_controls.js2.rgdwPOV[i] <= 13500) {
					return true;
				}
				break;
			case 2:
				if (m_controls.js2.rgdwPOV[i] >= 13500 && m_controls.js2.rgdwPOV[i] <= 22500) {
					return true;
				}
				break;
			case 3:
				if (m_controls.js2.rgdwPOV[i] >= 22500 && m_controls.js2.rgdwPOV[i] <= 31500) {
					return true;
				}
				break;
			}
		}
	}

	return false;
}

LONG DInputDevice::GetAxis(int a)
{
	return GetAxisValueFromOffset(a, m_controls.js2);
}

DInputDevice::~DInputDevice()
{
	if (m_device) {
		m_device->Unacquire();
		m_device->Release();
	}
}

void ReleaseFFB(int port)
{
	if (g_pEffectConstant[port])
		g_pEffectConstant[port]->Stop();
	if (g_pEffectSpring[port])
		g_pEffectSpring[port]->Stop();
	if (g_pEffectFriction[port])
		g_pEffectFriction[port]->Stop();
	if (g_pEffectRamp[port])
		g_pEffectRamp[port]->Stop();
	if (g_pEffectDamper[port])
		g_pEffectDamper[port]->Stop();

	SAFE_RELEASE(g_pEffectConstant[port]);
	SAFE_RELEASE(g_pEffectSpring[port]);
	SAFE_RELEASE(g_pEffectFriction[port]);
	SAFE_RELEASE(g_pEffectRamp[port]);
	SAFE_RELEASE(g_pEffectDamper[port]);

	FFB[port] = false;
}

void AddInputMap(int port, int cid, const InputMapped& im)
{
	g_Controls[port][cid] = im;
}

void RemoveInputMap(int port, int cid)
{
	g_Controls[port].erase(cid); //FIXME ini doesn't clear old entries duh
	// override with MT_NONE instead
	//g_Controls[port][cid].type = MT_NONE;
}

bool GetInputMap(int port, int cid, InputMapped& im)
{
	auto it = g_Controls[port].find(cid);
	if (it != g_Controls[port].end()) {
		im = it->second;
		return true;
	}
	return false;
}

void CreateFFB(int port, LPDIRECTINPUTDEVICE8 device, DWORD axis)
{
	HRESULT hres = 0;
	ReleaseFFB(port);

	if (!device)
		return;

	try {
		rgdwAxes[0] = axis;
		//LPDIRECTINPUTDEVICE8 device = joy->GetDevice();
		//create the constant force effect
		ZeroMemory(&eff, sizeof(eff));
		ZeroMemory(&effSpring, sizeof(effSpring));
		ZeroMemory(&effFriction, sizeof(effFriction));
		ZeroMemory(&cfw, sizeof(cfw));
		ZeroMemory(&cSpring, sizeof(cSpring));
		ZeroMemory(&cFriction, sizeof(cFriction));
		ZeroMemory(&cRamp, sizeof(cRamp));

		//constantforce
		eff.dwSize = sizeof(DIEFFECT);
		eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
		eff.dwSamplePeriod = 0;
		eff.dwGain = MIN(MAX(GAINZ[port][0], 0), 10000);
		eff.dwTriggerButton = DIEB_NOTRIGGER;
		eff.dwTriggerRepeatInterval = 0;
		eff.cAxes = countof(rgdwAxes);
		eff.rgdwAxes = rgdwAxes; //TODO set actual "steering" axis though usually is DIJOFS_X
		eff.rglDirection = rglDirection;
		eff.dwStartDelay = 0;
		eff.dwDuration = INFINITE;

		// copy default values
		effSpring = eff;
		effFriction = eff;
		effRamp = eff;
		effDamper = eff;

		cfw.lMagnitude = 0;

		eff.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
		eff.lpvTypeSpecificParams = &cfw;
		hres = device->CreateEffect(GUID_ConstantForce, &eff, &g_pEffectConstant[port], NULL);

		cSpring.lNegativeCoefficient = 0;
		cSpring.lPositiveCoefficient = 0;

		effSpring.cbTypeSpecificParams = sizeof(DICONDITION);
		effSpring.lpvTypeSpecificParams = &cSpring;
		hres = device->CreateEffect(GUID_Spring, &effSpring, &g_pEffectSpring[port], NULL);

		effFriction.cbTypeSpecificParams = sizeof(DICONDITION);
		effFriction.lpvTypeSpecificParams = &cFriction;
		hres = device->CreateEffect(GUID_Friction, &effFriction, &g_pEffectFriction[port], NULL);

		effRamp.cbTypeSpecificParams = sizeof(DIRAMPFORCE);
		effRamp.lpvTypeSpecificParams = &cRamp;
		hres = device->CreateEffect(GUID_RampForce, &effRamp, &g_pEffectRamp[port], NULL);

		effDamper.cbTypeSpecificParams = sizeof(DICONDITION);
		effDamper.lpvTypeSpecificParams = &cDamper;
		hres = device->CreateEffect(GUID_Damper, &effDamper, &g_pEffectDamper[port], NULL);

		FFB[port] = true;
	}
	catch (...) {};

	//start the effect
	if (g_pEffectConstant[port])
	{
		OSDebugOut(TEXT("DINPUT: Start Effect\n"));
		g_pEffectConstant[port]->SetParameters(&eff, DIEP_START | DIEP_GAIN | DIEP_AXES | DIEP_DIRECTION);
	}
}

#if 0
//-----------------------------------------------------------------------------
// Enum each PNP device using WMI and check each device ID to see if it contains 
// "IG_" (ex. "VID_045E&PID_028E&IG_00").  If it does, then it's an XInput device
// Unfortunately this information can not be found by just using DirectInput 
//-----------------------------------------------------------------------------
BOOL IsXInputDevice_correct(const GUID* pGuidProductFromDirectInput)
{
	IWbemLocator* pIWbemLocator = NULL;
	IEnumWbemClassObject* pEnumDevices = NULL;
	IWbemClassObject* pDevices[20] = { 0 };
	IWbemServices* pIWbemServices = NULL;
	BSTR                    bstrNamespace = NULL;
	BSTR                    bstrDeviceID = NULL;
	BSTR                    bstrClassName = NULL;
	DWORD                   uReturned = 0;
	bool                    bIsXinputDevice = false;
	UINT                    iDevice = 0;
	VARIANT                 var;
	HRESULT                 hr;

	// CoInit if needed
	hr = CoInitialize(NULL);
	bool bCleanupCOM = SUCCEEDED(hr);

	// So we can call VariantClear() later, even if we never had a successful IWbemClassObject::Get().
	VariantInit(&var);

	// Create WMI
	hr = CoCreateInstance(__uuidof(WbemLocator),
		NULL,
		CLSCTX_INPROC_SERVER,
		__uuidof(IWbemLocator),
		(LPVOID*)&pIWbemLocator);
	if (FAILED(hr) || pIWbemLocator == NULL)
		goto LCleanup;

	bstrNamespace = SysAllocString(L"\\\\.\\root\\cimv2"); if (bstrNamespace == NULL) goto LCleanup;
	bstrClassName = SysAllocString(L"Win32_PNPEntity");   if (bstrClassName == NULL) goto LCleanup;
	bstrDeviceID = SysAllocString(L"DeviceID");          if (bstrDeviceID == NULL)  goto LCleanup;

	// Connect to WMI 
	hr = pIWbemLocator->ConnectServer(bstrNamespace, NULL, NULL, 0L,
		0L, NULL, NULL, &pIWbemServices);
	if (FAILED(hr) || pIWbemServices == NULL)
		goto LCleanup;

	// Switch security level to IMPERSONATE. 
	CoSetProxyBlanket(pIWbemServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
		RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);

	hr = pIWbemServices->CreateInstanceEnum(bstrClassName, 0, NULL, &pEnumDevices);
	if (FAILED(hr) || pEnumDevices == NULL)
		goto LCleanup;

	// Loop over all devices
	for (;; )
	{
		// Get 20 at a time
		hr = pEnumDevices->Next(10000, 20, pDevices, &uReturned);
		if (FAILED(hr))
			goto LCleanup;
		if (uReturned == 0)
			break;

		for (iDevice = 0; iDevice < uReturned; iDevice++)
		{
			// For each device, get its device ID
			hr = pDevices[iDevice]->Get(bstrDeviceID, 0L, &var, NULL, NULL);
			if (SUCCEEDED(hr) && var.vt == VT_BSTR && var.bstrVal != NULL)
			{
				// Check if the device ID contains "IG_".  If it does, then it's an XInput device
				// This information can not be found from DirectInput 
				if (wcsstr(var.bstrVal, L"IG_"))
				{
					// If it does, then get the VID/PID from var.bstrVal
					DWORD dwPid = 0, dwVid = 0;
					WCHAR* strVid = wcsstr(var.bstrVal, L"VID_");
					if (strVid && swscanf(strVid, L"VID_%4X", &dwVid) != 1)
						dwVid = 0;
					WCHAR* strPid = wcsstr(var.bstrVal, L"PID_");
					if (strPid && swscanf(strPid, L"PID_%4X", &dwPid) != 1)
						dwPid = 0;

					// Compare the VID/PID to the DInput device
					DWORD dwVidPid = MAKELONG(dwVid, dwPid);
					if (dwVidPid == pGuidProductFromDirectInput->Data1)
					{
						bIsXinputDevice = true;
						goto LCleanup;
					}
				}
			}
			VariantClear(&var);
			SAFE_RELEASE(pDevices[iDevice]);
		}
	}

LCleanup:
	VariantClear(&var);
	if (bstrNamespace)
		SysFreeString(bstrNamespace);
	if (bstrDeviceID)
		SysFreeString(bstrDeviceID);
	if (bstrClassName)
		SysFreeString(bstrClassName);
	for (iDevice = 0; iDevice < 20; iDevice++)
		SAFE_RELEASE(pDevices[iDevice]);
	SAFE_RELEASE(pEnumDevices);
	SAFE_RELEASE(pIWbemLocator);
	SAFE_RELEASE(pIWbemServices);

	if (bCleanupCOM)
		CoUninitialize();

	return bIsXinputDevice;
}
#endif

bool IsXInputDevice(const GUID* pguid)
{
	if ((uint64_t)pguid->Data4 == 0x4449564449500000)// "\0\0PIDVID"
		return false;

	// https://support.steampowered.com/kb/5199-TOKV-4426/supported-controller-database?l=english
	static const std::vector<unsigned long> ids{
		0x18d40079, // GPD Win 2 X-Box Controller Xbox360
		0xb326044f, // Thrustmaster Gamepad GP XID Xbox360
		0x028e045e, // Microsoft X-Box 360 pad Xbox360
		0x028f045e, // Microsoft X-Box 360 pad v2 Xbox360
		0x0291045e, // Xbox 360 Wireless Receiver (XBOX) Xbox360
		0x02a0045e, // Microsoft X-Box 360 Big Button IR Xbox360
		0x02a1045e, // Microsoft X-Box 360 pad Xbox360
		0x02dd045e, // Microsoft X-Box One pad XboxOne
		0xb326044f, // Microsoft X-Box One pad (Firmware 2015) XboxOne
		0x02e0045e, // Microsoft X-Box One S pad (Bluetooth) XboxOne
		0x02e3045e, // Microsoft X-Box One Elite pad XboxOne
		0x02ea045e, // Microsoft X-Box One S pad XboxOne
		0x02fd045e, // Microsoft X-Box One S pad (Bluetooth) XboxOne
		0x02ff045e, // Microsoft X-Box One Elite pad XboxOne
		0x0719045e, // Xbox 360 Wireless Receiver Xbox360
		0xc21d046d, // Logitech Gamepad F310 Xbox360
		0xc21e046d, // Logitech Gamepad F510 Xbox360
		0xc21f046d, // Logitech Gamepad F710 Xbox360
		0xc242046d, // Logitech Chillstream Controller Xbox360
		0x00c10f0d, // HORI Pad Switch Switch
		0x00920f0d, // HORI Pokken Tournament DX Pro Pad Switch
		0x00f60f0d, // HORI Wireless Switch Pad Switch
		0x00dc0f0d, // HORI Battle Pad Switch
		0xa71120d6, // PowerA Wired Controller Plus/PowerA Wired Gamcube Controller Switch
		0x01850e6f, // PDP Wired Fight Pad Pro for Nintendo Switch Switch
		0x01800e6f, // PDP Faceoff Wired Pro Controller for Nintendo Switch Switch
		0x01810e6f, // PDP Faceoff Deluxe Wired Pro Controller for Nintendo Switch Switch
		0x0268054c, // Sony PS3 Controller PS3
		0x00050925, // Sony PS3 Controller PS3
		0x03088888, // Sony PS3 Controller PS3
		0x08361a34, // Afterglow PS3 PS3
		0x006e0f0d, // HORI horipad4 PS3 PS3
		0x00660f0d, // HORI horipad4 PS4 PS3
		0x005f0f0d, // HORI Fighting commander PS3 PS3
		0x005e0f0d, // HORI Fighting commander PS4 PS3
		0x82500738, // Madcats Fightpad Pro PS4 PS3
		0x181a0079, // Venom Arcade Stick PS3
		0x00060079, // PC Twin Shock Controller PS3
		0x05232563, // Digiflip GP006 PS3
		0x333111ff, // SRXJ-PH2400 PS3
		0x550020bc, // ShanWan PS3 PS3
		0xb315044f, // Firestorm Dual Analog 3 PS3
		0x004d0f0d, // Horipad 3 PS3
		0x00090f0d, // HORI BDA GP1 PS3
		0x00080e8f, // Green Asia PS3
		0x006a0f0d, // Real Arcade Pro 4 PS3
		0x011e0e6f, // Rock Candy PS4 PS3
		0x02140e6f, // Afterglow PS3 PS3
		0x2013056e, // JC-U4113SBK PS3
		0x88380738, // Madcatz Fightstick Pro PS3
		0x08361a34, // Afterglow PS3 PS3
		0x11000f30, // Quanba Q1 fight stick PS3
		0x00870f0d, // HORI fighting mini stick PS3
		0x00038380, // BTP 2163 PS3
		0x10001345, // PS2 ACME GA-D5 PS3
		0x30750e8f, // SpeedLink Strike FX PS3
		0x01280e6f, // Rock Candy PS3 PS3
		0x20002c22, // Quanba Drone PS3
		0xf62206a3, // Cyborg V3 PS3
		0xd007044f, // Thrustmaster wireless 3-1 PS3
		0x83c325f0, // Gioteck vx2 PS3
		0x100605b8, // JC-U3412SBK PS3
		0x576d20d6, // Power A PS3 PS3
		0x13140e6f, // PDP Afterglow Wireless PS3 controller PS3
		0x31800738, // Mad Catz Alpha PS3 mode PS3
		0x81800738, // Mad Catz Alpha PS4 mode PS3
		0x02030e6f, // Victrix Pro FS PS3
		0x05c4054c, // Sony PS4 Controller PS4
		0x09cc054c, // Sony PS4 Slim Controller PS4
		0x0ba0054c, // Sony PS4 Controller (Wireless dongle) PS4
		0x008a0f0d, // HORI Real Arcade Pro 4 PS4
		0x00550f0d, // HORIPAD 4 FPS PS4
		0x00660f0d, // HORIPAD 4 FPS Plus PS4
		0x83840738, // HORIPAD 4 FPS Plus PS4
		0x82500738, // Mad Catz FightPad Pro PS4 PS4
		0x83840738, // Mad Catz Fightstick TE S+ PS4
		0x0E100C12, // Armor Armor 3 Pad PS4 PS4
		0x1CF60C12, // EMIO PS4 Elite Controller PS4
		0x10001532, // Razer Raiju PS4 Controller PS4
		0X04011532, // Razer Panthera PS4 Controller PS4
		0x05c5054c, // STRIKEPAD PS4 Grip Add-on PS4
		0x0d01146b, // Nacon Revolution Pro Controller PS4
		0x0d02146b, // Nacon Revolution Pro Controller V2 PS4
		0x00a00f0d, // HORI TAC4 PS4
		0x009c0f0d, // HORI TAC PRO PS4
		0x0ef60c12, // Hitbox Arcade Stick PS4
		0x181b0079, // Venom Arcade Stick PS4
		0x32500738, // Mad Catz FightPad PRO PS4
		0x00ee0f0d, // HORI mini wired gamepad PS4
		0x84810738, // Mad Catz FightStick TE 2+ PS4 PS4
		0x84800738, // Mad Catz FightStick TE 2 PS4
		0x01047545, // Armor 3, Level Up Cobra PS4
		0x10071532, // Razer Raiju 2 Tournament Edition (USB) PS4
		0x100A1532, // Razer Raiju 2 Tournament Edition (BT) PS4
		0x10041532, // Razer Raiju 2 Ultimate Edition (USB) PS4
		0x10091532, // Razer Raiju 2 Ultimate Edition (BT) PS4
		0x10081532, // Razer Panthera Evo Fightstick PS4
		0x00259886, // Astro C40 PS4
		0x0e150c12, // Game:Pad 4 PS4
		0x01044001, // PS4 Fun Controller PS4
		0x2004056e, // Elecom JC-U3613M Xbox360
		0xf51a06a3, // Saitek P3600 Xbox360
		0x47160738, // Mad Catz Wired Xbox 360 Controller Xbox360
		0x47180738, // Mad Catz Street Fighter IV FightStick SE Xbox360
		0x47260738, // Mad Catz Xbox 360 Controller Xbox360
		0x47280738, // Mad Catz Street Fighter IV FightPad Xbox360
		0x47360738, // Mad Catz MicroCon Gamepad Xbox360
		0x47380738, // Mad Catz Wired Xbox 360 Controller (SFIV) Xbox360
		0x47400738, // Mad Catz Beat Pad Xbox360
		0x4a010738, // Mad Catz FightStick TE 2 XboxOne
		0xb7260738, // Mad Catz Xbox controller - MW2 Xbox360
		0xbeef0738, // Mad Catz JOYTECH NEO SE Advanced GamePad Xbox360
		0xcb020738, // Saitek Cyborg Rumble Pad - PC/Xbox 360 Xbox360
		0xcb030738, // Saitek P3200 Rumble Pad - PC/Xbox 360 Xbox360
		0xf7380738, // Super SFIV FightStick TE S Xbox360
		0x01050e6f, // HSM3 Xbox360 dancepad Xbox360
		0x01130e6f, // Afterglow AX.1 Gamepad for Xbox 360 Xbox360
		0x011f0e6f, // Rock Candy Gamepad Wired Controller Xbox360
		0x01330e6f, // Xbox 360 Wired Controller Xbox360
		0x01390e6f, // Afterglow Prismatic Wired Controller XboxOne
		0x013a0e6f, // PDP Xbox One Controller XboxOne
		0x01460e6f, // Rock Candy Wired Controller for Xbox One XboxOne
		0x01470e6f, // PDP Marvel Xbox One Controller XboxOne
		0x015c0e6f, // PDP Xbox One Arcade Stick XboxOne
		0x01610e6f, // PDP Xbox One Controller XboxOne
		0x01620e6f, // PDP Xbox One Controller XboxOne
		0x01630e6f, // PDP Xbox One Controller XboxOne
		0x01640e6f, // PDP Battlefield One XboxOne
		0x01650e6f, // PDP Titanfall 2 XboxOne
		0x02010e6f, // Pelican PL-3601 'TSZ' Wired Xbox 360 Controller Xbox360
		0x02130e6f, // Afterglow Gamepad for Xbox 360 Xbox360
		0x021f0e6f, // Rock Candy Gamepad for Xbox 360 Xbox360
		0x02460e6f, // Rock Candy Gamepad for Xbox One 2015 XboxOne
		0x03010e6f, // Logic3 Controller Xbox360
		0x03460e6f, // Rock Candy Gamepad for Xbox One 2016 XboxOne
		0x04010e6f, // Logic3 Controller Xbox360
		0x04130e6f, // Afterglow AX.1 Gamepad for Xbox 360 Xbox360
		0x05010e6f, // PDP Xbox 360 Controller Xbox360
		0xf9000e6f, // PDP Afterglow AX.1 Xbox360
		0x000a0f0d, // Hori Co. DOA4 FightStick Xbox360
		0x000c0f0d, // Hori PadEX Turbo Xbox360
		0x000d0f0d, // Hori Fighting Stick EX2 Xbox360
		0x00160f0d, // Hori Real Arcade Pro.EX Xbox360
		0x001b0f0d, // Hori Real Arcade Pro VX Xbox360
		0x00630f0d, // Hori Real Arcade Pro Hayabusa (USA) Xbox One XboxOne
		0x00670f0d, // HORIPAD ONE XboxOne
		0x00780f0d, // Hori Real Arcade Pro V Kai Xbox One XboxOne
		0x55f011c9, // Nacon GC-100XF Xbox360
		0x000412ab, // Honey Bee Xbox360 dancepad Xbox360
		0x030112ab, // PDP AFTERGLOW AX.1 Xbox360
		0x030312ab, // Mortal Kombat Klassic FightStick Xbox360
		0x47481430, // RedOctane Guitar Hero X-plorer Xbox360
		0xf8011430, // RedOctane Controller Xbox360
		0x0601146b, // BigBen Interactive XBOX 360 Controller Xbox360
		0x00371532, // Razer Sabertooth Xbox360
		0x0a001532, // Razer Atrox Arcade Stick XboxOne
		0x0a031532, // Razer Wildcat XboxOne
		0x3f0015e4, // Power A Mini Pro Elite Xbox360
		0x3f0a15e4, // Xbox Airflo wired controller Xbox360
		0x3f1015e4, // Batarang Xbox 360 controller Xbox360
		0xbeef162e, // Joytech Neo-Se Take2 Xbox360
		0xfd001689, // Razer Onza Tournament Edition Xbox360
		0xfd011689, // Razer Onza Classic Edition Xbox360
		0xfe001689, // Razer Sabertooth Xbox360
		0x00021bad, // Harmonix Rock Band Guitar Xbox360
		0x00031bad, // Harmonix Rock Band Drumkit Xbox360
		0xf0161bad, // Mad Catz Xbox 360 Controller Xbox360
		0xf0181bad, // Mad Catz Street Fighter IV SE Fighting Stick Xbox360
		0xf0191bad, // Mad Catz Brawlstick for Xbox 360 Xbox360
		0xf0211bad, // Mad Cats Ghost Recon FS GamePad Xbox360
		0xf0231bad, // MLG Pro Circuit Controller (Xbox) Xbox360
		0xf0251bad, // Mad Catz Call Of Duty Xbox360
		0xf0271bad, // Mad Catz FPS Pro Xbox360
		0xf0281bad, // Street Fighter IV FightPad Xbox360
		0xf02e1bad, // Mad Catz Fightpad Xbox360
		0xf0361bad, // Mad Catz MicroCon GamePad Pro Xbox360
		0xf0381bad, // Street Fighter IV FightStick TE Xbox360
		0xf0391bad, // Mad Catz MvC2 TE Xbox360
		0xf03a1bad, // Mad Catz SFxT Fightstick Pro Xbox360
		0xf03d1bad, // Street Fighter IV Arcade Stick TE - Chun Li Xbox360
		0xf03e1bad, // Mad Catz MLG FightStick TE Xbox360
		0xf03f1bad, // Mad Catz FightStick SoulCaliber Xbox360
		0xf0421bad, // Mad Catz FightStick TES+ Xbox360
		0xf0801bad, // Mad Catz FightStick TE2 Xbox360
		0xf5011bad, // HoriPad EX2 Turbo Xbox360
		0xf5021bad, // Hori Real Arcade Pro.VX SA Xbox360
		0xf5031bad, // Hori Fighting Stick VX Xbox360
		0xf5041bad, // Hori Real Arcade Pro. EX Xbox360
		0xf5051bad, // Hori Fighting Stick EX2B Xbox360
		0xf5061bad, // Hori Real Arcade Pro.EX Premium VLX Xbox360
		0xf9001bad, // Harmonix Xbox 360 Controller Xbox360
		0xf9011bad, // Gamestop Xbox 360 Controller Xbox360
		0xf9031bad, // Tron Xbox 360 controller Xbox360
		0xf9041bad, // PDP Versus Fighting Pad Xbox360
		0xf9061bad, // MortalKombat FightStick Xbox360
		0xfa011bad, // MadCatz GamePad Xbox360
		0xfd001bad, // Razer Onza TE Xbox360
		0xfd011bad, // Razer Onza Xbox360
		0x500024c6, // Razer Atrox Arcade Stick Xbox360
		0x530024c6, // PowerA MINI PROEX Controller Xbox360
		0x530324c6, // Xbox Airflo wired controller Xbox360
		0x530a24c6, // Xbox 360 Pro EX Controller Xbox360
		0x531a24c6, // PowerA Pro Ex Xbox360
		0x539724c6, // FUS1ON Tournament Controller Xbox360
		0x541a24c6, // PowerA Xbox One Mini Wired Controller XboxOne
		0x542a24c6, // Xbox ONE spectra XboxOne
		0x543a24c6, // PowerA Xbox One wired controller XboxOne
		0x550024c6, // Hori XBOX 360 EX 2 with Turbo Xbox360
		0x550124c6, // Hori Real Arcade Pro VX-SA Xbox360
		0x550224c6, // Hori Fighting Stick VX Alt Xbox360
		0x550324c6, // Hori Fighting Edge Xbox360
		0x550624c6, // Hori SOULCALIBUR V Stick Xbox360
		0x550d24c6, // Hori GEM Xbox controller Xbox360
		0x550e24c6, // Hori Real Arcade Pro V Kai 360 Xbox360
		0x551a24c6, // PowerA FUSION Pro Controller XboxOne
		0x561a24c6, // PowerA FUSION Controller XboxOne
		0x5b0224c6, // Thrustmaster Xbox360
		0x5b0324c6, // Thrustmaster Ferrari 458 Racing Wheel Xbox360
		0x5d0424c6, // Razer Sabertooth Xbox360
		0xfafe24c6, // Rock Candy Gamepad for Xbox 360 Xbox360
	};

	for (auto i : ids) {
		if (pguid->Data1 == i)
			return true;
	}

	return false;
}

BOOL CALLBACK EnumJoysticksCallback( const DIDEVICEINSTANCE* pdidInstance,
                                     VOID* pContext )
{
    HRESULT hr;
	bool xinput = IsXInputDevice(&pdidInstance->guidProduct);

    // Obtain an interface to the enumerated joystick.
    LPDIRECTINPUTDEVICE8 joy = nullptr;
    hr = g_pDI->CreateDevice( pdidInstance->guidInstance, &joy, NULL );
	if (SUCCEEDED(hr) && joy)
		g_pJoysticks.push_back(new DInputDevice(xinput ? CT_XINPUT : CT_JOYSTICK, joy, pdidInstance->guidInstance, pdidInstance->tszProductName, xinput ? xInputActiveCount : -1));

	if (xinput)
		xInputActiveCount++;

    return DIENUM_CONTINUE;
}

BOOL CALLBACK EnumObjectsCallback( const DIDEVICEOBJECTINSTANCE* pdidoi,
                                   VOID* pContext )
{
    HRESULT hr;
    LPDIRECTINPUTDEVICE8 pWheel = (LPDIRECTINPUTDEVICE8)pContext;
    // For axes that are returned, set the DIPROP_RANGE property for the
    // enumerated axis in order to scale min/max values.
    if( pdidoi->dwType & DIDFT_AXIS )
    {
        DIPROPRANGE diprg;
        diprg.diph.dwSize       = sizeof(DIPROPRANGE);
        diprg.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        diprg.diph.dwHow        = DIPH_BYID;
        diprg.diph.dwObj        = pdidoi->dwType; // Specify the enumerated axis
        diprg.lMin              = 0;
        diprg.lMax              = 65535;

        // Set the range for the axis  (not used, DX defaults 65535 all axis)
        if (FAILED(hr = pWheel->SetProperty(DIPROP_RANGE, &diprg.diph)))
            return DIENUM_STOP;

        //DIPROPDWORD dipdw;
        //dipdw.diph.dwSize = sizeof(DIPROPDWORD);
        //dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        //dipdw.diph.dwHow = DIPH_BYID; //DIPH_DEVICE;
        //dipdw.diph.dwObj = pdidoi->dwType; //0;
        //dipdw.dwData = DIPROPAXISMODE_ABS;

        //if (FAILED(hr = pWheel->SetProperty(DIPROP_AXISMODE, &dipdw.diph)))
        //    return DIENUM_CONTINUE;

        //dipdw.dwData = 0;
        //if (FAILED(hr = pWheel->SetProperty(DIPROP_DEADZONE, &dipdw.diph)))
        //    return DIENUM_CONTINUE;

        //dipdw.dwData = DI_FFNOMINALMAX;
        //if (FAILED(hr = pWheel->SetProperty(DIPROP_SATURATION, &dipdw.diph)))
        //    return DIENUM_CONTINUE;
    }

    return DIENUM_CONTINUE;
}

//read all joystick states
void PollDevices()
{
	for (auto& joy : g_pJoysticks) {
		joy->Poll();
	}
}

//the non-linear filter (input/output 0.0-1.0 only) (parameters -50 to +50, expanded with PRECMULTI)
float FilterControl(float input, LONG linear, LONG offset, LONG dead)
{
	//ugly, but it works gooood

	//format+shorten variables
	float lf = float(linear) / PRECMULTI;
	float hs = 0;
	if (linear > 0)
		hs = 1.0f - ((lf * 2) * 0.01f);
	else
		hs = 1.0f - (abs(lf * 2) * 0.01f);

	float hs2 = (offset + 50 * PRECMULTI) / PRECMULTI * 0.01f;
	float v = input;
	float d = float(dead) / PRECMULTI * 0.005f;

	//format and apply deadzone
	v = (v * (1.0f + (d * 2.0f))) - d;

	//clamp
	if (v < 0.0f) v = 0.0f;
	if (v > 1.0f) v = 1.0f;

	//clamp negdead
	//if (v == -d) v = 0.0;
	if (fabs(v + d) < FLT_EPSILON) v = 0.0f;

	//possibilities
	float c1 = v - (1.0f - (pow((1.0f - v) , (1.0f / hs))));
	float c2 = v - pow(v , hs);
	float c3 = float(v - (1.0 - (pow((1.0 - v) , hs))));
	float c4 = ((v - pow(v , (1.0f / hs))));
	float res = 0;

	if (linear < 0) {
		res = v - (((1.0f - hs2) * c3) + (hs2 * c4)); //get negative result
	} else {
		res = v - (((1.0f - hs2) * c1) + (hs2 * c2)); //get positive result
	}

	//return our result
	return res;
}

float ReadAxis(const InputMapped& im)
{
	assert(im.index < g_pJoysticks.size());
	if (im.index >= g_pJoysticks.size())
		return 0;

	LONG value = 0;
	if (im.type == MT_AXIS)
		value = g_pJoysticks[im.index]->GetAxis(im.mapped);
	else if (im.type == MT_BUTTON && g_pJoysticks[im.index]->GetButton(im.mapped)) // for the lulz
	{
		value = USHRT_MAX;
	}


	float retval =  0;

	if (im.HALF > 60000) // origin somewhere near top
	{
		retval =  (65535-value) * (1.0f/65535);
	}
	else if (im.HALF > 26000 && im.HALF < 38000)  // origin somewhere near center
	{
		if(im.INVERTED)
			retval =  (value-32767) * (1.0f/32767);
		else
			retval =  (32767-value) * (1.0f/32767);
	}
	else if (im.HALF >= 0 && im.HALF < 4000) // origin somewhere near bottom
	{
		retval =  value * (1.0f/65535);
	}

	if (retval < 0.0f) retval = 0.0f;

	return retval;
}

float ReadAxis(int port, int cid)
{
	InputMapped im;
	if (!GetInputMap(port, cid, im))
		return 0;
	return ReadAxis(im);
}

//using both above functions
float ReadAxisFiltered(int port, int cid)
{
	InputMapped im;
	if (!GetInputMap(port, cid, im))
		return 0;
	return FilterControl(ReadAxis(im), im.LINEAR, im.OFFSET, im.DEADZONE);
}

void AutoCenter(LPDIRECTINPUTDEVICE8 device, bool onoff)
{
	if (!device) return;
	//disable the auto-centering spring.
	DIPROPDWORD dipdw;
 	dipdw.diph.dwSize       = sizeof(DIPROPDWORD);
	dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	dipdw.diph.dwObj        = 0;
	dipdw.diph.dwHow        = DIPH_DEVICE;
	dipdw.dwData            = onoff ? DIPROPAUTOCENTER_ON : DIPROPAUTOCENTER_OFF;

	device->SetProperty( DIPROP_AUTOCENTER, &dipdw.diph );
}

void SetRamp(int port, const ramp& var)
{
}

void SetRampVariable(int port, int forceids, const variable& var)
{
	if (!FFB[port]) return;

	// one main loop is 2ms, too erratic
	effRamp.dwDuration = 2000 * (var.t1 + 1) * 25;

	// Force0 only (Force2 is Y axis?)
	if (forceids & 1)
	{
		int force = var.l1;
		int dir = (var.d1 & 1 ? 1 : -1);

		if (INVERTFORCES[port])
		{
			cRamp.lStart = (127 - force) * DI_FFNOMINALMAX / 127;
			int sign = 1;
			if (cRamp.lStart < 0) sign = -1; // pull to force's direction?
			cRamp.lEnd = sign * DI_FFNOMINALMAX * dir;
		}
		else
		{
			cRamp.lStart = -(127 - force) * DI_FFNOMINALMAX / 127;
			//int sign = -1;
			//if (cRamp.lStart < 0) sign = 1; // pull to force's direction?
			//cRamp.lEnd = sign * DI_FFNOMINALMAX * dir; // or to center?
			cRamp.lEnd = -(127 -(force + /* var.t1 **/ var.s1 * dir)) * DI_FFNOMINALMAX / 127;
		}
	}

	if (g_pEffectRamp[port])
		g_pEffectRamp[port]->SetParameters(&effRamp,
				DIEP_TYPESPECIFICPARAMS | DIEP_START | DIEP_DURATION);
}

void DisableRamp(int port)
{
	if (g_pEffectRamp[port])
		g_pEffectRamp[port]->Stop();
}

// IDK
void SetSpringSlopeForce(int port, const spring& spring)
{
	cSpring.lOffset = 0;
	cSpring.lNegativeCoefficient = (spring.s1 & 1 ? -1 : 1) * spring.k1 * 10000 / 15;
	cSpring.lPositiveCoefficient = (spring.s2 & 1 ? -1 : 1) * spring.k2 * 10000 / 15;
	cSpring.dwNegativeSaturation = spring.dead1 * 10000 / 0xFF;
	cSpring.dwPositiveSaturation = spring.dead2 * 10000 / 0xFF;
	cSpring.lDeadBand = 0;

	if (g_pEffectSpring[port])
		g_pEffectSpring[port]->SetParameters(&effSpring, DIEP_TYPESPECIFICPARAMS | DIEP_START);
}

void JoystickDeviceFF::SetConstantForce(int level)
{
	OSDebugOut(TEXT("constant force: %d\n"), level);

	//FIXME either this or usb-pad-ff was inverted
	if (INVERTFORCES[m_port])
		cfw.lMagnitude = -level * DI_FFNOMINALMAX / SHRT_MAX;
	else
		cfw.lMagnitude = level * DI_FFNOMINALMAX / SHRT_MAX;

	if (FFMULTI[m_port][0] > 0)
		cfw.lMagnitude *= 1 + FFMULTI[m_port][0];

	if(g_pEffectConstant[m_port]) {
		g_pEffectConstant[m_port]->SetParameters(&eff, DIEP_TYPESPECIFICPARAMS | DIEP_START);

		//DWORD flags;
		//g_pEffectConstant->GetEffectStatus(&flags);

		//if(!(flags & DIEGES_PLAYING))
		//{
		//	InitDI();
		//}
	}
}

void JoystickDeviceFF::SetSpringForce(const parsed_ff_data& ff)
{
	cSpring.dwNegativeSaturation = ff.u.condition.left_saturation * DI_FFNOMINALMAX / SHRT_MAX;
	cSpring.dwPositiveSaturation = ff.u.condition.right_saturation * DI_FFNOMINALMAX / SHRT_MAX;

	cSpring.lNegativeCoefficient = ff.u.condition.left_coeff * DI_FFNOMINALMAX / SHRT_MAX;
	cSpring.lPositiveCoefficient = ff.u.condition.right_coeff * DI_FFNOMINALMAX / SHRT_MAX;

	cSpring.lOffset = ff.u.condition.center * DI_FFNOMINALMAX / SHRT_MAX;
	cSpring.lDeadBand = ff.u.condition.deadband * DI_FFNOMINALMAX / USHRT_MAX;

	OSDebugOut(TEXT("spring: %d  %d coeff:%d/%d sat:%d/%d\n"),
		cSpring.lOffset, cSpring.lDeadBand,
		cSpring.lNegativeCoefficient, cSpring.lPositiveCoefficient,
		cSpring.dwNegativeSaturation, cSpring.dwPositiveSaturation);

	if (g_pEffectSpring[m_port])
		g_pEffectSpring[m_port]->SetParameters(&effSpring, DIEP_TYPESPECIFICPARAMS | DIEP_START);
}

void JoystickDeviceFF::SetDamperForce(const parsed_ff_data& ff)
{
	cDamper.dwNegativeSaturation = ff.u.condition.left_saturation * DI_FFNOMINALMAX / SHRT_MAX;
	cDamper.dwPositiveSaturation = ff.u.condition.right_saturation * DI_FFNOMINALMAX / SHRT_MAX;

	cDamper.lNegativeCoefficient = ff.u.condition.left_coeff * DI_FFNOMINALMAX / SHRT_MAX;
	cDamper.lPositiveCoefficient = ff.u.condition.right_coeff * DI_FFNOMINALMAX / SHRT_MAX;

	cDamper.lOffset = ff.u.condition.center * DI_FFNOMINALMAX / SHRT_MAX;
	cDamper.lDeadBand = ff.u.condition.deadband * DI_FFNOMINALMAX / USHRT_MAX;

	if (g_pEffectDamper[m_port])
		g_pEffectDamper[m_port]->SetParameters(&effDamper, DIEP_TYPESPECIFICPARAMS | DIEP_START);
}

//LG driver converts it into high-precision damper instead, hmm
void JoystickDeviceFF::SetFrictionForce(const parsed_ff_data& ff)
{
	cFriction.dwNegativeSaturation = ff.u.condition.left_saturation * DI_FFNOMINALMAX / SHRT_MAX;
	cFriction.dwPositiveSaturation = ff.u.condition.right_saturation * DI_FFNOMINALMAX / SHRT_MAX;

	cFriction.lNegativeCoefficient = ff.u.condition.left_coeff * DI_FFNOMINALMAX / SHRT_MAX;
	cFriction.lPositiveCoefficient = ff.u.condition.right_coeff * DI_FFNOMINALMAX / SHRT_MAX;

	cFriction.lOffset = ff.u.condition.center * DI_FFNOMINALMAX / SHRT_MAX;
	cFriction.lDeadBand = ff.u.condition.deadband * DI_FFNOMINALMAX / USHRT_MAX;

	OSDebugOut(TEXT("friction %d/%d %d\n"),
		cFriction.lNegativeCoefficient, cFriction.lPositiveCoefficient,
		cFriction.dwNegativeSaturation);

	if (g_pEffectFriction[m_port])
		g_pEffectFriction[m_port]->SetParameters(&effFriction, DIEP_TYPESPECIFICPARAMS | DIEP_START);
}

void JoystickDeviceFF::DisableForce(EffectID force)
{
	switch(force)
	{
	case EFF_CONSTANT:
		if (g_pEffectConstant[m_port])
			g_pEffectConstant[m_port]->Stop();
	break;
	case EFF_SPRING:
		if (g_pEffectSpring[m_port])
			g_pEffectSpring[m_port]->Stop();
	break;
	case EFF_DAMPER:
		if (g_pEffectDamper[m_port])
			g_pEffectDamper[m_port]->Stop();
	break;
	case EFF_FRICTION:
		if (g_pEffectFriction[m_port])
			g_pEffectFriction[m_port]->Stop();
	break;
	case EFF_RUMBLE:
	break;
	}
}

void JoystickDeviceFF::SetAutoCenter(int value)
{
	InputMapped im;
	LPDIRECTINPUTDEVICE8 dev = nullptr;
	if (GetInputMap(m_port, CID_STEERING, im))
		dev = reinterpret_cast<DInputDevice*>(g_pJoysticks[im.index])->GetDevice();

	AutoCenter(dev, value > 0);
}

void FreeDirectInput()
{
	if (!refCount || --refCount > 0) return;

	ReleaseFFB(0);
	ReleaseFFB(1);

    // Release any DirectInput objects.
	for (auto joy : g_pJoysticks)
		delete joy;
	g_pJoysticks.clear();

    SAFE_RELEASE( g_pDI );
	didDIinit=false;

	if (pXInputEnable && xInputActiveCount)
		pXInputEnable(0);
}

//initialize all available devices
HRESULT InitDirectInput( HWND hWindow, int port )
{

    HRESULT hr;
	LPDIRECTINPUTDEVICE8 pKeyboard = NULL;
	LPDIRECTINPUTDEVICE8 pMouse = NULL;

	//release any previous resources
	OSDebugOut(TEXT("DINPUT: FreeDirectInput %p\n"), hWindow);

	if (refCount == 0)
	{
		// Create a DInput object
		OSDebugOut(TEXT("DINPUT: DirectInput8Create %p\n"), hWindow);
		if (FAILED(hr = DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION,
			IID_IDirectInput8, (VOID**)&g_pDI, NULL)))
			return hr;

		OSDebugOut(TEXT("DINPUT: CreateDevice Keyboard %p\n"), hWindow);
		//Create Keyboard
		g_pDI->CreateDevice(GUID_SysKeyboard, &pKeyboard, NULL);
		if (pKeyboard)
		{
			pKeyboard->SetDataFormat(&c_dfDIKeyboard);
			pKeyboard->SetCooperativeLevel(hWindow, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
			pKeyboard->Acquire();
			g_pJoysticks.push_back(new DInputDevice(CT_KEYBOARD, pKeyboard, GUID_SysKeyboard, _T("SysKeyboard")));
		}

		OSDebugOut(TEXT("DINPUT: CreateDevice Mouse %p\n"), hWindow);
		//Create Mouse
		g_pDI->CreateDevice(GUID_SysMouse, &pMouse, NULL);
		if (pMouse)
		{
			pMouse->SetDataFormat(&c_dfDIMouse2);
			pMouse->SetCooperativeLevel(hWindow, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
			pMouse->Acquire();
			g_pJoysticks.push_back(new DInputDevice(CT_MOUSE, pMouse, GUID_SysMouse, _T("SysMouse")));
		}

		//enumerate attached only
		OSDebugOut(TEXT("DINPUT: EnumDevices Joystick %p\n"), hWindow);
		xInputActiveCount = 0;
		g_pDI->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumJoysticksCallback, NULL, DIEDFL_ATTACHEDONLY);
		if (xInputActiveCount)
			LoadXInput();

		//loop through all attached joysticks
		for (size_t i = 0; i < g_pJoysticks.size(); i++) {
			auto joy = g_pJoysticks[i];

			auto device = reinterpret_cast<DInputDevice*>(joy)->GetDevice();
			OSDebugOut(_T("DINPUT: SetDataFormat Joystick %s\n"), joy->Product().c_str());
			device->SetDataFormat(&c_dfDIJoystick2);

			DIDEVCAPS diCaps;
			diCaps.dwSize = sizeof(DIDEVCAPS);
			device->GetCapabilities(&diCaps);

			if (diCaps.dwFlags & DIDC_FORCEFEEDBACK) {
				OSDebugOut(_T("DINPUT: SetCooperativeLevel Joystick %s\n"), joy->Product().c_str());
				//Exclusive
				device->SetCooperativeLevel(hWindow, DISCL_EXCLUSIVE | DISCL_BACKGROUND);

				/*DIDEVICEINSTANCE instance_;
				ZeroMemory(&instance_, sizeof(DIDEVICEINSTANCE));
				instance_.dwSize = sizeof(DIDEVICEINSTANCE);
				g_pJoysticks[i]->GetDeviceInfo(&instance_);
				std::stringstream str;
				str << instance_.guidInstance;*/
			}
			else
				device->SetCooperativeLevel(hWindow, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);

			OSDebugOut(TEXT("DINPUT: EnumObjects Joystick %i\n"), i);
			device->EnumObjects(EnumObjectsCallback, device, DIDFT_ALL);
			OSDebugOut(TEXT("DINPUT: Acquire Joystick %i\n"), i);
			device->Acquire();
		}
	}

	++refCount;

	didDIinit=true;
	return S_OK;

}

HWND GetWindowHandle(DWORD tPID)
{
	//Get first window handle
	HWND res = FindWindow(NULL,NULL);
	DWORD mPID = 0;
	while(res != 0)
	{
		if(!GetParent(res))
		{
			GetWindowThreadProcessId(res,&mPID);
			if (mPID == tPID)
				return res;
		}
		res = GetWindow(res, GW_HWNDNEXT);
	}
	return NULL;
}

void GetID(TCHAR * name)
{
	hWin = ::FindWindow(name, NULL);
	::GetWindowThreadProcessId(hWin, &pid);
}

bool FindFFDevice(int port)
{
	InputMapped im;
	if (!GetInputMap(port, CID_STEERING, im))
		return false;

	if (g_pJoysticks[im.index]->GetControlType() != CT_JOYSTICK)
		return false;

	auto device = reinterpret_cast<DInputDevice*>(g_pJoysticks[im.index])->GetDevice();
	DIDEVCAPS diCaps;
	diCaps.dwSize = sizeof(DIDEVCAPS);
	device->GetCapabilities(&diCaps);

	//has ffb?
	if (!FFB[port] && (diCaps.dwFlags & DIDC_FORCEFEEDBACK)) {

		//FIXME im.mapped is offset to GetAxisValueFromOffset, compatibility with DIEFFECT::rgdwAxes is questionable after DIJOYSTATE2::rglSlider
		CreateFFB(port, device, im.mapped);

		AutoCenter(device, false); //TODO some games set autocenter. Figure out default for ones that don't.

		/*DIDEVICEINSTANCE instance_;
		ZeroMemory(&instance_, sizeof(DIDEVICEINSTANCE));
		instance_.dwSize = sizeof(DIDEVICEINSTANCE);
		g_pJoysticks[i]->GetDeviceInfo(&instance_);
		std::stringstream str;
		str << instance_.guidInstance;*/
	}
	return FFB[port];
}

//use direct input
void InitDI(int port, const char *dev_type)
{
	if(gsWnd) {
		hWin = gsWnd;
	} else {
		pid = GetCurrentProcessId();
		while(hWin == 0){ hWin = GetWindowHandle(pid);}
	}

	InitDirectInput(hWin, port);
	LoadDInputConfig(port, dev_type);
	FindFFDevice(port);
}

bool GetControl(int port, int id)
{
	InputMapped im;
	if (!GetInputMap(port, id, im))
		return false;

	assert(im.index < g_pJoysticks.size());
	if (im.index >= g_pJoysticks.size())
		return false;

	auto joy = g_pJoysticks[im.index];

	if (im.type == MT_AXIS) {
		return ReadAxisFiltered(port, id) >= 0.5f;
	}
	else if (im.type == MT_BUTTON) {
		return joy->GetButton(im.mapped);
	}
	return false;
}

float GetAxisControl(int port, ControlID id)
{
	if (id == CID_STEERING)
	{
		//apply steering, single axis is split to two for filtering
		if (ReadAxisFiltered(port, CID_STEERING) > 0.0) {
			return -ReadAxisFiltered(port, CID_STEERING);
		} else {
			if (ReadAxisFiltered(port, CID_STEERING_R) > 0.0) {
				return ReadAxisFiltered(port, CID_STEERING_R);
			} else {
				return 0;
			}
		}
	}

	return ReadAxisFiltered(port, id);
}

//set left/right ffb torque
void SetConstantForce(int port, LONG magnitude)
{
	OSDebugOut(TEXT("constant force: %d\n"), magnitude);
	if (!FFB[port]) return;

	if (INVERTFORCES[port])
		cfw.lMagnitude = -magnitude;
	else
		cfw.lMagnitude = magnitude;

	if (FFMULTI[port][0] > 0)
		cfw.lMagnitude *= 1 + FFMULTI[port][0];

	if (g_pEffectConstant[port])
		g_pEffectConstant[port]->SetParameters(&eff, DIEP_TYPESPECIFICPARAMS | DIEP_START);
}

void TestForce(int port)
{
	InputMapped im;
	LPDIRECTINPUTDEVICE8 dev = nullptr;
	if (!GetInputMap(port, CID_STEERING, im))
		return;
	if (g_pJoysticks[im.index]->GetControlType() != CT_JOYSTICK)
		return;

	dev = reinterpret_cast<DInputDevice*>(g_pJoysticks[im.index])->GetDevice();
	SetConstantForce(port, DI_FFNOMINALMAX / 2);
	Sleep(500);
	SetConstantForce(port, -DI_FFNOMINALMAX / 2);
	Sleep(1000);
	SetConstantForce(port, DI_FFNOMINALMAX / 2);
	Sleep(500);
	SetConstantForce(port, 0);

	if (dev) { //FIXME actually center, maybe
		AutoCenter(dev, true);
		Sleep(1500);
		AutoCenter(dev, false);
	}

}

}} //namespace
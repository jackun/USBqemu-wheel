#pragma once

#include <windows.h>
#include <stdint.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <atomic>
#include <algorithm>
#include <array>
#include <vector>
#include <map>
#include <sstream>

#include "../usb-pad.h"
#include "../../configuration.h"
#include "../../osdebugout.h"
#include "usb-pad/lg/lg_ff.h"

#define DINPUT_AXES_COUNT 32

namespace usb_pad { namespace dx {

extern int32_t BYPASSCAL;

//dinput control mappings

static const DWORD PRECMULTI = 100; //floating point precision multiplier, 100 - two digit precision after comma

extern int32_t GAINZ[2][1];
extern int32_t FFMULTI[2][1];
extern int32_t INVERTFORCES[2];

static bool didDIinit = false;					//we have a handle

std::ostream& operator<<(std::ostream& os, REFGUID guid);

enum ControlID
{
	CID_STEERING,
	CID_STEERING_R,
	CID_THROTTLE,
	CID_BRAKE,
	CID_HATUP,
	CID_HATDOWN,
	CID_HATLEFT,
	CID_HATRIGHT,
	CID_SQUARE,
	CID_TRIANGLE,
	CID_CROSS,
	CID_CIRCLE,
	CID_L1,
	CID_R1,
	CID_L2,
	CID_R2,
	CID_L3,
	CID_R3,
	CID_SELECT,
	CID_START,
	CID_BUTTON20,
	CID_BUTTON21,
	CID_BUTTON22,
	CID_BUTTON23,
	CID_COUNT,
};

// Maybe merge with JoystickDevice
class JoystickDeviceFF : public FFDevice
{
public:
	JoystickDeviceFF(int port): m_port(port) {}
	~JoystickDeviceFF() {}

	void SetConstantForce(int level);
	void SetSpringForce(const parsed_ff_data& ff);
	void SetDamperForce(const parsed_ff_data& ff);
	void SetFrictionForce(const parsed_ff_data& ff);
	void SetAutoCenter(int value);
	void DisableForce(EffectID force);

private:
	int m_port;
};

class XInputDeviceFF : public FFDevice
{
public:
	XInputDeviceFF(int port) : m_port(port) {}
	~XInputDeviceFF() {}

	void SetConstantForce(int level) {}
	void SetSpringForce(const parsed_ff_data& ff) {}
	void SetDamperForce(const parsed_ff_data& ff) {}
	void SetFrictionForce(const parsed_ff_data& ff) {}
	void SetAutoCenter(int value) {}
	void DisableForce(EffectID force) {}

private:
	int m_port;
};

enum ControlType
{
	CT_NONE,
	CT_KEYBOARD,
	CT_MOUSE,
	CT_JOYSTICK,
	CT_XINPUT,
};

enum MappingType
{
	MT_NONE = 0, //TODO leave for sanity checking?
	MT_AXIS,
	MT_BUTTON,
};

struct InputMapped
{
	size_t index; //index into g_pJoysticks
	MappingType type = MT_NONE;
	int32_t mapped; //device axis/button
	bool INVERTED;
	int32_t HALF;
	int32_t LINEAR;
	int32_t OFFSET;
	int32_t DEADZONE;
};

class JoystickDevice
{
public:
	//virtual DeviceType DeviceType() = 0;
	virtual bool Poll() = 0;
	virtual bool GetButton(int b) = 0;
	virtual LONG GetAxis(int a) = 0;
	virtual DIJOYSTATE2 GetDeviceState() = 0;
	virtual HRESULT GetDeviceState(DWORD sz, LPVOID ptr) = 0;
	virtual ControlType GetControlType() = 0;
	virtual GUID GetGUID() = 0;
	virtual const TSTDSTRING& Product() const = 0;
};

class DInputDevice : public JoystickDevice
{
public:
	DInputDevice(ControlType type, LPDIRECTINPUTDEVICE8 device, GUID guid, TSTDSTRING name, uint32_t xinput = -1)
	: m_type(type)
	, m_guid(guid)
	, m_product(name)
	, m_device(device)
	, m_xinput(xinput)
	{
	}

	virtual bool Poll();

	/*void GetDeviceState(size_t sz, void *ptr)
	{
		if (sz == sizeof(DIJOYSTATE2) && ptr)
			*ptr = m_jstate;
	}*/

	DIJOYSTATE2 GetDeviceState()
	{
		//assert(m_type == CT_JOYSTICK);
		return m_controls.js2;
	}

	HRESULT GetDeviceState(DWORD sz, LPVOID ptr)
	{
		return m_device->GetDeviceState(sz, ptr);
	}

	bool GetButton(int b);
	LONG GetAxis(int a);

	LPDIRECTINPUTDEVICE8 GetDevice()
	{
		return m_device;
	}

	GUID GetGUID()
	{
		return m_guid;
	}

	const TSTDSTRING& Product() const
	{
		return m_product;
	}

	ControlType GetControlType() { return m_type; }

	~DInputDevice();
private:
	ControlType m_type = CT_NONE;
	GUID m_guid;
	TSTDSTRING m_product;
	LPDIRECTINPUTDEVICE8 m_device;
	uint32_t m_xinput = -1;
	union {
		DIJOYSTATE2 js2;
		DIMOUSESTATE2 ms2;
		BYTE kbd[256];
	} m_controls = {};
};

class XInputDevice : public JoystickDevice
{
public:
	XInputDevice(GUID guid, TSTDSTRING name)
		:  m_guid(guid)
		, m_product(name)
		//, m_device(device)
	{
	}

	//virtual DeviceType DeviceType() = 0;
	bool Poll();
	bool GetButton(int b);
	LONG GetAxis(int a);

	DIJOYSTATE2 GetDeviceState() { return m_js2; }
	HRESULT GetDeviceState(DWORD sz, LPVOID ptr);
	ControlType GetControlType() { return CT_XINPUT; }
	GUID GetGUID() { return m_guid; }
	const TSTDSTRING& Product() const { return m_product; }
private:
	DIJOYSTATE2 m_js2;
	GUID m_guid;
	TSTDSTRING m_product;
	LPDIRECTINPUTDEVICE8 m_device;
};

extern std::vector<JoystickDevice *> g_pJoysticks;
extern std::map<int, InputMapped> g_Controls[2];
extern HWND hWnd;
extern bool listening;

void LoadDInputConfig(int port, const char *dev_type);
void SaveDInputConfig(int port, const char *dev_type);

void InitDI(int port, const char *dev_type);
HRESULT InitDirectInput( HWND hWindow, int port );
void FreeDirectInput();
void PollDevices();
float ReadAxis(const InputMapped& im);
float ReadAxis(int port, ControlID axisid);
float FilterControl(float input, LONG linear, LONG offset, LONG dead);
bool KeyDown(DWORD KeyID);
void TestForce(int port);
LONG GetAxisValueFromOffset(int axis, const DIJOYSTATE2& j);
bool GetControl(int port, int id);
float GetAxisControl(int port, ControlID id);
void CreateFFB(int port, LPDIRECTINPUTDEVICE8 device, DWORD axis);
bool FindFFDevice(int port);

void AddInputMap(int port, int cid, const InputMapped& im);
void RemoveInputMap(int port, int cid);
bool GetInputMap(int port, int cid, InputMapped& im);

void InitDialog(int port, const char* dev_type);
void DefaultFilters(int port, LONG id);
void LoadFilter(int port);
void ApplyFilter(int port);
void ListenForControl(int port);
void StartListen(ControlID controlid);
void ControlTest(int port);

}} //namespace

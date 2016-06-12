#pragma once
#include "../padproxy.h"
#include "../../configuration.h"
#include <linux/joystick.h>

#define BITS_TO_UCHAR(x) \
	(((x) + 8 * sizeof (unsigned char) - 1) / (8 * sizeof (unsigned char)))

// Keep in sync with PS2Buttons enum
enum JoyDevMap
{
	JOY_CROSS = 0,
	JOY_SQUARE,
	JOY_CIRCLE,
	JOY_TRIANGLE,
	JOY_R1,
	JOY_L1,
	JOY_R2,
	JOY_L2,
	JOY_SELECT,
	JOY_START,
	JOY_R3, JOY_L3, //order, afaik not used on any PS2 wheel anyway
	JOY_DOWN,
	JOY_LEFT,
	JOY_UP,
	JOY_RIGHT,
	JOY_STEERING,
	JOY_THROTTLE,
	JOY_BRAKE,
	JOY_MAPS_COUNT
};

static const char* JoyDevMapNames [] = {
	"cross",
	"square",
	"circle",
	"triangle",
	"r1",
	"l1",
	"r2",
	"l2",
	"select",
	"start",
	"r3",
	"l3",
	"left",
	"up",
	"right",
	"down",
	"steering",
	"throttle",
	"brake"
};

class JoyDevPad : public Pad
{
public:
	JoyDevPad(int port): Pad(port)
	, mIsGamepad(false)
	, mIsDualAnalog(false)
	{
	}

	~JoyDevPad() { Close(); }
	int Open();
	int Close();
	int TokenIn(uint8_t *buf, int len);
	int TokenOut(const uint8_t *data, int len);
	int Reset() { return 0; }

	static const TCHAR* Name()
	{
		return "Input (joydev)";
	}

	static int Configure(int port, void *data);
	static std::vector<CONFIGVARIANT> GetSettings();
protected:
	bool FindPad();
	void SetConstantForce(int force);
	void SetSpringForce(int force);
	void SetAutoCenter(int value);
	void SetGain(int gain);
	void DisableConstantForce();

	int mHandle;
	int mHandleFF;
	struct ff_effect mEffConstant;
	struct wheel_data_t mWheelData;
	uint8_t  mAxisMap[ABS_MAX + 1];
	uint16_t mBtnMap[KEY_MAX + 1];
	int mAxisCount;
	int mButtonCount;

	std::vector<uint16_t> mMappings;

	bool mIsGamepad; //xboxish gamepad
	bool mIsDualAnalog; // tricky, have to read the AXIS_RZ somehow and
						// determine if its unpressed value is zero
};

template< size_t _Size >
bool GetJoystickName(const std::string& path, char (&name)[_Size]);
bool LoadMappings(int port, const std::string& joyname, std::vector<uint16_t>& mappings);
bool SaveMappings(int port, const std::string& joyname, std::vector<uint16_t>& mappings);

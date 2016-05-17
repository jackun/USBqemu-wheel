#pragma once
#include "../padproxy.h"
#include "../../configuration.h"
#include <linux/joystick.h>

//TODO what to call this?
class JoyDevPad : public Pad
{
public:
	JoyDevPad(int port): mPort(port) {}
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

	int mHandle;
	int mHandleFF;
	struct ff_effect mEffect;
	struct wheel_data_t mWheelData;
	uint8_t  mAxisMap[ABS_MAX + 1];
	uint16_t mBtnMap[KEY_MAX + 1];
	int mAxisCount;
	int mButtonCount;
	int mPort;
};

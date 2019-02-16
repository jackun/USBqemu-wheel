#ifndef EVDEV_FF_H
#define EVDEV_FF_H

#include <linux/input.h>
#include "../usb-pad.h"

namespace usb_pad { namespace evdev {

class EvdevFF
{
public:
	EvdevFF(int fd);
	~EvdevFF();

	void SetConstantForce(int force);
	void SetSpringForce(int force);
	void SetAutoCenter(int value);
	void SetGain(int gain);
	void DisableConstantForce();
	void TokenOut(ff_data *ffdata, bool hires);

private:
	int mHandle;
	ff_effect mEffConstant;
	ff_effect mEffRumble;
	ff_state mFFstate;

	bool mUseRumble;
	int mLastValue;
};

}} //namespace
#endif

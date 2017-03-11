#ifndef EVDEV_FF_H
#define EVDEV_FF_H

#include <linux/input.h>
#include <cstdint>

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

private:
	int mHandle;
	struct ff_effect mEffConstant;
};

#endif
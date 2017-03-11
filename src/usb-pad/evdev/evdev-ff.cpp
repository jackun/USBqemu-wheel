#include "evdev-ff.h"
#include "osdebugout.h"
#include <unistd.h>
#include <cerrno>
#include <cstring>

#define BITS_TO_UCHAR(x) \
	(((x) + 8 * sizeof (unsigned char) - 1) / (8 * sizeof (unsigned char)))
#define testBit(bit, array) ( (((uint8_t*)(array))[(bit) / 8] >> ((bit) % 8)) & 1 )

EvdevFF::EvdevFF(int fd): mHandle(fd)
{
	mEffConstant.id = -1;

	unsigned char features[BITS_TO_UCHAR(FF_MAX)];
	if (ioctl(mHandle, EVIOCGBIT(EV_FF, sizeof(features)), features) < 0)
	{
		OSDebugOut("Get features failed: %s\n", strerror(errno));
	}
	int effects = 0;
	if (ioctl(mHandle, EVIOCGEFFECTS, &effects) < 0)
	{
		OSDebugOut("Get effects failed: %s\n", strerror(errno));
	}

	if (!testBit(FF_CONSTANT, features))
	{
		OSDebugOut("device does not support FF_CONSTANT\n");
	}

	if (!testBit(FF_GAIN, features))
	{
		OSDebugOut("device does not support FF_GAIN\n");
	}

	if (!testBit(FF_AUTOCENTER, features))
	{
		OSDebugOut("device does not support FF_AUTOCENTER\n");
	}

	// TODO check features and do FF_RUMBLE instead if gamepad?
	// XXX linux status (hid-lg4ff.c) - only constant and autocenter are implemented
	mEffConstant.type = FF_CONSTANT;
	mEffConstant.id = -1;
	mEffConstant.u.constant.level = 0;	/* Strength : 0x2000 == 25 % */
	// Logitech wheels' force vs turn direction: 255 - left, 127/128 - neutral, 0 - right
	// left direction
	mEffConstant.direction = 0x4000;
	mEffConstant.u.constant.envelope.attack_length = 0;//0x100;
	mEffConstant.u.constant.envelope.attack_level = 0;
	mEffConstant.u.constant.envelope.fade_length = 0;//0x100;
	mEffConstant.u.constant.envelope.fade_level = 0;
	mEffConstant.trigger.button = 0;
	mEffConstant.trigger.interval = 0;
	mEffConstant.replay.length = 0x7FFFUL;  /* mseconds */
	mEffConstant.replay.delay = 0;

	SetGain(75);
	SetAutoCenter(0);
}

EvdevFF::~EvdevFF()
{
	if (mEffConstant.id != -1 && ioctl(mHandle, EVIOCRMFF, mEffConstant.id) == -1)
	{
		OSDebugOut("Failed to unload FF effect.\n");
	}
}


void EvdevFF::SetConstantForce(int force)
{
	mEffConstant.u.constant.level = -(127-force) * 0x8000 / 127;
	OSDebugOut("force: %d, level: %d\n", force, mEffConstant.u.constant.level);

	if (ioctl(mHandle, EVIOCSFF, &(mEffConstant)) < 0) {
		OSDebugOut("Failed to upload effect\n");
	}

	struct input_event play;
	play.type = EV_FF;
	play.code = mEffConstant.id;
	play.value = 1;
	if (write(mHandle, (const void*) &play, sizeof(play)) == -1) {
		OSDebugOut("Play effect failed\n");
	}

}

void EvdevFF::DisableConstantForce()
{
	struct input_event play;
	play.type = EV_FF;
	play.code = mEffConstant.id;
	play.value = 0;
	if (write(mHandle, (const void*) &play, sizeof(play)) == -1) {
		OSDebugOut("Stop effect failed\n");
	}
}

void EvdevFF::SetSpringForce(int force)
{

}

void EvdevFF::SetAutoCenter(int value)
{
	struct input_event ie;

	ie.type = EV_FF;
	ie.code = FF_AUTOCENTER;
	ie.value = value * 0xFFFFUL / 100;

	if (write(mHandle, &ie, sizeof(ie)) == -1)
		OSDebugOut("Failed to set autocenter\n");
}

void EvdevFF::SetGain(int gain /* between 0 and 100 */)
{
	struct input_event ie;

	ie.type = EV_FF;
	ie.code = FF_GAIN;
	ie.value = 0xFFFFUL * gain / 100;

	if (write(mHandle, &ie, sizeof(ie)) == -1)
		OSDebugOut("Failed to set gain\n");
}
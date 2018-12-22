#include "evdev-ff.h"
#include "osdebugout.h"
#include <unistd.h>
#include <cerrno>
#include <cstring>

namespace usb_pad { namespace evdev {

#define BITS_TO_UCHAR(x) \
	(((x) + 8 * sizeof (unsigned char) - 1) / (8 * sizeof (unsigned char)))
#define testBit(bit, array) ( (((uint8_t*)(array))[(bit) / 8] >> ((bit) % 8)) & 1 )

EvdevFF::EvdevFF(int fd): mHandle(fd), mUseRumble(false)
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
		if (testBit(FF_RUMBLE, features))
			mUseRumble = true;
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

	mEffRumble.type = FF_RUMBLE;
	mEffRumble.u.rumble.strong_magnitude = 0;
	mEffRumble.u.rumble.weak_magnitude = 0;
	mEffRumble.replay.length = 500;
	mEffRumble.replay.delay = 0;
	mEffRumble.id = -1;

	SetGain(75);
	SetAutoCenter(0);
}

EvdevFF::~EvdevFF()
{
	if (mEffConstant.id != -1 && ioctl(mHandle, EVIOCRMFF, mEffConstant.id) == -1)
	{
		OSDebugOut("Failed to unload constant force effect.\n");
	}

	if (mEffRumble.id != -1 && ioctl(mHandle, EVIOCRMFF, mEffRumble.id) == -1)
	{
		OSDebugOut("Failed to unload rumble effect.\n");
	}
}

void EvdevFF::SetConstantForce(int force)
{
	struct input_event play;
	play.type = EV_FF;
	play.value = 1;

	if (!mUseRumble) {
		mEffConstant.u.constant.level = -(127-force) * 0x8000 / 127;
		OSDebugOut("force: %d, level: %d\n", force, mEffConstant.u.constant.level);

		if (ioctl(mHandle, EVIOCSFF, &(mEffConstant)) < 0) {
			OSDebugOut("Failed to upload constant effect: %s\n", strerror(errno));
		}
		play.code = mEffConstant.id;
	} else {

		mEffRumble.u.rumble.weak_magnitude = 0;
		mEffRumble.u.rumble.strong_magnitude = 0;

		int mag = std::abs((128-force) * 65535 / 128);
		int diff = std::abs(mag - mLastValue);

		// TODO random limits to cull down on too much rumble
		if (diff > 8292 && diff < 32767)
			mEffRumble.u.rumble.weak_magnitude = mag;
		if (diff / 8192 > 0)
			mEffRumble.u.rumble.strong_magnitude = mag;

		mLastValue = mag;

		if (ioctl(mHandle, EVIOCSFF, &(mEffRumble)) < 0) {
			OSDebugOut("Failed to upload constant effect: %s\n", strerror(errno));
		}
		play.code = mEffRumble.id;
	}

	if (write(mHandle, (const void*) &play, sizeof(play)) == -1) {
		OSDebugOut("Play effect failed: %s\n", strerror(errno));
	}

}

void EvdevFF::DisableConstantForce()
{
	struct input_event play;
	play.type = EV_FF;
	play.code = mEffConstant.id;
	play.value = 0;
	if (write(mHandle, (const void*) &play, sizeof(play)) == -1) {
		OSDebugOut("Stop effect failed: %s\n", strerror(errno));
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
		OSDebugOut("Failed to set autocenter: %s\n", strerror(errno));
}

void EvdevFF::SetGain(int gain /* between 0 and 100 */)
{
	struct input_event ie;

	ie.type = EV_FF;
	ie.code = FF_GAIN;
	ie.value = 0xFFFFUL * gain / 100;

	if (write(mHandle, &ie, sizeof(ie)) == -1)
		OSDebugOut("Failed to set gain: %s\n", strerror(errno));
}

void EvdevFF::TokenOut(ff_data *ffdata, bool hires)
{
	OSDebugOut(TEXT("FFB %02X, %02X, %02X, %02X : %02X, %02X, %02X, %02X\n"),
		ffdata->cmdslot, ffdata->type, ffdata->u.params[0], ffdata->u.params[1],
		ffdata->u.params[2], ffdata->u.params[3], ffdata->u.params[4], ffdata->padd0);

	if (ffdata->cmdslot != CMD_EXTENDED_CMD)
	{

		uint8_t slots = (ffdata->cmdslot & 0xF0) >> 4;
		uint8_t cmd  = ffdata->cmdslot & 0x0F;

		switch (cmd)
		{
		case CMD_DOWNLOAD:
			for (int i = 0; i < 4; i++)
			{
				if (slots & (1 << i))
					mFFstate.slot_type[i] = ffdata->type;
			}
			break;
		case CMD_DOWNLOAD_AND_PLAY: //0x01
		{
			for (int i = 0; i < 4; i++)
			{
				if (slots & (1 << i))
				{
					mFFstate.slot_type[i] = ffdata->type;
					if (ffdata->type == FTYPE_CONSTANT)
						mFFstate.slot_force[i] = ffdata->u.params[i];
				}
			}

			switch (ffdata->type)
			{
			case FTYPE_CONSTANT:
				//SetConstantForce(ffdata->u.params[2]); //DF/GTF and GT3
				if (slots == 0xF)
				{
					int force = 0x7F;
					//TODO hack, GT3 uses slot 3 usually
					for (int i = 0; i < 4; i++)
					{
						force = ffdata->u.params[i];
						if (force != 0x7F)
							break;
					}
					SetConstantForce(force);
				}
				else
				{
					for (int i = 0; i < 4; i++)
					{
						if (slots == (1 << i))
							SetConstantForce(ffdata->u.params[i]);
					}
				}
				break;
			//case FTYPE_SPRING:
			//case FTYPE_HIGH_RESOLUTION_SPRING:
				//SetSpringForce(NormalizeSteering(mWheelData.steering, mType), ffdata->u.spring, hires);
				//break;
			case FTYPE_VARIABLE: //Ramp-like
				//SetRampVariable(ffdata->u.variable);
				SetConstantForce(ffdata->u.params[0]);
				break;
			//case FTYPE_FRICTION:
				//SetFrictionForce(ffdata->u.friction);
				//break;
			//case FTYPE_DAMPER:
				//SetDamper(ffdata->u.damper, false);
				//break;
			//case FTYPE_HIGH_RESOLUTION_DAMPER:
				//SetDamper(ffdata->u.damper, hires);
				//break;
			default:
				OSDebugOut(TEXT("CMD_DOWNLOAD_AND_PLAY: unhandled force type 0x%02X in slots 0x%02X\n"), ffdata->type, slots);
				break;
			}
		}
		break;
		case CMD_STOP: //0x03
		{
			if (slots == 0x0F) //disable all effects, usually on startup
			{
				DisableConstantForce();
				//SetSpringForce(DI_FFNOMINALMAX + 1, spring { 0 }, hires);
			}
			else
			{
				for (int i = 0; i < 4; i++)
				{
					if (slots & (1 << i))
					{
						switch (mFFstate.slot_type[i])
						{
						case FTYPE_CONSTANT:
							DisableConstantForce();
							break;
						case FTYPE_VARIABLE:
							//DisableRamp();
							DisableConstantForce();
							break;
						//case FTYPE_SPRING:
						//case FTYPE_HIGH_RESOLUTION_SPRING:
							//DisableSpring();
							//break;
						//case FTYPE_AUTO_CENTER_SPRING:
							//DisableSpring();
							//break;
						//case FTYPE_FRICTION:
							//DisableFriction();
							//break;
						//case FTYPE_DAMPER:
						//case FTYPE_HIGH_RESOLUTION_DAMPER:
							//DisableDamper();
							//break;
						default:
							OSDebugOut(TEXT("CMD_STOP: unhandled force type 0x%02X in slot 0x%02X\n"), ffdata->type, slots);
							break;
						}
					}
				}
			}
		}
		break;
		case CMD_DEFAULT_SPRING_ON: //0x04
			OSDebugOut(TEXT("CMD_DEFAULT_SPRING_ON: unhandled cmd\n"));
			break;
		case CMD_DEFAULT_SPRING_OFF: //0x05
		{
			if (slots == 0x0F) {
				//just release force
				SetConstantForce(127);
			}
			else
			{
				OSDebugOut(TEXT("CMD_DEFAULT_SPRING_OFF: unhandled slots 0x%02X\n"), slots);
			}
		}
		break;
		case CMD_NORMAL_MODE: //0x08
			OSDebugOut(TEXT("CMD_NORMAL_MODE: unhandled cmd\n"));
			break;
		case CMD_SET_LED: //0x09
			OSDebugOut(TEXT("CMD_SET_LED: unhandled cmd\n"));
			break;
		case CMD_RAW_MODE: //0x0B
			OSDebugOut(TEXT("CMD_RAW_MODE: unhandled cmd\n"));
			break;
		case CMD_SET_DEFAULT_SPRING: //0x0E
			OSDebugOut(TEXT("CMD_SET_DEFAULT_SPRING: unhandled cmd\n"));
			break;
		case CMD_SET_DEAD_BAND: //0x0F
			OSDebugOut(TEXT("CMD_SET_DEAD_BAND: unhandled cmd\n"));
			break;
		}
	}
	else
	{
		// 0xF8, 0x05, 0x01, 0x00
		//if(ffdata->type == 5) //TODO
		//	sendCrap = true;
		if (ffdata->type == EXT_CMD_WHEEL_RANGE_900_DEGREES) {}
		if (ffdata->type == EXT_CMD_WHEEL_RANGE_200_DEGREES) {}
		OSDebugOut(TEXT("CMD_EXTENDED: unhandled cmd 0x%02X%02X%02X\n"),
			ffdata->type, ffdata->u.params[0], ffdata->u.params[1]);
	}
}

}} //namespace
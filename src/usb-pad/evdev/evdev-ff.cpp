#include "evdev-ff.h"
#include "osdebugout.h"
#include "usb-pad/lg/lg_ff.h"
#include <unistd.h>
#include <cerrno>
#include <cstring>

namespace usb_pad { namespace evdev {

#define BITS_TO_UCHAR(x) \
	(((x) + 8 * sizeof (unsigned char) - 1) / (8 * sizeof (unsigned char)))
#define testBit(bit, array) ( (((uint8_t*)(array))[(bit) / 8] >> ((bit) % 8)) & 1 )

EvdevFF::EvdevFF(int fd): mHandle(fd), mUseRumble(false)
{
	unsigned char features[BITS_TO_UCHAR(FF_MAX)];
	if (ioctl(mHandle, EVIOCGBIT(EV_FF, sizeof(features)), features) < 0) {
		OSDebugOut("Get features failed: %s\n", strerror(errno));
	}

	int effects = 0;
	if (ioctl(mHandle, EVIOCGEFFECTS, &effects) < 0) {
		OSDebugOut("Get effects failed: %s\n", strerror(errno));
	}

	if (!testBit(FF_CONSTANT, features)) {
		OSDebugOut("device does not support FF_CONSTANT\n");
		if (testBit(FF_RUMBLE, features))
			mUseRumble = true;
	}

	if (!testBit(FF_SPRING, features)) {
		OSDebugOut("device does not support FF_SPRING\n");
	}

	if (!testBit(FF_DAMPER, features)) {
		OSDebugOut("device does not support FF_DAMPER\n");
	}

	if (!testBit(FF_GAIN, features)) {
		OSDebugOut("device does not support FF_GAIN\n");
	}

	if (!testBit(FF_AUTOCENTER, features)) {
		OSDebugOut("device does not support FF_AUTOCENTER\n");
	}

	memset(&mEffect, 0, sizeof(mEffect));

	// TODO check features and do FF_RUMBLE instead if gamepad?
	// XXX linux status (hid-lg4ff.c) - only constant and autocenter are implemented
	mEffect.u.constant.level = 0;	/* Strength : 0x2000 == 25 % */
	// Logitech wheels' force vs turn direction: 255 - left, 127/128 - neutral, 0 - right
	// left direction
	mEffect.direction = 0x4000;
	mEffect.u.constant.envelope.attack_length = 0;//0x100;
	mEffect.u.constant.envelope.attack_level = 0;
	mEffect.u.constant.envelope.fade_length = 0;//0x100;
	mEffect.u.constant.envelope.fade_level = 0;
	mEffect.trigger.button = 0;
	mEffect.trigger.interval = 0;
	mEffect.replay.length = 0x7FFFUL;  /* mseconds */
	mEffect.replay.delay = 0;

	SetGain(100);
	SetAutoCenter(0);
}

EvdevFF::~EvdevFF()
{
	for(int i=0; i<countof(mEffIds); i++)
	{
		if (mEffIds[i] != -1 && ioctl(mHandle, EVIOCRMFF, mEffIds[i]) == -1) {
			OSDebugOut("Failed to unload EffectID(%d) effect.\n", i);
		}
	}
}

void EvdevFF::DisableForce(EffectID force)
{
	struct input_event play;
	play.type = EV_FF;
	play.code = mEffIds[force];
	play.value = 0;
	if (write(mHandle, (const void*) &play, sizeof(play)) == -1) {
		OSDebugOut("Stop effect failed: %s\n", strerror(errno));
	}
}

void EvdevFF::SetConstantForce(int level)
{
	struct input_event play;
	play.type = EV_FF;
	play.value = 1;

	if (!mUseRumble) {
		mEffect.type = FF_CONSTANT;
		mEffect.id = mEffIds[EFF_CONSTANT];
		mEffect.u.constant.level = level;

		if (ioctl(mHandle, EVIOCSFF, &(mEffect)) < 0) {
			OSDebugOut("Failed to upload constant effect: %s\n", strerror(errno));
			return;
		}
		play.code = mEffect.id;
		mEffIds[EFF_CONSTANT] = mEffect.id;
	} else {

		mEffect.type = FF_RUMBLE;
		mEffect.id = mEffIds[EFF_RUMBLE];

		mEffect.replay.length = 500;
		mEffect.replay.delay = 0;
		mEffect.u.rumble.weak_magnitude = 0;
		mEffect.u.rumble.strong_magnitude = 0;

		int mag = std::abs(level);
		int diff = std::abs(mag - mLastValue);

		// TODO random limits to cull down on too much rumble
		if (diff > 8292 && diff < 32767)
			mEffect.u.rumble.weak_magnitude = mag;
		if (diff / 8192 > 0)
			mEffect.u.rumble.strong_magnitude = mag;

		mLastValue = mag;

		if (ioctl(mHandle, EVIOCSFF, &(mEffect)) < 0) {
			OSDebugOut("Failed to upload constant effect: %s\n", strerror(errno));
			return;
		}
		play.code = mEffect.id;
		mEffIds[EFF_RUMBLE] = mEffect.id;
	}

	if (write(mHandle, (const void*) &play, sizeof(play)) == -1) {
		OSDebugOut("Play effect failed: %s\n", strerror(errno));
	}

}

void EvdevFF::SetSpringForce(const spring& force, int caps)
{
	struct input_event play;
	play.type = EV_FF;
	play.value = 1;

	mEffect.type = FF_SPRING;
	mEffect.id = mEffIds[EFF_SPRING];
	mEffect.u.condition[0].left_saturation = ff_lg_u8_to_u16(force.clip);
	mEffect.u.condition[0].right_saturation = ff_lg_u8_to_u16(force.clip);
	mEffect.u.condition[0].left_coeff =
		ff_lg_get_condition_coef(caps, force.k1, force.s1);
	mEffect.u.condition[0].right_coeff =
		ff_lg_get_condition_coef(caps, force.k2, force.s2);

	if (caps & FF_LG_CAPS_HIGH_RES_DEADBAND)
	{
		uint16_t d2 = ff_lg_get_spring_deadband(caps, force.dead2, (force.s2 >> 1) & 0x7);
		uint16_t d1 = ff_lg_get_spring_deadband(caps, force.dead1, (force.s1 >> 1) & 0x7);
		mEffect.u.condition[0].center = ff_lg_u16_to_s16((d1 + d2) / 2);
		mEffect.u.condition[0].deadband = d2 - d1;
	}
	else
	{
		mEffect.u.condition[0].center = ff_lg_u8_to_s16((force.dead1 + force.dead2) / 2);
		mEffect.u.condition[0].deadband = ff_lg_u8_to_u16(force.dead2 - force.dead1);
	}

	if (ioctl(mHandle, EVIOCSFF, &(mEffect)) < 0) {
		OSDebugOut("Failed to upload spring effect: %s\n", strerror(errno));
		return;
	}

	play.code = mEffect.id;
	mEffIds[EFF_SPRING] = mEffect.id;

	if (write(mHandle, (const void*) &play, sizeof(play)) == -1) {
		OSDebugOut("Play effect failed: %s\n", strerror(errno));
	}
}

void EvdevFF::SetDamperForce(const damper& force, int caps)
{
	struct input_event play;
	play.type = EV_FF;
	play.value = 1;

	mEffect.type = FF_DAMPER;
	mEffect.id = mEffIds[EFF_DAMPER];
	mEffect.u.condition[0].left_saturation = ff_lg_get_damper_clip(caps, force.clip);
	mEffect.u.condition[0].right_saturation = ff_lg_get_damper_clip(caps, force.clip);
	mEffect.u.condition[0].left_coeff =
		ff_lg_get_condition_coef(caps, force.k1, force.s1);
	mEffect.u.condition[0].right_coeff =
		ff_lg_get_condition_coef(caps, force.k2, force.s2);
	mEffect.u.condition[0].center = 0;
	mEffect.u.condition[0].deadband = 0;

	if (ioctl(mHandle, EVIOCSFF, &(mEffect)) < 0) {
		OSDebugOut("Failed to upload damper effect: %s\n", strerror(errno));
		return;
	}

	play.code = mEffect.id;
	mEffIds[EFF_DAMPER] = mEffect.id;

	if (write(mHandle, (const void*) &play, sizeof(play)) == -1) {
		OSDebugOut("Play effect failed: %s\n", strerror(errno));
	}
}

void EvdevFF::SetFrictionForce(const friction& frict)
{
	struct input_event play;
	play.type = EV_FF;
	play.value = 1;

	mEffect.type = FF_FRICTION;
	mEffect.id = mEffIds[EFF_FRICTION];

	//noideaTM
	mEffect.u.condition[0].center = 0;
	mEffect.u.condition[0].deadband = 0;
	int s1 = frict.s1 & 1 ? -1 : 1;
	int s2 = frict.s2 & 1 ? -1 : 1;

	mEffect.u.condition[0].left_coeff = frict.k1 * 0x7FFF / 255 * s1;
	mEffect.u.condition[0].right_coeff = frict.k2 * 0x7FFF / 255 * s2;

	mEffect.u.condition[0].left_saturation = 0x7FFF * frict.clip / 255;
	mEffect.u.condition[0].right_saturation = mEffect.u.condition[0].left_saturation;

	if (ioctl(mHandle, EVIOCSFF, &(mEffect)) < 0) {
		OSDebugOut("Failed to upload friction effect: %s\n", strerror(errno));
		return;
	}

	play.code = mEffect.id;
	mEffIds[EFF_FRICTION] = mEffect.id;

	if (write(mHandle, (const void*) &play, sizeof(play)) == -1) {
		OSDebugOut("Play effect failed: %s\n", strerror(errno));
	}
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

void EvdevFF::TokenOut(ff_data *ffdata, bool isDFP)
{
	int caps = 0;
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
			case FTYPE_SPRING:
				SetSpringForce(ffdata->u.spring, isDFP ? 0 : FF_LG_CAPS_OLD_LOW_RES_COEF);
				break;
			case FTYPE_HIGH_RESOLUTION_SPRING:
				SetSpringForce(ffdata->u.spring, FF_LG_CAPS_HIGH_RES_COEF | FF_LG_CAPS_HIGH_RES_DEADBAND);
				break;
			case FTYPE_VARIABLE: //Ramp-like
				//SetRampVariable(ffdata->u.variable);
				//SetConstantForce(ffdata->u.params[0]);
				static int warned = 0;
				if (slots & (1 << 0)) {
					if (ffdata->u.variable.t1 && ffdata->u.variable.s1) {
						if (warned == 0) {
							OSDebugOut("variable force cannot be converted to constant force (l1=%hu, t1=%hu, s1=%hu, d1=%hu\n",
								ffdata->u.variable.l1, ffdata->u.variable.t1, ffdata->u.variable.s1, ffdata->u.variable.d1);
							warned = 1;
						}
					} else {
						SetConstantForce(ffdata->u.variable.l1);
					}
				}
				else if (slots & (1 << 2)) {
					if (ffdata->u.variable.t2 && ffdata->u.variable.s2) {
						if (warned == 0) {
							OSDebugOut("variable force cannot be converted to constant force (l2=%hu, t2=%hu, s2=%hu, d2=%hu\n",
								ffdata->u.variable.l2, ffdata->u.variable.t2, ffdata->u.variable.s2, ffdata->u.variable.d2);
							warned = 1;
						}
					} else {
						SetConstantForce(ffdata->u.variable.l2);
					}
				}
				break;
			case FTYPE_FRICTION:
				SetFrictionForce(ffdata->u.friction);
				break;
			case FTYPE_DAMPER:
				SetDamperForce(ffdata->u.damper, 0);
				break;
			case FTYPE_HIGH_RESOLUTION_DAMPER:
				caps = FF_LG_CAPS_HIGH_RES_COEF;
				if (isDFP)
					caps |= FF_LG_CAPS_DAMPER_CLIP;
				SetDamperForce(ffdata->u.damper, caps);
				break;
			default:
				OSDebugOut(TEXT("CMD_DOWNLOAD_AND_PLAY: unhandled force type 0x%02X in slots 0x%02X\n"), ffdata->type, slots);
				break;
			}
		}
		break;
		case CMD_STOP: //0x03
		{
			for (int i = 0; i < 4; i++)
			{
				if (slots & (1 << i))
				{
					switch (mFFstate.slot_type[i])
					{
					case FTYPE_CONSTANT:
						DisableForce(EFF_CONSTANT);
						break;
					case FTYPE_VARIABLE:
						//DisableRamp();
						DisableForce(EFF_CONSTANT);
						break;
					case FTYPE_SPRING:
					case FTYPE_HIGH_RESOLUTION_SPRING:
						DisableForce(EFF_SPRING);
						break;
					//case FTYPE_AUTO_CENTER_SPRING:
						//DisableSpring();
						//break;
					case FTYPE_FRICTION:
						DisableForce(EFF_FRICTION);
						break;
					case FTYPE_DAMPER:
					case FTYPE_HIGH_RESOLUTION_DAMPER:
						DisableForce(EFF_DAMPER);
						break;
					default:
						OSDebugOut(TEXT("CMD_STOP: unhandled force type 0x%02X in slot 0x%02X\n"), ffdata->type, slots);
						break;
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

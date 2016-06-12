#include "joydev.h"
#include "../../USB.h"
#include "../../osdebugout.h"
#include <assert.h>
#include <unistd.h>
//#include <iostream>
#include <sstream>
//#include <math.h>

#define APINAME "joydev"

extern bool file_exists(std::string path);
extern bool dir_exists(std::string path);
static bool sendCrap = false;

#define NORM(x, n) (((uint32_t)(32767 + x) * n)/0xFFFE)
#define NORM2(x, n) (((uint32_t)(32767 + x) * n)/0x7FFF)
#define testBit(bit, array) ( (((uint8_t*)array)[bit / 8] >> (bit % 8)) & 1 )

static inline int range_max(PS2WheelTypes type)
{
	if(type == WT_DRIVING_FORCE_PRO)
		return 0x3FFF;
	return 0x3FF;
}

template< size_t _Size >
bool GetJoystickName(const std::string& path, char (&name)[_Size])
{
	int fd = 0;
	if ((fd = open(path.c_str(), O_RDONLY)) < 0)
	{
		fprintf(stderr, "Cannot open %s\n", path.c_str());
	}
	else
	{
		if (ioctl(fd, JSIOCGNAME(_Size), name) < -1)
		{
			fprintf(stderr, "Cannot get controller's name\n");
			close(fd);
			return false;
		}
		close(fd);
		return true;
	}
	return false;
}

bool LoadMappings(int port, const std::string& joyname, std::vector<uint16_t>& mappings)
{
	assert(JOY_MAPS_COUNT == ARRAYSIZE(JoyDevMapNames));
	if (joyname.empty())
		return false;

	mappings.resize(0);
	std::stringstream str;
	for (int i=0; i<JOY_MAPS_COUNT; i++)
	{
		str.clear();
		str.str("");
		str << "map_" << JoyDevMapNames[i];
		CONFIGVARIANT var(str.str().c_str(), CONFIG_TYPE_INT);
		if (LoadSetting(port, joyname, var))
			mappings.push_back(var.intValue);
		else
			mappings.push_back(-1);
	}
	return true;
}

bool SaveMappings(int port, const std::string& joyname, std::vector<uint16_t>& mappings)
{
	assert(JOY_MAPS_COUNT == ARRAYSIZE(JoyDevMapNames));
	if (joyname.empty() || mappings.size() != JOY_MAPS_COUNT)
		return false;

	std::stringstream str;
	for (int i=0; i<JOY_MAPS_COUNT; i++)
	{
		//XXX save anyway for manual editing
		//if (mappings[i] == (uint16_t)-1)
		//	continue;

		str.clear();
		str.str("");
		str << "map_" << JoyDevMapNames[i];
		CONFIGVARIANT var(str.str().c_str(), mappings[i]);
		if (!SaveSetting(port, joyname.c_str(), var))
			return false;
	}
	return true;
}

int JoyDevPad::TokenIn(uint8_t *buf, int buflen)
{
	ssize_t len;

	int range = range_max(mType);

	if(sendCrap)
	{
		// Setting to unpressed
		memset(&mWheelData, 0, sizeof(wheel_data_t));
		mWheelData.steering = range >> 1;
		mWheelData.clutch = 0xFF;
		mWheelData.throttle = 0xFF;
		mWheelData.brake = 0xFF;
		mWheelData.hatswitch = 0x8;
		mWheelData.hat_horz = 0x8;
		mWheelData.hat_vert = 0x8;
		pad_copy_data(mType, buf, mWheelData);
		sendCrap = false;
		return buflen;
	}

	struct js_event event;

	//Non-blocking read sets len to -1 and errno to EAGAIN if no new data
	while((len = read(mHandle, &event, sizeof(event))) > -1)
	{
		if (len == sizeof(event))
		{
			if ((event.type & ~JS_EVENT_INIT) == JS_EVENT_AXIS)
			{
				switch (mAxisMap[event.number])
				{
					case 0x80 | JOY_STEERING:
					case ABS_X: mWheelData.steering = NORM(event.value, range); break;
					case ABS_Y: mWheelData.clutch = NORM(event.value, 0xFF); break;
					//case ABS_RX: mWheelData.axis_rx = NORM(event.value, 0xFF); break;
					case 0x80 | JOY_BRAKE:
					case ABS_RY:
					treat_me_like_ABS_RY:
						mWheelData.throttle = 0xFF;
						mWheelData.brake = 0xFF;
						if (event.value < 0)
							mWheelData.throttle = NORM2(event.value, 0xFF);
						else
							mWheelData.brake = NORM2(-event.value, 0xFF);
					break;
					case 0x80 | JOY_THROTTLE:
					case ABS_Z:
						if (mIsGamepad)
							mWheelData.brake = 0xFF - NORM(event.value, 0xFF);
						else
							mWheelData.throttle = NORM(event.value, 0xFF);
					break;
					case ABS_RZ:
						if (mIsGamepad)
							mWheelData.throttle = 0xFF - NORM(event.value, 0xFF);
						else if (mIsDualAnalog)
							goto treat_me_like_ABS_RY;
						else
							mWheelData.brake = NORM(event.value, 0xFF);
					break;

					//FIXME hatswitch mapping maybe
					case ABS_HAT0X:
						if(event.value < 0 ) //left usually
							mWheelData.hat_horz = PAD_HAT_W;
						else if(event.value > 0 ) //right
							mWheelData.hat_horz = PAD_HAT_E;
						else
							mWheelData.hat_horz = PAD_HAT_COUNT;
					break;
					case ABS_HAT0Y:
						if(event.value < 0 ) //up usually
							mWheelData.hat_vert = PAD_HAT_N;
						else if(event.value > 0 ) //down
							mWheelData.hat_vert = PAD_HAT_S;
						else
							mWheelData.hat_vert = PAD_HAT_COUNT;
					break;
					default: break;
				}
			}
			else if ((event.type & ~JS_EVENT_INIT) == JS_EVENT_BUTTON)
			{
				//OSDebugOut("Button: %d, %d\n", event.number, event.value);
				PS2Buttons button = PAD_BUTTON_COUNT;
				if (mBtnMap[event.number] >= (0x8000 | JOY_CROSS) &&
					mBtnMap[event.number] <= (0x8000 | JOY_L3))
				{
					button = (PS2Buttons)(mBtnMap[event.number] & ~0x8000);
				}

				else if (mBtnMap[event.number] >= BTN_TRIGGER &&
					mBtnMap[event.number] < BTN_BASE5)
				{
					button = (PS2Buttons)(mBtnMap[event.number] - BTN_TRIGGER);
				}
				else
				{
					// Map to xbox360ish controller
					switch (mBtnMap[event.number])
					{
						// Digital hatswitch
						case 0x8000 | JOY_LEFT:
							mWheelData.hat_horz = PAD_HAT_W;
							break;
						case 0x8000 | JOY_RIGHT:
							mWheelData.hat_horz = PAD_HAT_E;
							break;
						case 0x8000 | JOY_UP:
							mWheelData.hat_vert = PAD_HAT_N;
							break;
						case 0x8000 | JOY_DOWN:
							mWheelData.hat_vert = PAD_HAT_S;
							break;
						case BTN_WEST: button = PAD_SQUARE; break;
						case BTN_NORTH: button = PAD_TRIANGLE; break;
						case BTN_EAST: button = PAD_CIRCLE; break;
						case BTN_SOUTH: button = PAD_CROSS; break;
						case BTN_SELECT: button = PAD_SELECT; break;
						case BTN_START: button = PAD_START; break;
						case BTN_TR: button = PAD_R1; break;
						case BTN_TL: button = PAD_L1; break;
						case BTN_THUMBR: button = PAD_R2; break;
						case BTN_THUMBL: button = PAD_L2; break;
						default:
								OSDebugOut("Unmapped Button: %d, %d\n", event.number, event.value);
						break;
					}
				}

				//if (button != PAD_BUTTON_COUNT)
				{
					if (event.value)
						mWheelData.buttons |= 1 << convert_wt_btn(mType, button); //on
					else
						mWheelData.buttons &= ~(1 << convert_wt_btn(mType, button)); //off
				}
			}
		}
		else
		{
			OSDebugOut(APINAME ": TokenIn: read error %d\n", errno);
			break;
		}
	}

	switch (mWheelData.hat_vert)
	{
		case PAD_HAT_N:
			switch (mWheelData.hat_horz)
			{
				case PAD_HAT_W: mWheelData.hatswitch = PAD_HAT_NW; break;
				case PAD_HAT_E: mWheelData.hatswitch = PAD_HAT_NE; break;
				default: mWheelData.hatswitch = PAD_HAT_N; break;
			}
		break;
		case PAD_HAT_S:
			switch (mWheelData.hat_horz)
			{
				case PAD_HAT_W: mWheelData.hatswitch = PAD_HAT_SW; break;
				case PAD_HAT_E: mWheelData.hatswitch = PAD_HAT_SE; break;
				default: mWheelData.hatswitch = PAD_HAT_S; break;
			}
		break;
		default:
			mWheelData.hatswitch = mWheelData.hat_horz;
		break;
	}

	pad_copy_data(mType, buf, mWheelData);
	return buflen;
}

bool JoyDevPad::FindPad()
{
	memset(&mWheelData, 0, sizeof(wheel_data_t));

	// Setting to unpressed
	mWheelData.steering = 0x3FF >> 1;
	mWheelData.clutch = 0xFF;
	mWheelData.throttle = 0xFF;
	mWheelData.brake = 0xFF;
	mWheelData.hatswitch = 0x8;
	mWheelData.hat_horz = 0x8;
	mWheelData.hat_vert = 0x8;
	memset(mAxisMap, -1, sizeof(mAxisMap));
	memset(mBtnMap, -1, sizeof(mBtnMap));

	mAxisCount = 0;
	mButtonCount = 0;
	mHandle = -1;
	mHandleFF = -1;
	mEffConstant.id = -1;

	std::string joypath;
	{
		CONFIGVARIANT var(N_JOYSTICK, CONFIG_TYPE_CHAR);
		if(LoadSetting(mPort, APINAME, var))
			joypath = var.strValue;
		else
		{
			OSDebugOut("Cannot load joystick setting: %s\n", N_JOYSTICK);
			return false;
		}
	}

	if(!joypath.empty() && file_exists(joypath))
	{
		char name[1024];
		GetJoystickName(joypath, name);
		LoadMappings(mPort, name, mMappings);

		if ((mHandle = open(joypath.c_str(), O_RDONLY | O_NONBLOCK)) < 0)
		{
			OSDebugOut("Cannot open joystick: %s\n", joypath.c_str());
		}
		else
		{
			//int flags = fcntl(mHandle, F_GETFL, 0);
			//fcntl(mHandle, F_SETFL, flags | O_NONBLOCK);

			unsigned int version;
			if (ioctl(mHandle, JSIOCGVERSION, &version) < 0)
			{
				SysMessage(APINAME ": Get version failed: %s\n", strerror(errno));
				return false;
			}

			if (version < 0x010000)
			{
				SysMessage(APINAME ": Driver version 0x%X is too old\n", version);
				goto quit;
			}

			// Axis Mapping
			if (ioctl(mHandle, JSIOCGAXMAP, mAxisMap) < 0)
			{
				SysMessage(APINAME ": Axis mapping failed: %s\n", strerror(errno));
				goto quit;
			}
			else
			{
				ioctl(mHandle, JSIOCGAXES, &mAxisCount);
				for(int i = 0; i < mAxisCount; ++i)
					OSDebugOut("Axis: %d -> %d\n", i, mAxisMap[i] );

				for (int k = 0; k < mAxisCount; k++)
					for (int i = JOY_STEERING; i < JOY_MAPS_COUNT; i++)
					{
						if (mAxisMap[k] == mMappings[i])
							mAxisMap[k] = 0x80 | i;
					}
			}

			// Button Mapping
			if (ioctl(mHandle, JSIOCGBTNMAP, mBtnMap) < 0)
			{
				SysMessage(APINAME ": Button mapping failed: %s\n", strerror(errno));
				goto quit;
			}
			else
			{
				ioctl(mHandle, JSIOCGBUTTONS, &mButtonCount);
				for(int i = 0; i < mButtonCount; ++i)
				{
					OSDebugOut("Button: %d -> %d BTN_[GAMEPAD|SOUTH]: %d\n", i, mBtnMap[i], mBtnMap[i] == BTN_GAMEPAD );
					if (mBtnMap[i] == BTN_GAMEPAD)
						mIsGamepad = true;
				}

				if (!mIsGamepad) //TODO Don't remap if gamepad?
				for (int k = 0; k < mButtonCount; k++)
					for (int i = 0; i < JOY_STEERING; i++)
					{
						if (mBtnMap[k] == mMappings[i])
							mBtnMap[k] = 0x8000 | i;
					}
			}

			std::stringstream event;
			int index = 0;
			const char *tmp = joypath.c_str();
			while(*tmp && !isdigit(*tmp))
				tmp++;

			sscanf(tmp, "%d", &index);
			OSDebugOut("input index: %d of '%s'\n", index, joypath.c_str());

			for (int j = 0; j <= 99; j++)
			{
				event.clear(); event.str(std::string());
				/* Try to discover the corresponding event number */
				event << "/sys/class/input/js" << index << "/device/event" << j;
				if (dir_exists(event.str())){

					event.clear(); event.str(std::string());
					event << "/dev/input/event" << j;
					break;
				}
			}
			if ((mHandleFF = open(event.str().c_str(), /*O_WRONLY*/ O_RDWR)) < 0)
			{
				OSDebugOut(APINAME ": Cannot open '%s'\n", event.str().c_str());
			}
			else
			{
				unsigned char features[BITS_TO_UCHAR(FF_MAX)];
				if (ioctl(mHandleFF, EVIOCGBIT(EV_FF, sizeof(features)), features) < 0)
				{
					OSDebugOut(APINAME ": Get features failed: %s\n", strerror(errno));
				}
				int effects = 0;
				if (ioctl(mHandleFF, EVIOCGEFFECTS, &effects) < 0)
				{
					OSDebugOut(APINAME ": Get effects failed: %s\n", strerror(errno));
				}

				if (testBit(FF_CONSTANT, features))
				{
					OSDebugOut(APINAME ": joystick does not support FF_CONSTANT\n");
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

				if (ioctl(mHandleFF, EVIOCSFF, &(mEffConstant)) < 0) {
					OSDebugOut(APINAME ": Failed to upload effect to '%s'\n", event.str().c_str());
				}

				SetGain(75);
				SetAutoCenter(0);
			}
			return true;
		}
	}

quit:
	Close();
	return false;
}

void JoyDevPad::SetConstantForce(int force)
{
	struct input_event play;
	play.type = EV_FF;
	play.code = mEffConstant.id;
	play.value = 1;

	OSDebugOut("Force: %d\n", force);
	//mEffConstant.type = FF_CONSTANT;
	//mEffConstant.id = -1;
	//mEffConstant.u.constant.level = 0x8000;	/* Strength : 0x2000 == 25 % */
	//mEffConstant.direction = (0xFFFF * (force)) / 255;
	int y = 0, x = 0;
	float magnitude = -(127-force) * 78.4803149606299;
	mEffConstant.u.constant.level = (MAX(MIN(magnitude, 10000), -10000) / 10) * 32;
	mEffConstant.direction = 0x4000; //(int)((3 * M_PI / 2 - atan2(y, x)) * -0x7FFF / M_PI);
	OSDebugOut("dir: %d lvl: %d\n", mEffConstant.direction, mEffConstant.u.constant.level);

//	mEffConstant.u.constant.envelope.attack_length = 0;
//	mEffConstant.u.constant.envelope.attack_level = 0;
//	mEffConstant.u.constant.envelope.fade_length = 0;
//	mEffConstant.u.constant.envelope.fade_level = 0;
//	mEffConstant.trigger.button = 0;
//	mEffConstant.trigger.interval = 0;
//	mEffConstant.replay.length = 0xFFFF;  /* INFINITE mseconds */
//	mEffConstant.replay.delay = 0;

	if (ioctl(mHandleFF, EVIOCSFF, &(mEffConstant)) < 0) {
		OSDebugOut(APINAME ": Failed to upload effect\n");
	}

	if (write(mHandleFF, (const void*) &play, sizeof(play)) == -1) {
		OSDebugOut(APINAME ": Play effect failed\n");
	}

}

void JoyDevPad::DisableConstantForce()
{
	struct input_event play;
	play.type = EV_FF;
	play.code = mEffConstant.id;
	play.value = 0;
	if (write(mHandleFF, (const void*) &play, sizeof(play)) == -1) {
		OSDebugOut(APINAME ":stop effect failed\n");
	}
}

void JoyDevPad::SetSpringForce(int force)
{

}

void JoyDevPad::SetAutoCenter(int value)
{
	struct input_event ie;

	ie.type = EV_FF;
	ie.code = FF_AUTOCENTER;
	ie.value = value * 0xFFFFUL / 100;

	if (write(mHandleFF, &ie, sizeof(ie)) == -1)
		OSDebugOut(APINAME ": Failed to set autocenter\n");
}

void JoyDevPad::SetGain(int gain /* between 0 and 100 */)
{
	struct input_event ie;

	ie.type = EV_FF;
	ie.code = FF_GAIN;
	ie.value = 0xFFFFUL * gain / 100;

	if (write(mHandleFF, &ie, sizeof(ie)) == -1)
		OSDebugOut("Failed to set gain\n");
}

int JoyDevPad::TokenOut(const uint8_t *data, int len)
{
	ff_data *ffdata = (ff_data*)data;

	OSDebugOut(TEXT("FFB %02X, %02X, %02X, %02X : %02X, %02X, %02X, %02X\n"),
		ffdata->cmdslot, ffdata->type, ffdata->u.params[0], ffdata->u.params[1],
		ffdata->u.params[2], ffdata->u.params[3], ffdata->u.params[4], ffdata->padd0);

	bool hires = (mType == WT_DRIVING_FORCE_PRO);
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
					SetConstantForce(ffdata->u.params[2]); //DF/GTF and GT3
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
		if(ffdata->type == 5) //TODO
			sendCrap = true;
		if (ffdata->type == EXT_CMD_WHEEL_RANGE_900_DEGREES) {}
		if (ffdata->type == EXT_CMD_WHEEL_RANGE_200_DEGREES) {}
		OSDebugOut(TEXT("CMD_EXTENDED: unhandled cmd 0x%02X%02X%02X\n"),
			ffdata->type, ffdata->u.params[0], ffdata->u.params[1]);
	}

	return len;
}

int JoyDevPad::Open()
{
	if (FindPad())
		return 0;
	return 1;
}

int JoyDevPad::Close()
{
	if(mHandle != -1)
		close(mHandle);

	if(mHandleFF != -1)
	{
		if (mEffConstant.id != -1 && ioctl(mHandleFF, EVIOCRMFF, mEffConstant.id) == -1)
		{
			OSDebugOut(APINAME ": Failed to unload FF effect.\n");
		}
		close(mHandleFF);
	}

	mHandle = -1;
	mHandleFF = -1;
	return 0;
}

std::vector<CONFIGVARIANT> JoyDevPad::GetSettings()
{
	std::vector<CONFIGVARIANT> params;
	params.push_back(CONFIGVARIANT(S_CONFIG_JOY, N_JOYSTICK, CONFIG_TYPE_CHAR));
	return params;
}

REGISTER_PAD(APINAME, JoyDevPad);
#undef APINAME

#include "joydev.h"
#include "../../USB.h"
#include "../../osdebugout.h"
#include <cassert>
#include <sstream>

#define APINAME "joydev"

extern bool file_exists(std::string path);
extern bool dir_exists(std::string path);

#define NORM(x, n) (((uint32_t)(32768 + x) * n)/0xFFFF)
#define NORM2(x, n) (((uint32_t)(32768 + x) * n)/0x7FFF)

int JoyDevPad::TokenIn(uint8_t *buf, int buflen)
{
	ssize_t len;

	int range = range_max(mType);

	struct js_event event;

	//Non-blocking read sets len to -1 and errno to EAGAIN if no new data
	while((len = read(mHandle, &event, sizeof(event))) > -1)
	{
		if (len == sizeof(event))
		{
			if ((event.type & ~JS_EVENT_INIT) == JS_EVENT_AXIS)
			{
				OSDebugOut("Axis: %d, mapped: 0x%02x, val: %d\n", event.number, mAxisMap[event.number], event.value);
				switch (mAxisMap[event.number])
				{
					case 0x80 | JOY_STEERING:
					case ABS_X: mWheelData.steering = NORM(event.value, range); break;
					case ABS_Y: mWheelData.clutch = NORM(event.value, 0xFF); break;
					//case ABS_RX: mWheelData.axis_rx = NORM(event.value, 0xFF); break;
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
					case 0x80 | JOY_BRAKE:
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
					case ABS_HAT1X:
					case ABS_HAT2X:
					case ABS_HAT3X:
						if(event.value < 0 ) //left usually
							mWheelData.hat_horz = PAD_HAT_W;
						else if(event.value > 0 ) //right
							mWheelData.hat_horz = PAD_HAT_E;
						else
							mWheelData.hat_horz = PAD_HAT_COUNT;
					break;
					case ABS_HAT0Y:
					case ABS_HAT1Y:
					case ABS_HAT2Y:
					case ABS_HAT3Y:
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
				OSDebugOut("Button: %d, mapped: 0x%02x, val: %d\n", event.number, mBtnMap[event.number], event.value);
				PS2Buttons button = PAD_BUTTON_COUNT;
				if (mBtnMap[event.number] >= (0x8000 | JOY_CROSS) &&
					mBtnMap[event.number] <= (0x8000 | JOY_L3))
				{
					button = (PS2Buttons)(mBtnMap[event.number] & ~0x8000);
				}

				else if (mBtnMap[event.number] >= BTN_TRIGGER &&
					mBtnMap[event.number] < BTN_BASE5)
				{
					button = (PS2Buttons)((mBtnMap[event.number] - BTN_TRIGGER) & ~0x8000);
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

int JoyDevPad::TokenOut(const uint8_t *data, int len)
{
	if (!mEvdevFF) return len;
	ff_data *ffdata = (ff_data*)data;
	bool hires = (mType == WT_DRIVING_FORCE_PRO);
	mEvdevFF->TokenOut(ffdata, hires);

	return len;
}

int JoyDevPad::Open()
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

	std::string joypath;
	{
		CONFIGVARIANT var(N_JOYSTICK, CONFIG_TYPE_CHAR);
		if(LoadSetting(mPort, APINAME, var))
			joypath = var.strValue;
		else
		{
			OSDebugOut("Cannot load joystick setting: %s\n", N_JOYSTICK);
			return 1;
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
						if (k == mMappings[i])
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
						if (k == mMappings[i])
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
				mEvdevFF = new EvdevFF(mHandleFF);
			return 0;
		}
	}

quit:
	Close();
	return 1;
}

int JoyDevPad::Close()
{
	delete mEvdevFF;
	mEvdevFF = nullptr;

	if(mHandle != -1)
		close(mHandle);
	if(mHandleFF != -1)
		close(mHandleFF);

	mHandle = -1;
	mHandleFF = -1;
	return 0;
}

REGISTER_PAD(APINAME, JoyDevPad);
#undef APINAME

#include "evdev.h"
#include "osdebugout.h"
#include <cassert>
#include <sstream>
#include <linux/hidraw.h>

namespace usb_pad { namespace evdev {

#define APINAME "evdev"

// hidraw* to input/event*:
// /sys/class/hidraw/hidraw*/device/input/input*/event*/uevent

#define NORM(x, n) (((uint32_t)(32768 + x) * n)/0xFFFF)
#define NORM2(x, n) (((uint32_t)(32768 + x) * n)/0x7FFF)

bool FindHidraw(const std::string &evphys, std::string& hid_dev)
{
	int fd;
	int res;
	char buf[256];

	std::stringstream str;
	struct dirent* dp;

	DIR* dirp = opendir("/dev/");
	if(dirp == NULL) {
		perror("Error opening /dev/");
		return false;
	}

	while((dp = readdir(dirp)) != NULL)
	{
		if(strncmp(dp->d_name, "hidraw", 6) == 0) {
			OSDebugOut("%s\n", dp->d_name);

			str.clear(); str.str("");
			str << "/dev/" << dp->d_name;
			fd = open(str.str().c_str(), O_RDWR|O_NONBLOCK);

			if (fd < 0) {
				perror("Unable to open device");
				continue;
			}

			memset(buf, 0x0, sizeof(buf));
			//res = ioctl(fd, HIDIOCGRAWNAME(256), buf);

			res = ioctl(fd, HIDIOCGRAWPHYS(256), buf);
			if (res < 0)
				perror("HIDIOCGRAWPHYS");
			else
				OSDebugOut("Raw Phys: %s\n", buf);
			close(fd);
			if (evphys == buf) {
				closedir(dirp);
				hid_dev = str.str();
				return true;
			}
		}
	}
quit:
	closedir(dirp);
	return false;
}

void EvDevPad::PollAxesValues()
{
	struct input_absinfo absinfo;

	/* Poll all axis */
	for (int i = ABS_X; i < ABS_MAX; i++) {
		absinfo = {};

		if ((ioctl(mHandle, EVIOCGABS(i), &absinfo) >= 0) &&
			mAbsCorrect[i].used) {
			absinfo.value = AxisCorrect(mAbsCorrect[i], absinfo.value);
		}
		SetAxis(i, absinfo.value);
	}
}

void EvDevPad::SetAxis(int event_code, int value)
{
	int range = range_max(mType);
	int code = mAxisMap[event_code] != (uint8_t)-1 ? mAxisMap[event_code] : -1 /* allow axis to be unmapped */; //event_code;
	//value = AxisCorrect(mAbsCorrect[event_code], value);

	switch (code)
	{
		case 0x80 | JOY_STEERING:
		case ABS_X: mWheelData.steering = mAxisInverted[0] ? range - NORM(value, range) : NORM(value, range); break;
		//case ABS_Y: mWheelData.clutch = NORM(value, 0xFF); break; //no wheel on PS2 has one, afaik
		//case ABS_RX: mWheelData.axis_rx = NORM(event.value, 0xFF); break;
		case ABS_RY:
		treat_me_like_ABS_RY:
			mWheelData.throttle = 0xFF;
			mWheelData.brake = 0xFF;
			if (value < 0)
				mWheelData.throttle = NORM2(value, 0xFF);
			else
				mWheelData.brake = NORM2(-value, 0xFF);
		break;
		case 0x80 | JOY_THROTTLE:
		case ABS_Z:
			/*if (mIsGamepad)
				mWheelData.brake = 0xFF - NORM(value, 0xFF);
			else*/
				mWheelData.throttle = mAxisInverted[1] ? NORM(value, 0xFF) : 0xFF - NORM(value, 0xFF);
		break;
		case 0x80 | JOY_BRAKE:
		case ABS_RZ:
			/*if (mIsGamepad)
				mWheelData.throttle = 0xFF - NORM(value, 0xFF);
			else if (mIsDualAnalog)
				goto treat_me_like_ABS_RY;
			else*/
				mWheelData.brake = mAxisInverted[2] ? NORM(value, 0xFF) : 0xFF - NORM(value, 0xFF);
		break;

		//TODO hatswitch mapping maybe
		case ABS_HAT0X:
		case ABS_HAT1X:
		case ABS_HAT2X:
		case ABS_HAT3X:
			if (value < 0) //left usually
				mWheelData.hat_horz = PAD_HAT_W;
			else if (value > 0) //right
				mWheelData.hat_horz = PAD_HAT_E;
			else
				mWheelData.hat_horz = PAD_HAT_COUNT;
		break;
		case ABS_HAT0Y:
		case ABS_HAT1Y:
		case ABS_HAT2Y:
		case ABS_HAT3Y:
			if (value < 0) //up usually
				mWheelData.hat_vert = PAD_HAT_N;
			else if (value > 0) //down
				mWheelData.hat_vert = PAD_HAT_S;
			else
				mWheelData.hat_vert = PAD_HAT_COUNT;
		break;
		default: break;
	}
}

int EvDevPad::TokenIn(uint8_t *buf, int buflen)
{
	ssize_t len;

	input_event events[32];

	//Non-blocking read sets len to -1 and errno to EAGAIN if no new data
	while((len = read(mHandle, &events, sizeof(events))) > -1)
	{
		len /= sizeof(events[0]);
		for (int i = 0; i < len; i++)
		{
			input_event& event = events[i];
			int code, value;
			switch (event.type)
			{
				case EV_ABS:
				{
					//TODO
					value = AxisCorrect(mAbsCorrect[event.code], event.value);
					if (event.code == 0)
					OSDebugOut("Axis: %d, mapped: 0x%02x, val: %d, corrected: %d\n", event.code, mAxisMap[event.code] & ~0x80, event.value, value);
					SetAxis(event.code, value);
				}
				break;
				case EV_KEY:
				{
					//TODO
					code = mBtnMap[event.code] != (uint16_t)-1 ? mBtnMap[event.code] : event.code;
					OSDebugOut("Button: 0x%02x, mapped: 0x%02x, val: %d\n", event.code, mBtnMap[event.code], event.value);

					PS2Buttons button = PAD_BUTTON_COUNT;
					if (code >= (0x8000 | JOY_CROSS) &&
						code <= (0x8000 | JOY_L3))
					{
						button = (PS2Buttons)(code & ~0x8000);
					}
					else if (code >= BTN_TRIGGER && code < BTN_BASE5)
					{
						button = (PS2Buttons)((code - BTN_TRIGGER) & ~0x8000);
					}
					else
					{
						// Map to xbox360ish controller
						switch (code)
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
								OSDebugOut("Unmapped Button: %d, %d\n", code, event.value);
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
				break;
				case EV_SYN: //TODO useful?
				{
					switch(event.code) {
						case SYN_DROPPED:
							//restore last good state
							mWheelData = {};
							PollAxesValues();
						break;
					}
				}
				break;
				default:
				break;
			}
		}

		if (len <= 0)
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

int EvDevPad::TokenOut(const uint8_t *data, int len)
{
	if (mUseRawFF) {

		OSDebugOut("FF: %02x %02x %02x %02x %02x %02x %02x\n",
			data[0], data[1], data[2], data[3], data[4], data[5], data[6]);

		if (data[0] == 0x8 || data[0] == 0xB) return len;
		if (data[0] == 0xF8 && 
			/* Allow range changes */
			!(data[1] == 0x81 || data[1] == 0x02 || data[1] == 0x03))
			return len; //don't send extended commands

		std::array<uint8_t, 8> report{ 0 };

		memcpy(report.data() + 1, data, report.size() - 1);

		if (!mFFData.enqueue(report)) {
			OSDebugOut("Failed to enqueue ffb command\n");
			return 0;
		}
		return len;
	}

	if (!mEvdevFF) return len;

	ff_data *ffdata = (ff_data*)data;
	bool hires = (mType == WT_DRIVING_FORCE_PRO);
	mEvdevFF->TokenOut(ffdata, hires);

	return len;
}

int EvDevPad::Open()
{
	int t;
	std::stringstream name;
	char buf[1024];
	mWheelData = {};

	unsigned long keybit[NBITS(KEY_MAX)] = { 0 };
	unsigned long absbit[NBITS(ABS_MAX)] = { 0 };

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

	std::string evphys, hid_dev;
	struct hidraw_devinfo info;
	memset(&info, 0x0, sizeof(info));

	std::string joypath;
	if (!LoadSetting(mDevType, mPort, APINAME, N_JOYSTICK, joypath))
	{
		OSDebugOut("Cannot load joystick setting: %s\n", N_JOYSTICK);
		return 1;
	}

	LoadSetting(mDevType, mPort, APINAME, N_HIDRAW_FF_PT, mUseRawFF);

	if(joypath.empty() || !file_exists(joypath))
		goto quit;

	if (GetEvdevName(joypath, buf)) {
		LoadMappings(mDevType, mPort, buf, mMappings, mAxisInverted);
	}

	if ((mHandle = open(joypath.c_str(), O_RDWR | O_NONBLOCK)) < 0)
	{
		OSDebugOut("Cannot open joystick: %s\n", joypath.c_str());
		goto quit;
	}

	if ((ioctl(mHandle, EVIOCGBIT(EV_ABS, sizeof(absbit)), absbit) < 0) ||
		(ioctl(mHandle, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit) < 0)) {
		// Probably isn't a evdev joystick
		SysMessage(APINAME ": Getting all the bits failed: %s\n", strerror(errno));
		goto quit;
	}

	if (mUseRawFF) {
		memset(buf, 0, sizeof(buf));
		if (ioctl(mHandle, EVIOCGPHYS(sizeof(buf) - 1), buf) > 0) {
			evphys = buf;
			OSDebugOut("Evdev Phys: %s\n", evphys.c_str());

			if ((mUseRawFF = FindHidraw(evphys, hid_dev))) {
				mHidHandle = open(hid_dev.c_str(), O_RDWR|O_NONBLOCK);
				if (mHidHandle < 0) {
					mUseRawFF = false;
					perror("Unable to open device");
				}

				// For safety, only allow Logitech devices
				if (ioctl(mHidHandle, HIDIOCGRAWINFO, &info) < 0) {
					perror("HIDIOCGRAWINFO");
				} else {
					if (info.vendor != 0x046D /* Logitech */ /*|| info.bustype != BUS_USB*/) {
						mUseRawFF = false;
						close (mHidHandle);
						mHidHandle = -1;
					}
				}
			}
		} else {
			perror("EVIOCGPHYS failed");
		}
	}

	/*unsigned int version;
	if (ioctl(mHandle, EVIOCGVERSION, &version) < 0)
	{
		SysMessage(APINAME ": Get version failed: %s\n", strerror(errno));
		return false;
	}*/

	// Map hatswitches automatically
	for (int i = ABS_HAT0X; i <= ABS_HAT3Y; ++i) {
		mAxisMap[i] = i;
	}

	// SDL2
	for (int i = 0; i < ABS_MAX; ++i) {
		if (test_bit(i, absbit)) {
			struct input_absinfo absinfo;

			if (ioctl(mHandle, EVIOCGABS(i), &absinfo) < 0) {
				continue;
			}

			OSDebugOut("Axis %d absinfo min %d max %d\n", i, absinfo.minimum, absinfo.maximum);

			//mAxisMap[i] = mAxisCount;

			// convert values into 16 bit range
			if (absinfo.minimum == absinfo.maximum) {
				mAbsCorrect[i].used = 0;
			} else {
				mAbsCorrect[i].used = 1;
				mAbsCorrect[i].coef[0] =
					(absinfo.maximum + absinfo.minimum) - 2 * absinfo.flat;
				mAbsCorrect[i].coef[1] =
					(absinfo.maximum + absinfo.minimum) + 2 * absinfo.flat;
				t = ((absinfo.maximum - absinfo.minimum) - 4 * absinfo.flat);
				if (t != 0) {
					mAbsCorrect[i].coef[2] =
						(1 << 28) / t;
				} else {
					mAbsCorrect[i].coef[2] = 0;
				}
			}

			//TODO joystick/gamepad is dual analog?
			if (i == ABS_RZ) {
				//absinfo.value = AxisCorrect(mAbsCorrect[i], absinfo.value);
				if (std::abs(absinfo.value) < 200) /* 200 is random, allows for some dead zone */
					mIsDualAnalog = true;
			}

			for (int k = JOY_STEERING; k < JOY_MAPS_COUNT; k++)
			{
				if (i == mMappings[k])
					mAxisMap[i] = 0x80 | k;
			}

			++mAxisCount;
		}
	}

	for(int i = 0; i < ABS_MAX; ++i) {
		if (mAxisMap[i] != (uint8_t)-1 && (mAxisMap[i] & 0x80))
			OSDebugOut("Axis: 0x%02x -> %s\n", i, JoystickMapNames[mAxisMap[i] & ~0x80]);
	}

	for (int i = BTN_JOYSTICK; i < KEY_MAX; ++i) {
		if (test_bit(i, keybit)) {
			//OSDebugOut("Joystick has button: 0x%x\n", i);
			mBtnMap[i] = -1;//mButtonCount;
			++mButtonCount;
			if (i == BTN_GAMEPAD) {
				mIsGamepad = true;
				OSDebugOut("Joystick is a gamepad\n");
			}
			for (int k = 0; k < JOY_STEERING; k++)
			{
				if (i == mMappings[k]) {
					mBtnMap[i] = 0x8000 | k;
					OSDebugOut("Remap button: 0x%x -> %s\n", i, JoystickMapNames[k]);
				}
			}
		}
	}
	for (int i = 0; i < BTN_JOYSTICK; ++i) {
		if (test_bit(i, keybit)) {
			OSDebugOut("Joystick has button: 0x%x\n", i);
			mBtnMap[i] = -1;//mButtonCount;
			++mButtonCount;
			for (int k = 0; k < JOY_STEERING; k++)
			{
				if (i == mMappings[k])
					mBtnMap[i] = 0x8000 | k;
			}
		}
	}

	mEvdevFF = new EvdevFF(mHandle);

	if (mUseRawFF && !mWriterThreadIsRunning)
	{
		if (mWriterThread.joinable())
			mWriterThread.join();
		mWriterThread = std::thread(EvDevPad::WriterThread, this);
	}
	return 0;

quit:
	Close();
	return 1;
}

int EvDevPad::Close()
{
	delete mEvdevFF;
	mEvdevFF = nullptr;

	if (mHandle != -1)
		close(mHandle);

	if (mHidHandle != -1) {
		uint8_t reset[7] = { 0 };
		reset[0] = 0xF3; //stop forces
		write(mHidHandle, reset, sizeof(reset));
		close(mHidHandle);
	}

	mHandle = -1;
	mHidHandle = -1;
	return 0;
}

void EvDevPad::WriterThread(void *ptr)
{
	std::array<uint8_t, 8> buf;
	int res;

	EvDevPad *pad = static_cast<EvDevPad *>(ptr);
	pad->mWriterThreadIsRunning = true;

	while (pad->mHidHandle != -1)
	{
		//if (pad->mFFData.wait_dequeue_timed(buf, std::chrono::milliseconds(1000))) //FIXME SIGABORT :S
		if (pad->mFFData.try_dequeue(buf))
		{
			res = write(pad->mHidHandle, buf.data(), buf.size());
			if (res < 0) {
				printf("Error: %d\n", errno);
				perror("write");
			}
		} else {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
	OSDebugOut(TEXT("WriterThread exited.\n"));

	pad->mWriterThreadIsRunning = false;
}
REGISTER_PAD(APINAME, EvDevPad);
#undef APINAME

}} //namespace
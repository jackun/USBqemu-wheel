#include "evdev.h"
#include "osdebugout.h"
#include <cassert>
#include <sstream>
#include <linux/hidraw.h>
#include "linux/util.h"

namespace usb_pad { namespace evdev {

// hidraw* to input/event*:
// /sys/class/hidraw/hidraw*/device/input/input*/event*/uevent

#define NORM(x, n) (((uint32_t)(32768 + x) * n)/0xFFFF)
#define NORM2(x, n) (((uint32_t)(32768 + x) * n)/0x7FFF)

bool str_ends_with(const char * str, const char * suffix)
{
	if (str == nullptr || suffix == nullptr)
		return false;

	size_t str_len = strlen(str);
	size_t suffix_len = strlen(suffix);

	if (suffix_len > str_len)
		return false;

	return 0 == strncmp( str + str_len - suffix_len, suffix, suffix_len );
}

bool FindHidraw(const std::string &evphys, std::string& hid_dev, int *vid, int *pid)
{
	int fd;
	int res;
	char buf[256];

	std::stringstream str;
	struct dirent* dp;

	DIR* dirp = opendir("/dev/");
	if (!dirp) {
		perror("Error opening /dev/");
		return false;
	}

	while((dp = readdir(dirp)))
	{
		if(strncmp(dp->d_name, "hidraw", 6) == 0) {
			OSDebugOut("%s\n", dp->d_name);

			str.clear(); str.str("");
			str << "/dev/" << dp->d_name;
			std::string path = str.str();
			fd = open(path.c_str(), O_RDWR|O_NONBLOCK);

			if (fd < 0) {
				perror("Unable to open device");
				continue;
			}

			memset(buf, 0x0, sizeof(buf));
			//res = ioctl(fd, HIDIOCGRAWNAME(256), buf);

			res = ioctl(fd, HIDIOCGRAWPHYS(sizeof(buf)), buf);
			if (res < 0)
				perror("HIDIOCGRAWPHYS");
			else
				OSDebugOut("Raw Phys: %s\n", buf);

			struct hidraw_devinfo info;
			memset(&info, 0x0, sizeof(info));

			if (ioctl(fd, HIDIOCGRAWINFO, &info) < 0) {
				perror("HIDIOCGRAWINFO");
			} else {
				if (vid) *vid = info.vendor;
				if (pid) *pid = info.product;
			}

			close(fd);
			if (evphys == buf) {
				closedir(dirp);
				hid_dev = path;
				return true;
			}
		}
	}
quit:
	closedir(dirp);
	return false;
}

#define EVDEV_DIR "/dev/input/by-id/"
void EnumerateDevices(vstring& list)
{
	int fd;
	int res;
	char buf[256];

	std::stringstream str;
	struct dirent* dp;

	//TODO do some caching? ioctl is "very" slow
	static vstring list_cache;

	DIR* dirp = opendir(EVDEV_DIR);
	if (!dirp) {
		perror("Error opening " EVDEV_DIR);
		return;
	}

	// get rid of unplugged devices
	for (int i=0; i < list_cache.size(); ) {
		if (!file_exists(list_cache[i].second))
			list_cache.erase(list_cache.begin() + i);
		else
			i++;
	}

	while ((dp = readdir(dirp)))
	{
		//if (strncmp(dp->d_name, "event", 5) == 0) {
		if (str_ends_with(dp->d_name, "event-kbd")
			|| str_ends_with(dp->d_name, "event-mouse")
			|| str_ends_with(dp->d_name, "event-joystick"))
		{
			OSDebugOut(EVDEV_DIR "%s\n", dp->d_name);

			str.clear(); str.str("");
			str << EVDEV_DIR << dp->d_name;
			std::string path = str.str();

			auto it = std::find_if(list_cache.begin(), list_cache.end(),
				[&path](auto& pair){
					return pair.second == path;
			});
			if (it != list_cache.end())
				continue;

			fd = open(path.c_str(), O_RDWR|O_NONBLOCK);

			if (fd < 0) {
				perror("Unable to open device");
				continue;
			}

			res = ioctl(fd, EVIOCGNAME(sizeof(buf)), buf);
			if (res < 0)
				perror("EVIOCGNAME");
			else
			{
				OSDebugOut("Evdev device name: %s\n", buf);
				list_cache.push_back(std::make_pair(std::string(buf) + " (evdev)", path));
			}

			close(fd);
		}
	}

	list.assign(list_cache.begin(), list_cache.end());
quit:
	closedir(dirp);
}

void EvDevPad::PollAxesValues(const device_data& device)
{
	struct input_absinfo absinfo;

	/* Poll all axis */
	for (int i = ABS_X; i < ABS_MAX; i++) {
		absinfo = {};

		if ((ioctl(device.fd, EVIOCGABS(i), &absinfo) >= 0) &&
			device.abs_correct[i].used) {
			absinfo.value = AxisCorrect(device.abs_correct[i], absinfo.value);
		}
		SetAxis(device, i, absinfo.value);
	}
}

void EvDevPad::SetAxis(const device_data& device, int event_code, int value)
{
	int range = range_max(mType);
	int code = device.axis_map[event_code] != (uint8_t)-1 ? device.axis_map[event_code] : -1 /* allow axis to be unmapped */; //event_code;
	//value = AxisCorrect(mAbsCorrect[event_code], value);

	switch (code)
	{
		case 0x80 | JOY_STEERING:
		case ABS_X: mWheelData.steering = device.axis_inverted[0] ? range - NORM(value, range) : NORM(value, range); break;
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
				mWheelData.throttle = device.axis_inverted[1] ? NORM(value, 0xFF) : 0xFF - NORM(value, 0xFF);
		break;
		case 0x80 | JOY_BRAKE:
		case ABS_RZ:
			/*if (mIsGamepad)
				mWheelData.throttle = 0xFF - NORM(value, 0xFF);
			else if (mIsDualAnalog)
				goto treat_me_like_ABS_RY;
			else*/
				mWheelData.brake = device.axis_inverted[2] ? NORM(value, 0xFF) : 0xFF - NORM(value, 0xFF);
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
	fd_set fds;
	int maxfd;

	FD_ZERO(&fds);
	maxfd = -1;

	for (auto& device: mDevices) {
		FD_SET(device.fd, &fds);
		if (maxfd < device.fd) maxfd = device.fd;
	}

	struct timeval timeout;
	timeout.tv_usec = timeout.tv_sec = 0; // 0 - return from select immediately
	int result = select(maxfd+1, &fds, NULL, NULL, &timeout);

	if (result <= 0) {
		return USB_RET_NAK; // If no new data, NAK it
	}

	for (auto& device: mDevices)
	{
		if (!FD_ISSET(device.fd, &fds)) {
			continue;
		}

		const auto& mappings = device.mappings;
		//Non-blocking read sets len to -1 and errno to EAGAIN if no new data
		while((len = read(device.fd, &events, sizeof(events))) > -1)
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
						value = AxisCorrect(device.abs_correct[event.code], event.value);
						/*if (event.code == 0)
							OSDebugOut("Axis: %d, mapped: 0x%02x, val: %d, corrected: %d\n",
								event.code, device.axis_map[event.code] & ~0x80, event.value, value);
						*/
						SetAxis(device, event.code, value);
					}
					break;
					case EV_KEY:
					{
						code = device.btn_map[event.code] != (uint16_t)-1 ? device.btn_map[event.code] : event.code;
						OSDebugOut("%s Button: 0x%02x, mapped: 0x%02x, val: %d\n",
							device.name.c_str(), event.code, device.btn_map[event.code], event.value);

						PS2Buttons button = PAD_BUTTON_COUNT;
						if (code >= (0x8000 | JOY_CROSS) && // user mapped
							code <= (0x8000 | JOY_L3))
						{
							button = (PS2Buttons)(code & ~0x8000);
						}
						else if (code >= BTN_TRIGGER && code < BTN_BASE5) // try to guess
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
									mWheelData.hat_horz = (event.value == 0 ? PAD_HAT_COUNT : PAD_HAT_W);
									break;
								case 0x8000 | JOY_RIGHT:
									mWheelData.hat_horz = (event.value == 0 ? PAD_HAT_COUNT : PAD_HAT_E);
									break;
								case 0x8000 | JOY_UP:
									mWheelData.hat_vert = (event.value == 0 ? PAD_HAT_COUNT : PAD_HAT_N);
									break;
								case 0x8000 | JOY_DOWN:
									mWheelData.hat_vert = (event.value == 0 ? PAD_HAT_COUNT : PAD_HAT_S);
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
								PollAxesValues(device);
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
				OSDebugOut("%s: TokenIn: read error %d\n", APINAME, errno);
				break;
			}
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

	const ff_data *ffdata = (const ff_data *)data;
	bool hires = (mType == WT_DRIVING_FORCE_PRO || mType == WT_DRIVING_FORCE_PRO_1102);
	ParseFFData(ffdata, hires);

	return len;
}

int EvDevPad::Open()
{
	int t;
	std::stringstream name;
	vstring device_list;
	char buf[1024];
	mWheelData = {};

	unsigned long keybit[NBITS(KEY_MAX)];
	unsigned long absbit[NBITS(ABS_MAX)];
	memset(keybit, 0, sizeof(keybit));
	memset(absbit, 0, sizeof(absbit));

	// Setting to unpressed
	mWheelData.steering = 0x3FF >> 1;
	mWheelData.clutch = 0xFF;
	mWheelData.throttle = 0xFF;
	mWheelData.brake = 0xFF;
	mWheelData.hatswitch = 0x8;
	mWheelData.hat_horz = 0x8;
	mWheelData.hat_vert = 0x8;
	//memset(mAxisMap, -1, sizeof(mAxisMap));
	//memset(mBtnMap, -1, sizeof(mBtnMap));

	//mAxisCount = 0;
	//mButtonCount = 0;
	//mHandle = -1;

	std::string evphys, hid_dev;

	std::string joypath;
	if (!LoadSetting(mDevType, mPort, APINAME, N_JOYSTICK, joypath))
	{
		OSDebugOut("Cannot load device setting: %s\n", N_JOYSTICK);
		return 1;
	}

	LoadSetting(mDevType, mPort, APINAME, N_HIDRAW_FF_PT, mUseRawFF);

	if (mUseRawFF) {
		if (joypath.empty() || !file_exists(joypath))
			goto quit;

		int fd = -1;
		if ((fd = open(joypath.c_str(), O_RDWR | O_NONBLOCK)) < 0)
		{
			OSDebugOut("Cannot open device: %s\n", joypath.c_str());
			goto quit;
		}

		memset(buf, 0, sizeof(buf));
		if (ioctl(fd, EVIOCGPHYS(sizeof(buf) - 1), buf) > 0) {
			evphys = buf;
			OSDebugOut("Evdev Phys: %s\n", evphys.c_str());

			int pid, vid;
			if ((mUseRawFF = FindHidraw(evphys, hid_dev, &vid, &pid))) {

				// For safety, only allow Logitech (classic ffb) devices
				if (vid != 0x046D /* Logitech */ /*|| info.bustype != BUS_USB*/
					|| pid == 0xc262 /* G920 hid mode */
					|| pid == 0xc261 /* G920 xbox mode */
				) {
					mUseRawFF = false;
				}

				// check if still using hidraw and run the thread
				if (mUseRawFF && !mWriterThreadIsRunning)
				{
					if (mWriterThread.joinable())
						mWriterThread.join();
					mWriterThread = std::thread(EvDevPad::WriterThread, this);
				}
			}
		} else {
			perror("EVIOCGPHYS failed");
		}
		close(fd);
	}

	EnumerateDevices(device_list);

	for (const auto& it : device_list)
	{
		bool has_mappings = false;
		mDevices.push_back({});

		struct device_data& device = mDevices.back();
		device.name = it.first;

		if ((device.fd = open(it.second.c_str(), O_RDWR | O_NONBLOCK)) < 0)
		{
			OSDebugOut("Cannot open device: %s\n", it.second.c_str());
			continue;
		}

		int ret_abs = ioctl(device.fd, EVIOCGBIT(EV_ABS, sizeof(absbit)), absbit);
		int ret_key = ioctl(device.fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit);
		memset(device.axis_map, 0xFF, sizeof(device.axis_map));
		memset(device.btn_map, 0xFF, sizeof(device.btn_map));

		if ((ret_abs < 0) && (ret_key < 0)) {
			// Probably isn't a evdev joystick
			SysMessage("%s: Getting atleast some of the bits failed: %s\n", APINAME, strerror(errno));
			continue;
		}

		/*unsigned int version;
		if (ioctl(mHandle, EVIOCGVERSION, &version) < 0)
		{
			SysMessage("%s: Get version failed: %s\n", APINAME, strerror(errno));
			return false;
		}*/

		LoadMappings(mDevType, mPort, device.name,
			device.mappings, device.axis_inverted, device.axis_initial);

		// Map hatswitches automatically
		//FIXME has_mappings is gonna ignore hatsw only devices
		for (int i = ABS_HAT0X; i <= ABS_HAT3Y; ++i) {
			device.axis_map[i] = i;
		}

		// SDL2
		for (int i = 0; i < ABS_MAX; ++i) {
			if (test_bit(i, absbit)) {
				struct input_absinfo absinfo;

				if (ioctl(device.fd, EVIOCGABS(i), &absinfo) < 0) {
					continue;
				}

				OSDebugOut("Axis %d absinfo min %d max %d\n", i, absinfo.minimum, absinfo.maximum);

				//device.axis_map[i] = device.axes;

				// convert values into 16 bit range
				CalcAxisCorr(device.abs_correct[i], absinfo);

				//TODO joystick/gamepad is dual analog?
				if (i == ABS_RZ) {
					//absinfo.value = AxisCorrect(mAbsCorrect[i], absinfo.value);
					if (std::abs(absinfo.value) < 200) /* 200 is random, allows for some dead zone */
						device.is_dualanalog = true;
				}

				for (int k = JOY_STEERING; k < JOY_MAPS_COUNT; k++)
				{
					if (i == device.mappings[k]) {
						has_mappings = true;
						device.axis_map[i] = 0x80 | k;
						if (k == JOY_STEERING && !mFFdev)
							mFFdev = new EvdevFF(device.fd);
					}
				}

				++device.axes;
			}
		}

		for(int i = 0; i < ABS_MAX; ++i) {
			if (device.axis_map[i] != (uint8_t)-1 && (device.axis_map[i] & 0x80))
				OSDebugOut("Axis: 0x%02x -> %s\n", i, JoystickMapNames[device.axis_map[i] & ~0x80]);
		}

		for (int i = BTN_JOYSTICK; i < KEY_MAX; ++i) {
			if (test_bit(i, keybit)) {
				//OSDebugOut("Device has button: 0x%x\n", i);
				device.btn_map[i] = -1;//device.buttons;
				++device.buttons;
				if (i == BTN_GAMEPAD) {
					device.is_gamepad = true;
					OSDebugOut("Device is a gamepad\n");
				}
				for (int k = 0; k < JOY_STEERING; k++)
				{
					if (i == device.mappings[k]) {
						has_mappings = true;
						device.btn_map[i] = 0x8000 | k;
						OSDebugOut("Remap button: 0x%x -> %s\n", i, JoystickMapNames[k]);
					}
				}
			}
		}
		for (int i = 0; i < BTN_JOYSTICK; ++i) {
			if (test_bit(i, keybit)) {
				OSDebugOut("Device has button: 0x%x\n", i);
				device.btn_map[i] = -1;//device.buttons;
				++device.buttons;
				for (int k = 0; k < JOY_STEERING; k++)
				{
					if (i == device.mappings[k]) {
						has_mappings = true;
						device.btn_map[i] = 0x8000 | k;
					}
				}
			}
		}
		if (!has_mappings) {
			OSDebugOut("Device %s [%s] has no mappings, discarding\n", device.name.c_str(), it.second.c_str());
			close(device.fd);
			mDevices.pop_back();
		}
	}

	// TODO Instead of single FF instance, create for every device with X-axis???
	// and then switch between them according to which device was used recently
	//mFFdev = new EvdevFF(mHandle);
	return 0;

quit:
	Close();
	return 1;
}

int EvDevPad::Close()
{
	delete mFFdev;
	mFFdev = nullptr;

	if (mHidHandle != -1) {
		uint8_t reset[7] = { 0 };
		reset[0] = 0xF3; //stop forces
		write(mHidHandle, reset, sizeof(reset));
		close(mHidHandle);
	}

	mHidHandle = -1;
	for (auto& it : mDevices) {
		close(it.fd);
		it.fd = -1;
	}
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
		} else { // TODO skip sleep for few while cycles?
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
	OSDebugOut(TEXT("WriterThread exited.\n"));

	pad->mWriterThreadIsRunning = false;
}

}} //namespace
#include "joydev.h"
#include "../../osdebugout.h"
#include <cassert>
#include <sstream>
#include "linux/util.h"

namespace usb_pad { namespace joydev {

using namespace evdev;

#define APINAME "joydev"

#define NORM(x, n) (((uint32_t)(32768 + x) * n)/0xFFFF)
#define NORM2(x, n) (((uint32_t)(32768 + x) * n)/0x7FFF)

void EnumerateDevices(vstring& list)
{
	int fd;
	int res;
	char buf[256];

	std::stringstream str;
	struct dirent* dp;

	DIR* dirp = opendir("/dev/input/");
	if (!dirp) {
		perror("Error opening /dev/input/");
		return;
	}

	while ((dp = readdir(dirp)))
	{
		if (strncmp(dp->d_name, "js", 2) == 0) {
			OSDebugOut("%s\n", dp->d_name);

			str.clear(); str.str("");
			str << "/dev/input/" << dp->d_name;
			std::string path = str.str();
			fd = open(path.c_str(), O_RDONLY|O_NONBLOCK);

			if (fd < 0) {
				perror("Unable to open device");
				continue;
			}

			res = ioctl(fd, JSIOCGNAME(sizeof(buf)), buf);
			if (res < 0)
				perror("JSIOCGNAME");
			else
			{
				OSDebugOut("Joydev device name: %s\n", buf);
				list.push_back(std::make_pair(std::string(buf), path));
			}

			close(fd);
		}
	}
quit:
	closedir(dirp);
}

int JoyDevPad::TokenIn(uint8_t *buf, int buflen)
{
	ssize_t len;
	struct js_event events[32];
	fd_set fds;
	int maxfd;

	int range = range_max(mType);

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
				js_event& event = events[i];
				if ((event.type & ~JS_EVENT_INIT) == JS_EVENT_AXIS)
				{
					OSDebugOut("Axis: %d, mapped: 0x%02x, val: %d\n", event.number, device.axis_map[event.number], event.value);
					switch (device.axis_map[event.number])
					{
						case 0x80 | JOY_STEERING:
						case ABS_X: mWheelData.steering = device.axis_inverted[0] ? range - NORM(event.value, range) : NORM(event.value, range); break;
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
							if (device.is_gamepad)
								mWheelData.brake = 0xFF - NORM(event.value, 0xFF);
							else
								mWheelData.throttle = device.axis_inverted[1] ? NORM(event.value, 0xFF) : 0xFF - NORM(event.value, 0xFF);
						break;
						case 0x80 | JOY_BRAKE:
						case ABS_RZ:
							if (device.is_gamepad)
								mWheelData.throttle = 0xFF - NORM(event.value, 0xFF);
							else if (device.is_dualanalog)
								goto treat_me_like_ABS_RY;
							else
								mWheelData.brake = device.axis_inverted[2] ? NORM(event.value, 0xFF) : 0xFF - NORM(event.value, 0xFF);
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
					OSDebugOut("Button: %d, mapped: 0x%02x, val: %d\n", event.number, device.btn_map[event.number], event.value);
					PS2Buttons button = PAD_BUTTON_COUNT;
					if (device.btn_map[event.number] >= (0x8000 | JOY_CROSS) &&
						device.btn_map[event.number] <= (0x8000 | JOY_L3))
					{
						button = (PS2Buttons)(device.btn_map[event.number] & ~0x8000);
					}

					else if (device.btn_map[event.number] >= BTN_TRIGGER &&
						device.btn_map[event.number] < BTN_BASE5)
					{
						button = (PS2Buttons)(device.btn_map[event.number] - BTN_TRIGGER);
					}
					else
					{
						// Map to xbox360ish controller
						switch (device.btn_map[event.number])
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

			if (len <= 0)
			{
				OSDebugOut(APINAME ": TokenIn: read error %d\n", errno);
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

int JoyDevPad::TokenOut(const uint8_t *data, int len)
{
	const ff_data *ffdata = (const ff_data*)data;
	bool hires = (mType == WT_DRIVING_FORCE_PRO);
	ParseFFData(ffdata, hires);

	return len;
}

int JoyDevPad::Open()
{
	vstring device_list;
	bool has_steering;
	memset(&mWheelData, 0, sizeof(wheel_data_t));

	// Setting to unpressed
	mWheelData.steering = 0x3FF >> 1;
	mWheelData.clutch = 0xFF;
	mWheelData.throttle = 0xFF;
	mWheelData.brake = 0xFF;
	mWheelData.hatswitch = 0x8;
	mWheelData.hat_horz = 0x8;
	mWheelData.hat_vert = 0x8;

	mHandleFF = -1;

	std::string joypath;
	if (!LoadSetting(mDevType, mPort, APINAME, N_JOYSTICK, joypath))
	{
		OSDebugOut("Cannot load joystick setting: %s\n", N_JOYSTICK);
		return 1;
	}

	EnumerateDevices(device_list);

	for (const auto& it : device_list)
	{
		has_steering = false;
		mDevices.push_back({});

		struct device_data& device = mDevices.back();
		device.name = it.first;

		if ((device.fd = open(it.second.c_str(), O_RDWR | O_NONBLOCK)) < 0)
		{
			OSDebugOut("Cannot open device: %s\n", it.second.c_str());
			continue;
		}

		//int flags = fcntl(device.fd, F_GETFL, 0);
		//fcntl(device.fd, F_SETFL, flags | O_NONBLOCK);

		unsigned int version;
		if (ioctl(device.fd, JSIOCGVERSION, &version) < 0)
		{
			SysMessage(APINAME ": Get version failed: %s\n", strerror(errno));
			continue;
		}

		if (version < 0x010000)
		{
			SysMessage(APINAME ": Driver version 0x%X is too old\n", version);
			continue;
		}

		LoadMappings(mDevType, mPort, device.name,
			device.mappings, device.axis_inverted);

		// Axis Mapping
		if (ioctl(device.fd, JSIOCGAXMAP, device.axis_map) < 0)
		{
			SysMessage(APINAME ": Axis mapping failed: %s\n", strerror(errno));
			continue;
		}
		else
		{
			ioctl(device.fd, JSIOCGAXES, &(device.axes));
			for(int i = 0; i < device.axes; ++i)
				OSDebugOut("Axis: %d -> %d\n", i, device.axis_map[i] );

			for (int k = 0; k < device.axes; k++)
				for (int i = JOY_STEERING; i < JOY_MAPS_COUNT; i++)
				{
					if (k == device.mappings[i]) {
						device.axis_map[k] = 0x80 | i;
						if (i == JOY_STEERING)
							has_steering = true;
					}
				}
		}

		// Button Mapping
		if (ioctl(device.fd, JSIOCGBTNMAP, device.btn_map) < 0)
		{
			SysMessage(APINAME ": Button mapping failed: %s\n", strerror(errno));
			continue;
		}
		else
		{
			ioctl(device.fd, JSIOCGBUTTONS, &(device.buttons));
			for(int i = 0; i < device.buttons; ++i)
			{
				OSDebugOut("Button: %d -> %d BTN_[GAMEPAD|SOUTH]: %d\n", i, device.btn_map[i], device.btn_map[i] == BTN_GAMEPAD );
				if (device.btn_map[i] == BTN_GAMEPAD)
					device.is_gamepad = true;
			}

			if (!device.is_gamepad) //TODO Don't remap if gamepad?
			for (int k = 0; k < device.buttons; k++)
				for (int i = 0; i < JOY_STEERING; i++)
				{
					if (k == device.mappings[i])
						device.btn_map[k] = 0x8000 | i;
				}
		}

		std::stringstream event;
		int index = 0;
		const char *tmp = it.second.c_str();
		while(*tmp && !isdigit(*tmp))
			tmp++;

		sscanf(tmp, "%d", &index);
		OSDebugOut("input index: %d of '%s'\n", index, it.second.c_str());

		//TODO kernel limit is 32?
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

		if (!mFFdev && has_steering) {
			if ((mHandleFF = open(event.str().c_str(), /*O_WRONLY*/ O_RDWR)) < 0)
			{
				OSDebugOut(APINAME ": Cannot open '%s'\n", event.str().c_str());
			}
			else
				mFFdev = new evdev::EvdevFF(mHandleFF);
		}
		return 0;
	}

quit:
	Close();
	return 1;
}

int JoyDevPad::Close()
{
	delete mFFdev;
	mFFdev = nullptr;

	if(mHandleFF != -1)
		close(mHandleFF);

	mHandleFF = -1;
	for (auto& it : mDevices) {
		close(it.fd);
		it.fd = -1;
	}
	return 0;
}

REGISTER_PAD(APINAME, JoyDevPad);
#undef APINAME

}} //namespace
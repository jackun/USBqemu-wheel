#include "evdev.h"
#include "osdebugout.h"

#include <chrono>
#include <thread>
#include <stdio.h>
#include <sstream>
#include <gtk/gtk.h>

namespace usb_pad { namespace evdev {

using sys_clock = std::chrono::system_clock;
using ms = std::chrono::milliseconds;

#define EVDEV_DIR "/dev/input/by-id/"

static void PopulateJoysticks(vstring& jsdata)
{
	std::stringstream str;
	struct dirent* dp;

	jsdata.clear();
	jsdata.push_back(std::make_pair("None", ""));
	//jsdata.push_back(std::make_pair("Fake Vendor FakePad", "/dev/null"));

	DIR* dirp = opendir(EVDEV_DIR);
	if(dirp == NULL) {
		fprintf(stderr, "Error opening " EVDEV_DIR ": %s\n", strerror(errno));
		return;
	}

	// Loop over dir entries using readdir
	int len = strlen("event-joystick");
	while((dp = readdir(dirp)) != NULL)
	{
		// Only select names that end in 'event-joystick'
		int devlen = strlen(dp->d_name);
		if(devlen >= len)
		{
			const char* const start = dp->d_name + devlen - len;
			if(strncmp(start, "event-joystick", len) == 0) {
				OSDebugOut("%s%s\n", EVDEV_DIR, dp->d_name);

				str.clear(); str.str("");
				str << EVDEV_DIR << dp->d_name;

				char name[1024];
				if (!GetEvdevName(str.str(), name))
				{
					//XXX though it also could mean that controller is unusable
					jsdata.push_back(std::make_pair(dp->d_name, str.str()));
				}
				else
				{
					jsdata.push_back(std::make_pair(std::string(name) + " (evdev)", str.str()));
				}
			}
		}
	}
	closedir(dirp);
}

static bool PollInput(const std::string &joypath, bool isaxis, int& value, bool& inverted)
{
	int fd;
	ssize_t len;
	input_event event;

	struct axis_correct abs_correct[ABS_MAX];

	if ((fd = open(joypath.c_str(), O_RDONLY | O_NONBLOCK)) < 0)
	{
		OSDebugOut("Cannot open joystick: %s\n", joypath.c_str());
		return false;
	}

	inverted = false;

	// empty event queue
	while ((len = read(fd, &event, sizeof(event))) > 0);

	struct AxisValue { int16_t value; bool initial; };
	AxisValue axisVal[ABS_MAX + 1] = { 0 };

	int t;
	unsigned long absbit[NBITS(ABS_MAX)] = { 0 };

	if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absbit)), absbit) >= 0) {
		for (int i = 0; i < ABS_MAX; ++i) {
			if (test_bit(i, absbit)) {
				struct input_absinfo absinfo;

				if (ioctl(fd, EVIOCGABS(i), &absinfo) < 0) {
					continue;
				}

				OSDebugOut("Axis %d absinfo min %d max %d\n", i, absinfo.minimum, absinfo.maximum);
				//TODO from SDL2, usable here?
				if (absinfo.minimum == absinfo.maximum) {
					abs_correct[i].used = 0;
				} else {
					OSDebugOut("Using axis %d correction for '%s'\n", i, joypath.c_str());
					abs_correct[i].used = 1;
					abs_correct[i].coef[0] =
						(absinfo.maximum + absinfo.minimum) - 2 * absinfo.flat;
					abs_correct[i].coef[1] =
						(absinfo.maximum + absinfo.minimum) + 2 * absinfo.flat;
					t = ((absinfo.maximum - absinfo.minimum) - 4 * absinfo.flat);
					if (t != 0) {
						abs_correct[i].coef[2] = (1 << 28) / t;
					} else {
						abs_correct[i].coef[2] = 0;
					}
				}
			}
		}
	}

	auto last = sys_clock::now();
	//Non-blocking read sets len to -1 and errno to EAGAIN if no new data
	while (true)
	{
		auto dur = std::chrono::duration_cast<ms>(sys_clock::now()-last).count();
		if (dur > 5000) goto error;

		if ((len = read(fd, &event, sizeof(event))) > -1 && (len == sizeof(event)))
		{
			if (isaxis && event.type == EV_ABS)
			{
				auto& val = axisVal[event.code];

				if (!val.initial)
				{
					//val.value = event.value;
					val.value = AxisCorrect(abs_correct[event.code], event.value);
					val.initial = true;
				}
				//else if (std::abs(event.value - val.value) > 1000)
				else
				{
					int ac_val = AxisCorrect(abs_correct[event.code], event.value);
					int diff = ac_val - val.value;
					OSDebugOut("Axis %d value difference: %d, corrected %d raw %d\n", event.code, diff, ac_val, event.value);
					if (std::abs(diff) > 2047)
					{
						value = event.code;
						inverted = (diff < 0);
						break;
					}
				}
			}
			else if (!isaxis && event.type == EV_KEY)
			{
				if (event.value)
				{
					value = event.code;
					break;
				}
			}
		}
		else if (errno != EAGAIN)
		{
			OSDebugOut("PollInput: read error %d\n", errno);
			goto error;
		}
		else
		{
			while (gtk_events_pending ())
				gtk_main_iteration_do (FALSE);
			std::this_thread::sleep_for(ms(1));
		}
	}

	close(fd);
	return true;
error:
	close(fd);
	return false;
}

int EvDevPad::Configure(int port, void *data)
{
	ApiCallbacks apicbs {PopulateJoysticks, PollInput};
	int ret = GtkPadConfigure(port, "Evdev Settings", "evdev", GTK_WINDOW (data), apicbs);
	return ret;
}

#undef APINAME
#undef EVDEV_DIR
}} //namespace
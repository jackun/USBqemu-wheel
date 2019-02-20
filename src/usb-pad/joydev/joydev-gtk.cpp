#include "joydev.h"
#include "../../osdebugout.h"

#include <chrono>
#include <thread>
#include <stdio.h>
#include <sstream>
#include <gtk/gtk.h>

namespace usb_pad { namespace joydev {

using sys_clock = std::chrono::system_clock;
using ms = std::chrono::milliseconds;

#define JOYTYPE "joytype"
#define CFG "cfg"

static void PopulateJoysticks(vstring& jsdata)
{
	jsdata.clear();
	jsdata.push_back(std::make_pair("None", ""));
	//jsdata.push_back(std::make_pair("Fake Vendor FakePad", "/dev/null"));

	std::stringstream str;
	// up to 32?
	for (int count = 0; count < MAX_JOYS; count++)
	{
		str.clear();
		str.str(""); str << "/dev/input/js" << count;
		/* Check if joystick device exists */
		if (file_exists(str.str()))
		{
			char name[1024];
			if (!GetJoystickName(str.str(), name))
			{
				//XXX though it also could mean that controller is unusable
				jsdata.push_back(std::make_pair(str.str(), str.str()));
			}
			else
			{
				jsdata.push_back(std::make_pair(std::string(name), str.str()));
			}
		}
	}
}

static bool PollInput(const std::string &joypath, bool isaxis, int& value, bool& inverted)
{
	int fd;
	ssize_t len;
	struct js_event event;

	if ((fd = open(joypath.c_str(), O_RDONLY | O_NONBLOCK)) < 0)
	{
		OSDebugOut("Cannot open joystick: %s\n", joypath.c_str());
		return false;
	}

	inverted = false;

	// empty event queue
	while ((len = read(fd, &event, sizeof(event))) > 0);

	struct axis_value { int16_t value; bool initial; };
	axis_value axisVal[ABS_MAX + 1] = { 0 };

	auto last = sys_clock::now();
	//Non-blocking read sets len to -1 and errno to EAGAIN if no new data
	while (true)
	{
		auto dur = std::chrono::duration_cast<ms>(sys_clock::now()-last).count();
		if (dur > 5000) goto error;

		if ((len = read(fd, &event, sizeof(event))) > -1 && (len == sizeof(event)))
		{
			if (isaxis && event.type == JS_EVENT_AXIS)
			{
				auto& val = axisVal[event.number];

				if (!val.initial)
				{
					val.value = event.value;
					val.initial = true;
				}
				else
				{
					int diff = event.value - val.value;
					OSDebugOut("Axis %d value: %d, difference: %d\n", event.number, event.value, diff);
					if (std::abs(diff) > 2047) {
						value = event.number;
						inverted = (diff < 0);
						break;
					}
				}
			}
			else if (!isaxis && event.type == JS_EVENT_BUTTON)
			{
				if (event.value)
				{
					value = event.number;
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

int JoyDevPad::Configure(int port, const char* dev_type, void *data)
{
	evdev::ApiCallbacks apicbs {PopulateJoysticks, PollInput};
	int ret = evdev::GtkPadConfigure(port, dev_type, "Joydev Settings", "joydev", GTK_WINDOW (data), apicbs);
	return ret;
}

}} //namespace
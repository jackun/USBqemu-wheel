#include "usb-pad.h"
#include "../USB.h"

#include <linux/joystick.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <math.h>

#if _DEBUG
#define Dbg(...) fprintf(stderr, __VA_ARGS__)
#else
#define Dbg(...)
#endif

struct LinuxPADState
{
	PADState padState;
	int fd;
	int fdFF;
	struct ff_effect effect;
	uint8_t  axismap[ABS_MAX + 1];
	uint16_t btnmap[KEY_MAX + 1];
	int axis_count;
	int button_count;
};

extern bool file_exists(std::string filename);
extern bool dir_exists(std::string filename);
static struct wheel_data_t wheel_data;
static struct ff_data ffdata;
static bool sendCrap = false;

static void SetConstantForce(LinuxPADState *s, int force);
static void SetSpringForce(LinuxPADState *s, int force);
static void SetAutoCenter(LinuxPADState *s, int value);
static void SetGain(LinuxPADState *s, int gain);

//int axis_count = 0, button_count = 0;

//uint8_t  axismap[2][ABS_MAX + 1] = {{(uint8_t)-1}};
//uint16_t btnmap[2][KEY_MAX + 1] = {{(uint16_t)-1}}; // - BTN_MISC + 1];

#define NORM(x, n) (((uint32_t)(32767 + x) * n)/0xFFFE)

static inline int range_max(uint32_t idx)
{
	int type = conf.WheelType[idx];
	if(type == WT_DRIVING_FORCE_PRO)
		return 0x3FFF;
	return 0x3FF;
}

static int usb_pad_poll(PADState *ps, uint8_t *buf, int buflen)
{
	LinuxPADState *s = (LinuxPADState*)ps;
	uint8_t idx = 1 - s->padState.port;
	ssize_t len;
	if(idx > 1) return 0; //invalid port

	int range = range_max(idx);

	if(sendCrap)
	{
		// Setting to unpressed
		memset(&wheel_data, 0, sizeof(wheel_data_t));
		wheel_data.axis_x = range >> 1;
		wheel_data.axis_y = 0xFF;
		wheel_data.axis_z = 0xFF;
		wheel_data.axis_rz = 0xFF;
		wheel_data.hatswitch = 0x8;
		pad_copy_data(idx, buf, wheel_data);
		sendCrap = false;
		return len;
	}

	// Events are raised if state changes, so keep old generic_data intact
	//memset(&wheel_data, 0, sizeof(wheel_data_t));

	struct js_event event;
	int wtype = conf.WheelType[idx];

	//Non-blocking read sets len to -1 and errno to EAGAIN if no new data
	//TODO what happens when emulator is paused?
	while((len = read(s->fd, &event, sizeof(event))) > -1)
	{
		//Dbg("Read js len: %d %d\n", len, errno);
		if (len == sizeof(event))
		{ // ok
			if (event.type & JS_EVENT_AXIS)
			{
				//Dbg("Axis: %d, %d\n", event.number, event.value);
				switch(s->axismap[event.number])
				{
					case ABS_X: wheel_data.axis_x = NORM(event.value, range); break;
					case ABS_Y: wheel_data.axis_y = NORM(event.value, 0xFF); break;
					case ABS_Z: wheel_data.axis_z = NORM(event.value, 0xFF); break;
					//case ABS_RX: wheel_data.axis_rx = NORM(event.value, 0xFF); break;
					//case ABS_RY: wheel_data.axis_ry = NORM(event.value, 0xFF); break;
					case ABS_RZ: wheel_data.axis_rz = NORM(event.value, 0xFF); break;

					//FIXME hatswitch mapping
					//TODO Ignoring diagonal directions
					case ABS_HAT0X:
					case ABS_HAT1X:
						if(event.value < 0 ) //left usually
							wheel_data.hatswitch = PAD_HAT_W;
						else if(event.value > 0 ) //right
							wheel_data.hatswitch = PAD_HAT_E;
						else
							wheel_data.hatswitch = PAD_HAT_COUNT;
					break;
					case ABS_HAT0Y:
					case ABS_HAT1Y:
						if(event.value < 0 ) //up usually
							wheel_data.hatswitch = PAD_HAT_N;
						else if(event.value > 0 ) //down
							wheel_data.hatswitch = PAD_HAT_S;
						else
							wheel_data.hatswitch = PAD_HAT_COUNT;
					break;
					default: break;
				}
			}
			else if (event.type & JS_EVENT_BUTTON)
			{
				//Dbg("Button: %d, %d\n", event.number, event.value);
				//TODO can have 12 bits for buttons?
				if(event.number < 10)
				{
					//FIXME bit juggling
					if(event.value)
						wheel_data.buttons |= 1 << event.number; //on
					else
						wheel_data.buttons &= ~(1 << event.number); //off
				}
			}
		}
		else
		{
			Dbg("usb_pad_poll: unknown read error\n");
			break;
		}
	}

	//Dbg("call pad_copy_data\n");
	pad_copy_data(idx, buf, wheel_data);
	return buflen;
}

static bool find_pad(LinuxPADState *s)
{
	uint8_t idx = 1 - s->padState.port;
	if(idx > 1) return false;

	memset(&wheel_data, 0, sizeof(wheel_data_t));

	// Setting to unpressed
	wheel_data.axis_x = 0x3FF >> 1;
	wheel_data.axis_y = 0xFF;
	wheel_data.axis_z = 0xFF;
	wheel_data.axis_rz = 0xFF;
	memset(s->axismap, -1, sizeof(s->axismap));
	memset(s->btnmap, -1, sizeof(s->btnmap));

	s->axis_count = 0;
	s->button_count = 0;
	s->fd = -1;
	s->fdFF = -1;
	s->effect.id = -1;

	if(!player_joys[idx].empty() && file_exists(player_joys[idx]))
	{
		if ((s->fd = open(player_joys[idx].c_str(), O_RDONLY | O_NONBLOCK)) < 0)
		{
			Dbg("Cannot open player %d's controller: %s\n", idx+1, player_joys[idx].c_str());
		}
		else
		{
			//int flags = fcntl(s->fd, F_GETFL, 0);
			//fcntl(s->fd, F_SETFL, flags | O_NONBLOCK);

			// Axis Mapping
			if (ioctl(s->fd, JSIOCGAXMAP, s->axismap) < 0)
			{
				Dbg("Axis mapping: %s\n", strerror(errno));
			}
			else
			{
				ioctl(s->fd, JSIOCGAXES, &s->axis_count);
				for(int i = 0; i < s->axis_count; ++i)
					Dbg("Axis: %d -> %d\n", i, s->axismap[i] );
			}

			// Button Mapping
			if (ioctl(s->fd, JSIOCGBTNMAP, s->btnmap) < 0)
			{
				Dbg("Button mapping: %s\n", strerror(errno));
			}
			else
			{

				ioctl(s->fd, JSIOCGBUTTONS, &s->button_count);
				for(int i = 0; i < s->button_count; ++i)
					Dbg("Button: %d -> %d \n", i, s->btnmap[i] );
			}

			std::stringstream event;
			int index = 0;
			const char *tmp = player_joys[idx].c_str();
			while(*tmp && !isdigit(*tmp))
				tmp++;

			sscanf(tmp, "%d", &index);
			Dbg("input index: %d of '%s'\n", index, player_joys[idx].c_str());

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
			if ((s->fdFF = open(event.str().c_str(), O_WRONLY)) < 0)
			{
				Dbg("Cannot open '%s'\n", event.str().c_str());
			}
			else
			{
				s->effect.type = FF_CONSTANT;
				s->effect.id = -1;
				s->effect.u.constant.level = 0;	/* Strength : 25 % */
				s->effect.direction = 0x4000;
				s->effect.u.constant.envelope.attack_length = 0;//0x100;
				s->effect.u.constant.envelope.attack_level = 0;
				s->effect.u.constant.envelope.fade_length = 0;//0x100;
				s->effect.u.constant.envelope.fade_level = 0;
				s->effect.trigger.button = 0;
				s->effect.trigger.interval = 0;
				s->effect.replay.length = 0X7FFFUL;  /* mseconds */
				s->effect.replay.delay = 0;

				if (ioctl(s->fdFF, EVIOCSFF, &(s->effect)) < 0) {
					Dbg("Failed to upload effect to '%s'\n", event.str().c_str());
				}

				SetGain(s, 75);
				SetAutoCenter(s, 0);
			}
			return true;
		}
	}

	return false;
}

static void SetConstantForce(LinuxPADState *s, int force)
{
	struct input_event play;
	play.type = EV_FF;
	play.code = s->effect.id;
	play.value = 0;
	//if (write(s->fdFF, (const void*) &play, sizeof(play)) == -1) {
	//    Dbg("stop effect failed\n");
	//}

	Dbg("Force: %d\n", force);
	//s->effect.type = FF_CONSTANT;
	//s->effect.id = -1;
	//s->effect.u.constant.level = 0x8000;	/* Strength : 0x2000 == 25 % */
	//s->effect.direction = (0xFFFF * (force)) / 255;
	int y = 0, x = 0;
	float magnitude = -(127-force) * 78.4803149606299;
	s->effect.u.constant.level = (MAX(MIN(magnitude, 10000), -10000) / 10) * 32;
	s->effect.direction = 0x4000; //(int)((3 * M_PI / 2 - atan2(y, x)) * -0x7FFF / M_PI);
	Dbg("dir: %d lvl: %d\n", s->effect.direction, s->effect.u.constant.level);

//	s->effect.u.constant.envelope.attack_length = 0;
//	s->effect.u.constant.envelope.attack_level = 0;
//	s->effect.u.constant.envelope.fade_length = 0;
//	s->effect.u.constant.envelope.fade_level = 0;
//	s->effect.trigger.button = 0;
//	s->effect.trigger.interval = 0;
//	s->effect.replay.length = 0xFFFF;  /* INFINITE mseconds */
//	s->effect.replay.delay = 0;

	if (ioctl(s->fdFF, EVIOCSFF, &(s->effect)) < 0) {
		Dbg("Failed to upload effect\n");
	}


	//play.type = EV_FF;
	play.code = s->effect.id;
	play.value = 1;
	if (write(s->fdFF, (const void*) &play, sizeof(play)) == -1) {
		Dbg("play effect failed\n");
	}

}

static void SetSpringForce(LinuxPADState *s, int force)
{

}

static void SetAutoCenter(LinuxPADState *s, int value)
{
	struct input_event ie;

	ie.type = EV_FF;
	ie.code = FF_AUTOCENTER;
	ie.value = value * 0xFFFFUL / 100;

	if (write(s->fdFF, &ie, sizeof(ie)) == -1)
		Dbg("Failed to set autocenter\n");
}

static void SetGain(LinuxPADState *s, int gain /* between 0 and 100 */)
{
	struct input_event ie;	/* structure used to communicate with the driver */

	ie.type = EV_FF;
	ie.code = FF_GAIN;
	ie.value = 0xFFFFUL * gain / 100;

	if (write(s->fdFF, &ie, sizeof(ie)) == -1)
		Dbg("Failed to set gain\n");
}

static int token_out(PADState *ps, uint8_t *data, int len)
{
	LinuxPADState *s = (LinuxPADState*)ps;
	uint8_t idx = 1 - s->padState.port;
	if(idx>1) return 0;

	memcpy(&ffdata,data, sizeof(ffdata));
	//if(idx!=0)return 0;
	Dbg("FFB 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x0%4X\n", ffdata.reportid, ffdata.index, ffdata.data1, ffdata.data2, *(int*)((char*)&ffdata + 4) );

	switch(ffdata.reportid)
	{
		case 0xF8:
			//TODO needed?
			if(ffdata.index == 5)
				sendCrap = true;
			//TODO guess-work
			//if(ffdata.index == 3)
			//	SetAutoCenter(s, 100);
		break;
		case 9:
			{
				//not handled
			}
			break;
		case 0x13:
			//some games issue this command on pause
			//if(ffdata.reportid == 19 && ffdata.data2 == 0)break;
			if(ffdata.index == 0x8)
				SetConstantForce(s, 127); //data1 looks like previous force sent with reportid 0x11
			//TODO unset spring
			else if(ffdata.index == 3)
				SetSpringForce(s, 127);

			//Dbg("FFB 0x%X, 0x%X, 0x%X\n", ffdata.reportid, ffdata.index, ffdata.data1);
			break;
		case 0x11://constant force
			{
				//handle calibration commands
				//if(!calibrating)
				{
					SetConstantForce(s, ffdata.data1);
				}
			}
			break;
		case 0x21:
			if(ffdata.index == 0xB)
			{
				//if(!calibrating)
				{
					//TODO guess-work
					SetAutoCenter(s, 100);
					//SetSpringForce(s, ffdata.data1); //spring is broken?
				}
				break;
			}
			//drop through
		case 0xFE://autocenter?
		case 0xFF://autocenter?
		case 0xF4://autocenter?
			//{
			//	SetAutoCenter(s, 0);
			//}
			//break;
		case 0xF5://autocenter?
			{
				SetAutoCenter(s, 0);
				//just release force
				SetConstantForce(s, 127);
			}
			break;
		case 0xF1:
			//DF/GTF and GT3
			//if(!calibrating)
			{
				SetConstantForce(s, ffdata.pad1);
			}
			break;
		case 0xF3://initialize
			{
				//SetAutoCenter(s, 0xFFFFUL);
			}
			break;
	}

	return len;
}

static int open(USBDevice *dev)
{
	bool ret = find_pad((LinuxPADState*)dev);
	return 0;
}

static void close(USBDevice *dev)
{
	if(!dev) return;

	LinuxPADState *s = (LinuxPADState*)dev;
	uint8_t idx = 1 - s->padState.port;
	if(idx>1) return;

	if(s->fd != -1)
		close(s->fd);

	if(s->fdFF != -1)
	{
		if (s->effect.id != -1 && ioctl(s->fdFF, EVIOCRMFF, s->effect.id) == -1)
		{
			Dbg("Failed to unload FF effect.\n");
		}
		close(s->fdFF);
	}


	s->fd = -1;
	s->fdFF = -1;
}

static void destroy_pad(USBDevice *dev)
{
	if(!dev) return;
	close(dev);
	free(dev);
}

PADState* get_new_padstate()
{
	auto s = (LinuxPADState*)qemu_mallocz(sizeof(LinuxPADState));


	s->padState.dev.open = open;
	s->padState.dev.close = close;

	s->padState.destroy_pad = destroy_pad;
	s->padState.token_out = token_out;
	s->padState.usb_pad_poll = usb_pad_poll;
	return (PADState*)s;
}

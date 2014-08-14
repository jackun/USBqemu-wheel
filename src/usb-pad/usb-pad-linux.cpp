#include "usb-pad.h"
#include "../USB.h"

#include <linux/joystick.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

extern bool file_exists(std::string filename);
extern bool dir_exists(std::string filename);
static struct wheel_data_t	wheel_data;

int fd[2] = {-1};
int axis_count = 0, button_count = 0;

uint8_t  axismap[2][ABS_MAX + 1] = {{-1}};
uint16_t btnmap[2][KEY_MAX + 1] = {{-1}}; // - BTN_MISC + 1];

#define NORM(x, n) (((uint32_t)(32767 + x) * n)/0xFFFE)

int usb_pad_poll(PADState *s, uint8_t *buf, int buflen)
{
	uint8_t idx = 1 - s->port;
	ssize_t len;
	if(idx > 1) return 0; //invalid port

	// Events are raised if state changes, so keep old generic_data intact
	//memset(&wheel_data, 0, sizeof(wheel_data_t));

	struct js_event event;

	//Non-blocking read sets len to -1 and errno to EAGAIN if no new data
	//TODO what happens when emulator is paused?
	while((len = read(fd[idx], &event, sizeof(event))) > -1)
	{
		if (len == sizeof(event))
		{ // ok
			if (event.type & JS_EVENT_AXIS)
			{
				//fprintf(stderr, "Axis: %d, %d\n", event.number, event.value);
				switch(axismap[idx][event.number])
				{
					case ABS_X: wheel_data.axis_x = NORM(event.value, 0x3FF); break;
					case ABS_Y: wheel_data.axis_y = NORM(event.value, 0xFF); break;
					case ABS_Z: wheel_data.axis_z = NORM(event.value, 0xFF); break;
					//case ABS_RX: wheel_data.axis_rx = NORM(event.value, 0xFF); break;
					//case ABS_RY: wheel_data.axis_ry = NORM(event.value, 0xFF); break;
					case ABS_RZ: wheel_data.axis_rz = NORM(event.value, 0xFF); break;

					//FIXME hatswitch mapping
					case ABS_HAT0X:
					case ABS_HAT1X:
						wheel_data.hatswitch &= ~0x03;
						if(event.value < 0 ) //left usually
							wheel_data.hatswitch |= 1 << 0;
						else if(event.value > 0 ) //right
							wheel_data.hatswitch |= 1 << 1;
					break;
					case ABS_HAT0Y:
					case ABS_HAT1Y:
						wheel_data.hatswitch &= ~0x0C;
						if(event.value < 0 ) //up usually
							wheel_data.hatswitch |= 1 << 2;
						else if(event.value > 0 ) //down
							wheel_data.hatswitch |= 1 << 3;
					break;
					default: break;
				}
			}
			else if (event.type & JS_EVENT_BUTTON)
			{
				//fprintf(stderr, "Button: %d, %d\n", event.number, event.value);
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
			fprintf(stderr, "usb_pad_poll: unknown read error\n");
		}
	}

	pad_copy_data(idx, buf, wheel_data);
	return buflen;
}

PADState* get_new_padstate()
{
	return (PADState*)qemu_mallocz(sizeof(PADState));
}

bool find_pad(PADState *s)
{
	uint8_t idx = 1 - s->port;
	if(idx > 1) return false;

	memset(&wheel_data, 0, sizeof(wheel_data_t));

	// Setting to unpressed
	wheel_data.axis_x = 0x3FF >> 1;
	wheel_data.axis_y = 0xFF;
	wheel_data.axis_z = 0xFF;
	wheel_data.axis_rz = 0xFF;

	if(!player_joys[idx].empty() && file_exists(player_joys[idx]))
	{
		if ((fd[idx] = open(player_joys[idx].c_str(), O_RDONLY)) < 0)
		{
			fprintf(stderr, "Cannot open player %d's controller: %s\n", idx+1, player_joys[idx].c_str());
		}
		else
		{
			// Axis Mapping
			if (ioctl(fd[idx], JSIOCGAXMAP, axismap[idx]) < 0)
			{
				fprintf(stderr, "Axis mapping: %s\n", strerror(errno));
			}
			else
			{
				//for(int i = 0; i < num_axis; ++i)
				//	printf("Axis: %d -> %d\n", i, (int)axismap[i] );
				// Button Mapping
				if (ioctl(fd[idx], JSIOCGBTNMAP, btnmap[idx]) < 0)
				{
					fprintf(stderr, "Button mapping: %s\n", strerror(errno));
				}
				else
				{
					//for(int i = 0; i < num_button; ++i)
					//	printf("Button: %d -> %d \n", i, (int)btnmap[i] );
				}

				//user could have a wheel with no buttons
				return true;
			}

		}
	}

	return false;
}

int token_out(PADState *s, uint8_t *data, int len)
{
	return USB_RET_STALL;
}

void destroy_pad(PADState *s)
{
	if(!s) return;

	uint8_t idx = 1 - s->port;
	if(idx>1) return;

	if(fd[idx] != -1)
		close(fd[idx]);

	fd[idx] = -1;
}

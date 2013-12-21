#ifndef USBCONFIG_H
#define USBCONFIG_H

#include <string>

//Generic wheel buttons, may need fixing
enum PS2Buttons : uint32_t {
	PAD_CROSS = 0, PAD_SQUARE, PAD_CIRCLE, PAD_TRIANGLE, 
	PAD_L1, PAD_L2, PAD_R1, PAD_R2, PAD_SELECT, PAD_START,
	PAD_BUTTON_COUNT
};

enum PS2Axis : uint32_t {
	PAD_AXIS_X,
	PAD_AXIS_Y,
	PAD_AXIS_Z,
	PAD_AXIS_RZ,
	PAD_AXIS_COUNT
};

//Hardcoded Logitech MOMO racing wheel, idea is that gamepad/wheel would be selectable instead
#define PAD_VID			0x046D
#define PAD_PID			0xCA03
#define DF_PID			0xC294 //generic PID
#define DFP_PID			0xC298
#define MAX_BUTTONS		32
#define MAX_AXES		6 //random 6: wheel, clutch, brake, accel + 2
#define MAX_JOYS		16

#define PLAYER_TWO_PORT 0
#define PLAYER_ONE_PORT 1
#define USB_PORT PLAYER_ONE_PORT

typedef struct PADState {
	USBDevice	dev;
	uint8_t		port;
	bool		doPassthrough;// = false; //Mainly for Win32 Driving Force Pro passthrough
	PS2Buttons	btnsmap[MAX_BUTTONS];
	PS2Axis		axesmap[MAX_AXES];

} PADState;

extern std::string player_joys[2]; //two players
extern bool has_rumble[2];

void LoadMappings(int ply, int vid, int pid, PS2Buttons *maps, PS2Axis *axes);
void SaveMappings(int ply, int vid, int pid, PS2Buttons *maps, PS2Axis *axes);
#endif

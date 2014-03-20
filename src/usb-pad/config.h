#ifndef USBPADCONFIG_H
#define USBPADCONFIG_H

#include <vector>
#include <string>

#define ARRAY_SIZE(x)	(sizeof(x) / sizeof((x)[0]))
#define CHECK(exp)		{ if(!(exp)) goto Error; }
#define SAFE_FREE(p)	{ if(p) { free(p); (p) = NULL; } }

//L3/R3 for newer wheels
//enum PS2Buttons : uint32_t {
//	PAD_CROSS = 0, PAD_SQUARE, PAD_CIRCLE, PAD_TRIANGLE, 
//	PAD_L1, PAD_L2, PAD_R1, PAD_R2,
//	PAD_SELECT, PAD_START,
//	PAD_L3, PAD_R3, //order
//	PAD_BUTTON_COUNT
//};

//???
//enum DFButtons : uint32_t {
//	PAD_CROSS = 0, PAD_SQUARE, PAD_CIRCLE, PAD_TRIANGLE, 
//	PAD_R2, 
//	PAD_L2,
//	PAD_R1, 
//	PAD_L1,
//	PAD_SELECT, PAD_START,
//	PAD_BUTTON_COUNT
//};

//DF Pro buttons (?)
//Based on Tokyo Xtreme Racer Drift 2
//GT4 flips R1/L1 with R2/L2 with DF wheel type
enum PS2Buttons : uint32_t {
	PAD_CROSS = 0, PAD_SQUARE, PAD_CIRCLE, PAD_TRIANGLE, 
	PAD_R1, 
	PAD_L1, 
	PAD_R2, 
	PAD_L2,
	PAD_SELECT, PAD_START,
	PAD_R3, PAD_L3, //order
	PAD_BUTTON_COUNT
};

enum PS2Axis : uint32_t {
	PAD_AXIS_X,
	PAD_AXIS_Y,
	PAD_AXIS_Z,
	PAD_AXIS_RZ,
	PAD_AXIS_HAT,//Treat as axis for mapping purposes
	PAD_AXIS_COUNT
};

//Ugh
enum PS2HatSwitch {
	PAD_HAT_N = 0,
	PAD_HAT_NE,
	PAD_HAT_E,
	PAD_HAT_SE,
	PAD_HAT_S,
	PAD_HAT_SW,
	PAD_HAT_W,
	PAD_HAT_NW,
	PAD_HAT_COUNT
};

//y u no bitmap, ugh
static int hats7to4 [] = {PAD_HAT_N, PAD_HAT_E, PAD_HAT_S, PAD_HAT_W};

enum PS2WheelTypes {
	WT_GENERIC, // DF or any other LG wheel in non-native mode
	WT_DRIVING_FORCE_PRO, //LPRC-11000? DF GT can be downgraded to Pro (?)
};

//Hardcoded Logitech MOMO racing wheel, idea is that gamepad/wheel would be selectable instead
#define PAD_VID			0x046D
#define PAD_PID			0xCA03 //black MOMO
#define GENERIC_PID		0xC294 //actually Driving Force aka PID that most logitech wheels initially report
#define DF_PID			0xC294
#define DFP_PID			0xC298 //SELECT + R3 + RIGHT SHIFT PADDLE (R1) ???
#define DFGT_PID		0xC29A
#define MAX_BUTTONS		32
#define MAX_AXES		7 //random 7: axes + hatswitch
#define MAX_JOYS		16

#define PLAYER_TWO_PORT 0
#define PLAYER_ONE_PORT 1
#define USB_PORT PLAYER_ONE_PORT

typedef struct PADState {
	USBDevice	dev;

	int (*usb_pad_poll)(PADState *ps, uint8_t *buf, int len);
	int (*token_out)(PADState *ps, uint8_t *data, int len);
	void (*destroy_pad)(USBDevice *dev);

	uint8_t		port;
	int			initStage;
	//Config instead?
	bool		doPassthrough;// = false; //Mainly for Win32 Driving Force Pro passthrough
} PADState;

// hold intermediate wheel data
struct wheel_data_t
{
	int32_t axis_x;
	uint32_t buttons;
	uint32_t  hatswitch;

	int32_t axis_y;
	int32_t axis_z;
	int32_t axis_rz;
};

extern std::string player_joys[2]; //two players
extern bool has_rumble[2];
#endif

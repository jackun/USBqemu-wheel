#ifndef USBPADCONFIG_H
#define USBPADCONFIG_H

#include <vector>
#include <string>

//L3/R3 for newer wheels
enum PS2Buttons : uint32_t {
	PAD_CROSS = 0, PAD_SQUARE, PAD_CIRCLE, PAD_TRIANGLE, 
	PAD_L1, PAD_L2, PAD_R1, PAD_R2,
	PAD_SELECT, PAD_START,
	PAD_L3, PAD_R3, //order
	PAD_BUTTON_COUNT
};

enum PS2Axis : uint32_t {
	PAD_AXIS_X,
	PAD_AXIS_Y,
	PAD_AXIS_Z,
	PAD_AXIS_RZ,
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
	WT_GENERIC, // 'dumb' wheel
	WT_DRIVING_FORCE, //has the usual buttons instead of X/Y/A/B on GT Force
	WT_DRIVING_FORCE_PRO, //LPRC-11000? DF GT can be downgraded to Pro (?)
};

//Hardcoded Logitech MOMO racing wheel, idea is that gamepad/wheel would be selectable instead
#define PAD_VID			0x046D
#define PAD_PID			0xCA03 //black MOMO
#define GENERIC_PID		0xC294 //generic PID
#define DF_PID			0xC296 //?? wingman fgp 0xC283
#define DFP_PID			0xC298 //SELECT + R3 + RIGHT SHIFT PADDLE (R1) ???
#define DFGT_PID		0xC29A
#define MAX_BUTTONS		32
#define MAX_AXES		6 //random 6: wheel, clutch, brake, accel + 2
#define MAX_JOYS		16

#define PLAYER_TWO_PORT 0
#define PLAYER_ONE_PORT 1
#define USB_PORT PLAYER_ONE_PORT

typedef struct PADState {
	USBDevice	dev;
	uint8_t		port;
	int			initStage;
	bool		doPassthrough;// = false; //Mainly for Win32 Driving Force Pro passthrough
} PADState;

// convert momo to 'generic' wheel aka 0xC294
struct wheel_data_t
{
	int32_t axis_x;
	uint32_t buttons;
	int32_t  hatswitch;

	int8_t axis_y;
	int8_t axis_z;
	int8_t axis_rz;
};

extern std::string player_joys[2]; //two players
extern bool has_rumble[2];

//Maybe getting too convoluted
//Check for which player(s) the mapping is for
//Using MSB (right? :P) to indicate if valid mapping
#define PLY_IS_MAPPED(p, x) (x & (0x8000 << (16*p)))
// Add owning player bits to mapping
#define PLY_SET_MAPPED(p, x) (((x & 0x7FFF) | 0x8000) << (16*p))
#define PLY_UNSET_MAPPED(p, x) (x & ~(0xFFFF << (16*p)))
#define PLY_GET_VALUE(p, x) ((x >> (16*p)) & 0x7FFF)

//Joystick: btnMap/axisMap/hatMap[physical button] = ps2 button
//Keyboard: btnMap/axisMap/hatMap[ps2 button] = vkey
struct Mappings {
	uint32_t	btnMap[MAX_BUTTONS];
	uint32_t	axisMap[MAX_AXES];
	uint32_t	hatMap[8];
	wheel_data_t data[2];
	std::string devName;
#if _WIN32
	std::string hidPath;
#endif
};

typedef std::vector<Mappings*> MapVector;
static MapVector mapVector;

void LoadMappings(MapVector *maps);
void SaveMappings(MapVector *maps);

#endif

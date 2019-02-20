#ifndef RAWCONFIG_H
#define RAWCONFIG_H

#include "../usb-pad.h"
#include "shared/rawinput.h"

namespace usb_pad { namespace raw {

#define APINAME "rawinput"

/*
Layout:
	0x8000 bit means it is a valid mapping,
	where value is PS2 button/axis and 
	array (Mappings::btnMap etc.) index is physical button/axis
	(reversed for keyboard mappings).
	[31..16] bits player 2 mapping
	[15..0]  bits player 1 mapping
*/
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
	std::wstring devName;
#if _WIN32
	std::wstring hidPath;
#endif
};

struct RawDlgConfig
{
	int port;
	const char *dev_type;
	std::wstring player_joys[2];
	bool pt[2];
	RawDlgConfig(int p, const char *dev_type_) : port(p), dev_type(dev_type_) {}
};

typedef std::vector<Mappings> MapVector;
static MapVector mapVector;

void LoadMappings(const char *dev_type, MapVector& maps);
void SaveMappings(const char *dev_type, MapVector& maps);

}} //namespace
#endif

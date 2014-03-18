#ifndef RAWCONFIG_H
#define RAWCONFIG_H

#include "../config.h"
#include <setupapi.h>
extern "C" {
	#include "../../ddk/hidsdi.h"
}

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
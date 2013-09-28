#ifndef USBCONFIG_H
#define USBCONFIG_H

#include <string>

#if _WIN32
#include <setupapi.h>
extern "C"{
	#include "../ddk/hidsdi.h"
}
#endif

//Hardcoded Logitech MOMO racing wheel, idea is that gamepad/wheel would be selectable instead
#define PAD_VID			0x046D
#define PAD_PID			0xCA03
#define DF_PID			0xC294 //generic PID
#define DFP_PID			0xC298
#define MAX_BUTTONS		128
#define MAX_JOYS		16

#define PLAYER_TWO_PORT 0
#define PLAYER_ONE_PORT 1
#define USB_PORT PLAYER_ONE_PORT

typedef struct PADState {
	USBDevice dev;
	uint8_t port;
	bool doPassthrough;// = false;

#if _WIN32
	HIDP_CAPS caps;
	HIDD_ATTRIBUTES attr;
	PHIDP_PREPARSED_DATA pPreparsedData;
	USAGE usage[MAX_BUTTONS];
	//ULONG value;// = 0;
	USHORT numberOfButtons;// = 0;
	USHORT numberOfValues;// = 0;
	HANDLE usbHandle;// = (HANDLE)-1;
	//HANDLE readData;// = (HANDLE)-1;
	OVERLAPPED ovl;
	
	uint32_t reportInSize;// = 0;
	uint32_t reportOutSize;// = 0;
#endif

} PADState;

extern std::string player_joys[2]; //two players
extern bool has_rumble[2];

bool file_exists(std::string filename);
bool dir_exists(std::string filename);

#endif

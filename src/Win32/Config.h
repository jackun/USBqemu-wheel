#ifndef WIN32CONFIG_H
#define WIN32CONFIG_H

#include "../usb-pad/config.h"

#include <setupapi.h>
extern "C" {
	#include "../ddk/hidsdi.h"
}

typedef struct Win32PADState {
	PADState padState;

	HIDP_CAPS caps;
	HIDD_ATTRIBUTES attr;
	PHIDP_PREPARSED_DATA pPreparsedData;
	PHIDP_BUTTON_CAPS pButtonCaps;
	//ULONG value;// = 0;
	USHORT numberOfButtons;// = 0;
	USHORT numberOfValues;// = 0;
	HANDLE usbHandle;// = (HANDLE)-1;
	//HANDLE readData;// = (HANDLE)-1;
	OVERLAPPED ovl;
	OVERLAPPED ovlW;
	
	uint32_t reportInSize;// = 0;
	uint32_t reportOutSize;// = 0;
} Win32PADState;

#endif
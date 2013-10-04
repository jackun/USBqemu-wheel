#include "usb-pad.h"
#include "config.h"
#include <setupapi.h>
extern "C" {
	#include "../ddk/hidsdi.h"
}

ULONG value = 0;

typedef struct Win32PADState {
	PADState padState;

	HIDP_CAPS caps;
	HIDD_ATTRIBUTES attr;
	PHIDP_PREPARSED_DATA pPreparsedData;
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

int usb_pad_poll(PADState *ps, uint8_t *buf, int len)
{
	Win32PADState *s = (Win32PADState*) ps;
	uint8_t idx = 1 - s->padState.port;
	if(idx>1) return 0;
	if(s->usbHandle==INVALID_HANDLE_VALUE) return 0;

	uint8_t data[64];
	DWORD waitRes;

	//fprintf(stderr,"usb-pad: poll len=%li\n", len);
	if(s->padState.doPassthrough)
	{
		//ZeroMemory(buf, len);
		ReadFile(s->usbHandle, buf, s->caps.InputReportByteLength, 0, &s->ovl);
		waitRes = WaitForSingleObject(s->ovl.hEvent, 5);
		if(waitRes == WAIT_TIMEOUT || waitRes == WAIT_ABANDONED){
			CancelIo(s->usbHandle);
			return 0;
		}
		return len;
	}

	ZeroMemory(data, 64);
	// Be sure to read 'reportInSize' bytes or you get interleaved reads of data and garbage or something
	ReadFile(s->usbHandle, data, s->caps.InputReportByteLength, 0, &s->ovl);
	waitRes = WaitForSingleObject(s->ovl.hEvent, 5);
	// if the transaction timed out, then we have to manually cancel the request
	if(waitRes == WAIT_TIMEOUT || waitRes == WAIT_ABANDONED){
		CancelIo(s->usbHandle);
		return 0;
	}

	//fprintf(stderr, "\tData 0:%02X 8:%02X 16:%02X 24:%02X 32:%02X %02X %02X %02X\n", data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);

	/** try to get DFP working, faaail, only buttons **/
	/**
	ZeroMemory(buf, len);
	//bitarray_copy(data, 8, 10, buf, 0);
	//bitarray_copy(data, 28, 10, buf, 14 + 2); //buttons, 10 bits -> 12 bits
	int wheel = (data[1] | (data[2] & 0x3) << 8) * (0x3FFF/0x3FF) ;
	buf[0] = 0;//wheel & 0xFF;
	buf[1] = (wheel >> 8) & 0xFF;

	// Axis X
	buf[3] = 0x0;
	buf[4] = 0x0;
	buf[5] = 0x0;
	buf[6] = 0x0;
	buf[7] = 0x0;
	buf[8] = 0x0;
	buf[9] = 0x0;
	buf[10] = 0x0;
	buf[11] = 0x0;
	buf[12] = 0x0;
	buf[13] = 0x0;
	buf[14] = 0x0;
	buf[15] = 0x0;

	// MOMO to DFP buttons
	buf[1] |= (data[2] << 4) & 0xC0; //2 bits 2..3 -> 6..7
	buf[2] |= (data[2] >> 4) & 0xF; //4 bits 4..7 -> 0..3
	buf[2] |= (data[3] << 4) & 0xF0; //4 bits 0..3 -> 4..7
	**/

	//buf[1] = 1<<4; //
	//buf[1] = 1<<5; // ??
	//buf[1] |= 1<<6; // cross 15
	//buf[1] = 1<<7; // square 16
	//buf[2] = 1<<0; // circle 17
	//buf[2] = 1<<1; // triangle 18
	//buf[2] |= 1<<2; // R1 gear up 19
	//buf[2] |= 1<<3; // L1 gear down 20
	//buf[2] |= 1<<4; // R2 21
	//buf[2] |= 1<<5; // L2 22
	//buf[2] |= 1<<6; // select 23
	//buf[2] |= 1<<7; // start??? 24

	//buf[3] = 1<<0; //25
	//buf[3] = 1<<1;//shift?? //26
	//buf[3] = 1<<2;//shift?? //27
	//buf[3] = 1<<3;//shift up?? //28
	//buf[3] |= 1<<4;//view right //29
	//buf[3] |= 1<<5;//view FR?? //30 // 4|5 bits view RL
	//buf[3] = 1<<6;//view back R2?? //31
	//buf[3] |= 1<<7;//view ahead?? //32
	//buf[4] = 0xff;//???

	//buf[5] = 0x0;//data[5]; //y -> y
	//buf[6] = 0xFF ;//data[6]; //z -> rz
	//buf[7] = 1<<4; //steer left
	//buf[10] = 0xFF;

	//fprintf(stderr, "\tData %02X %02X %02X %02X %02X %02X %02X %02X\n", data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
	//memcpy(&momo_data, data, sizeof(momo_data_t));
	//fprintf(stderr, "\tmomo_data: %d %04X %02X %02X\n",momo_data.axis_x, momo_data.buttons, momo_data.axis_z, momo_data.axis_rz);
	//memcpy(buf, &data, len);
	//memset(&dfp_data, 0, sizeof(dfp_data_t));
	//dfp_data.buttons = momo_data.buttons;

	//dfp_data.axis_x = momo_data.axis_x ;// * (0x3fff/0x3ff);
	//dfp_data.axis_z = momo_data.axis_z;
	//dfp_data.axis_rz = momo_data.axis_rz;

	//fprintf(stderr, "\tdfp_data: %04X %02X %02X\n", dfp_data.axis_x, dfp_data.axis_z, dfp_data.axis_rz);

	//memcpy(buf, &dfp_data, sizeof(dfp_data_t));

	/*df_data.axis_x = momo_data.axis_x;
	df_data.axis_z = momo_data.axis_z;
	df_data.axis_rz = momo_data.axis_rz;
	df_data.buttons = momo_data.buttons;
	memcpy(buf, &df_data, sizeof(df_data));*/

	/*** raw version, MOMO to generic 0xC294, works kinda ***/
	/*ZeroMemory(&generic_data, sizeof(generic_data_t));
	memcpy(&momo_data, data, sizeof(momo_data_t));
	//memcpy(&generic_data, data, sizeof(momo_data_t));
	generic_data.buttons = momo_data.buttons;
	generic_data.axis_x = momo_data.axis_x;
	generic_data.axis_y = 0xff; //set to 0xFF aka not pressed
	generic_data.axis_z = momo_data.axis_z;
	generic_data.axis_rz = momo_data.axis_rz;
	memcpy(buf, &generic_data, sizeof(generic_data_t));*/

	/** *
	 * 
	 * 
	 * rawinput API version 
	 * 
	 * 
	 ***/
 
	PHIDP_PREPARSED_DATA pPreparsedData;
	USHORT capsLength = 0;
	USAGE usage[MAX_BUTTONS] = {0};

	ZeroMemory(&generic_data, sizeof(generic_data_t));

	// Setting to unpressed
	generic_data.axis_x = 0x3FF >> 1;
	generic_data.axis_y = 0xFF;
	generic_data.axis_z = 0xFF;
	generic_data.axis_rz = 0xFF;

	HidD_GetPreparsedData(s->usbHandle, &pPreparsedData);
	//HidP_GetCaps(pPreparsedData, &caps);

	/// Get buttons' values
	HANDLE heap = GetProcessHeap();
	PHIDP_BUTTON_CAPS pButtonCaps =
		(PHIDP_BUTTON_CAPS)HeapAlloc(heap, 0, sizeof(HIDP_BUTTON_CAPS) * s->caps.NumberInputButtonCaps);

	ULONG usageLength = s->numberOfButtons;
	NTSTATUS stat;
	if((stat = HidP_GetUsages(
			HidP_Input, pButtonCaps->UsagePage, 0, usage, &usageLength, /*s->*/pPreparsedData,
			(PCHAR)data, s->caps.InputReportByteLength)) == HIDP_STATUS_SUCCESS )
	{
		for(int i = 0; i < usageLength; i++)
		{
			generic_data.buttons |=  (1 << (usage[i] - pButtonCaps->Range.UsageMin - 1)) & 0x3FF; //10bit mask
		}
	}
	//else if(stat != HIDP_STATUS_BUTTON_NOT_PRESSED) //damn HIDP_STATUS_USAGE_NOT_FOUND
	//	fprintf(stderr, "ntstatus %08X\n", stat);

	/// Get axes' values
	PHIDP_VALUE_CAPS pValueCaps
		= (PHIDP_VALUE_CAPS)HeapAlloc(heap, 0, sizeof(HIDP_VALUE_CAPS) * s->caps.NumberInputValueCaps);

	capsLength = s->caps.NumberInputValueCaps;
	if(HidP_GetValueCaps(HidP_Input, pValueCaps, &capsLength, s->pPreparsedData) == HIDP_STATUS_SUCCESS )
	{
		for(USHORT i = 0; i < capsLength; i++)
		{
			if(HidP_GetUsageValue(
					HidP_Input, pValueCaps[i].UsagePage, 0,
					pValueCaps[i].Range.UsageMin, &value, s->pPreparsedData,
					(PCHAR)data, s->caps.InputReportByteLength
				) != HIDP_STATUS_SUCCESS )
			{
				continue; // if here then maybe something is up with HIDP_CAPS.NumberInputValueCaps
			}

			//fprintf(stderr, "Min/max %d/%d\t", pValueCaps[i].LogicalMin, pValueCaps[i].LogicalMax);
			switch(pValueCaps[i].Range.UsageMin)
			{
				case 0x30: // X-axis
					//lAxisX = (LONG)value - 128;
					//fprintf(stderr, "X: %d\n", value);
					generic_data.axis_x = value; // * (pValueCaps[i].LogicalMax / 1023);
					break;

				case 0x31: // Y-axis
					//lAxisY = (LONG)value - 128;
					//fprintf(stderr, "Y: %d\n", value);
					if(!(s->attr.VendorID == 0x046D && s->attr.ProductID == 0xCA03)) //FIXME MOMO always gives 128, wtf
						generic_data.axis_y = value;
					break;

				case 0x32: // Z-axis
					//lAxisZ = (LONG)value - 128;
					//fprintf(stderr, "Z: %d\n", value);
					generic_data.axis_z = value;
					break;

				case 0x33: // Rotate-X
					//lAxisRx = (LONG)value - 128;
					//fprintf(stderr, "Rx: %d\n", value);
					break;

				case 0x34: // Rotate-Y
					//lAxisRy = (LONG)value - 128;
					//fprintf(stderr, "Ry: %d\n", value);
					break;

				case 0x35: // Rotate-Z
					//lAxisRz = (LONG)value - 128;
					//fprintf(stderr, "Rz: %d\n", value);
					generic_data.axis_rz = value;
					break;

				case 0x39: // Hat Switch
					//lHat = value;
					//fprintf(stderr, "Hat: %02X\n", value);
					generic_data.hatswitch = value;
					break;
			}
		}
	}

	//TODO Currently config descriptor has 16 byte buffer and generic_data_t is just 8 bytes
	memcpy(buf, &generic_data, sizeof(generic_data_t));

	HeapFree(heap, 0, pButtonCaps);
	HeapFree(heap, 0, pValueCaps);
	HidD_FreePreparsedData(pPreparsedData);
	return len;
}

PADState* get_new_padstate()
{
	return (PADState*)qemu_mallocz(sizeof(Win32PADState));
}

bool find_pad(PADState *ps)
{
	
	Win32PADState *s = (Win32PADState*) ps;
	uint8_t idx = 1 - s->padState.port;
	if(idx > 1) return false;

	PHIDP_PREPARSED_DATA pPreparsedData;

	memset(&s->ovl, 0, sizeof(OVERLAPPED));
	memset(&s->ovlW, 0, sizeof(OVERLAPPED));
	s->ovl.hEvent = CreateEvent(0, 0, 0, 0);
	s->ovlW.hEvent = CreateEvent(0, 0, 0, 0);

	s->padState.doPassthrough = false;
	s->usbHandle = (HANDLE)-1;
	s->pPreparsedData = NULL;
	ZeroMemory(&generic_data, sizeof(generic_data_t));

	s->usbHandle = CreateFile(player_joys[idx].c_str(), GENERIC_READ|GENERIC_WRITE,
		FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
	if(s->usbHandle != INVALID_HANDLE_VALUE)
	{
		HidD_GetAttributes(s->usbHandle, &(s->attr));
		HidD_GetPreparsedData(s->usbHandle, &pPreparsedData);
		HidP_GetCaps(pPreparsedData, &(s->caps));

		if(s->caps.UsagePage == HID_USAGE_PAGE_GENERIC && s->caps.Usage == HID_USAGE_GENERIC_JOYSTICK)
		{
			if(s->attr.ProductID == DFP_PID)
				s->padState.doPassthrough = true;

			s->pPreparsedData = pPreparsedData;

			USHORT capsLength = s->caps.NumberInputButtonCaps;
			PHIDP_BUTTON_CAPS pButtonCaps = (PHIDP_BUTTON_CAPS)malloc(sizeof(HIDP_BUTTON_CAPS) * capsLength);
			if(HidP_GetButtonCaps(HidP_Input, pButtonCaps, &capsLength, pPreparsedData) == HIDP_STATUS_SUCCESS )
				s->numberOfButtons = pButtonCaps->Range.UsageMax - pButtonCaps->Range.UsageMin + 1;
			free(pButtonCaps);

			fprintf(stderr, "Wheel found !!! %04X:%04X\n", s->attr.VendorID, s->attr.ProductID);
			return true;
		}
		else
		{
			CloseHandle(s->usbHandle);
			s->usbHandle = INVALID_HANDLE_VALUE;
			HidD_FreePreparsedData(pPreparsedData);
		}
	}

	fprintf(stderr, "Could not open device '%s'\n", player_joys[idx]);
	return false;
}

int token_out(PADState *ps, uint8_t *data, int len)
{
	Win32PADState *s = (Win32PADState*) ps;
	DWORD out = 0, err = 0, waitRes = 0;
	BOOL res;
	uint8_t outbuf[65];

	//If i'm reading it correctly MOMO report size for output has Report Size(8) and Report Count(7), so that's 7 bytes
	//Now move that 7 bytes over by one and add report id of 0 (right?). Supposedly mandatory for HIDs.
	memcpy(outbuf + 1, data, len - 1);
	outbuf[0] = 0;

	//CancelIo(s->usbHandle); //Mind the ERROR_IO_PENDING, may break FFB
	res = WriteFile(s->usbHandle, outbuf, s->caps.OutputReportByteLength, &out, &s->ovlW);
	waitRes = WaitForSingleObject(s->ovlW.hEvent, 300);

	if(waitRes == WAIT_TIMEOUT || waitRes == WAIT_ABANDONED)
		CancelIo(s->usbHandle);
	//err = GetLastError();
	//fprintf(stderr,"usb-pad: wrote %d, res: %d, err: %d\n", out, res, err);

	return 0;
}

void destroy_pad(PADState *ps)
{
	Win32PADState *s = (Win32PADState*) ps;
	if(s->pPreparsedData)
		HidD_FreePreparsedData(s->pPreparsedData);
	if(s->usbHandle != INVALID_HANDLE_VALUE)
		CloseHandle(s->usbHandle);

	CloseHandle(s->ovl.hEvent);
	CloseHandle(s->ovlW.hEvent);
	s->usbHandle = INVALID_HANDLE_VALUE;
	s->pPreparsedData = NULL;
}

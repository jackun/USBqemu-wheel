#include "usb-pad.h"
#include "../Win32/Config.h"

int usb_pad_poll(PADState *ps, uint8_t *buf, int len)
{
	Win32PADState *s = (Win32PADState*) ps;
	uint8_t idx = 1 - s->padState.port;
	if(idx>1) return 0;
	if(s->usbHandle==INVALID_HANDLE_VALUE) return 0;

	uint8_t data[64];
	DWORD waitRes;
	ULONG value = 0;

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
		fprintf(stderr, "usb_pad_poll: waitRes 0x%08X\n", waitRes);
		return 0;
	}

	//fprintf(stderr, "\tData 0:%02X 8:%02X 16:%02X 24:%02X 32:%02X %02X %02X %02X\n", data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
	
	/***
	 *
	 *
	 * rawinput API version 
	 *
	 *
	 ***/
	USHORT capsLength = 0;
	USAGE usage[MAX_BUTTONS] = {0};

	ZeroMemory(&generic_data, sizeof(generic_data_t));

	// Setting to unpressed
	generic_data.axis_x = 0x3FF >> 1;
	generic_data.axis_y = 0xFF;
	generic_data.axis_z = 0xFF;
	generic_data.axis_rz = 0xFF;
	
	ULONG usageLength = s->numberOfButtons;
	NTSTATUS stat;
	if((stat = HidP_GetUsages(
			HidP_Input, s->pButtonCaps->UsagePage, 0, usage, &usageLength, s->pPreparsedData,
			(PCHAR)data, s->caps.InputReportByteLength)) == HIDP_STATUS_SUCCESS )
	{
		for(uint32_t i = 0; i < usageLength; i++)
		{
			generic_data.buttons |=  (1 << (ps->btnsmap[usage[i] - s->pButtonCaps->Range.UsageMin])) & 0x3FF; //10bit mask
		}
	}

	/// Get axes' values
	capsLength = s->caps.NumberInputValueCaps;
	if(HidP_GetValueCaps(HidP_Input, s->pValueCaps, &capsLength, s->pPreparsedData) == HIDP_STATUS_SUCCESS )
	{
		for(USHORT i = 0; i < capsLength; i++)
		{
			if(HidP_GetUsageValue(
					HidP_Input, s->pValueCaps[i].UsagePage, 0,
					s->pValueCaps[i].Range.UsageMin, &value, s->pPreparsedData,
					(PCHAR)data, s->caps.InputReportByteLength
				) != HIDP_STATUS_SUCCESS )
			{
				continue; // if here then maybe something is up with HIDP_CAPS.NumberInputValueCaps
			}

			//fprintf(stderr, "Min/max %d/%d\t", pValueCaps[i].LogicalMin, pValueCaps[i].LogicalMax);
			switch(s->pValueCaps[i].Range.UsageMin)
			{
				case 0x30: // X-axis
					//fprintf(stderr, "X: %d\n", value);
					// Need for logical min too?
					//generic_data.axis_x = ((value - pValueCaps[i].LogicalMin) * 0x3FF) / pValueCaps[i].LogicalMax;

					//XXX Limit value range to 0..1023 if using 'generic' wheel descriptor
					generic_data.axis_x = (value * 0x3FF) / s->pValueCaps[i].LogicalMax;
					break;

				case 0x31: // Y-axis
					//fprintf(stderr, "Y: %d\n", value);
					//FIXME MOMO always gives 128. Some flag in caps to detect this?
					if(!(s->attr.VendorID == 0x046D && s->attr.ProductID == 0xCA03))
						//XXX Limit value range to 0..255 if using 'generic' wheel descriptor
						generic_data.axis_y = (value * 0xFF) / s->pValueCaps[i].LogicalMax;
					break;

				case 0x32: // Z-axis
					//fprintf(stderr, "Z: %d\n", value);
					//XXX Limit value range to 0..255 if using 'generic' wheel descriptor
					generic_data.axis_z = (value * 0xFF) / s->pValueCaps[i].LogicalMax;
					break;

				case 0x33: // Rotate-X
					//lAxisRx = (LONG)value - 128;
					//fprintf(stderr, "Rx: %d\n", value);
					break;

				case 0x34: // Rotate-Y
					//fprintf(stderr, "Ry: %d\n", value);
					break;

				case 0x35: // Rotate-Z
					//fprintf(stderr, "Rz: %d\n", value);
					//XXX Limit value range to 0..255 if using 'generic' wheel descriptor
					generic_data.axis_rz = (value * 0xFF) / s->pValueCaps[i].LogicalMax;
					break;

				case 0x39: // TODO Hat Switch, also map some keyboard keys (for F1 '06 etc.)
					//fprintf(stderr, "Hat: %02X\n", value);
					generic_data.hatswitch = value;
					break;
			}
		}
	}

	//TODO Currently config descriptor has 16 byte buffer and generic_data_t is just 8 bytes
	memcpy(buf, &generic_data, sizeof(generic_data_t));
	return len;
}

//Too much C, not enough C++ ? :P
PADState* get_new_padstate()
{
	return (PADState*)qemu_mallocz(sizeof(Win32PADState));
}

bool find_pad(PADState *ps)
{
	
	Win32PADState *s = (Win32PADState*) ps;
	uint8_t idx = 1 - s->padState.port;
	if(idx > 1) return false;

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
		HidD_GetPreparsedData(s->usbHandle, &(s->pPreparsedData));
		HidP_GetCaps(s->pPreparsedData, &(s->caps));

		if(s->caps.UsagePage == HID_USAGE_PAGE_GENERIC && 
			s->caps.Usage == HID_USAGE_GENERIC_JOYSTICK)
		{
			if(s->attr.ProductID == DFP_PID)
				s->padState.doPassthrough = true;

			//Get buttons' caps
			USHORT capsLength = s->caps.NumberInputButtonCaps;
			s->pButtonCaps = (PHIDP_BUTTON_CAPS)malloc(sizeof(HIDP_BUTTON_CAPS) * capsLength);
			if(HidP_GetButtonCaps(HidP_Input, s->pButtonCaps, &capsLength, s->pPreparsedData) == HIDP_STATUS_SUCCESS )
				s->numberOfButtons = s->pButtonCaps->Range.UsageMax - s->pButtonCaps->Range.UsageMin + 1;

			//Get axes' caps
			s->pValueCaps = (PHIDP_VALUE_CAPS)malloc(sizeof(HIDP_VALUE_CAPS) * s->caps.NumberInputValueCaps);

			LoadMappings(idx, s->attr.VendorID, s->attr.ProductID, ps->btnsmap, ps->axesmap);

			fprintf(stderr, "Wheel found !!! %04X:%04X\n", s->attr.VendorID, s->attr.ProductID);
			return true;
		}
		else
		{
			CloseHandle(s->usbHandle);
			s->usbHandle = INVALID_HANDLE_VALUE;
			HidD_FreePreparsedData(s->pPreparsedData);
		}
	}

	fprintf(stderr, "Could not open device '%s'\n", player_joys[idx]);
	return false;
}

void token_out(PADState *ps, uint8_t *data, int len)
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
}

void destroy_pad(PADState *ps)
{
	Win32PADState *s = (Win32PADState*) ps;
	if(s->pPreparsedData)
		HidD_FreePreparsedData(s->pPreparsedData);
	if(s->pButtonCaps)
		free(s->pButtonCaps);
	if(s->pValueCaps)
		free(s->pValueCaps);
	if(s->usbHandle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(s->usbHandle);
		CloseHandle(s->ovl.hEvent);
		CloseHandle(s->ovlW.hEvent);
	}

	s->usbHandle = INVALID_HANDLE_VALUE;
	s->pPreparsedData = NULL;
	s->pButtonCaps = NULL;
	s->pValueCaps = NULL;
}

#include <thread>
#include <array>
#include <atomic>
#include "../padproxy.h"
#include "../../USB.h"
#include "../../Win32/Config-win32.h"
#include "raw-config.h"
#include "readerwriterqueue/readerwriterqueue.h"

namespace usb_pad{ namespace raw{

class RawInputPad : public Pad
{
public:
	RawInputPad(int port) : Pad(port)
	, mDoPassthrough(false)
	, mUsbHandle(INVALID_HANDLE_VALUE)
	, mWriterThreadIsRunning(false)
	, mReaderThreadIsRunning(false)
	{
		if (!InitHid())
			throw PadError("InitHid() failed!");
	}
	~RawInputPad()
	{ 
		Close();
		if (mWriterThread.joinable())
			mWriterThread.join();
	}
	int Open();
	int Close();
	int TokenIn(uint8_t *buf, int len);
	int TokenOut(const uint8_t *data, int len);
	int Reset()
	{
		uint8_t reset[7] = { 0 };
		reset[0] = 0xF3; //stop forces
		TokenOut(reset, sizeof(reset));
		return 0;
	}

	static const TCHAR* Name()
	{
		return TEXT("Raw Input");
	}

	static int Configure(int port, void *data);
protected:
	static void WriterThread(void *ptr);
	static void ReaderThread(void *ptr);
	HIDP_CAPS mCaps;
	HIDD_ATTRIBUTES attr;
	//PHIDP_PREPARSED_DATA pPreparsedData;
	//PHIDP_BUTTON_CAPS pButtonCaps;
	//PHIDP_VALUE_CAPS pValueCaps;
	//ULONG value;// = 0;
	USHORT numberOfButtons;// = 0;
	USHORT numberOfValues;// = 0;
	HANDLE mUsbHandle;// = (HANDLE)-1;
	//HANDLE readData;// = (HANDLE)-1;
	OVERLAPPED mOLRead;
	OVERLAPPED mOLWrite;
	
	uint32_t reportInSize;// = 0;
	uint32_t reportOutSize;// = 0;
	bool mDoPassthrough;
	wheel_data_t mDataCopy;
	std::thread mWriterThread;
	std::thread mReaderThread;
	std::atomic<bool> mWriterThreadIsRunning;
	std::atomic<bool> mReaderThreadIsRunning;
	moodycamel::BlockingReaderWriterQueue<std::array<uint8_t, 8>, 32> mFFData;
	moodycamel::BlockingReaderWriterQueue<std::array<uint8_t, 32>, 16> mReportData; //TODO 32 is random
};

void RawInputPad::WriterThread(void *ptr)
{
	DWORD res = 0, res2 = 0, written = 0;
	std::array<uint8_t, 8> buf;

	RawInputPad *pad = static_cast<RawInputPad *>(ptr);
	pad->mWriterThreadIsRunning = true;

	while (pad->mUsbHandle != INVALID_HANDLE_VALUE)
	{
		if (pad->mFFData.wait_dequeue_timed(buf, std::chrono::milliseconds(1000)))
		{
			res = WriteFile(pad->mUsbHandle, buf.data(), buf.size(), &written, &pad->mOLWrite);
			uint8_t *d = buf.data();
			OSDebugOut(TEXT("FFB %02X, %02X : %02X, %02X : %02X, %02X : %02X\n"),
				d[1], d[2], d[3], d[4], d[5], d[6], d[7]);

			WaitForSingleObject(pad->mOLWrite.hEvent, 1000);
			if (GetOverlappedResult(pad->mUsbHandle, &pad->mOLWrite, &written, FALSE))
				OSDebugOut(TEXT("last write ffb: %d %d, written %d\n"), res, res2, written);
		}
	}
	OSDebugOut(TEXT("WriterThread exited.\n"));

	pad->mWriterThreadIsRunning = false;
}

void RawInputPad::ReaderThread(void *ptr)
{
	RawInputPad *pad = static_cast<RawInputPad *>(ptr);
	DWORD res = 0, res2 = 0, read = 0;
	std::array<uint8_t, 32> report; //32 is random

	pad->mReaderThreadIsRunning = true;
	int errCount = 0;

	while (pad->mUsbHandle != INVALID_HANDLE_VALUE)
	{
		if (GetOverlappedResult(pad->mUsbHandle, &pad->mOLRead, &read, FALSE)) // TODO check if previous read finally completed after WaitForSingleObject timed out
			ReadFile(pad->mUsbHandle, report.data(), std::min(pad->mCaps.InputReportByteLength, (USHORT) report.size()), nullptr, &pad->mOLRead); // Seems to only read data when input changes and not current state overall

		if (WaitForSingleObject(pad->mOLRead.hEvent, 1000) == WAIT_OBJECT_0)
		{
			if (!pad->mReportData.try_enqueue(report)) // TODO May leave queue with too stale data. Use multi-producer/consumer queue?
			{
				if (!errCount)
					OSDebugOut(TEXT("Could not enqueue report data: %d\n"), pad->mReportData.size_approx());
				errCount = (++errCount) % 16;
			}
		}
	}
	OSDebugOut(TEXT("ReaderThread exited.\n"));

	pad->mReaderThreadIsRunning = false;
}

int RawInputPad::TokenIn(uint8_t *buf, int len)
{
	ULONG value = 0;
	int ply = 1 - mPort;

	//fprintf(stderr,"usb-pad: poll len=%li\n", len);
	if (mDoPassthrough)
	{
		std::array<uint8_t, 32> report; //32 is random
		if (mReportData.try_dequeue(report)) {
			//ZeroMemory(buf, len);
			int size = std::min((int)mCaps.InputReportByteLength, len);
			memcpy(buf, report.data(), size);
			return size;
		}
		return 0;
	}

	//in case compiler magic with bitfields interferes
	wheel_data_t data_summed;
	memset(&data_summed, 0xFF, sizeof(data_summed));
	data_summed.hatswitch = 0x8;
	data_summed.buttons = 0;

	int copied = 0;
	//TODO fix the logics, also Config.cpp
	for (auto& it : mapVector)
	{

		if (data_summed.steering < it.data[ply].steering)
		{
			data_summed.steering = it.data[ply].steering;
			copied |= 1;
		}

		//if(data_summed.clutch < it.data[ply].clutch)
		//	data_summed.clutch = it.data[ply].clutch;

		if (data_summed.throttle < it.data[ply].throttle)
		{
			data_summed.throttle = it.data[ply].throttle;
			copied |= 2;
		}

		if (data_summed.brake < it.data[ply].brake)
		{
			data_summed.brake = it.data[ply].brake;
			copied |= 4;
		}

		data_summed.buttons |= it.data[ply].buttons;
		if(data_summed.hatswitch > it.data[ply].hatswitch)
			data_summed.hatswitch = it.data[ply].hatswitch;
	}

	if (!copied)
		memcpy(&data_summed, &mDataCopy, sizeof(wheel_data_t));
	else
	{
		if (!(copied & 1))
			data_summed.steering = mDataCopy.steering;
		if (!(copied & 2))
			data_summed.throttle = mDataCopy.throttle;
		if (!(copied & 4))
			data_summed.brake = mDataCopy.brake;
	}

	pad_copy_data(mType, buf, data_summed);

	if (copied)
		memcpy(&mDataCopy, &data_summed, sizeof(wheel_data_t));
	return len;
}

int RawInputPad::TokenOut(const uint8_t *data, int len)
{
	if(mUsbHandle == INVALID_HANDLE_VALUE) return 0;

	if (data[0] == 0x8 || data[0] == 0xB) return len;
	//if(data[0] == 0xF8 && data[1] == 0x5) sendCrap = true;
	if (data[0] == 0xF8 && 
		/* Allow range changes */
		!(data[1] == 0x81 || data[1] == 0x02 || data[1] == 0x03))
		return len; //don't send extended commands

	std::array<uint8_t, 8> report{ 0 };

	//If i'm reading it correctly MOMO report size for output has Report Size(8) and Report Count(7), so that's 7 bytes
	//Now move that 7 bytes over by one and add report id of 0 (right?). Supposedly mandatory for HIDs.
	memcpy(report.data() + 1, data, report.size() - 1);

	if (!mFFData.enqueue(report)) {
		OSDebugOut(TEXT("Failed to enqueue ffb command\n"));
		return 0;
	}

	return len;
}

static void ParseRawInputHID(PRAWINPUT pRawInput)
{
	PHIDP_PREPARSED_DATA pPreparsedData = NULL;
	HIDP_CAPS            Caps;
	PHIDP_BUTTON_CAPS    pButtonCaps = NULL;
	PHIDP_VALUE_CAPS     pValueCaps = NULL;
	UINT                 bufferSize = 0;
	ULONG                usageLength, value;
	TCHAR                name[1024] = {0};
	UINT                 nameSize = 1024;
	RID_DEVICE_INFO      devInfo = {0};
	std::wstring         devName;
	USHORT               capsLength = 0;
	USAGE                usage[MAX_BUTTONS] = {0};
	Mappings             *mapping = NULL;
	int                  numberOfButtons;

	GetRawInputDeviceInfo(pRawInput->header.hDevice, RIDI_DEVICENAME, name, &nameSize);

	devName = name;
	std::transform(devName.begin(), devName.end(), devName.begin(), ::toupper);

	for(auto& it : mapVector)
	{
		if(it.hidPath == devName)
		{
			mapping = &it;
			break;
		}
	}

	if(mapping == NULL)
		return;

	CHECK( GetRawInputDeviceInfo(pRawInput->header.hDevice, RIDI_PREPARSEDDATA, NULL, &bufferSize) == 0 );
	CHECK( pPreparsedData = (PHIDP_PREPARSED_DATA)malloc(bufferSize) );
	CHECK( (int)GetRawInputDeviceInfo(pRawInput->header.hDevice, RIDI_PREPARSEDDATA, pPreparsedData, &bufferSize) >= 0 );
	CHECK( HidP_GetCaps(pPreparsedData, &Caps) == HIDP_STATUS_SUCCESS );
	//pSize = sizeof(devInfo);
	//GetRawInputDeviceInfo(pRawInput->header.hDevice, RIDI_DEVICEINFO, &devInfo, &pSize);

	//Unset buttons/axes mapped to this device
	//ResetData(&mapping->data[0]);
	//ResetData(&mapping->data[1]);
	memset(&mapping->data[0], 0xFF, sizeof(wheel_data_t));
	memset(&mapping->data[1], 0xFF, sizeof(wheel_data_t));
	mapping->data[0].buttons = 0;
	mapping->data[1].buttons = 0;
	mapping->data[0].hatswitch = 0x8;
	mapping->data[1].hatswitch = 0x8;

	//Get pressed buttons
	CHECK( pButtonCaps = (PHIDP_BUTTON_CAPS)malloc(sizeof(HIDP_BUTTON_CAPS) * Caps.NumberInputButtonCaps) );
	//If fails, maybe wheel only has axes
	capsLength = Caps.NumberInputButtonCaps;
	HidP_GetButtonCaps(HidP_Input, pButtonCaps, &capsLength, pPreparsedData);

	numberOfButtons = pButtonCaps->Range.UsageMax - pButtonCaps->Range.UsageMin + 1;
	usageLength = numberOfButtons;
	NTSTATUS stat;
	if((stat = HidP_GetUsages(
			HidP_Input, pButtonCaps->UsagePage, 0, usage, &usageLength, pPreparsedData,
			(PCHAR)pRawInput->data.hid.bRawData, pRawInput->data.hid.dwSizeHid)) == HIDP_STATUS_SUCCESS )
	{
		for(uint32_t i = 0; i < usageLength; i++)
		{
			uint16_t btn = mapping->btnMap[usage[i] - pButtonCaps->Range.UsageMin];
			for(int j=0; j<2; j++)
			{
				PS2WheelTypes wt = (PS2WheelTypes)conf.WheelType[j];
				if(PLY_IS_MAPPED(j, btn))
				{
					uint32_t wtbtn = (1 << convert_wt_btn(wt, PLY_GET_VALUE(j, btn))) & 0xFFF; //12bit mask
					mapping->data[j].buttons |= wtbtn;
				}
			}
		}
	}

	/// Get axes' values
	CHECK( pValueCaps = (PHIDP_VALUE_CAPS)malloc(sizeof(HIDP_VALUE_CAPS) * Caps.NumberInputValueCaps) );
	capsLength = Caps.NumberInputValueCaps;
	if(HidP_GetValueCaps(HidP_Input, pValueCaps, &capsLength, pPreparsedData) == HIDP_STATUS_SUCCESS )
	{
		for(USHORT i = 0; i < capsLength; i++)
		{
			if(HidP_GetUsageValue(
					HidP_Input, pValueCaps[i].UsagePage, 0,
					pValueCaps[i].Range.UsageMin, &value, pPreparsedData,
					(PCHAR)pRawInput->data.hid.bRawData, pRawInput->data.hid.dwSizeHid
				) != HIDP_STATUS_SUCCESS )
			{
				continue; // if here then maybe something is up with HIDP_CAPS.NumberInputValueCaps
			}

			//fprintf(stderr, "Min/max %d/%d\t", pValueCaps[i].LogicalMin, pValueCaps[i].LogicalMax);
			//TODO can be simpler?
			//Get mapped axis for physical axis
			uint16_t v = 0;
			switch(pValueCaps[i].Range.UsageMin)
			{
				case HID_USAGE_GENERIC_X: //0x30
				case HID_USAGE_GENERIC_Y:
				case HID_USAGE_GENERIC_Z:
				case HID_USAGE_GENERIC_RX:
				case HID_USAGE_GENERIC_RY:
				case HID_USAGE_GENERIC_RZ: //0x35
					v = mapping->axisMap[pValueCaps[i].Range.UsageMin - HID_USAGE_GENERIC_X];
					break;
				case HID_USAGE_GENERIC_HATSWITCH:
					//fprintf(stderr, "Hat: %02X\n", value);
					v = mapping->axisMap[6];
					break;
			}

			int type = 0;
			for(int j=0; j<2; j++)
			{
				if(!PLY_IS_MAPPED(j, v))
					continue;

				type = conf.WheelType[j];

				switch(PLY_GET_VALUE(j, v))
				{
				case PAD_AXIS_X: // X-axis
					//fprintf(stderr, "X: %d\n", value);
					// Need for logical min too?
					//generic_data.axis_x = ((value - pValueCaps[i].LogicalMin) * 0x3FF) / (pValueCaps[i].LogicalMax - pValueCaps[i].LogicalMin);
					if(type == WT_DRIVING_FORCE_PRO)
						mapping->data[j].steering = (value * 0x3FFF) / pValueCaps[i].LogicalMax;
					else
						//XXX Limit value range to 0..1023 if using 'generic' wheel descriptor
						mapping->data[j].steering = (value * 0x3FF) / pValueCaps[i].LogicalMax;
					break;

				case PAD_AXIS_Y: // Y-axis
					if(!(devInfo.hid.dwVendorId == 0x046D && devInfo.hid.dwProductId == 0xCA03))
						//XXX Limit value range to 0..255
						mapping->data[j].clutch = (value * 0xFF) / pValueCaps[i].LogicalMax;
					break;

				case PAD_AXIS_Z: // Z-axis
					//fprintf(stderr, "Z: %d\n", value);
					//XXX Limit value range to 0..255
					mapping->data[j].throttle = (value * 0xFF) / pValueCaps[i].LogicalMax;
					break;

				case PAD_AXIS_RZ: // Rotate-Z
					//fprintf(stderr, "Rz: %d\n", value);
					//XXX Limit value range to 0..255
					mapping->data[j].brake = (value * 0xFF) / pValueCaps[i].LogicalMax;
					break;

				case PAD_AXIS_HAT: // TODO Hat Switch
					//fprintf(stderr, "Hat: %02X\n", value);
					//TODO 4 vs 8 direction hat switch
					if(pValueCaps[i].LogicalMax == 4 && value < 4)
						mapping->data[j].hatswitch = HATS_8TO4[value];
					else
						mapping->data[j].hatswitch = value;
					break;
				}
			}
		}
	}

	Error:
	SAFE_FREE(pPreparsedData);
	SAFE_FREE(pButtonCaps);
	SAFE_FREE(pValueCaps);
}

static void ParseRawInputKB(PRAWINPUT pRawInput)
{
	Mappings *mapping = nullptr;

	for(auto& it : mapVector)
	{
		if(!it.hidPath.compare(TEXT("Keyboard")))
		{
			mapping = &it;
			break;
		}
	}

	if(mapping == NULL)
		return;

	for(uint32_t i = 0; i < PAD_BUTTON_COUNT; i++)
	{
		uint16_t btn = mapping->btnMap[i];
		for(int j=0; j<2; j++)
		{
			if(PLY_IS_MAPPED(j, btn))
			{
				PS2WheelTypes wt = (PS2WheelTypes)conf.WheelType[j];
				if(PLY_GET_VALUE(j, mapping->btnMap[i]) == pRawInput->data.keyboard.VKey)
				{
					uint32_t wtbtn = convert_wt_btn(wt, i);
					if(pRawInput->data.keyboard.Flags & RI_KEY_BREAK)
						mapping->data[j].buttons &= ~(1 << wtbtn); //unset
					else //if(pRawInput->data.keyboard.Flags == RI_KEY_MAKE)
						mapping->data[j].buttons |= (1 << wtbtn); //set
				}
			}
		}
	}

	for(uint32_t i = 0; i < 4 /*PAD_HAT_COUNT*/; i++)
	{
		uint16_t btn = mapping->hatMap[i];
		for(int j=0; j<2; j++)
		{
			if(PLY_IS_MAPPED(j, btn))
			{
				if(PLY_GET_VALUE(j, mapping->hatMap[i]) == pRawInput->data.keyboard.VKey)
				if(pRawInput->data.keyboard.Flags & RI_KEY_BREAK)
					mapping->data[j].hatswitch = 0x8;
				else //if(pRawInput->data.keyboard.Flags == RI_KEY_MAKE)
					mapping->data[j].hatswitch = HATS_8TO4[i];
			}
		}
	}
}

int RawInputPad::Open()
{
	PHIDP_PREPARSED_DATA pPreparsedData = nullptr;

	Close();

	LoadMappings(mapVector);

	memset(&mOLRead, 0, sizeof(OVERLAPPED));
	memset(&mOLWrite, 0, sizeof(OVERLAPPED));

	mUsbHandle = INVALID_HANDLE_VALUE;
	std::wstring path;
	if (!LoadSetting(mPort, APINAME, N_JOYSTICK, path))
		return 1;

	LoadSetting(mPort, APINAME, N_WHEEL_PT, mDoPassthrough);

	mUsbHandle = CreateFileW(path.c_str(), GENERIC_READ|GENERIC_WRITE,
		FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);

	if(mUsbHandle != INVALID_HANDLE_VALUE)
	{
		mOLRead.hEvent = CreateEvent(0, 0, 0, 0);
		mOLWrite.hEvent = CreateEvent(0, 0, 0, 0);

		HidD_GetAttributes(mUsbHandle, &(attr));
		if (attr.VendorID != PAD_VID) {
			fwprintf(stderr, TEXT("USBqemu [" APINAME "]: Vendor is not Logitech. Not sending force feedback commands for safety reasons.\n"));
			mDoPassthrough = false;
			Close();
			return 0;
		}

		if (!mWriterThreadIsRunning)
		{
			if (mWriterThread.joinable())
				mWriterThread.join();
			mWriterThread = std::thread(RawInputPad::WriterThread, this);
		}

		// for passthrough only
		HidD_GetPreparsedData(mUsbHandle, &pPreparsedData);
		HidP_GetCaps(pPreparsedData, &(mCaps));
		HidD_FreePreparsedData(pPreparsedData);

		if (mDoPassthrough && !mReaderThreadIsRunning)
		{
			if (mReaderThread.joinable())
				mReaderThread.join();
			mReaderThread = std::thread(RawInputPad::ReaderThread, this);
		}
		return 0;
	}
	else
		fwprintf(stderr, TEXT("USBqemu [" APINAME "]: Could not open device '%s'.\nPassthrough and FFB will not work.\n"), path.c_str());

	return 0;
}

int RawInputPad::Close()
{
	Reset();
	if(mUsbHandle != INVALID_HANDLE_VALUE)
	{
		Sleep(100); // give WriterThread some time to write out Reset() commands
		CloseHandle(mUsbHandle);
		CloseHandle(mOLRead.hEvent);
		CloseHandle(mOLWrite.hEvent);
	}

	mUsbHandle = INVALID_HANDLE_VALUE;
	return 0;
}

static void ParseRawInput(PRAWINPUT pRawInput)
{
	if(pRawInput->header.dwType == RIM_TYPEKEYBOARD)
		ParseRawInputKB(pRawInput);
	else
		ParseRawInputHID(pRawInput);
}

REGISTER_PAD(APINAME, RawInputPad);

}} //namespace

// ---------
#include "raw-config-res.h"
INT_PTR CALLBACK ConfigureRawDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam);
int usb_pad::raw::RawInputPad::Configure(int port, void *data)
{
	Win32Handles *h = (Win32Handles*)data;
	INT_PTR res = RESULT_FAILED;
	if (common::rawinput::Initialize(h->hWnd))
	{
		RawDlgConfig config(port);
		res = DialogBoxParam(h->hInst, MAKEINTRESOURCE(IDD_RAWCONFIG), h->hWnd, ConfigureRawDlgProc, (LPARAM)&config);
		common::rawinput::Uninitialize();
	}
	return res;
}
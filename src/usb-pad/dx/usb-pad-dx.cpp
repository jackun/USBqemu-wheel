#include "usb-pad-dx.h"
#include "dx.h"
#include <cmath>

namespace usb_pad { namespace dx {

static bool bdown=false;
static DWORD calibrationtime = 0;
static int calidata = 0;
static bool alternate = false;
static bool calibrating = false;

DInputPad::~DInputPad() { FreeDirectInput(); }

int DInputPad::TokenIn(uint8_t *buf, int len)
{
	int range = range_max(mType);

	// Setting to unpressed
	ZeroMemory(&mWheelData, sizeof(wheel_data_t));
	mWheelData.steering = range >> 1;
	mWheelData.clutch = 0xFF;
	mWheelData.throttle = 0xFF;
	mWheelData.brake = 0xFF;
	mWheelData.hatswitch = 0x8;

	PollDevices();

	//Allow in both ports but warn in configure dialog that only one DX wheel is supported for now
	//if(idx == 0){
		//mWheelData.steering = 8191 + (int)(GetControl(STEERING, false)* 8191.0f) ;
		
		if(calibrating){
			//Alternate full extents
			if (alternate)calidata--;
			else calidata++;

			if(calidata>range-1 || calidata < 1) alternate = !alternate;  //invert

			mWheelData.steering = calidata;		//pass fake

			//breakout after 11 seconds
			if(GetTickCount()-calibrationtime > 11000){
				calibrating = false;
				mWheelData.steering = range >> 1;
			}
		}else{
			mWheelData.steering = (range>>1) + std::lround(GetAxisControl(mPort, CID_STEERING) * (float)(range>>1));
		}

		mWheelData.throttle = std::lround(255.f - (GetAxisControl(mPort, CID_THROTTLE) * 255.0f));
		mWheelData.brake    = std::lround(255.f - (GetAxisControl(mPort, CID_BRAKE) * 255.0f));

		if(GetControl(mPort, CID_CROSS))		mWheelData.buttons |= 1 << convert_wt_btn(mType, PAD_CROSS);
		if(GetControl(mPort, CID_SQUARE))		mWheelData.buttons |= 1 << convert_wt_btn(mType, PAD_SQUARE);
		if(GetControl(mPort, CID_CIRCLE))		mWheelData.buttons |= 1 << convert_wt_btn(mType, PAD_CIRCLE);
		if(GetControl(mPort, CID_TRIANGLE))		mWheelData.buttons |= 1 << convert_wt_btn(mType, PAD_TRIANGLE);
		if(GetControl(mPort, CID_R1))			mWheelData.buttons |= 1 << convert_wt_btn(mType, PAD_R1);
		if(GetControl(mPort, CID_L1))			mWheelData.buttons |= 1 << convert_wt_btn(mType, PAD_L1);
		if(GetControl(mPort, CID_R2))			mWheelData.buttons |= 1 << convert_wt_btn(mType, PAD_R2);
		if(GetControl(mPort, CID_L2))			mWheelData.buttons |= 1 << convert_wt_btn(mType, PAD_L2);

		if(GetControl(mPort, CID_SELECT))		mWheelData.buttons |= 1 << convert_wt_btn(mType, PAD_SELECT);
		if(GetControl(mPort, CID_START))		mWheelData.buttons |= 1 << convert_wt_btn(mType, PAD_START);
		if(GetControl(mPort, CID_R3))			mWheelData.buttons |= 1 << convert_wt_btn(mType, PAD_R3);
		if(GetControl(mPort, CID_L3))			mWheelData.buttons |= 1 << convert_wt_btn(mType, PAD_L3);

		//diagonal
		if(GetControl(mPort, CID_HATUP) && GetControl(mPort, CID_HATRIGHT))
			mWheelData.hatswitch = 1;
		if(GetControl(mPort, CID_HATRIGHT) && GetControl(mPort, CID_HATDOWN))
			mWheelData.hatswitch = 3;
		if(GetControl(mPort, CID_HATDOWN) && GetControl(mPort, CID_HATLEFT))
			mWheelData.hatswitch = 5;
		if(GetControl(mPort, CID_HATLEFT) && GetControl(mPort, CID_HATUP))
			mWheelData.hatswitch = 7;

		//regular
		if(mWheelData.hatswitch==0x8){
			if(GetControl(mPort, CID_HATUP))
				mWheelData.hatswitch = 0;
			if(GetControl(mPort, CID_HATRIGHT))
				mWheelData.hatswitch = 2;
			if(GetControl(mPort, CID_HATDOWN))
				mWheelData.hatswitch = 4;
			if(GetControl(mPort, CID_HATLEFT))
				mWheelData.hatswitch = 6;
		}

		pad_copy_data(mType, buf, mWheelData);
	//} //if(idx ...
	return len;
}

int DInputPad::TokenOut(const uint8_t *data, int len)
{
	const ff_data *ffdata = (const ff_data *)data;
	bool hires = (mType == WT_DRIVING_FORCE_PRO || mType == WT_DRIVING_FORCE_PRO_1102);
	ParseFFData(ffdata, hires);

	return len;
}

int DInputPad::Open()
{
	LoadSetting(mDevType, mPort, APINAME, TEXT("UseRamp"), mUseRamp);
	InitDI(mPort, mDevType);
	if (!mFFdev)
		mFFdev = new JoystickDeviceFF(mPort /*, mUseRamp*/);
	return 0;
}

int DInputPad::Close()
{
	FreeDirectInput();
	return 0;
}

}} //namespace
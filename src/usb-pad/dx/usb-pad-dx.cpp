#include "usb-pad-dx.h"
#include "global.h"
#include "dialog.h"

namespace usb_pad { namespace dx {

static bool bdown=false;
static DWORD calibrationtime = 0;
static int calidata = 0;
static bool alternate = false;
static bool calibrating = false;

enum CONTROLID
{
	STEERING,
	THROTTLE,
	BRAKE,
	HATUP,
	HATDOWN,
	HATLEFT,
	HATRIGHT,
	SQUARE,
	TRIANGLE,
	CROSS,
	CIRCLE,
	L1,
	R1,
	L2,
	R2,
	L3,
	R3,
	SELECT,
	START,
};

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
			mWheelData.steering = (range>>1)+(int)(GetControl(mPort, STEERING, false)* (float)(range>>1)) ;
		}

		mWheelData.throttle = 255-(int)(GetControl(mPort, THROTTLE, false)*255.0f);
		mWheelData.brake = 255-(int)(GetControl(mPort, BRAKE, false)*255.0f);

		if(GetControl(mPort, CROSS, true))		mWheelData.buttons |= 1 << convert_wt_btn(mType, PAD_CROSS);
		if(GetControl(mPort, SQUARE, true))		mWheelData.buttons |= 1 << convert_wt_btn(mType, PAD_SQUARE);
		if(GetControl(mPort, CIRCLE, true))		mWheelData.buttons |= 1 << convert_wt_btn(mType, PAD_CIRCLE);
		if(GetControl(mPort, TRIANGLE, true))	mWheelData.buttons |= 1 << convert_wt_btn(mType, PAD_TRIANGLE);
		if(GetControl(mPort, R1, true))			mWheelData.buttons |= 1 << convert_wt_btn(mType, PAD_R1);
		if(GetControl(mPort, L1, true))			mWheelData.buttons |= 1 << convert_wt_btn(mType, PAD_L1);
		if(GetControl(mPort, R2, true))			mWheelData.buttons |= 1 << convert_wt_btn(mType, PAD_R2);
		if(GetControl(mPort, L2, true))			mWheelData.buttons |= 1 << convert_wt_btn(mType, PAD_L2);

		if(GetControl(mPort, SELECT))		mWheelData.buttons |= 1 << convert_wt_btn(mType, PAD_SELECT);
		if(GetControl(mPort, START))		mWheelData.buttons |= 1 << convert_wt_btn(mType, PAD_START);
		if(GetControl(mPort, R3))			mWheelData.buttons |= 1 << convert_wt_btn(mType, PAD_R3);
		if(GetControl(mPort, L3))			mWheelData.buttons |= 1 << convert_wt_btn(mType, PAD_L3);

		//diagonal
		if(GetControl(mPort, HATUP, true) > DBL_EPSILON && GetControl(mPort, HATRIGHT, true) > DBL_EPSILON)
			mWheelData.hatswitch = 1;
		if(GetControl(mPort, HATRIGHT, true) > DBL_EPSILON && GetControl(mPort, HATDOWN, true) > DBL_EPSILON)
			mWheelData.hatswitch = 3;
		if(GetControl(mPort, HATDOWN, true) > DBL_EPSILON && GetControl(mPort, HATLEFT, true) > DBL_EPSILON)
			mWheelData.hatswitch = 5;
		if(GetControl(mPort, HATLEFT, true) > DBL_EPSILON && GetControl(mPort, HATUP, true) > DBL_EPSILON)
			mWheelData.hatswitch = 7;

		//regular
		if(mWheelData.hatswitch==0x8){
			if(GetControl(mPort, HATUP, true) > DBL_EPSILON)
				mWheelData.hatswitch = 0;
			if(GetControl(mPort, HATRIGHT, true) > DBL_EPSILON)
				mWheelData.hatswitch = 2;
			if(GetControl(mPort, HATDOWN, true) > DBL_EPSILON)
				mWheelData.hatswitch = 4;
			if(GetControl(mPort, HATLEFT, true) > DBL_EPSILON)
				mWheelData.hatswitch = 6;
		}

		pad_copy_data(mType, buf, mWheelData);
	//} //if(idx ...
	return len;
}

int DInputPad::TokenOut(const uint8_t *data, int len)
{
	int caps = 0;
	bool isDFP = (mType == WT_DRIVING_FORCE_PRO) || (mType == WT_DRIVING_FORCE_PRO_1102);
	ff_data *ffdata = (ff_data*)data;

	OSDebugOut(TEXT("FFB %02X, %02X : %02X, %02X : %02X, %02X : %02X, %02X\n"),
		ffdata->cmdslot, ffdata->type, ffdata->u.params[0], ffdata->u.params[1],
		ffdata->u.params[2], ffdata->u.params[3], ffdata->u.params[4], ffdata->padd0);

	if (ffdata->cmdslot != CMD_EXTENDED_CMD)
	{

		uint8_t slots = (ffdata->cmdslot & 0xF0) >> 4;
		uint8_t cmd  = ffdata->cmdslot & 0x0F;

		switch (cmd)
		{
		case CMD_DOWNLOAD:
			for (int i = 0; i < 4; i++)
			{
				if (slots & (1 << i))
					mFFstate.slot_type[i] = ffdata->type;
			}
			break;
		case CMD_DOWNLOAD_AND_PLAY: //0x01
		{
			for (int i = 0; i < 4; i++)
			{
				if (slots & (1 << i))
				{
					mFFstate.slot_type[i] = ffdata->type;
					// TODO save state, reapply from open()
					//memcpy(&mFFstate.slot_ffdata[i], ffdata, sizeof(ff_data));
					if (ffdata->type == FTYPE_CONSTANT)
						mFFstate.slot_force[i] = ffdata->u.params[i];
				}
			}

			switch (ffdata->type)
			{
			case FTYPE_CONSTANT:
				if (!calibrating)
				{
					//TODO do some mixing of forces, little weird
					// param0: 0xFF, param1: 0x00, param2-3: 0x7F == no force
					// param0: 0xFF, param1-3: 0x7F == full force to left
					// param0: 0x00, param1-3: 0x7F == full force to right
					// param0: 0xFF/0x3F, param1: 0x3F/0xFF, param2-3: 0x7F == ~half force to left
					// param0: 0x00, param1: 0xFF, param2: 0x00, param3: 0x7F == full force to right
					// param0: 0x00, param1: 0xFF, param2: 0x3F, param3: 0x7F == ~half force to right
					// param0: 0x3F, param1: 0xFF, param2: 0x3F, param3: 0x7F == no force
					// param0: 0x3F, param1: 0xFF, param2: 0x3F, param3: 0xFF == full force to left
					// param0: 0x3F/0x00, param1: 0xFF, param2: 0x00/0x3F, param3: 0xFF == ~half force to left
					if (slots == 0xF)
					{
						int force = 0x7F;
						//TODO hack, GT3 uses slot 3 usually
						for (int i = 0; i < 4; i++)
						{
							force = ffdata->u.params[i];
							if (force != 0x7F)
								break;
						}
						SetConstantForce(mPort, force);
					}
					else
					{
						for (int i = 0; i < 4; i++)
						{
							if (slots == (1 << i))
								SetConstantForce(mPort, ffdata->u.params[i]);
						}
					}
				}
				break;
			case FTYPE_SPRING:
				//SetSpringForce(mPort, ffdata->u.spring, false, isDFP);
				SetSpringForceGIMX(mPort, ffdata->u.spring, isDFP ? 0 : FF_LG_CAPS_OLD_LOW_RES_COEF);
				break;
			case FTYPE_HIGH_RESOLUTION_SPRING:
				//SetSpringForce(mPort, ffdata->u.spring, true, isDFP);
				SetSpringForceGIMX(mPort, ffdata->u.spring, FF_LG_CAPS_HIGH_RES_COEF);
				break;
			case FTYPE_VARIABLE: //Ramp-like
				if (!calibrating)
				{
					if (mUseRamp)
						SetRampVariable(mPort, slots, ffdata->u.variable);
					else {
						if (slots & (1 << 0))
							SetConstantForce(mPort, ffdata->u.variable.l1);
						else if (slots & (1 << 2))
							SetConstantForce(mPort, ffdata->u.variable.l2);
					}
				}
				break;
			case FTYPE_FRICTION:
				SetFrictionForce(mPort, ffdata->u.friction);
				break;
			case FTYPE_DAMPER:
				//SetDamper(mPort, ffdata->u.damper, false);
				SetDamperGIMX(mPort, ffdata->u.damper, isDFP ? 0 : FF_LG_CAPS_OLD_LOW_RES_COEF);
				break;
			case FTYPE_HIGH_RESOLUTION_DAMPER:
				//SetDamper(mPort, ffdata->u.damper, isDFP);
				caps = FF_LG_CAPS_HIGH_RES_COEF;
				if (isDFP)
					caps |= FF_LG_CAPS_DAMPER_CLIP;
				SetDamperGIMX(mPort, ffdata->u.damper, caps);
				break;
			default:
				OSDebugOut(TEXT("CMD_DOWNLOAD_AND_PLAY: unhandled force type 0x%02X in slots 0x%02X\n"), ffdata->type, slots);
				break;
			}
		}
		break;
		case CMD_STOP: //0x03
		{
			if (BYPASSCAL)
			{
				alternate = false;
				calidata = 0;
				calibrating = true;
				calibrationtime = GetTickCount();
			}

			for (int i = 0; i < 4; i++)
			{
				if (slots & (1 << i))
				{
					switch (mFFstate.slot_type[i])
					{
					case FTYPE_CONSTANT:
						DisableConstantForce(mPort);
						break;
					case FTYPE_VARIABLE:
						if (mUseRamp)
							DisableRamp(mPort);
						else
							DisableConstantForce(mPort);
						break;
					case FTYPE_SPRING:
					case FTYPE_HIGH_RESOLUTION_SPRING:
						DisableSpring(mPort);
						break;
					case FTYPE_AUTO_CENTER_SPRING:
						DisableSpring(mPort);
						break;
					case FTYPE_FRICTION:
						DisableFriction(mPort);
						break;
					case FTYPE_DAMPER:
					case FTYPE_HIGH_RESOLUTION_DAMPER:
						DisableDamper(mPort);
						break;
					default:
						OSDebugOut(TEXT("CMD_STOP: unhandled force type 0x%02X in slot 0x%02X\n"), ffdata->type, slots);
						break;
					}
				}
			}
		}
		break;
		case CMD_DEFAULT_SPRING_ON: //0x04
			OSDebugOut(TEXT("CMD_DEFAULT_SPRING_ON: slots 0x%X\n"), slots);
			//if ((slots & 1) || (slots & 2))
			AutoCenter(AXISID[mPort][0] / 8, true);
			break;
		case CMD_DEFAULT_SPRING_OFF: //0x05
		{
			//if ((slots & 1) || (slots & 2))
			AutoCenter(AXISID[mPort][0] / 8, false);
			OSDebugOut(TEXT("CMD_DEFAULT_SPRING_OFF: slots 0x%X\n"), slots);
		}
		break;
		case CMD_NORMAL_MODE: //0x08
			OSDebugOut(TEXT("CMD_NORMAL_MODE: unhandled cmd\n"));
			break;
		case CMD_SET_LED: //0x09
			OSDebugOut(TEXT("CMD_SET_LED: unhandled cmd\n"));
			break;
		case CMD_RAW_MODE: //0x0B
			OSDebugOut(TEXT("CMD_RAW_MODE: unhandled cmd\n"));
			break;
		case CMD_SET_DEFAULT_SPRING: //0x0E
			OSDebugOut(TEXT("CMD_SET_DEFAULT_SPRING: unhandled cmd\n"));
			break;
		case CMD_SET_DEAD_BAND: //0x0F
			OSDebugOut(TEXT("CMD_SET_DEAD_BAND: unhandled cmd\n"));
			break;
		}
	}
	else
	{
		// 0xF8, 0x05, 0x01, 0x00
		switch (ffdata->type) {
			case EXT_CMD_WHEEL_RANGE_900_DEGREES:
				{ OSDebugOut(TEXT("CMD_EXTENDED: unhandled set wheel 900 degrees\n")); }
				break;
			case EXT_CMD_WHEEL_RANGE_200_DEGREES:
				{ OSDebugOut(TEXT("CMD_EXTENDED: unhandled set wheel 200 degrees\n")); }
				break;
			default:
			{
				OSDebugOut(TEXT("CMD_EXTENDED: unhandled cmd 0x%02X : %02X, %02X\n"),
					ffdata->type, ffdata->u.params[0], ffdata->u.params[1]);
			}
		}
	}
	return len;
}

int DInputPad::Open()
{
	LoadSetting(mDevType, mPort, APINAME, TEXT("UseRamp"), mUseRamp);
	InitDI(mPort, mDevType);
	return 0;
}

int DInputPad::Close()
{
	FreeDirectInput();
	return 0;
}

int DInputPad::Configure(int port, const char* dev_type, void *data)
{
	Win32Handles h = *(Win32Handles*)data;
	struct DXDlgSettings s;
	s.port = port;
	s.dev_type = dev_type;
	return DialogBoxParam(h.hInst, MAKEINTRESOURCE(IDD_DIALOG1), h.hWnd, DxDialogProc, (LPARAM)&s);
}

REGISTER_PAD(APINAME, DInputPad);
#undef APINAME
}} //namespace
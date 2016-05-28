#include "../../USB.h"
#include "../padproxy.h"
#include "../../Win32/Config-win32.h"
#include "global.h"
#include "dialog.h"

#define APINAME "dinput"

//externs
/*extern void PollDevices();
extern float GetControl(int port, int id,  bool axisbutton=true);
extern void SetSpringForce(LONG magnitude);
extern void SetConstantForce(LONG magnitude);
extern void AutoCenter(int jid, bool onoff);
extern void DisableConstantForce();
extern int FFBindex;
extern DWORD INVERTFORCES[2];
extern void InitDI(int port);
extern void FreeDirectInput();
extern DWORD BYPASSCAL;*/

//struct generic_data_t	generic_data;
static struct wheel_data_t	wheel_data;
static struct ff_data	ffdata;

static bool bdown=false;
static DWORD calibrationtime = 0;
static int calidata = 0;
static bool alternate = false;
static bool calibrating = false;
static bool sendCrap = false;

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

class DInputPad : public Pad
{
public:
	DInputPad(int port): mPort(port) {}
	~DInputPad() { FreeDirectInput(); }
	int Open();
	int Close();
	int TokenIn(uint8_t *buf, int len);
	int TokenOut(const uint8_t *data, int len);
	int Reset() { return 0; }

	static const TCHAR* Name()
	{
		return L"DInput";
	}

	static int Configure(int port, void *data);
	static std::vector<CONFIGVARIANT> GetSettings()
	{
		//TODO GetSettings()
		return std::vector<CONFIGVARIANT>();
	}
protected:
	int mPort;
};

static inline int range_max(PS2WheelTypes type)
{
	if(type == WT_DRIVING_FORCE_PRO)
		return 0x3FFF;
	return 0x3FF;
}

int DInputPad::TokenIn(uint8_t *buf, int len)
{
	int range = range_max(mType);

	// Setting to unpressed
	ZeroMemory(&wheel_data, sizeof(wheel_data_t));
	wheel_data.steering = range >> 1;
	wheel_data.clutch = 0xFF;
	wheel_data.throttle = 0xFF;
	wheel_data.brake = 0xFF;
	wheel_data.hatswitch = 0x8;

	//TODO Atleast GT4 detects DFP then
	if(sendCrap)
	{
		pad_copy_data(mType, buf, wheel_data);
		sendCrap = false;
		return len;
	}

	PollDevices();

	//Allow in both ports but warn in configure dialog that only one DX wheel is supported for now
	//if(idx == 0){
		//wheel_data.steering = 8191 + (int)(GetControl(STEERING, false)* 8191.0f) ;
		
		if(calibrating){
			//Alternate full extents
			if (alternate)calidata--;
			else calidata++;

			if(calidata>range-1 || calidata < 1) alternate = !alternate;  //invert

			wheel_data.steering = calidata;		//pass fake

			//breakout after 11 seconds
			if(GetTickCount()-calibrationtime > 11000){
				calibrating = false;
				wheel_data.steering = range >> 1;
			}
		}else{
			wheel_data.steering = (range>>1)+(int)(GetControl(mPort, STEERING, false)* (float)(range>>1)) ;
		}

		wheel_data.throttle = 255-(int)(GetControl(mPort, THROTTLE, false)*255.0f);
		wheel_data.brake = 255-(int)(GetControl(mPort, BRAKE, false)*255.0f);

		if(GetControl(mPort, CROSS))		wheel_data.buttons |= 1 << convert_wt_btn(mType, PAD_CROSS);
		if(GetControl(mPort, SQUARE))		wheel_data.buttons |= 1 << convert_wt_btn(mType, PAD_SQUARE);
		if(GetControl(mPort, CIRCLE))		wheel_data.buttons |= 1 << convert_wt_btn(mType, PAD_CIRCLE);
		if(GetControl(mPort, TRIANGLE))		wheel_data.buttons |= 1 << convert_wt_btn(mType, PAD_TRIANGLE);
		if(GetControl(mPort, R1))			wheel_data.buttons |= 1 << convert_wt_btn(mType, PAD_R1);
		if(GetControl(mPort, L1))			wheel_data.buttons |= 1 << convert_wt_btn(mType, PAD_L1);
		if(GetControl(mPort, R2))			wheel_data.buttons |= 1 << convert_wt_btn(mType, PAD_R2);
		if(GetControl(mPort, L2))			wheel_data.buttons |= 1 << convert_wt_btn(mType, PAD_L2);

		if(GetControl(mPort, SELECT))		wheel_data.buttons |= 1 << convert_wt_btn(mType, PAD_SELECT);
		if(GetControl(mPort, START))		wheel_data.buttons |= 1 << convert_wt_btn(mType, PAD_START);
		if(GetControl(mPort, R3))			wheel_data.buttons |= 1 << convert_wt_btn(mType, PAD_R3);
		if(GetControl(mPort, L3))			wheel_data.buttons |= 1 << convert_wt_btn(mType, PAD_L3);

		//diagonal
		if(GetControl(mPort, HATUP, true)  && GetControl(mPort, HATRIGHT, true))
			wheel_data.hatswitch = 1;
		if(GetControl(mPort, HATRIGHT, true) && GetControl(mPort, HATDOWN, true))
			wheel_data.hatswitch = 3;
		if(GetControl(mPort, HATDOWN, true) && GetControl(mPort, HATLEFT, true))
			wheel_data.hatswitch = 5;
		if(GetControl(mPort, HATLEFT, true) && GetControl(mPort, HATUP, true))
			wheel_data.hatswitch = 7;

		//regular
		if(wheel_data.hatswitch==0x8){
			if(GetControl(mPort, HATUP, true))
				wheel_data.hatswitch = 0;
			if(GetControl(mPort, HATRIGHT, true))
				wheel_data.hatswitch = 2;
			if(GetControl(mPort, HATDOWN, true))
				wheel_data.hatswitch = 4;
			if(GetControl(mPort, HATLEFT, true))
				wheel_data.hatswitch = 6;
		}

		pad_copy_data(mType, buf, wheel_data);
	//} //if(idx ...
	return len;
}

int DInputPad::TokenOut(const uint8_t *data, int len)
{
	ff_data *ffdata = (ff_data*)data;

	OSDebugOut(TEXT("FFB %02X, %02X, %02X, %02X : %02X, %02X, %02X, %02X\n"),
		ffdata->reportid, ffdata->index, ffdata->data1, ffdata->data2,
		ffdata->data_ext1, ffdata->data_ext2, ffdata->data_ext3, ffdata->data_ext4);

	if (ffdata->reportid != CMD_EXTENDED_CMD)
	{

		uint8_t slot = ffdata->reportid & 0xF0;
		uint8_t cmd  = ffdata->reportid & 0x0F;

		switch (cmd)
		{
			case CMD_DOWNLOAD_AND_PLAY: //0x01
			{
				switch (slot)
				{
				case 0x10:
				case 0xF0:
					if (!calibrating)
					{
						if (ffdata->index == FTYPE_VARIABLE)
							SetConstantForce(mPort, ffdata->data1);
						else if (ffdata->index == FTYPE_CONSTANT)
							SetConstantForce(mPort, ffdata->data_ext1); //DF/GTF and GT3
						else
							OSDebugOut(TEXT("CMD_DOWNLOAD_AND_PLAY: unhandled type 0x%02X:0x02X\n"), slot, ffdata->index);
					}
					break;
				case 0x20:
					if (ffdata->index == 0xB) // hi res spring?
					{
						//if(!calibrating){
						//SetConstantForce(ffdata->data1);
						//TODO spring effect is currently not started 
						SetSpringForce(mPort, ffdata->data1);
						//}
					}
					else if (ffdata->index == FTYPE_CONSTANT)
						SetConstantForce(mPort, ffdata->data1);
					else if (ffdata->index == FTYPE_SPRING)
						SetSpringForce(mPort, ffdata->data1);
					break;
				default:
					OSDebugOut(TEXT("CMD_DOWNLOAD_AND_PLAY: unhandled slot 0x%02X\n"), slot);
					break;
				}
			}
			break;
			case CMD_STOP: //0x03
			{
				switch (slot)
				{
					case 0xF0: //0xF3, usually sent on init
					{
						if (BYPASSCAL)
						{
							alternate=false;
							calidata=0;
							calibrating = true;
							calibrationtime = GetTickCount();
						}
						SetConstantForce(mPort, 127);
					}
					break;
					case 0x10:
					case 0x20:
					{
						//some games issue this command on pause
						//if(ffdata->reportid == 0x13 && ffdata->data2 == 0)break;
						if (ffdata->index == 0x8)
							SetConstantForce(mPort, 127); //data1 looks like previous force sent with reportid 0x11
						else if (ffdata->index == FTYPE_AUTO_CENTER_SPRING)
							SetSpringForce(mPort, 127);
						else if (ffdata->index == FTYPE_HIGH_RESOLUTION_SPRING)
							SetSpringForce(mPort, 127);
					}
					break;
					default:
						OSDebugOut(TEXT("CMD_STOP: unhandled slot 0x%02X\n"), slot);
					break;
				}
			}
			break;
			case CMD_DEFAULT_SPRING_ON: //0x04
				OSDebugOut(TEXT("CMD_DEFAULT_SPRING_ON: unhandled cmd\n"));
				break;
			case CMD_DEFAULT_SPRING_OFF: //0x05
			{
				if (slot == 0xF0) {
					//just release force
					SetConstantForce(mPort, 127);
				}
				else
				{
					OSDebugOut(TEXT("CMD_DEFAULT_SPRING_OFF: unhandled slot 0x%02X\n"), slot);
				}
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
		//if(ffdata->index == 5) //TODO
		//	sendCrap = true;
		if (ffdata->index == EXT_CMD_WHEEL_RANGE_900_DEGREES) {}
		if (ffdata->index == EXT_CMD_WHEEL_RANGE_200_DEGREES) {}
		OSDebugOut(TEXT("CMD_EXTENDED: unhandled cmd 0x%02X%02X%02X\n"),
			ffdata->index, ffdata->data1, ffdata->data2);
	}
	return len;
}

int DInputPad::Open()
{
	InitDI(mPort);
	return 0;
}

int DInputPad::Close()
{
	FreeDirectInput();
	return 0;
}

int DInputPad::Configure(int port, void *data)
{
	Win32Handles h = *(Win32Handles*)data;
	return DialogBoxParam(h.hInst, MAKEINTRESOURCE(IDD_DIALOG1), h.hWnd, DxDialogProc, port);
}

REGISTER_PAD(APINAME, DInputPad);
#undef APINAME

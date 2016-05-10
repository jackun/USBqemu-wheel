#include "../../USB.h"
#include "../padproxy.h"

#define APINAME "dinput"

//externs
extern void PollDevices();
extern float GetControl(int id,  bool axisbutton=true);
extern void SetSpringForce(LONG magnitude);
extern void SetConstantForce(LONG magnitude);
extern void AutoCenter(int jid, bool onoff);
extern void DisableConstantForce();
extern int FFBindex;
extern DWORD INVERTFORCES;
extern void InitDI();
extern void FreeDirectInput();
extern DWORD BYPASSCAL;

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

	static const wchar_t* Name()
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
	wheel_data.axis_x = range >> 1;
	wheel_data.axis_y = 0xFF;
	wheel_data.axis_z = 0xFF;
	wheel_data.axis_rz = 0xFF;
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
		//wheel_data.axis_x = 8191 + (int)(GetControl(STEERING, false)* 8191.0f) ;
		
		//steering
		if(calibrating){
			//Alternate full extents
			if (alternate)calidata--;
			else calidata++;

			if(calidata>range-1 || calidata < 1) alternate = !alternate;  //invert

			wheel_data.axis_x = calidata;		//pass fake

			//breakout after 11 seconds
			if(GetTickCount()-calibrationtime > 11000){
				calibrating = false;
				wheel_data.axis_x = range >> 1;
			}
		}else{
			wheel_data.axis_x = (range>>1)+(int)(GetControl(STEERING, false)* (float)(range>>1)) ;
		}

		//throttle
		wheel_data.axis_z = 255-(int)(GetControl(THROTTLE, false)*255.0f);
		//brake
		wheel_data.axis_rz = 255-(int)(GetControl(BRAKE, false)*255.0f);

		if(GetControl(CROSS))		wheel_data.buttons |= 1 << convert_wt_btn(mType, PAD_CROSS);
		if(GetControl(SQUARE))		wheel_data.buttons |= 1 << convert_wt_btn(mType, PAD_SQUARE);
		if(GetControl(CIRCLE))		wheel_data.buttons |= 1 << convert_wt_btn(mType, PAD_CIRCLE);
		if(GetControl(TRIANGLE))	wheel_data.buttons |= 1 << convert_wt_btn(mType, PAD_TRIANGLE);
		if(GetControl(R1))			wheel_data.buttons |= 1 << convert_wt_btn(mType, PAD_R1);
		if(GetControl(L1))			wheel_data.buttons |= 1 << convert_wt_btn(mType, PAD_L1);
		if(GetControl(R2))			wheel_data.buttons |= 1 << convert_wt_btn(mType, PAD_R2);
		if(GetControl(L2))			wheel_data.buttons |= 1 << convert_wt_btn(mType, PAD_L2);

		if(GetControl(SELECT))		wheel_data.buttons |= 1 << convert_wt_btn(mType, PAD_SELECT);
		if(GetControl(START))		wheel_data.buttons |= 1 << convert_wt_btn(mType, PAD_START);
		if(GetControl(R3))			wheel_data.buttons |= 1 << convert_wt_btn(mType, PAD_R3);
		if(GetControl(L3))			wheel_data.buttons |= 1 << convert_wt_btn(mType, PAD_L3);

		//diagonal
		if(GetControl(HATUP, true)  && GetControl(HATRIGHT, true))
			wheel_data.hatswitch = 1;
		if(GetControl(HATRIGHT, true) && GetControl(HATDOWN, true))
			wheel_data.hatswitch = 3;
		if(GetControl(HATDOWN, true) && GetControl(HATLEFT, true))
			wheel_data.hatswitch = 5;
		if(GetControl(HATLEFT, true) && GetControl(HATUP, true))
			wheel_data.hatswitch = 7;

		//regular
		if(wheel_data.hatswitch==0x8){
			if(GetControl(HATUP, true))
				wheel_data.hatswitch = 0;
			if(GetControl(HATRIGHT, true))
				wheel_data.hatswitch = 2;
			if(GetControl(HATDOWN, true))
				wheel_data.hatswitch = 4;
			if(GetControl(HATLEFT, true))
				wheel_data.hatswitch = 6;
		}

		pad_copy_data(mType, buf, wheel_data);
	//} //if(idx ...
	return len;
}

int DInputPad::TokenOut(const uint8_t *data, int len)
{
	//TODO just cast data to struct pointer, maybe
	memcpy(&ffdata, data, sizeof(ffdata));

	switch(ffdata.reportid)
	{
		case 0xF8:
			//TODO needed?
			if(ffdata.index == 5)
				sendCrap = true;
		break;
		case 9:
			{
				//not handled
			}
			break;
		case 19:
			//some games issue this command on pause
			//if(ffdata.reportid == 19 && ffdata.data2 == 0)break;
			if(ffdata.index == 0x8)
				SetConstantForce(127); //data1 looks like previous force sent with reportid 0x11
			//TODO unset spring
			else if(ffdata.index == 3)
				SetSpringForce(127);

			//fprintf(stderr, "FFB 0x%X, 0x%X, 0x%X\n", ffdata.reportid, ffdata.index, ffdata.data1);
			break;
		case 17://constant force
			{
				//handle calibration commands
				if(!calibrating){SetConstantForce(ffdata.data1);}
			}
			break;
		case 0x21:
			if(ffdata.index == 0xB)
			{
				//if(!calibrating){
					//SetConstantForce(ffdata.data1);
					SetSpringForce(ffdata.data1); //spring is broken?
				//}
				break;
			}
			//drop through
		case 254://autocenter?
		case 255://autocenter?
		case 244://autocenter?
		case 245://autocenter?
			{
					//just release force
					SetConstantForce(127);
			}
			break;
		case 241:
			//DF/GTF and GT3
			if(!calibrating){SetConstantForce(ffdata.pad1);}
			break;
		case 243://initialize
			{
				if(BYPASSCAL){
					alternate=false;
					calidata=0;
					calibrating = true;
					calibrationtime = GetTickCount();
				}
			}
			break;
	}
	return len;
}

int DInputPad::Open()
{
	InitDI();
	return 0;
}

int DInputPad::Close()
{
	FreeDirectInput();
	return 0;
}

int DInputPad::Configure(int port, void *data)
{
	return RESULT_CANCELED;
}

REGISTER_PAD(APINAME, DInputPad);
#undef APINAME

#include "../../USB.h"
#include "../usb-pad.h"

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

typedef struct Win32PADState {
	PADState padState;

} Win32PADState;

typedef enum CONTROLID
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

static inline int range_max(uint32_t idx)
{
	int type = conf.WheelType[idx];
	if(type == WT_DRIVING_FORCE_PRO)
		return 0x3FFF;
	return 0x3FF;
}

static int usb_pad_poll(PADState *ps, uint8_t *buf, int len)
{
	Win32PADState *s = (Win32PADState*) ps;
	uint8_t idx = 1 - s->padState.port;
	if(idx>1) return 0;
	int range = range_max(idx);

	// Setting to unpressed
	ZeroMemory(&wheel_data, sizeof(wheel_data_t));
	wheel_data.axis_x = range >> 1;
	wheel_data.axis_y = 0xFF;
	wheel_data.axis_z = 0xFF;
	wheel_data.axis_rz = 0xFF;
	wheel_data.hatswitch = 0x8;

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


		if(GetControl(CROSS))		wheel_data.buttons |= 1 << PAD_CROSS;
		if(GetControl(SQUARE))		wheel_data.buttons |= 1 << PAD_SQUARE;
		if(GetControl(CIRCLE))		wheel_data.buttons |= 1 << PAD_CIRCLE;
		if(GetControl(TRIANGLE))	wheel_data.buttons |= 1 << PAD_TRIANGLE;

		if(conf.WheelType[idx] == WT_GENERIC) {
			//DF (and GT Force?) flip Rs and Ls
			if(GetControl(R1))			wheel_data.buttons |= 1 << PAD_R2;
			if(GetControl(L1))			wheel_data.buttons |= 1 << PAD_L2;
			if(GetControl(R2))			wheel_data.buttons |= 1 << PAD_R1;
			if(GetControl(L2))			wheel_data.buttons |= 1 << PAD_L1;
		} else {
			//DF Pro
			if(GetControl(R1))			wheel_data.buttons |= 1 << PAD_R1;
			if(GetControl(L1))			wheel_data.buttons |= 1 << PAD_L1;
			if(GetControl(R2))			wheel_data.buttons |= 1 << PAD_R2;
			if(GetControl(L2))			wheel_data.buttons |= 1 << PAD_L2;
		}

		if(GetControl(SELECT))		wheel_data.buttons |= 1 << PAD_SELECT;
		if(GetControl(START))		wheel_data.buttons |= 1 << PAD_START;
		if(GetControl(R3))			wheel_data.buttons |= 1 << PAD_R3;
		if(GetControl(L3))			wheel_data.buttons |= 1 << PAD_L3;

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

		//memcpy(buf, &generic_data, sizeof(generic_data));
		pad_copy_data(idx, buf, wheel_data);
	//} //if(idx ...
	return len;
}

static int token_out(PADState *ps, uint8_t *data, int len)
{

	Win32PADState *s = (Win32PADState*) ps;
	uint8_t idx = 1 - s->padState.port;
	if(idx>1) return 0;

	memcpy(&ffdata,data, sizeof(ffdata));
	//if(idx!=0)return 0;

	switch(ffdata.reportid)
	{
		case 9:
			{
				//not handled
			}
			break;
		case 19:
		case 17://constant force
			{
				//some games issue this command on pause
				if(ffdata.reportid == 19 && ffdata.data2 == 0)break;

				//handle calibration commands
				if(!calibrating){SetConstantForce(ffdata.data1);}
			}
			break;

		case 254://autocenter?
		case 255://autocenter?
		case 244://autocenter?
		case 245://autocenter?
			{
					//just release force
					SetConstantForce(127);
			}
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

static void destroy_pad(PADState *ps)
{
	if(ps)
		free(ps);
}

static void open()
{
	InitDI();
}

static void close()
{
	FreeDirectInput();
}

PADState* get_new_dx_padstate()
{
	PADState *s = (PADState*)qemu_mallocz(sizeof(Win32PADState));

	s->dev.open = open;
	s->dev.close = close;

	s->destroy_pad = destroy_pad;
	s->token_out = token_out;
	s->usb_pad_poll = usb_pad_poll;
	s->find_pad = NULL;
	return s;
}

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

struct generic_data_t	generic_data;
struct ff_data	ffdata;

bool bdown=false;
DWORD calibrationtime = 0;
int calidata = 0;
bool alternate = false;
bool calibrating = false;

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

static int usb_pad_poll(PADState *ps, uint8_t *buf, int len)
{
	Win32PADState *s = (Win32PADState*) ps;
	uint8_t idx = 1 - s->padState.port;
	if(idx>1) return 0;


	// Setting to unpressed
	ZeroMemory(&generic_data, sizeof(generic_data_t));
	generic_data.axis_x = 0x3FF >> 1;
	generic_data.axis_y = 0xFF;
	generic_data.axis_z = 0xFF;
	generic_data.axis_rz = 0xFF;
	generic_data.hatswitch = 0x8;

	PollDevices();

	//Allow in both ports but warn in configure dialog that only one DX wheel is supported for now
	//if(idx == 0){
		//generic_data.axis_x = 8191 + (int)(GetControl(STEERING, false)* 8191.0f) ;
		
		//steering
		if(calibrating){
			//Alternate full extents
			if (alternate)calidata--;
			else calidata++;

			if(calidata>1022 || calidata < 1) alternate = !alternate;  //invert

			generic_data.axis_x = calidata;		//pass fake

			//breakout after 11 seconds
			if(GetTickCount()-calibrationtime > 11000){
				calibrating = false;
				generic_data.axis_x = 511;
			}
		}else{
			generic_data.axis_x = 511+(int)(GetControl(STEERING, false)*511.0f) ;
		}

		//throttle
		generic_data.axis_z = 255-(int)(GetControl(THROTTLE, false)*255.0f);
		//brake
		generic_data.axis_rz = 255-(int)(GetControl(BRAKE, false)*255.0f);


		if(GetControl(CROSS))		generic_data.buttons = generic_data.buttons | 1;
		if(GetControl(SQUARE))		generic_data.buttons = generic_data.buttons | 2;
		if(GetControl(CIRCLE))		generic_data.buttons = generic_data.buttons | 4;
		if(GetControl(TRIANGLE))	generic_data.buttons = generic_data.buttons | 8;
		if(GetControl(R1))			generic_data.buttons = generic_data.buttons | 16;
		if(GetControl(L1))			generic_data.buttons = generic_data.buttons | 32;
		if(GetControl(R2))			generic_data.buttons = generic_data.buttons | 64;
		if(GetControl(L2))			generic_data.buttons = generic_data.buttons | 128;
		if(GetControl(SELECT))		generic_data.buttons = generic_data.buttons | 256;
		if(GetControl(START))		generic_data.buttons = generic_data.buttons | 512;
		if(GetControl(R3))			generic_data.buttons = generic_data.buttons | 1024;
		if(GetControl(L3))			generic_data.buttons = generic_data.buttons | 2048;

		//diagonal
		if(GetControl(HATUP, true)  && GetControl(HATRIGHT, true))
			generic_data.hatswitch = 1;
		if(GetControl(HATRIGHT, true) && GetControl(HATDOWN, true))
			generic_data.hatswitch = 3;
		if(GetControl(HATDOWN, true) && GetControl(HATLEFT, true))
			generic_data.hatswitch = 5;
		if(GetControl(HATLEFT, true) && GetControl(HATUP, true))
			generic_data.hatswitch = 7;

		//regular
		if(generic_data.hatswitch==0x8){
			if(GetControl(HATUP, true))
				generic_data.hatswitch = 0;
			if(GetControl(HATRIGHT, true))
				generic_data.hatswitch = 2;
			if(GetControl(HATDOWN, true))
				generic_data.hatswitch = 4;
			if(GetControl(HATLEFT, true))
				generic_data.hatswitch = 6;

		}

		memcpy(buf, &generic_data, sizeof(generic_data));
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
		case 9://mode switch?
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

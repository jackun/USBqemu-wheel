#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <atomic>
#include "usb-pad/lg/lg_ff.h"

#define SAFE_DELETE(p)  { if(p) { delete (p);     (p)=NULL; } }
#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=NULL; } }

#define SAMPLE_BUFFER_SIZE 16
namespace usb_pad { namespace dx {

static std::atomic<int> refCount (0);

LPDIRECTINPUT8       g_pDI       = NULL;
LPDIRECTINPUTDEVICE8 g_pKeyboard = NULL;
LPDIRECTINPUTDEVICE8 g_pMouse	 = NULL;
LPDIRECTINPUTDEVICE8 g_pJoysticks[10] = {NULL};

DWORD rgdwAxes[1] = { DIJOFS_X };
LONG  rglDirection[1] = { 0 };

//only two effect (constant force, spring)
bool FFB[2] = { false };
int FFBindex[2] = { -1 };
LPDIRECTINPUTEFFECT  g_pEffect[2] = { NULL };
LPDIRECTINPUTEFFECT  g_pEffectSpring[2] = { NULL };
LPDIRECTINPUTEFFECT  g_pEffectFriction[2] = { NULL }; //DFP mode only
LPDIRECTINPUTEFFECT  g_pEffectRamp[2] = { NULL };
LPDIRECTINPUTEFFECT  g_pEffectDamper[2] = { NULL };
DWORD g_dwNumForceFeedbackAxis[4] = {0};
DIEFFECT eff;
DIEFFECT effSpring;
DIEFFECT effFriction;
DIEFFECT effRamp;
DIEFFECT effDamper;
DICONSTANTFORCE cfw;
DICONDITION cSpring;
DICONDITION cFriction;
DIRAMPFORCE cRamp;
DICONDITION cDamper;

BYTE diks[256];							// DirectInput keyboard state buffer
DIMOUSESTATE2 dims2;					// DirectInput mouse state structure
DIJOYSTATE2 js[10] = {0};			// DInput joystick state
DIJOYSTATE2 jso[10] = {0};           // DInput joystick old state
DIJOYSTATE2 jsi[10] = {0};           // DInput joystick initial state


DWORD numj = 0;							//current attached joysticks
DWORD maxj = 10;						//maximum attached joysticks
const DWORD PRECMULTI = 100;

//dinput control mappings

const DWORD numc = 20; //total control maps

LONG AXISID[2][numc] = { { 0 } };
LONG INVERT[2][numc] = { { 0 } };
LONG HALF[2][numc] = { { 0 } };
LONG BUTTON[2][numc] = { { 0 } };
LONG LINEAR[2][numc] = { { 0 } };
LONG OFFSET[2][numc] = { { 0 } };
LONG DEADZONE[2][numc] = { { 0 } };
LONG GAINZ[2][1] = { { 0 } };
LONG FFMULTI[2][1] = { { 0 } };
DWORD INVERTFORCES[2] = { 0 };

bool listening = false;
DWORD listenend = 0;
DWORD listennext = 0;
DWORD listentimeout = 10000;
DWORD listeninterval = 500;

bool didDIinit = false;					//we have a handle

void ReleaseFFB(int port)
{
	if (g_pEffect[port])
		g_pEffect[port]->Stop();
	if (g_pEffectSpring[port])
		g_pEffectSpring[port]->Stop();
	if (g_pEffectFriction[port])
		g_pEffectFriction[port]->Stop();
	if (g_pEffectRamp[port])
		g_pEffectRamp[port]->Stop();
	if (g_pEffectDamper[port])
		g_pEffectDamper[port]->Stop();

	SAFE_RELEASE(g_pEffect[port]);
	SAFE_RELEASE(g_pEffectSpring[port]);
	SAFE_RELEASE(g_pEffectFriction[port]);
	SAFE_RELEASE(g_pEffectRamp[port]);
	SAFE_RELEASE(g_pEffectDamper[port]);

	FFBindex[port] = -1;
	FFB[port] = false;
}

void CreateFFB(int port, DWORD joy)
{
	HRESULT hres = 0;
	ReleaseFFB(port);

	if (joy >= numj)
		return;

	try {
		//create the constant force effect
		ZeroMemory(&eff, sizeof(eff));
		ZeroMemory(&effSpring, sizeof(effSpring));
		ZeroMemory(&effFriction, sizeof(effFriction));
		ZeroMemory(&cfw, sizeof(cfw));
		ZeroMemory(&cSpring, sizeof(cSpring));
		ZeroMemory(&cFriction, sizeof(cFriction));
		ZeroMemory(&cRamp, sizeof(cRamp));

		//constantforce
		eff.dwSize = sizeof(DIEFFECT);
		eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
		eff.dwSamplePeriod = 0;
		eff.dwGain = MIN(MAX(GAINZ[port][0], 0), 10000);
		eff.dwTriggerButton = DIEB_NOTRIGGER;
		eff.dwTriggerRepeatInterval = 0;
		eff.cAxes = countof(rgdwAxes);
		eff.rgdwAxes = rgdwAxes;
		eff.rglDirection = rglDirection;
		eff.dwStartDelay = 0;
		eff.dwDuration = INFINITE;

		// copy default values
		effSpring = eff;
		effFriction = eff;
		effRamp = eff;
		effDamper = eff;

		cfw.lMagnitude = 0;

		eff.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
		eff.lpvTypeSpecificParams = &cfw;
		hres = g_pJoysticks[joy]->CreateEffect(GUID_ConstantForce, &eff, &g_pEffect[port], NULL);

		cSpring.lNegativeCoefficient = 0;
		cSpring.lPositiveCoefficient = 0;

		effSpring.cbTypeSpecificParams = sizeof(DICONDITION);
		effSpring.lpvTypeSpecificParams = &cSpring;
		hres = g_pJoysticks[joy]->CreateEffect(GUID_Spring, &effSpring, &g_pEffectSpring[port], NULL);

		effFriction.cbTypeSpecificParams = sizeof(DICONDITION);
		effFriction.lpvTypeSpecificParams = &cFriction;
		hres = g_pJoysticks[joy]->CreateEffect(GUID_Friction, &effFriction, &g_pEffectFriction[port], NULL);

		effRamp.cbTypeSpecificParams = sizeof(DIRAMPFORCE);
		effRamp.lpvTypeSpecificParams = &cRamp;
		hres = g_pJoysticks[joy]->CreateEffect(GUID_RampForce, &effRamp, &g_pEffectRamp[port], NULL);

		effDamper.cbTypeSpecificParams = sizeof(DICONDITION);
		effDamper.lpvTypeSpecificParams = &cDamper;
		hres = g_pJoysticks[joy]->CreateEffect(GUID_Damper, &effDamper, &g_pEffectDamper[port], NULL);

		FFBindex[port] = joy;
		FFB[port] = true;
	}
	catch (...) {};

	//start the effect
	//SetSpringForce(10000);
	if (g_pEffect[port])
	{
		OSDebugOut(TEXT("DINPUT: Start Effect\n"));
		g_pEffect[port]->SetParameters(&eff, DIEP_START | DIEP_GAIN | DIEP_AXES | DIEP_DIRECTION);
	}
}

void FreeDirectInput()
{
	if (!refCount || --refCount > 0) return;

	numj=0;  //no joysticks

    // Unacquire the device one last time just in case
    // the app tried to exit while the device is still acquired.
    if( g_pKeyboard )
        g_pKeyboard->Unacquire();
	if( g_pMouse )
        g_pMouse->Unacquire();

	for(DWORD i=0; i<numj; i++){
		if( g_pJoysticks[i] )
			g_pJoysticks[i]->Unacquire();
	}

	ReleaseFFB(0);
	ReleaseFFB(1);

    // Release any DirectInput objects.
    SAFE_RELEASE( g_pKeyboard );
    SAFE_RELEASE( g_pMouse );
	for(DWORD i=0; i<numj; i++) SAFE_RELEASE( g_pJoysticks[i] );
    SAFE_RELEASE( g_pDI );
	didDIinit=false;
}
BOOL CALLBACK EnumJoysticksCallback( const DIDEVICEINSTANCE* pdidInstance,
                                     VOID* pContext )
{
    HRESULT hr;

    // Obtain an interface to the enumerated joystick.
    hr = g_pDI->CreateDevice( pdidInstance->guidInstance, &g_pJoysticks[numj], NULL );
	numj++;

	//too many joysticks please stop
	if (numj >= maxj)return DIENUM_STOP;

	return DIENUM_CONTINUE;


}
BOOL CALLBACK EnumObjectsCallback( const DIDEVICEOBJECTINSTANCE* pdidoi,
                                   VOID* pContext )
{
    HRESULT hr;
    LPDIRECTINPUTDEVICE8 pWheel = (LPDIRECTINPUTDEVICE8)pContext;
    // For axes that are returned, set the DIPROP_RANGE property for the
    // enumerated axis in order to scale min/max values.
    if( pdidoi->dwType & DIDFT_AXIS )
    {
        DIPROPRANGE diprg;
        diprg.diph.dwSize       = sizeof(DIPROPRANGE);
        diprg.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        diprg.diph.dwHow        = DIPH_BYID;
        diprg.diph.dwObj        = pdidoi->dwType; // Specify the enumerated axis
        diprg.lMin              = 0;
        diprg.lMax              = 65535;

        // Set the range for the axis  (not used, DX defaults 65535 all axis)
        if (FAILED(hr = pWheel->SetProperty(DIPROP_RANGE, &diprg.diph)))
            return DIENUM_STOP;

        //DIPROPDWORD dipdw;
        //dipdw.diph.dwSize = sizeof(DIPROPDWORD);
        //dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        //dipdw.diph.dwHow = DIPH_BYID; //DIPH_DEVICE;
        //dipdw.diph.dwObj = pdidoi->dwType; //0;
        //dipdw.dwData = DIPROPAXISMODE_ABS;

        //if (FAILED(hr = pWheel->SetProperty(DIPROP_AXISMODE, &dipdw.diph)))
        //    return DIENUM_CONTINUE;

        //dipdw.dwData = 0;
        //if (FAILED(hr = pWheel->SetProperty(DIPROP_DEADZONE, &dipdw.diph)))
        //    return DIENUM_CONTINUE;

        //dipdw.dwData = DI_FFNOMINALMAX;
        //if (FAILED(hr = pWheel->SetProperty(DIPROP_SATURATION, &dipdw.diph)))
        //    return DIENUM_CONTINUE;
    }

    return DIENUM_CONTINUE;
}


//read all joystick states
void PollDevices()
{
	HRESULT hr;

	//KEYBOARD
	if( g_pKeyboard )
	{
		ZeroMemory( diks, sizeof(diks) );
		hr = g_pKeyboard->GetDeviceState( sizeof(diks), diks );
		if( FAILED(hr) )
		{
			g_pKeyboard->Acquire();
		}
	}

	//MOUSE
	if( g_pMouse )
	{
		ZeroMemory( &dims2, sizeof(dims2) );
		hr = g_pMouse->GetDeviceState( sizeof(DIMOUSESTATE2), &dims2 );

		if( FAILED(hr) )
			g_pMouse->Acquire();
	}

	//JOYSTICK
	for(DWORD i=0; i<numj; i++){
		if( g_pJoysticks[i] )
		{
			hr = g_pJoysticks[i]->Poll();
			if( FAILED(hr) )
			{
				g_pJoysticks[i]->Acquire();

			}
			else
			{
				g_pJoysticks[i]->GetDeviceState( sizeof(DIJOYSTATE2), &js[i] );

			}
		}
	}
}

//checks all devices for digital button id/index
bool KeyDown(DWORD KeyID)
{


	//Check keyboard
	if (KeyID < 256)
	{
		if( g_pKeyboard )
			if ( diks[KeyID] & 0x80 )
				return true;
	}
	//Check mouse
	if (KeyID > 256 && KeyID < 265)
	{
		if( g_pMouse )
			if ( dims2.rgbButtons[KeyID - 257] & 0x80 )
				return true;
	}


	//Check each joystick button (64 possibilities with 10 joysticks)
	if (KeyID > 264 && KeyID < 905)
	{
		int i = (KeyID-265) / 64;  //get controller index
		int b = (KeyID-265) % 64; //get button index
		if( g_pJoysticks[i] )
		{
			if(b<32){
				if ( js[i].rgbButtons[b] & 0x80 )
				return true;
			}else{
				//hat switch cases (4 hat switches with 4 directions possible)  surely a better way... but this would allow funky joysticks to work.
				switch (b-32)
				{
					//pov 0
					case 1:
						if((js[i].rgdwPOV[0] <= 4500 || js[i].rgdwPOV[0] >= 31500) && js[i].rgdwPOV[0] != -1)
							return true;
						break;
					case 2:
						if(js[i].rgdwPOV[0] >= 4500 && js[i].rgdwPOV[0] <=  13500)
							return true;
						break;
					case 3:
						if(js[i].rgdwPOV[0] >= 13500 && js[i].rgdwPOV[0] <= 22500)
							return true;
						break;
					case 4:
						if(js[i].rgdwPOV[0] >= 22500 && js[i].rgdwPOV[0] <= 31500)
							return true;
						break;

					//pov 1
					case 5:
						if((js[i].rgdwPOV[1] <= 4500 || js[i].rgdwPOV[1] >= 31500) && js[i].rgdwPOV[1] != -1)
							return true;
						break;
					case 6:
						if(js[i].rgdwPOV[1] >= 4500 && js[i].rgdwPOV[1] <=  13500)
							return true;
						break;
					case 7:
						if(js[i].rgdwPOV[1] >= 13500 && js[i].rgdwPOV[1] <= 22500)
							return true;
						break;
					case 8:
						if(js[i].rgdwPOV[1] >= 22500 && js[i].rgdwPOV[1] <= 31500)
							return true;
						break;

					//pov 2
					case 9:
						if((js[i].rgdwPOV[2] <= 4500 || js[i].rgdwPOV[2] >= 31500) && js[i].rgdwPOV[2] != -1)
							return true;
						break;
					case 10:
						if(js[i].rgdwPOV[2] >= 4500 && js[i].rgdwPOV[2] <=  13500)
							return true;
						break;
					case 11:
						if(js[i].rgdwPOV[2] >= 13500 && js[i].rgdwPOV[2] <= 22500)
							return true;
						break;
					case 12:
						if(js[i].rgdwPOV[2] >= 22500 && js[i].rgdwPOV[2] <= 31500)
							return true;
						break;

					//pov 3
					case 13:
						if((js[i].rgdwPOV[3] <= 4500 || js[i].rgdwPOV[3] >= 31500) && js[i].rgdwPOV[3] != -1)
							return true;
						break;
					case 14:
						if(js[i].rgdwPOV[3] >= 4500 && js[i].rgdwPOV[3] <=  13500)
							return true;
						break;
					case 15:
						if(js[i].rgdwPOV[3] >= 13500 && js[i].rgdwPOV[3] <= 22500)
							return true;
						break;
					case 16:
						if(js[i].rgdwPOV[3] >= 22500 && js[i].rgdwPOV[3] <= 31500)
							return true;
						break;
					default:return false;break;//still 16 more reserved cases..
				}

			}
		}
	}
	//over max 904
	return false;
}




//the non-linear filter (input/output 0.0-1.0 only) (parameters -50 to +50)
float FilterControl(float input, LONG linear, LONG offset, LONG dead)
{

	//ugly, but it works gooood

	float hs=0;
	float linearf = float(linear) / PRECMULTI;
	if(linear>0){hs = (float)(1.0-((linearf*2) *(float)0.01));}	//format+shorten variable
	else{hs = (float)(1.0-(abs(linearf*2) *(float)0.01));}		//format+shorten variable
	float hs2 = (float)(offset+50 * PRECMULTI) / PRECMULTI * (float)0.01;				//format+shorten variable
	float v = input;											//format+shorten variable
	float d = float(dead) / PRECMULTI * (float)0.005;							//format+shorten variable

	//format and apply deadzone
	v=(v*(1.0f+(d*2.0f)))-d;

	//clamp
	if(v<0.0f)v=0.0f;
	if(v>1.0f)v=1.0f;

	//clamp negdead
	//if(v==-d)v=0.0;
	if (fabs(v + d) < DBL_EPSILON) v = 0.0;

	//possibilities
	float c1 = float(v - (1.0 - (pow((double)(1.0 - v) , (double)(1.0 / hs)))));
	float c2 = float(((v - pow(v , hs))));
	float c3 = float(v - (1.0 - (pow((double)(1.0 - v) , (double)hs))));
	float c4 = ((v - pow((float)v , (float)(1.0 / hs))));
	float res = 0;

	if(linear<0){res = v - (((1.0f - hs2) * c3) + (hs2 * c4));}		//get negative result
	else{res = v - (((1.0f - hs2) * c1) + (hs2 * c2));}				//get positive result

	//return our result
	return res;
}

//gets axis value from id/index (config only)
float ReadAxis(LONG axisid, LONG inverted, LONG initial)
{
	//could probably be condensed
	//TODO mouse axis

	float retval =  0;

	int i = axisid / 8;  //obtain controller index
	int ax = axisid % 8;  //obtain axis index

	if (initial > 60000) // origin somewhere near top
	{
		if(ax == 0) retval =  (65535-js[i].lX) * (1.0f/65535);
		if(ax == 1) retval =  (65535-js[i].lY) * (1.0f/65535);
		if(ax == 2) retval =  (65535-js[i].lZ) * (1.0f/65535);
		if(ax == 3) retval =  (65535-js[i].lRx) * (1.0f/65535);
		if(ax == 4) retval =  (65535-js[i].lRy) * (1.0f/65535);
		if(ax == 5) retval =  (65535-js[i].lRz) * (1.0f/65535);
		if(ax == 6) retval =  (65535-js[i].rglSlider[0]) * (1.0f/65535);
		if(ax == 7) retval =  (65535-js[i].rglSlider[1]) * (1.0f/65535);
	}
	if (initial > 26000 && initial < 38000)  // origin somewhere near center
	{
		if(inverted)
		{
			if(ax == 0) retval =  (js[i].lX-32767) * (1.0f/32767);
			if(ax == 1) retval =  (js[i].lY-32767) * (1.0f/32767);
			if(ax == 2) retval =  (js[i].lZ-32767) * (1.0f/32767);
			if(ax == 3) retval =  (js[i].lRx-32767) * (1.0f/32767);
			if(ax == 4) retval =  (js[i].lRy-32767) * (1.0f/32767);
			if(ax == 5) retval =  (js[i].lRz-32767) * (1.0f/32767);
			if(ax == 6) retval =  (js[i].rglSlider[0]-32767) * (1.0f/32767);
			if(ax == 7) retval =  (js[i].rglSlider[1]-32767) * (1.0f/32767);
		}else{
			if(ax == 0) retval =  (32767-js[i].lX) * (1.0f/32767);
			if(ax == 1) retval =  (32767-js[i].lY) * (1.0f/32767);
			if(ax == 2) retval =  (32767-js[i].lZ) * (1.0f/32767);
			if(ax == 3) retval =  (32767-js[i].lRx) * (1.0f/32767);
			if(ax == 4) retval =  (32767-js[i].lRy) * (1.0f/32767);
			if(ax == 5) retval =  (32767-js[i].lRz) * (1.0f/32767);
			if(ax == 6) retval =  (32767-js[i].rglSlider[0]) * (1.0f/32767);
			if(ax == 7) retval =  (32767-js[i].rglSlider[1]) * (1.0f/32767);
		}
	}
	if (initial >= 0 && initial < 4000) // origin somewhere near bottom
	{
		if(ax == 0) retval =  (js[i].lX) * (1.0f/65535);
		if(ax == 1) retval =  (js[i].lY) * (1.0f/65535);
		if(ax == 2) retval =  (js[i].lZ) * (1.0f/65535);
		if(ax == 3) retval =  (js[i].lRx) * (1.0f/65535);
		if(ax == 4) retval =  (js[i].lRy) * (1.0f/65535);
		if(ax == 5) retval =  (js[i].lRz) * (1.0f/65535);
		if(ax == 6) retval =  (js[i].rglSlider[0]) * (1.0f/65535);
		if(ax == 7) retval =  (js[i].rglSlider[1]) * (1.0f/65535);
	}

	if (retval < 0.0f) retval = 0.0f;

	return retval;
}
//using both above functions
float ReadAxisFiltered(int port, LONG id)
{
	return FilterControl(ReadAxis(AXISID[port][id], INVERT[port][id],HALF[port][id]), LINEAR[port][id], OFFSET[port][id], DEADZONE[port][id]);
}

//config only
void ListenUpdate()
{
	for(DWORD i=0; i<numj; i++){
		if( g_pJoysticks[i] ) {
			memcpy(&jso[i], &js[i],sizeof(DIJOYSTATE2));
		}
	}
	PollDevices();
}

//poll and store all joystick states for comparison (config only)
void ListenAxis()
{
	PollDevices();
	for(DWORD i=0; i<numj; i++){
		memcpy(&jso[i], &js[i],sizeof(DIJOYSTATE2));
		memcpy(&jsi[i], &js[i],sizeof(DIJOYSTATE2));
	}

	listenend = listentimeout+GetTickCount();
	listennext = GetTickCount();
	listening=true;
}

//get listen time left in ms (config only)
DWORD GetListenTimeout()
{
	return listenend-GetTickCount();
}

//compare all device axis for difference (config only)
bool AxisDown(LONG axisid, LONG & inverted, LONG & initial)
{
	//could probably be condensed
	//TODO mouse axis

		DWORD i = axisid / 8;
		if (i >= numj) return false;

		if( g_pJoysticks[i] ) {
			LONG detectrange = 2000;
			LONG lXdiff = js[i].lX - jso[i].lX;
			LONG lYdiff = js[i].lY - jso[i].lY;
			LONG lZdiff = js[i].lZ - jso[i].lZ;
			LONG lRxdiff = js[i].lRx - jso[i].lRx;
			LONG lRydiff = js[i].lRy - jso[i].lRy;
			LONG lRzdiff = js[i].lRz - jso[i].lRz;
			LONG lSlider1diff = js[i].rglSlider[0] - jso[i].rglSlider[0];
			LONG lSlider2diff = js[i].rglSlider[1] - jso[i].rglSlider[1];

			if(axisid % 8 == 0 && lXdiff > detectrange) { initial = jsi[i].lX; inverted = TRUE; return true;}
			if(axisid % 8 == 0 && lXdiff < -detectrange) {initial = jsi[i].lX; inverted = FALSE; return true;}

			if(axisid % 8 == 1 && lYdiff > detectrange) { initial = jsi[i].lY;inverted = TRUE; return true;}
			if(axisid % 8 == 1 && lYdiff < -detectrange) { initial = jsi[i].lY;inverted = FALSE; return true;}

			if(axisid % 8 == 2 && lZdiff > detectrange) { initial = jsi[i].lZ;inverted = TRUE; return true;}
			if(axisid % 8 == 2 && lZdiff < -detectrange) { initial = jsi[i].lZ;inverted = FALSE; return true;}

			if(axisid % 8 == 3 && lRxdiff > detectrange) { initial = jsi[i].lRx;inverted = TRUE; return true;}
			if(axisid % 8 == 3 && lRxdiff < -detectrange) {initial = jsi[i].lRx; inverted = FALSE; return true;}

			if(axisid % 8 == 4 && lRydiff > detectrange) { initial = jsi[i].lRy;inverted = TRUE; return true;}
			if(axisid % 8 == 4 && lRydiff < -detectrange) { initial = jsi[i].lRy;inverted = FALSE; return true;}

			if(axisid % 8 == 5 && lRzdiff > detectrange) { initial = jsi[i].lRz;inverted = TRUE; return true;}
			if(axisid % 8 == 5 && lRzdiff < -detectrange) { initial = jsi[i].lRz;inverted = FALSE; return true;}

			if(axisid % 8 == 6 && lSlider1diff > detectrange) { initial = jsi[i].rglSlider[0];inverted = TRUE; return true;}
			if(axisid % 8 == 6 && lSlider1diff < -detectrange) { initial = jsi[i].rglSlider[0];inverted = FALSE; return true;}

			if(axisid % 8 == 7 && lSlider2diff > detectrange) { initial = jsi[i].rglSlider[1];inverted = TRUE; return true;}
			if(axisid % 8 == 7 && lSlider2diff < -detectrange) { initial = jsi[i].rglSlider[1];inverted = FALSE; return true;}
		}
	return false;
	//max 160
}

//search all axis/buttons (config only)
bool FindControl(LONG port, LONG & axis,LONG & inverted, LONG & initial, LONG & button)
{
	if(listening==true){
		if(listenend>GetTickCount())
		{
			if(listennext<GetTickCount())
			{
				listennext = listeninterval+GetTickCount();
				ListenUpdate();
				for(int i = 0; i<160;i++){
					if(AxisDown(i, inverted, initial)){
						listening = false;
						axis=i;
						button=-1;
						if (axis % 8 == 0)
						{
							CreateFFB(port, axis / 8);
						}
						return true;
					}
				}
				for(int i = 0; i<904;i++){
					if(KeyDown(i)){
						listening = false;
						axis=-1;
						button=i;
						return true;
					}
				}
			}
		}else{
			listening=false;
			return false; //timedout
		}
	}
	return false;
}

void AutoCenter(int jid, bool onoff)
{
	if(jid<0)return;
	//disable the auto-centering spring.
	DIPROPDWORD dipdw;
 	dipdw.diph.dwSize       = sizeof(DIPROPDWORD);
	dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	dipdw.diph.dwObj        = 0;
	dipdw.diph.dwHow        = DIPH_DEVICE;
	dipdw.dwData            = onoff ? DIPROPAUTOCENTER_ON : DIPROPAUTOCENTER_OFF;

	g_pJoysticks[jid]->SetProperty( DIPROP_AUTOCENTER, &dipdw.diph );
}

//set left/right ffb torque
extern void InitDI(int port);
void SetConstantForce(int port, LONG magnitude)
{
	OSDebugOut(TEXT("constant force: %d\n"), magnitude);
	if (FFBindex[port] == -1) return;

	if(INVERTFORCES[port])
		cfw.lMagnitude = (127-magnitude) * DI_FFNOMINALMAX / 127;
	else
		cfw.lMagnitude = -(127-magnitude) * DI_FFNOMINALMAX / 127;

	if (FFMULTI[port][0] > 0)
		cfw.lMagnitude *= 1 + FFMULTI[port][0];

	if(g_pEffect[port]) {
		g_pEffect[port]->SetParameters(&eff, DIEP_TYPESPECIFICPARAMS | DIEP_START);


		//DWORD flags;
		//g_pEffect->GetEffectStatus(&flags);

		//if(!(flags & DIEGES_PLAYING))
		//{
		//	InitDI();
		//}
	}
}

//start/stop effect
void DisableConstantForce(int port)
{
	if (g_pEffect[port])
		g_pEffect[port]->Stop();
}

void SetRamp(int port, const ramp& var)
{
}

void SetRampVariable(int port, int forceids, const variable& var)
{
	if (FFBindex[port] == -1) return;

	// one main loop is 2ms, too erratic
	effRamp.dwDuration = 2000 * (var.t1 + 1) * 25;

	// Force0 only (Force2 is Y axis?)
	if (forceids & 1)
	{
		int force = var.l1;
		int dir = (var.d1 & 1 ? 1 : -1);

		if (INVERTFORCES[port])
		{
			cRamp.lStart = (127 - force) * DI_FFNOMINALMAX / 127;
			int sign = 1;
			if (cRamp.lStart < 0) sign = -1; // pull to force's direction?
			cRamp.lEnd = sign * DI_FFNOMINALMAX * dir;
		}
		else
		{
			cRamp.lStart = -(127 - force) * DI_FFNOMINALMAX / 127;
			//int sign = -1;
			//if (cRamp.lStart < 0) sign = 1; // pull to force's direction?
			//cRamp.lEnd = sign * DI_FFNOMINALMAX * dir; // or to center?
			cRamp.lEnd = -(127 -(force + /* var.t1 **/ var.s1 * dir)) * DI_FFNOMINALMAX / 127;
		}
	}

	if (g_pEffectRamp[port])
		g_pEffectRamp[port]->SetParameters(&effRamp,
				DIEP_TYPESPECIFICPARAMS | DIEP_START | DIEP_DURATION);
}

void DisableRamp(int port)
{
	if (g_pEffectRamp[port])
		g_pEffectRamp[port]->Stop();
}

//set spring offset  (not used by ps2? only rfactor pc i think.  using as centering spring hack)
void SetSpringForce(int port, const spring& spring, bool hires, bool isdfp)
{
	// broken maths by reversing linux ff-memless-next patch
	int deadband, center;
	int dead1 = spring.dead1;
	int dead2 = spring.dead2;

	int sign1 = spring.s1 & 1 ? -1 : 1;
	int sign2 = spring.s2 & 1 ? -1 : 1;

	if (hires) //XXX enthusia doesn't set deadbands' low bits though
	{
		// XXX After some testing this seems to be mostly working
		dead1 = (dead1 << 3) | ((spring.s1 >> 1) & 0x7);
		dead2 = (dead2 << 3) | ((spring.s2 >> 1) & 0x7);

		//deadband = (dead2 << 5) - (dead1 << 5);
		deadband = ((dead2 * DI_FFNOMINALMAX / 1024) - (dead1 * DI_FFNOMINALMAX / 1024)) / 2; //TODO check math why it seems to need to be divided by 2

		//center = ((dead2 << 5) - 0x8000 + (dead1 << 5) - 0x8000 ) / 2;
		//center = (((dead2 + dead1) << 5) - 0x10000) / 2; //or above reduced
		//XXX straight to DI_FFNOMINALMAX
		center = ((dead2 + dead1) * DI_FFNOMINALMAX / 1024 - (2 * DI_FFNOMINALMAX)) / 2;
	}
	else
	{
		deadband = ((dead2 * DI_FFNOMINALMAX / 128) - (dead1 * DI_FFNOMINALMAX / 128)) / 2; //TODO check math why it seems to need to be divided by 2 (assumed from hires code :P)
		center = ((dead2 + dead1) * DI_FFNOMINALMAX / 128 - (2 * DI_FFNOMINALMAX)) / 2;
	}

	static const float coeffs1[] = { 0.25f, 0.5f, 0.75f, 1.f, 1.5f, 2.f, 3.f, 4.f };
	static const float coeffs2[] = { 0.25f, 0.5f, 0.75f, 1.f, 1.5f, 3.f, 2.f, 4.f }; //DFP
	const float *coeffs = isdfp ? coeffs2 : coeffs1;

	if (hires)
	{
		cSpring.lNegativeCoefficient = sign1 * spring.k1 * DI_FFNOMINALMAX / 16;
		cSpring.lPositiveCoefficient = sign2 * spring.k2 * DI_FFNOMINALMAX / 16;
	}
	else
	{
		// FIXME
		cSpring.lNegativeCoefficient = LONG(center * coeffs[spring.k1 & 7]);
		cSpring.lPositiveCoefficient = LONG(center * coeffs[spring.k2 & 7]);
	}

	cSpring.dwNegativeSaturation = spring.clip * DI_FFNOMINALMAX / 255;
	cSpring.dwPositiveSaturation = cSpring.dwNegativeSaturation;

	//int check_x = (center - deadband/2 + 0x8000) >> 5;
	//int check_y = (center + deadband/2 + 0x8000) >> 5;

	cSpring.lOffset = center;// * DI_FFNOMINALMAX / 0x8000;
	cSpring.lDeadBand = deadband; // * DI_FFNOMINALMAX / 0x1fff;

	//OSDebugOut(TEXT("spring check: %d/%d = %d/%d\n"), dead1, dead2, check_x, check_y);

	OSDebugOut(TEXT("spring: %d  %d coeff:%d/%d sat:%d/%d\n"),
		cSpring.lOffset, cSpring.lDeadBand,
		cSpring.lNegativeCoefficient, cSpring.lPositiveCoefficient,
		cSpring.dwNegativeSaturation, cSpring.dwPositiveSaturation);

	if (g_pEffectSpring[port])
			g_pEffectSpring[port]->SetParameters(&effSpring, DIEP_TYPESPECIFICPARAMS | DIEP_START);
}

void SetSpringForceGIMX(int port, const spring& spring, int caps)
{
	cSpring.dwNegativeSaturation = spring.clip * DI_FFNOMINALMAX / 255;
	cSpring.dwPositiveSaturation = cSpring.dwNegativeSaturation;

	cSpring.lNegativeCoefficient =
		ff_lg_get_condition_coef(caps, spring.k1, spring.s1, DI_FFNOMINALMAX);
	cSpring.lPositiveCoefficient =
		ff_lg_get_condition_coef(caps, spring.k2, spring.s2, DI_FFNOMINALMAX);

	if (caps & FF_LG_CAPS_HIGH_RES_DEADBAND)
	{
		uint16_t d2 = ff_lg_get_spring_deadband(caps, spring.dead2, (spring.s2 >> 1) & 0x7);
		uint16_t d1 = ff_lg_get_spring_deadband(caps, spring.dead1, (spring.s1 >> 1) & 0x7);
		cSpring.lOffset = ff_lg_u16_to_s16((d1 + d2) / 2) * DI_FFNOMINALMAX / 0x7FFF;
		cSpring.lDeadBand = (d2 - d1) * DI_FFNOMINALMAX / 0x7FFF;
	}
	else
	{
		cSpring.lOffset = ff_lg_u8_to_s16((spring.dead1 + spring.dead2) / 2, DI_FFNOMINALMAX);
		cSpring.lDeadBand = ff_lg_u8_to_u16(spring.dead2 - spring.dead1, DI_FFNOMINALMAX);
	}

	OSDebugOut(TEXT("spring: %d  %d coeff:%d/%d sat:%d/%d\n"),
		cSpring.lOffset, cSpring.lDeadBand,
		cSpring.lNegativeCoefficient, cSpring.lPositiveCoefficient,
		cSpring.dwNegativeSaturation, cSpring.dwPositiveSaturation);

	if (g_pEffectSpring[port])
			g_pEffectSpring[port]->SetParameters(&effSpring, DIEP_TYPESPECIFICPARAMS | DIEP_START);
}

void DisableSpring(int port)
{
	if (g_pEffectSpring[port])
		g_pEffectSpring[port]->Stop();
}

void SetDamper(int port, const damper& damper, bool isdfp)
{
	//noidea(TM)
	cDamper.lNegativeCoefficient = DI_FFNOMINALMAX * damper.k1 / 15 * (damper.s1 & 1 ? -1 : 1);
	cDamper.lPositiveCoefficient = DI_FFNOMINALMAX * damper.k2 / 15 * (damper.s2 & 1 ? -1 : 1);
	cDamper.dwNegativeSaturation = DI_FFNOMINALMAX;
	cDamper.dwPositiveSaturation = DI_FFNOMINALMAX;

	if (isdfp)
	{
		cDamper.lNegativeCoefficient = cDamper.lNegativeCoefficient * damper.clip / 255;
		cDamper.lPositiveCoefficient = cDamper.lPositiveCoefficient * damper.clip / 255;
	}

	if (g_pEffectDamper[port])
		g_pEffectDamper[port]->SetParameters(&effDamper, DIEP_TYPESPECIFICPARAMS | DIEP_START);
}

void SetDamperGIMX(int port, const damper& damper, int caps)
{
	cDamper.lNegativeCoefficient = ff_lg_get_condition_coef(caps, damper.k1, damper.s1, DI_FFNOMINALMAX);
	cDamper.lPositiveCoefficient = ff_lg_get_condition_coef(caps, damper.k2, damper.s2, DI_FFNOMINALMAX);
	cDamper.dwNegativeSaturation = DI_FFNOMINALMAX;
	cDamper.dwPositiveSaturation = DI_FFNOMINALMAX;
	cDamper.lOffset = 0;
	cDamper.lDeadBand = 0;

	OSDebugOut(TEXT("damper %d/%d\n"), cDamper.lNegativeCoefficient, cDamper.lPositiveCoefficient);

	if (g_pEffectDamper[port])
		g_pEffectDamper[port]->SetParameters(&effDamper, DIEP_TYPESPECIFICPARAMS | DIEP_START);
}

void DisableDamper(int port)
{
	if (g_pEffectDamper[port])
		g_pEffectDamper[port]->Stop();
}
// IDK
void SetSpringSlopeForce(int port, const spring& spring)
{
	cSpring.lOffset = 0;
	cSpring.lNegativeCoefficient = (spring.s1 & 1 ? -1 : 1) * spring.k1 * 10000 / 15;
	cSpring.lPositiveCoefficient = (spring.s2 & 1 ? -1 : 1) * spring.k2 * 10000 / 15;
	cSpring.dwNegativeSaturation = spring.dead1 * 10000 / 0xFF;
	cSpring.dwPositiveSaturation = spring.dead2 * 10000 / 0xFF;
	cSpring.lDeadBand = 0;

	if (g_pEffectSpring[port])
		g_pEffectSpring[port]->SetParameters(&effSpring, DIEP_TYPESPECIFICPARAMS | DIEP_START);
}

//LG driver converts it into high-precision damper instead, hmm
void SetFrictionForce(int port, const friction& frict)
{
	//noideaTM
	cFriction.lOffset = 0;
	cFriction.lDeadBand = 0;
	int s1 = frict.s1 & 1 ? -1 : 1;
	int s2 = frict.s2 & 1 ? -1 : 1;

	cFriction.lNegativeCoefficient = frict.k1 * DI_FFNOMINALMAX / 255 * s1;
	cFriction.lPositiveCoefficient = frict.k2 * DI_FFNOMINALMAX / 255 * s2;

	//cFriction.lNegativeCoefficient = cFriction.lNegativeCoefficient * frict.clip / 255;
	//cFriction.lPositiveCoefficient = cFriction.lPositiveCoefficient * frict.clip / 255;

	cFriction.dwNegativeSaturation = DI_FFNOMINALMAX * frict.clip / 255;
	cFriction.dwPositiveSaturation = cFriction.dwNegativeSaturation;

	OSDebugOut(TEXT("friction %d/%d %d\n"),
		cFriction.lNegativeCoefficient, cFriction.lPositiveCoefficient,
		cFriction.dwNegativeSaturation);

	if (g_pEffectFriction[port])
		g_pEffectFriction[port]->SetParameters(&effFriction, DIEP_TYPESPECIFICPARAMS | DIEP_START);
}

void DisableFriction(int port)
{
	if (g_pEffectFriction[port])
		g_pEffectFriction[port]->Stop();
}

void TestForce(int port)
{

	//SetSpringForce(0);
	SetConstantForce(port, 127-63);

	Sleep(500);
	SetConstantForce(port, 127+63);

	Sleep(500);
	//SetSpringForce(10000);
	SetConstantForce(port, 127);
}

//initialize all available devices
HRESULT InitDirectInput( HWND hWindow, int port )
{

    HRESULT hr;

	//release any previous resources
	OSDebugOut(TEXT("DINPUT: FreeDirectInput %p\n"), hWindow);

	if (refCount == 0)
	{
		// Create a DInput object
		OSDebugOut(TEXT("DINPUT: DirectInput8Create %p\n"), hWindow);
		if (FAILED(hr = DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION,
			IID_IDirectInput8, (VOID**)&g_pDI, NULL)))
			return hr;

		OSDebugOut(TEXT("DINPUT: CreateDevice Keyboard %p\n"), hWindow);
		//Create Keyboard
		g_pDI->CreateDevice(GUID_SysKeyboard, &g_pKeyboard, NULL);
		if (g_pKeyboard)
		{
			g_pKeyboard->SetDataFormat(&c_dfDIKeyboard);
			g_pKeyboard->SetCooperativeLevel(hWindow, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
			g_pKeyboard->Acquire();
		}
		OSDebugOut(TEXT("DINPUT: CreateDevice Mouse %p\n"), hWindow);
		//Create Mouse
		g_pDI->CreateDevice(GUID_SysMouse, &g_pMouse, NULL);
		if (g_pMouse)
		{
			g_pMouse->SetDataFormat(&c_dfDIMouse2);
			g_pMouse->SetCooperativeLevel(hWindow, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
			g_pMouse->Acquire();
		}

		//Create Joysticks
		FFBindex[port] = -1;
		FFB[port] = false;  //no FFB device selected

		//enumerate attached only
		OSDebugOut(TEXT("DINPUT: EnumDevices Joystick %p\n"), hWindow);
		g_pDI->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumJoysticksCallback, NULL, DIEDFL_ATTACHEDONLY);
	}

	++refCount;

	//loop through all attached joysticks
	for(DWORD i=0; i<numj; i++){

		if( g_pJoysticks[i] )
		{
			if (refCount == 1)
			{
				OSDebugOut(TEXT("DINPUT: SetDataFormat Joystick %i\n"), i);
				g_pJoysticks[i]->SetDataFormat(&c_dfDIJoystick2);
				OSDebugOut(TEXT("DINPUT: SetCooperativeLevel Joystick %i\n"), i);
			}

			DIDEVCAPS diCaps;
			diCaps.dwSize = sizeof(DIDEVCAPS);
			g_pJoysticks[i]->GetCapabilities(&diCaps);

			//TODO Select joystick for FFB that has X axis (assumed!!) mapped as wheel
			int joyid = AXISID[port][0] / 8;

			//has ffb?
			if(FFB[port] == false && joyid == i && (diCaps.dwFlags & DIDC_FORCEFEEDBACK)){
				//First FFB device detected

				//Exclusive
				g_pJoysticks[i]->SetCooperativeLevel( hWindow, DISCL_EXCLUSIVE|DISCL_BACKGROUND );

				AutoCenter(i, false); //TODO some games set autocenter. Figure out default for ones that don't.

				CreateFFB(port, i);

				/*DIDEVICEINSTANCE instance_;
				ZeroMemory(&instance_, sizeof(DIDEVICEINSTANCE));
				instance_.dwSize = sizeof(DIDEVICEINSTANCE);
				g_pJoysticks[i]->GetDeviceInfo(&instance_);
				std::stringstream str;
				str << instance_.guidInstance;*/

			}
			else
				g_pJoysticks[i]->SetCooperativeLevel( hWindow, DISCL_NONEXCLUSIVE|DISCL_BACKGROUND );

			if (refCount == 1)
			{
				OSDebugOut(TEXT("DINPUT: EnumObjects Joystick %i\n"), i);
				g_pJoysticks[i]->EnumObjects(EnumObjectsCallback, g_pJoysticks[i], DIDFT_ALL);
				OSDebugOut(TEXT("DINPUT: Acquire Joystick %i\n"), i);
				g_pJoysticks[i]->Acquire();
			}
		}
	}

	didDIinit=true;
	return S_OK;

}

}} //namespace
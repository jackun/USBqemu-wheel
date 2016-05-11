#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#define SAFE_DELETE(p)  { if(p) { delete (p);     (p)=NULL; } }
#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=NULL; } }

#define SAMPLE_BUFFER_SIZE 16

LPDIRECTINPUT8       g_pDI       = NULL;        
LPDIRECTINPUTDEVICE8 g_pKeyboard = NULL;
LPDIRECTINPUTDEVICE8 g_pMouse	 = NULL;  
LPDIRECTINPUTDEVICE8 g_pJoysticks[10] = {NULL}; 

//only two effect (constant force, spring)
bool FFB=false;
int FFBindex = -1;
LPDIRECTINPUTEFFECT  g_pEffect[2] = { NULL };
LPDIRECTINPUTEFFECT  g_pEffectSpring = NULL;
DWORD g_dwNumForceFeedbackAxis[4] = {0};
DIEFFECT eff;
DIEFFECT effSpring;
DICONSTANTFORCE cfw;
DICONDITION c;


BYTE diks[256];							// DirectInput keyboard state buffer 
DIMOUSESTATE2 dims2;					// DirectInput mouse state structure
DIJOYSTATE2 js[10] = {NULL};			// DInput joystick state 
DIJOYSTATE2 jso[10] = {NULL};           // DInput joystick old state 
DIJOYSTATE2 jsi[10] = {NULL};           // DInput joystick initial state 


DWORD numj = 0;							//current attached joysticks
DWORD maxj = 10;						//maximum attached joysticks


bool listening = false;
DWORD listenend = 0;
DWORD listennext = 0;
DWORD listentimeout = 10000;
DWORD listeninterval = 500;

bool didDIinit = false;					//we have a handle

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

    // Release any DirectInput objects.
    SAFE_RELEASE( g_pKeyboard );
    SAFE_RELEASE( g_pMouse );
	for(DWORD i=0; i<numj; i++) SAFE_RELEASE( g_pJoysticks[i] );
    SAFE_RELEASE( g_pDI );
	SAFE_RELEASE( g_pEffect[0] );
	SAFE_RELEASE( g_pEffect[1] );
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

    // For axes that are returned, set the DIPROP_RANGE property for the
    // enumerated axis in order to scale min/max values.
    if( pdidoi->dwType & DIDFT_AXIS )
    {
        DIPROPRANGE diprg; 
        diprg.diph.dwSize       = sizeof(DIPROPRANGE); 
        diprg.diph.dwHeaderSize = sizeof(DIPROPHEADER); 
        diprg.diph.dwHow        = DIPH_BYID; 
        diprg.diph.dwObj        = pdidoi->dwType; // Specify the enumerated axis
        diprg.lMin              = -1000; 
        diprg.lMax              = +1000; 
    
        // Set the range for the axis  (not used, DX defaults 65535 all axis)
        //if( FAILED( g_pJoystick->SetProperty( DIPROP_RANGE, &diprg.diph ) ) ) 
        //    return DIENUM_STOP;
         
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
		int i = (KeyID-265)*0.015625f;  //get controller index
		int b = (KeyID-265)-i*64; //get button index
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
	if(linear>0){hs = (float)(1.0-((linear*2) *(float)0.01));}	//format+shorten variable
	else{hs = (float)(1.0-(abs(linear*2) *(float)0.01));}		//format+shorten variable
	float hs2 = (float)(offset+50) * (float)0.01;				//format+shorten variable
	float v = input;											//format+shorten variable
	float d = (dead) * (float)0.005;							//format+shorten variable


	//format and apply deadzone
	v=(v*(1.0f+(d*2.0)))-d; 

	//clamp
	if(v<0.0f)v=0.0f;
	if(v>1.0f)v=1.0f;

	//clamp negdead
	if(v==-d)v=0.0;



	//possibilities
	float c1 = v - (1.0 - (pow((double)(1.0 - v) , (double)(1.0 / hs))));
	float c2 = ((v - pow(v , hs)));
	float c3 = v - (1.0 - (pow((double)(1.0 - v) , (double)hs)));
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

	int i = axisid*0.125;  //obtain controller index
	int ax = axisid-i*8;  //obtain axis index
	
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

	for(DWORD i=0; i<numj; i++){
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

			
			if(axisid-i*8 == 0 && lXdiff > detectrange) { initial = jsi[i].lX; inverted = true; return true;}
			if(axisid-i*8 == 0 && lXdiff < -detectrange) {initial = jsi[i].lX; inverted = false; return true;}

			if(axisid-i*8 == 1 && lYdiff > detectrange) { initial = jsi[i].lY;inverted = true; return true;}
			if(axisid-i*8 == 1 && lYdiff < -detectrange) { initial = jsi[i].lY;inverted = false; return true;}

			if(axisid-i*8 == 2 && lZdiff > detectrange) { initial = jsi[i].lZ;inverted = true; return true;}
			if(axisid-i*8 == 2 && lZdiff < -detectrange) { initial = jsi[i].lZ;inverted = false; return true;}

			if(axisid-i*8 == 3 && lRxdiff > detectrange) { initial = jsi[i].lRx;inverted = true; return true;}
			if(axisid-i*8 == 3 && lRxdiff < -detectrange) {initial = jsi[i].lRx; inverted = false; return true;}

			if(axisid-i*8 == 4 && lRydiff > detectrange) { initial = jsi[i].lRy;inverted = true; return true;}
			if(axisid-i*8 == 4 && lRydiff < -detectrange) { initial = jsi[i].lRy;inverted = false; return true;}

			if(axisid-i*8 == 5 && lRzdiff > detectrange) { initial = jsi[i].lRz;inverted = true; return true;}
			if(axisid-i*8 == 5 && lRzdiff < -detectrange) { initial = jsi[i].lRz;inverted = false; return true;}

			if(axisid-i*8 == 6 && lSlider1diff > detectrange) { initial = jsi[i].rglSlider[0];inverted = true; return true;}
			if(axisid-i*8 == 6 && lSlider1diff < -detectrange) { initial = jsi[i].rglSlider[0];inverted = false; return true;}

			if(axisid-i*8 == 7 && lSlider2diff > detectrange) { initial = jsi[i].rglSlider[1];inverted = true; return true;}
			if(axisid-i*8 == 7 && lSlider2diff < -detectrange) { initial = jsi[i].rglSlider[1];inverted = false; return true;}
		}
	}
	return false;
	//max 160
}

//search all axis/buttons (config only)
bool FindControl(LONG & axis,LONG & inverted, LONG & initial, LONG & button)
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
	dipdw.dwData            = onoff;

	g_pJoysticks[jid]->SetProperty( DIPROP_AUTOCENTER, &dipdw.diph );  
}

//set left/right ffb torque
extern void InitDI(int port);
void SetConstantForce(int port, LONG magnitude)
{
	if(FFBindex==-1)return;

	WriteLogFile(L"DINPUT: Apply Force");

	if(INVERTFORCES[port])
		cfw.lMagnitude = (127-magnitude) * 78.4803149606299;
	else
		cfw.lMagnitude = -(127-magnitude) * 78.4803149606299;
	
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
//set spring offset  (not used by ps2? only rfactor pc i think.  using as centering spring hack)
void SetSpringForce(LONG magnitude)
{
	//if(magnitude == 0)
	//{
	//	g_pEffectSpring->Stop();
	//}

	//not used
	c.lOffset = 0;//(127-offset) * 78.4803149606299;
	c.lNegativeCoefficient = magnitude;
	c.lPositiveCoefficient = magnitude;
	c.dwNegativeSaturation = 0;
	c.dwPositiveSaturation = 0;
	c.lDeadBand = 0;
	
	if(g_pEffectSpring) g_pEffectSpring->SetParameters(&effSpring, DIEP_TYPESPECIFICPARAMS | DIEP_START);
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
	swprintf_s(logstring, L"DINPUT: FreeDirectInput %p", hWin);WriteLogFile(logstring);

	if (refCount == 0)
	{
		refCount++;
		FreeDirectInput();

		// Create a DInput object
		swprintf_s(logstring, L"DINPUT: DirectInput8Create %p", hWin); WriteLogFile(logstring);
		if (FAILED(hr = DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION,
			IID_IDirectInput8, (VOID**)&g_pDI, NULL)))
			return hr;

		swprintf_s(logstring, L"DINPUT: CreateDevice Keyboard %p", hWin); WriteLogFile(logstring);
		//Create Keyboard
		g_pDI->CreateDevice(GUID_SysKeyboard, &g_pKeyboard, NULL);
		if (g_pKeyboard)
		{
			g_pKeyboard->SetDataFormat(&c_dfDIKeyboard);
			g_pKeyboard->SetCooperativeLevel(hWindow, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
			g_pKeyboard->Acquire();
		}
		swprintf_s(logstring, L"DINPUT: CreateDevice Mouse %p", hWin); WriteLogFile(logstring);
		//Create Mouse
		g_pDI->CreateDevice(GUID_SysMouse, &g_pMouse, NULL);
		if (g_pMouse)
		{
			g_pMouse->SetDataFormat(&c_dfDIMouse2);
			g_pMouse->SetCooperativeLevel(hWindow, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
			g_pMouse->Acquire();
		}

		//Create Joysticks
		FFBindex = -1;
		FFB = false;  //no FFB device selected

		//enumerate attached only
		swprintf_s(logstring, L"DINPUT: EnumDevices Joystick %p", hWin); WriteLogFile(logstring);
		g_pDI->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumJoysticksCallback, NULL, DIEDFL_ATTACHEDONLY);
	}

	//loop through all attached joysticks
	for(DWORD i=0; i<numj; i++){

		if( g_pJoysticks[i] )
		{
			if (refCount == 0)
			{
				swprintf_s(logstring, L"DINPUT: SetDataFormat Joystick %i", i); WriteLogFile(logstring);
				g_pJoysticks[i]->SetDataFormat(&c_dfDIJoystick2);
				swprintf_s(logstring, L"DINPUT: SetCooperativeLevel Joystick %i", i); WriteLogFile(logstring);
			}

			DIDEVCAPS diCaps;
			diCaps.dwSize = sizeof(DIDEVCAPS);
			g_pJoysticks[i]->GetCapabilities(&diCaps);

			//TODO Select joystick for FFB that has X axis (assumed!!) mapped as wheel
			int joyid = AXISID[port][0] * 0.125;

			//has ffb?  
			if(FFB==false && joyid == i && (diCaps.dwFlags & DIDC_FORCEFEEDBACK)){
				//First FFB device detected
				
				//create effect
				try{
					
					//Exclusive
					g_pJoysticks[i]->SetCooperativeLevel( hWindow, DISCL_EXCLUSIVE|DISCL_BACKGROUND );
 
					AutoCenter(i, false);  //just keep it off for all wheels

					//create the constant force effect
					DWORD           rgdwAxes[2]     = { DIJOFS_X,DIJOFS_Y };
					LONG            rglDirection[2] = { 0, 0 };
					ZeroMemory( &eff, sizeof(eff) );
					ZeroMemory( &effSpring, sizeof(effSpring) );
					ZeroMemory( &cfw, sizeof(cfw) );
					ZeroMemory( &c, sizeof(c) );

					//constantforce
					eff.dwSize                  = sizeof(DIEFFECT);
					eff.dwFlags                 = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
					eff.dwSamplePeriod          = 0;
					eff.dwGain                  = 10000;
					eff.dwTriggerButton         = DIEB_NOTRIGGER;
					eff.dwTriggerRepeatInterval = 0;
					eff.cAxes                   = 1;
					eff.rgdwAxes                = rgdwAxes;
					eff.rglDirection            = rglDirection;
					eff.dwStartDelay            = 0;
					eff.dwDuration              = INFINITE;

					cfw.lMagnitude = 0;
					
					eff.cbTypeSpecificParams    = sizeof(DICONSTANTFORCE);
					eff.lpvTypeSpecificParams   = &cfw;
					g_pJoysticks[i]->CreateEffect( GUID_ConstantForce, &eff, &g_pEffect[port], NULL );

					//spring
					

					effSpring.dwSize                  = sizeof(DIEFFECT);
					effSpring.dwFlags                 = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
					effSpring.dwSamplePeriod          = 0;
					effSpring.dwGain                  = 10000;
					effSpring.dwTriggerButton         = DIEB_NOTRIGGER;
					effSpring.dwTriggerRepeatInterval = 0;
					effSpring.cAxes                   = 1;
					effSpring.rgdwAxes                = rgdwAxes;
					effSpring.rglDirection            = rglDirection;
					effSpring.dwStartDelay            = 0;
					effSpring.dwDuration              = INFINITE;

					c.lNegativeCoefficient = 0;
					c.lPositiveCoefficient = 0;

					effSpring.cbTypeSpecificParams    = sizeof(DICONDITION);
					effSpring.lpvTypeSpecificParams   = &c;
					//g_pJoysticks[i]->CreateEffect( GUID_Spring, &effSpring, &g_pEffectSpring, NULL );

					FFBindex=i;
					FFB = true;
				}catch(...){};

			}
			else
				g_pJoysticks[i]->SetCooperativeLevel( hWindow, DISCL_NONEXCLUSIVE|DISCL_BACKGROUND );
			
			if (refCount == 0)
			{
				swprintf_s(logstring, L"DINPUT: EnumObjects Joystick %i", i); WriteLogFile(logstring);
				g_pJoysticks[i]->EnumObjects(EnumObjectsCallback, (VOID*)hWindow, DIDFT_ALL);
				swprintf_s(logstring, L"DINPUT: Acquire Joystick %i", i); WriteLogFile(logstring);
				g_pJoysticks[i]->Acquire();
			}
		}
	}

	WriteLogFile(L"DINPUT: Start Effect");
	//start the effect
	//SetSpringForce(10000);
	if(g_pEffect[port]) g_pEffect[port]->SetParameters(&eff, DIEP_START);

	didDIinit=true;
	return S_OK;

}


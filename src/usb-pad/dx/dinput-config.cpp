#pragma warning (push)
// floats to int
#pragma warning (disable : 4244)

#include "dx.h"

#include <commctrl.h>
#include <stdlib.h>
#include <shellapi.h>
#include <stdio.h>
#include "versionproxy.h"

#include "usb-pad-dx.h"

namespace usb_pad { namespace dx {

static bool useRamp = false;

bool listening = false;
DWORD listenend = 0;
DWORD listennext = 0;
DWORD listentimeout = 10000;
DWORD listeninterval = 500;

extern BYTE diks[256];          // DirectInput keyboard state buffer
extern DIMOUSESTATE2 dims2;     // DirectInput mouse state structure

std::vector<DIJOYSTATE2> jso;   // DInput joystick old state, only for config
std::vector<DIJOYSTATE2> jsi;   // DInput joystick initial state, only for config

int32_t AXISID[2][CID_COUNT];
int32_t BUTTON[2][CID_COUNT];
int32_t INVERT[2][CID_COUNT];
int32_t HALF[2][CID_COUNT];
int32_t LINEAR[2][CID_COUNT];
int32_t OFFSET[2][CID_COUNT];
int32_t DEADZONE[2][CID_COUNT];

bool BYPASSCAL = 0;
int32_t GAINZ[2][1];
int32_t FFMULTI[2][1];
bool INVERTFORCES[2] = { false, false };

bool dialogOpen = false;

HWND hKey;
HWND hWnd;
TCHAR text[1024];
ControlID CID = CID_COUNT;

HFONT hFont;
HDC hDC;
PAINTSTRUCT Ps;
RECT rect;
static WNDPROC pFnPrevFunc;
LONG filtercontrol = 0;
float TESTV=0;
float TESTVF=0;
DWORD m_dwScalingTime;
DWORD m_dwDrawingTime;
DWORD m_dwCreationTime;
DWORD m_dwMemory;
DWORD m_dwOption;
DWORD m_dwTime;
HBITMAP m_hOldAABitmap;
HBITMAP m_hAABitmap;
HDC m_hAADC;
HBITMAP m_hOldMemBitmap;
HBITMAP m_hMemBitmap;
HDC m_hMemDC;


//label enum
DWORD LABELS[CID_COUNT] = {
	IDC_LABEL0,
	IDC_LABEL1,
	IDC_LABEL2,
	IDC_LABEL3,
	IDC_LABEL4,
	IDC_LABEL5,
	IDC_LABEL6,
	IDC_LABEL7,
	IDC_LABEL8,
	IDC_LABEL9,
	IDC_LABEL10,
	IDC_LABEL11,
	IDC_LABEL12,
	IDC_LABEL13,
	IDC_LABEL14,
	IDC_LABEL15,
	IDC_LABEL16,
	IDC_LABEL17,
	IDC_LABEL18,
	IDC_LABEL19,
};

struct DXDlgSettings
{
	int port;
	const char* dev_type;
};

void SetControlLabel(int cid, const InputMapped& im)
{
	if (cid >= CID_COUNT)
		return;

	if (im.type == MT_AXIS)
		swprintf_s(text, L"Axis %i/%i/%s/%i", im.index, im.mapped, im.INVERTED ? L"i" : L"n", im.HALF);
	else if (im.type == MT_BUTTON)
		swprintf_s(text, L"Button %i/%i", im.index, im.mapped);
	else
		swprintf_s(text, L"Unmapped");

	SetWindowText(GetDlgItem(hWnd, LABELS[cid]), text);
}

//config only
void ListenUpdate()
{
	for (size_t i=0; i<g_pJoysticks.size(); i++) {
		jso[i] = g_pJoysticks[i]->GetDeviceState();
	}
	PollDevices();
}

//poll and store all joystick states for comparison (config only)
void ListenAxis()
{
	PollDevices();
	for (size_t i=0; i<g_pJoysticks.size(); i++) {
		if (g_pJoysticks[i]->GetControlType() != CT_JOYSTICK)
			continue;
		jso[i] = g_pJoysticks[i]->GetDeviceState();
		jsi[i] = jso[i];
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
bool AxisDown(size_t ijoy, InputMapped& im)
{
	//TODO mouse axis
	if (g_pJoysticks[ijoy]->GetControlType() != CT_JOYSTICK)
		return false;

	DIJOYSTATE2 js = g_pJoysticks[ijoy]->GetDeviceState();
	std::cerr << __func__ << ": joystick[" << ijoy << "]: " <<
		"\tlX " << js.lX << "\n" <<
		"\tlY " << js.lY << "\n" <<
		"\tlZ " << js.lZ << "\n" <<
		"\tlRx " << js.lRx << "\n" <<
		"\tlRy " << js.lRy << "\n" <<
		"\tlRz " << js.lRz << "\n" <<
		"\trglSlider0 " << js.rglSlider[0] << "\n" <<
		"\trglSlider1 " << js.rglSlider[1] << "\n" <<
		std::endl;

	LONG detectrange = 2000;
	for (size_t axisid = 0; axisid < DINPUT_AXES_COUNT; axisid++)
	{
		LONG diff = 0;
		im.index = ijoy;
		im.mapped = axisid;
		im.type = MappingType::MT_NONE;

		// TODO mind the POV axes, one axis for all directions?
		diff = GetAxisValueFromOffset(axisid, js) - GetAxisValueFromOffset(axisid, jso[ijoy]);
		if (diff > detectrange) {
			im.HALF = GetAxisValueFromOffset(axisid, jsi[ijoy]);
			im.INVERTED = true;
			im.type = MappingType::MT_AXIS;
			return true;
		}
		if (diff < -detectrange) {
			im.HALF = GetAxisValueFromOffset(axisid, jsi[ijoy]);
			im.INVERTED = false;
			im.type = MappingType::MT_AXIS;
			return true;
		}
	}
	return false;
}

//checks all devices for digital button id/index
bool KeyDown(size_t ijoy, InputMapped& im)
{
	assert(ijoy < g_pJoysticks.size());
	if (ijoy >= g_pJoysticks.size())
		return false;

	auto joy = g_pJoysticks[ijoy];
	im.index = ijoy;
	im.type = MT_NONE;

	int buttons = 0;

	switch (joy->GetControlType()) {
	case CT_JOYSTICK:
		buttons = ARRAY_SIZE(DIJOYSTATE2::rgbButtons) + 16 /* POV */;
		break;
	case CT_KEYBOARD:
		buttons = 256;
		break;
	case CT_MOUSE:
		buttons = ARRAY_SIZE(DIMOUSESTATE2::rgbButtons);
		break;
	default:
		break;
	}

	for (int b = 0; b < buttons; b++) {
		if (joy->GetButton(b)) {
			im.mapped = b;
			im.type = MT_BUTTON;
			return true;
		}
	}

	return false;
}

//search all axis/buttons (config only)
bool FindControl(LONG port, ControlID cid, InputMapped& im)
{
	if (listening==true) {
		if (listenend>GetTickCount())
		{
			if (listennext<GetTickCount())
			{
				listennext = listeninterval+GetTickCount();
				ListenUpdate();

				for (size_t i = 0; i<g_pJoysticks.size(); i++) {
					if (AxisDown(i, im)) {
						listening = false;
						if (CID_STEERING == cid) {
							CreateFFB(port, g_pJoysticks[im.index]->GetDevice(), im.mapped);
						}
						AddInputMap(port, cid, im);
						return true;
					}
					else if (KeyDown(i, im)) {
						listening = false;
						AddInputMap(port, cid, im);
						return true;
					}
				}
			}
		}else{
			GetInputMap(port, cid, im);
			SetControlLabel(cid, im);
			listening=false;
			return false; //timedout
		}
	}
	return false;
}

void ApplyFilter(int port)
{
	filtercontrol = SendMessage(GetDlgItem(hWnd,IDC_COMBO1), CB_GETCURSEL, 0, 0);

	if(filtercontrol==-1)return;
	//slider
	LINEAR[port][filtercontrol] = SendMessage(GetDlgItem(hWnd,IDC_SLIDER1), TBM_GETPOS, 0, 0)-50 * PRECMULTI;
	OFFSET[port][filtercontrol] = SendMessage(GetDlgItem(hWnd,IDC_SLIDER2), TBM_GETPOS, 0, 0)-50 * PRECMULTI;
	DEADZONE[port][filtercontrol] = SendMessage(GetDlgItem(hWnd,IDC_SLIDER3), TBM_GETPOS, 0, 0)-50 * PRECMULTI;
	GAINZ[port][0] = SendMessage(GetDlgItem(hWnd,IDC_SLIDER4), TBM_GETPOS, 0, 0);
	FFMULTI[port][0] = SendMessage(GetDlgItem(hWnd, IDC_SLIDER5), TBM_GETPOS, 0, 0);

	swprintf_s(text, TEXT("LINEARITY: %0.02f"), float(LINEAR[port][filtercontrol]) / PRECMULTI);
	SetWindowText(GetDlgItem(hWnd,IDC_LINEAR), text);
	swprintf_s(text, TEXT("OFFSET: %0.02f"), float(OFFSET[port][filtercontrol]) / PRECMULTI);
	SetWindowText(GetDlgItem(hWnd,IDC_OFFSET), text);
	swprintf_s(text, TEXT("DEAD-ZONE: %0.02f"), float(DEADZONE[port][filtercontrol]) / PRECMULTI);
	SetWindowText(GetDlgItem(hWnd,IDC_DEADZONE), text);

	GetClientRect(GetDlgItem(hWnd,IDC_PICTURE), &rect);
	MapWindowPoints(GetDlgItem(hWnd,IDC_PICTURE), hWnd, (POINT *) &rect, 2);
	InvalidateRect( hWnd, &rect, TRUE );
}

void LoadFilter(int port)
{
	filtercontrol = SendMessage(GetDlgItem(hWnd,IDC_COMBO1), CB_GETCURSEL, 0, 0);
	if(filtercontrol==-1)return;
	//InputMapped im = {};
	//GetInputMap(port, (ControlID)filtercontrol, im);
	//slider
	SendMessage(GetDlgItem(hWnd,IDC_SLIDER1), TBM_SETPOS, 1, LINEAR[port][filtercontrol]+50 * PRECMULTI);
	SendMessage(GetDlgItem(hWnd,IDC_SLIDER2), TBM_SETPOS, 1, OFFSET[port][filtercontrol]+50 * PRECMULTI);
	SendMessage(GetDlgItem(hWnd,IDC_SLIDER3), TBM_SETPOS, 1, DEADZONE[port][filtercontrol]+50 * PRECMULTI);
	SendMessage(GetDlgItem(hWnd, IDC_SLIDER4), TBM_SETPOS, 1, GAINZ[port][0]);
	SendMessage(GetDlgItem(hWnd, IDC_SLIDER5), TBM_SETPOS, 1, FFMULTI[port][0]);

	ApplyFilter(port);
}

void DefaultFilters(int port, LONG id)
{
	for(int i=0;i<6;i++)
	{
		LINEAR[port][i] = 0;
		OFFSET[port][i] = 0;
		DEADZONE[port][i] = 0;
	}

	switch(id)
	{
		case 0: 
			LINEAR[port][0]=0;
			OFFSET[port][0]=0;
			LINEAR[port][1]=0;
			OFFSET[port][1]=0;
			break;
		case 1: 
			LINEAR[port][0]=6 * PRECMULTI;
			OFFSET[port][0]=0;
			LINEAR[port][1]=6 * PRECMULTI;
			OFFSET[port][1]=0;
			break;
		case 2: 
			LINEAR[port][0]=12 * PRECMULTI;
			OFFSET[port][0]=0;
			LINEAR[port][1]=12 * PRECMULTI;
			OFFSET[port][1]=0;
			break;
		case 3: 
			LINEAR[port][0]=18 * PRECMULTI;
			OFFSET[port][0]=0;
			LINEAR[port][1]=18 * PRECMULTI;
			OFFSET[port][1]=0;
			break;
		case 4: 
			LINEAR[port][0]=25 * PRECMULTI;
			OFFSET[port][0]=0;
			LINEAR[port][1]=25 * PRECMULTI;
			OFFSET[port][1]=0;
			break;
	}

	LoadFilter(port);
}

void ControlTest(int port) //thread: waits for window
{
	InputMapped im;

	filtercontrol = SendMessage(GetDlgItem(hWnd,IDC_COMBO1), CB_GETCURSEL, 0, 0);
	if(filtercontrol>=0 && listening==false)
	{
		PollDevices();

		if (GetInputMap(port, (ControlID)filtercontrol, im))
		{
			TESTV = ReadAxis(im);
			TESTVF = FilterControl(ReadAxis(im), LINEAR[port][filtercontrol], OFFSET[port][filtercontrol], DEADZONE[port][filtercontrol]);
			GetClientRect(GetDlgItem(hWnd, IDC_PICTURE), &rect);
			MapWindowPoints(GetDlgItem(hWnd, IDC_PICTURE), hWnd, (POINT*)& rect, 2);
			InvalidateRect(hWnd, &rect, TRUE);
		}
	}else{
		TESTV=0;
	}

	return;
}

void ListenForControl(int port)
{
	InputMapped im ({});

	if (FindControl(port, CID, im))
	{
		if (CID <= CID_BRAKE) {
			LINEAR[port][CID] = im.LINEAR;
			OFFSET[port][CID] = im.OFFSET;
			DEADZONE[port][CID] = im.DEADZONE;
		}

		AddInputMap(port, CID, im);
		SetControlLabel(CID, im);
	}
	else if(listening) {
		swprintf_s(text, L"Listening... %u", GetListenTimeout()/1000+1);
		SetWindowText(GetDlgItem(hWnd,LABELS[CID]),text);
	}
}

void StartListen(ControlID controlid)
{
	if(listening)return;

	CID = controlid;
	swprintf_s(text, L"Listening...");SetWindowText(GetDlgItem(hWnd,LABELS[CID]),text);
	OSDebugOut(TEXT("Begin Listen %i\n"), -1);
	ListenAxis();
}

void DeleteControl(int port, ControlID controlid)
{
	CID = controlid;
	RemoveInputMap(port, controlid);
	SetWindowText(GetDlgItem(hWnd,LABELS[CID]), _T("Unmapped"));
}

void CreateDrawing(int port, HDC hDrawingDC, int scale)
{
	GetClientRect(GetDlgItem(hWnd,IDC_PICTURE), &rect);
	//MapWindowPoints(GetDlgItem(hWnd,IDC_PICTURE), hWnd, (POINT *) &rect, 2);

	int px = 0;//rect.left;
	int py = 0;//rect.top;
	int pwidth = rect.right-rect.left;
	int pheight = rect.bottom-rect.top;

	HPEN bluepen = CreatePen(PS_SOLID , 4*scale, COLORREF RGB(79,97,117));
	HPEN gridpen = CreatePen(PS_SOLID , 1*scale, COLORREF RGB(0,0,0));
	HPEN blackpen = CreatePen(PS_SOLID , 4*scale, COLORREF RGB(0,0,0));
	HBRUSH hbrush = (HBRUSH)GetStockObject( LTGRAY_BRUSH );

	SelectObject(hDrawingDC,hbrush);

	rect.right *=scale;
	rect.bottom *=scale;

	FillRect(hDrawingDC, &rect, hbrush);

	//draw grid
	SelectObject(hDrawingDC,gridpen);
	float step[2] = {pwidth / 5.f, pheight / 5.f};
	for(int x=1; x<5; x++)
	{
		MoveToEx(hDrawingDC, (step[0]*x+px)*scale, (py), 0);
		LineTo(hDrawingDC, (step[0]*x+px)*scale, (pheight+py)*scale);
	}
	for(int y=1; y<5; y++)
	{
		MoveToEx(hDrawingDC, (px)*scale, (step[1]*y+py)*scale, 0);
		LineTo(hDrawingDC, (pwidth+px)*scale, (step[1]*y+py)*scale);
	}

	//draw linear line
	SelectObject(hDrawingDC,blackpen);
	MoveToEx(hDrawingDC, (px)*scale, (pheight+py)*scale, 0);
	for(float x=0; x<1.0f; x+=0.001f)
	{
		LineTo(hDrawingDC, (x*pwidth+px)*scale, (-x*pheight+pheight+py)*scale);
	}

	filtercontrol = SendMessage(GetDlgItem(hWnd,IDC_COMBO1), CB_GETCURSEL, 0, 0);
	if(filtercontrol>=0){

		//draw nonlinear line
		SelectObject(hDrawingDC,bluepen);
		MoveToEx(hDrawingDC, (px+8)*scale, (pheight+py-8)*scale, 0);
		for(float x=0; x<1.0f; x+=0.001f)
		{
			float y1 = FilterControl(x, LINEAR[port][filtercontrol], OFFSET[port][filtercontrol], DEADZONE[port][filtercontrol]);
			LineTo(hDrawingDC, (x*(pwidth-16)+px+8)*scale, (-y1*(pheight-16)+(pheight-8)+py)*scale);
		}
		LineTo(hDrawingDC, (1.0f*(pwidth-16)+px+8)*scale, (-1.0f*(pheight-16)+(pheight-8)+py)*scale);

		//draw output
		int tx = (TESTV*(pwidth-16)+px+8)*scale;
		int ty = (- TESTVF *(pheight-16)+(pheight-8)+py)*scale;

		Ellipse(hDrawingDC, tx-scale*10, ty-scale*10, tx+scale*10, ty+scale*10);
	}
	//cleanup
	DeleteObject(bluepen);
	DeleteObject(gridpen);
	DeleteObject(blackpen);
}

void CreateAAImage(int port, HDC hAADC, int scale)
{
	GetClientRect(GetDlgItem(hWnd,IDC_PICTURE), &rect);
	MapWindowPoints(GetDlgItem(hWnd,IDC_PICTURE), hWnd, (POINT *) &rect, 2);

	int px = rect.left;
	int py = rect.top;
	int pwidth = rect.right-rect.left;
	int pheight = rect.bottom-rect.top;

	DWORD startTime, endTime;

	// Calculate memory requested
	m_dwMemory = (scale*pwidth) * (scale*pheight) * 4;

	// Get screen DC
	HDC hDC = ::GetDC(NULL);

	// Create temporary DC and bitmap
	startTime = GetTickCount();
	HDC hTempDC = ::CreateCompatibleDC(hDC);
	HBITMAP hTempBitmap = ::CreateCompatibleBitmap(hDC, scale*pwidth, scale*pheight);
	HBITMAP hOldTempBitmap = (HBITMAP)::SelectObject(hTempDC, hTempBitmap);
	endTime = GetTickCount();
	m_dwCreationTime = endTime - startTime;

	// Release screen DC
	::ReleaseDC(NULL, hDC);

	// Create drawing
	startTime = GetTickCount();
	CreateDrawing(port, hTempDC, scale);
	endTime = GetTickCount();
	m_dwDrawingTime = endTime - startTime;

/*	// Copy temporary DC to anti-aliazed DC
	startTime = GetTickCount();
	int oldStretchBltMode = ::SetStretchBltMode(hAADC, HALFTONE);
	::StretchBlt(hAADC, 0, 0, 300, 200, hTempDC, 0, 0, scale*300, scale*200, SRCCOPY);
	::SetStretchBltMode(hAADC, oldStretchBltMode);
	endTime = GetTickCount();
	m_dwScalingTime = endTime - startTime;*/

	startTime = GetTickCount();

	// Get source bits
	int srcWidth = scale * pwidth;
	int srcHeight = scale * pheight;
	int srcPitch = srcWidth * 4;
	int srcSize = srcWidth * srcPitch;
	BYTE* lpSrcBits = new BYTE[srcSize];
	GetBitmapBits(hTempBitmap, srcSize, lpSrcBits);

	// Get destination bits
	int dstWidth = pwidth;
	int dstHeight = pheight;
	int dstPitch = dstWidth * 4;
	int dstSize = dstWidth * dstPitch;
	BYTE* lpDstBits = new BYTE[dstSize];
	HBITMAP hAABitmap = (HBITMAP)GetCurrentObject(hAADC, OBJ_BITMAP);
	GetBitmapBits(hAABitmap, dstSize, lpDstBits);

	int gridSize = scale * scale;
	int resultRed, resultGreen, resultBlue;
	int dstX, dstY=0, dstOffset;
	int srcX, srcY, srcOffset;
	int tmpX, tmpY, tmpOffset;
	for (int y=1; y<dstHeight-2; y++)
	{
		dstX = 0;
		srcX = 0;
		srcY = (y * scale) * srcPitch;
		for (int x=1; x<dstWidth-2; x++)
		{
			srcX = (x * scale) * 4;
			srcOffset = srcY + srcX;

			resultRed = resultGreen = resultBlue = 0;
			tmpY = -srcPitch;
			for (int i=0; i<scale; i++)
			{
				tmpX = -4;
				for (int j=0; j<scale; j++)
				{
					tmpOffset = tmpY + tmpX;

					resultRed += lpSrcBits[srcOffset+tmpOffset+2];
					resultGreen += lpSrcBits[srcOffset+tmpOffset+1];
					resultBlue += lpSrcBits[srcOffset+tmpOffset];
					
					tmpX += 4;
				}
				tmpY += srcPitch;
			}

			dstOffset = dstY + dstX;
			lpDstBits[dstOffset+2] = (BYTE)(resultRed / gridSize);
			lpDstBits[dstOffset+1] = (BYTE)(resultGreen / gridSize);
			lpDstBits[dstOffset] = (BYTE)(resultBlue / gridSize);
			dstX += 4;
		}

		dstY += dstPitch;
	}
	SetBitmapBits(hAABitmap, dstSize, lpDstBits);

	// Destroy source bits
	delete[] lpSrcBits;

	// Destroy destination bits
	delete[] lpDstBits;

	endTime = GetTickCount();

	m_dwScalingTime = endTime - startTime;

	// Destroy temporary DC and bitmap
	if (hTempDC)
	{
		::SelectObject(hTempDC, hOldTempBitmap);
		::DeleteDC(hTempDC);
		::DeleteObject(hTempBitmap);
	}
}

void InitialUpdate()
{
	GetClientRect(GetDlgItem(hWnd,IDC_PICTURE), &rect);
	MapWindowPoints(GetDlgItem(hWnd,IDC_PICTURE), hWnd, (POINT *) &rect, 2);

	int px = rect.left;
	int py = rect.top;
	int pwidth = rect.right-rect.left;
	int pheight = rect.bottom-rect.top;

	// Get screen DC
	HDC hDC = ::GetDC(NULL);

	// Create memory DC and bitmap
	m_hMemDC = ::CreateCompatibleDC(hDC);
	m_hMemBitmap = ::CreateCompatibleBitmap(hDC, pwidth, pheight);
	m_hOldMemBitmap = (HBITMAP)::SelectObject(m_hMemDC, m_hMemBitmap);

	// Create anti-alias DC and bitmap
	m_hAADC = ::CreateCompatibleDC(hDC);
	m_hAABitmap = ::CreateCompatibleBitmap(hDC, pwidth, pheight);
	m_hOldAABitmap = (HBITMAP)::SelectObject(m_hAADC, m_hAABitmap);

	// Release screen DC
	::ReleaseDC(NULL, hDC);

	// Create drawing
	//CreateDrawing(m_hMemDC, 1);
	//CreateAAImage(m_hAADC, 1);
}

void OnPaint(int port)
{
	GetClientRect(GetDlgItem(hWnd,IDC_PICTURE), &rect);
	MapWindowPoints(GetDlgItem(hWnd,IDC_PICTURE), hWnd, (POINT *) &rect, 2);

	int px = rect.left;
	int py = rect.top;
	int pwidth = rect.right-rect.left;
	int pheight = rect.bottom-rect.top;

	//hDC = GetDC(hWnd);//
	CreateAAImage(port, m_hAADC, 2);

	hDC=BeginPaint(hWnd, &Ps);

	//CreateDrawing(m_hAADC);
			// Draw 2x2 anti-aliazed image
	::BitBlt(hDC, px+2, py+2, pwidth-4, pheight-4, m_hAADC, 0, 0, SRCCOPY);

	EndPaint(hWnd, &Ps);
}

INT_PTR CALLBACK StaticProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	(*pFnPrevFunc)(hDlg, uMsg, wParam, lParam);
	switch(uMsg)
	{
		case WM_ERASEBKGND:
			return TRUE;
		case WM_PAINT:
			break;
	}
	return TRUE;
}

void InitDialog(int port, const char *dev_type)
{
	hFont = CreateFont(18,
					0,
					0,
					0,
					FW_BOLD,
					0,
					0,
					0,
					ANSI_CHARSET,
					OUT_DEFAULT_PRECIS,
					CLIP_DEFAULT_PRECIS,
					DEFAULT_QUALITY,
					DEFAULT_PITCH | FF_DONTCARE,
					TEXT("Tahoma"));
	HFONT hFont2 = CreateFont(14,
					0,
					0,
					0,
					FW_BOLD,
					0,
					0,
					0,
					ANSI_CHARSET,
					OUT_DEFAULT_PRECIS,
					CLIP_DEFAULT_PRECIS,
					DEFAULT_QUALITY,
					DEFAULT_PITCH | FF_DONTCARE,
					TEXT("Tahoma"));
	
	//pFnPrevFunc = (WNDPROC)SetWindowLongPtr(GetDlgItem(hWnd,IDC_PICTURE),GWLP_WNDPROC,(LONG_PTR) StaticProc);
	InitDirectInput(hWnd, port);
	LoadDInputConfig(port, dev_type); // settings rely on GUIDs so load after device enum
	FindFFDevice(port);

	jso.resize(g_pJoysticks.size());
	jsi.resize(g_pJoysticks.size());

	InitCommonControls();

	InitialUpdate();

	SetTimer(hWnd, 22, 40, (TIMERPROC) NULL);

	//StartTest();
	const wchar_t * string[] = {L"STEER LEFT", L"STEER RIGHT", L"THROTTLE", L"BRAKE"};

	for(int i=0;i<4;i++)
		SendMessageW(GetDlgItem(hWnd, IDC_COMBO1),CB_ADDSTRING,0, (LPARAM)string[i]);

	const wchar_t * stringp[] = {L"200 deg" , L"360 deg", L"540 deg", L"720 deg", L"900 deg"};

	for(int i=0;i<5;i++)
		SendMessageW(GetDlgItem(hWnd, IDC_COMBO3),CB_ADDSTRING,0, (LPARAM)stringp[i]);

	//slider
	SendMessage(GetDlgItem(hWnd,IDC_SLIDER1), TBM_SETPOS, 1, 50 * PRECMULTI);
	SendMessage(GetDlgItem(hWnd,IDC_SLIDER2), TBM_SETPOS, 1, 50 * PRECMULTI);
	SendMessage(GetDlgItem(hWnd,IDC_SLIDER3), TBM_SETPOS, 1, 50 * PRECMULTI);
	SendMessage(GetDlgItem(hWnd, IDC_SLIDER1), TBM_SETRANGEMAX, 1, 100 * PRECMULTI);
	SendMessage(GetDlgItem(hWnd, IDC_SLIDER2), TBM_SETRANGEMAX, 1, 100 * PRECMULTI);
	SendMessage(GetDlgItem(hWnd, IDC_SLIDER3), TBM_SETRANGEMAX, 1, 100 * PRECMULTI);
	SendMessage(GetDlgItem(hWnd, IDC_SLIDER1), TBM_SETTICFREQ, 10 * PRECMULTI, 0);
	SendMessage(GetDlgItem(hWnd, IDC_SLIDER2), TBM_SETTICFREQ, 10 * PRECMULTI, 0);
	SendMessage(GetDlgItem(hWnd, IDC_SLIDER3), TBM_SETTICFREQ, 10 * PRECMULTI, 0);

	SendMessage(GetDlgItem(hWnd,IDC_SLIDER4), TBM_SETRANGEMAX, 1, 10000);
	SendMessage(GetDlgItem(hWnd,IDC_SLIDER4), TBM_SETTICFREQ, 1000, 0);
	SendMessage(GetDlgItem(hWnd,IDC_SLIDER4), TBM_SETPOS, 1, GAINZ[port][0]);

	SendMessage(GetDlgItem(hWnd, IDC_SLIDER5), TBM_SETRANGEMAX, 1, 10);
	SendMessage(GetDlgItem(hWnd, IDC_SLIDER5), TBM_SETTICFREQ, 1, 0);
	SendMessage(GetDlgItem(hWnd, IDC_SLIDER5), TBM_SETPOS, 1, FFMULTI[port][0]);

	SendMessage(GetDlgItem(hWnd,IDC_CHECK1), BM_SETCHECK, INVERTFORCES[port], 0);
	SendMessage(GetDlgItem(hWnd,IDC_CHECK2), BM_SETCHECK, BYPASSCAL, 0);
	SendMessage(GetDlgItem(hWnd,IDC_CHECK3), BM_SETCHECK, useRamp, 0);
	//HANDLE hBitmap = LoadImage(NULL,MAKEINTRESOURCE(IDB_BITMAP1), IMAGE_BITMAP,0,0,LR_DEFAULTSIZE);
	//SendMessage(GetDlgItem(hWnd,IDC_PICTURELINK), STM_SETIMAGE, IMAGE_BITMAP, LPARAM(hBitmap)); 

	//fonts
	SendMessage(GetDlgItem(hWnd,IDC_GROUP1), WM_SETFONT, (WPARAM)hFont, 1);
	SendMessage(GetDlgItem(hWnd,IDC_GROUP2), WM_SETFONT, (WPARAM)hFont, 1);
	SendMessage(GetDlgItem(hWnd,IDC_GROUP3), WM_SETFONT, (WPARAM)hFont, 1);
	SendMessage(GetDlgItem(hWnd,IDC_GROUP4), WM_SETFONT, (WPARAM)hFont2, 1);
	SendMessage(GetDlgItem(hWnd,IDC_GROUP5), WM_SETFONT, (WPARAM)hFont2, 1);
	SendMessage(GetDlgItem(hWnd,IDC_GROUP6), WM_SETFONT, (WPARAM)hFont2, 1);
	SendMessage(GetDlgItem(hWnd,IDC_GROUP7), WM_SETFONT, (WPARAM)hFont2, 1);
	SendMessage(GetDlgItem(hWnd,IDC_GROUP8), WM_SETFONT, (WPARAM)hFont2, 1);
	SendMessage(GetDlgItem(hWnd,IDC_GROUP9), WM_SETFONT, (WPARAM)hFont2, 1);
	SendMessage(GetDlgItem(hWnd,IDC_GROUP10), WM_SETFONT, (WPARAM)hFont2, 1);
	SendMessage(GetDlgItem(hWnd,IDC_GROUP11), WM_SETFONT, (WPARAM)hFont2, 1);
	SendMessage(GetDlgItem(hWnd,IDC_GROUP12), WM_SETFONT, (WPARAM)hFont2, 1);
	SendMessage(GetDlgItem(hWnd,IDC_GROUP13), WM_SETFONT, (WPARAM)hFont2, 1);
	SendMessage(GetDlgItem(hWnd,IDC_GROUP14), WM_SETFONT, (WPARAM)hFont2, 1);
	SendMessage(GetDlgItem(hWnd,IDC_GROUP15), WM_SETFONT, (WPARAM)hFont2, 1);
	SendMessage(GetDlgItem(hWnd,IDC_GROUP16), WM_SETFONT, (WPARAM)hFont2, 1);
	SendMessage(GetDlgItem(hWnd,IDC_GROUP17), WM_SETFONT, (WPARAM)hFont2, 1);
	SendMessage(GetDlgItem(hWnd,IDC_GROUP18), WM_SETFONT, (WPARAM)hFont2, 1);
	SendMessage(GetDlgItem(hWnd,IDC_GROUP19), WM_SETFONT, (WPARAM)hFont2, 1);
	SendMessage(GetDlgItem(hWnd,IDC_GROUP20), WM_SETFONT, (WPARAM)hFont2, 1);
	SendMessage(GetDlgItem(hWnd,IDC_GROUP21), WM_SETFONT, (WPARAM)hFont2, 1);
	SendMessage(GetDlgItem(hWnd,IDC_GROUP22), WM_SETFONT, (WPARAM)hFont2, 1);
	SendMessage(GetDlgItem(hWnd,IDC_GROUP23), WM_SETFONT, (WPARAM)hFont2, 1);
	SendMessage(GetDlgItem(hWnd,IDC_GROUP24), WM_SETFONT, (WPARAM)hFont2, 1);
	SendMessage(GetDlgItem(hWnd,IDC_GROUP25), WM_SETFONT, (WPARAM)hFont2, 1);
	SendMessage(GetDlgItem(hWnd,IDC_GROUP26), WM_SETFONT, (WPARAM)hFont2, 1);

	for (int i = 0; i<CID_COUNT; i++)
	{
		InputMapped im = {};
		GetInputMap(port, (ControlID)i, im);
		SetControlLabel(i, im);
	}
	ShowWindow(hWnd, SW_SHOW);

	dialogOpen = true;
	//UpdateWindow( hWnd );
}

INT_PTR CALLBACK DxDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	DXDlgSettings *s = nullptr;
	//return false;
	switch(uMsg)
	{
		case WM_CREATE:
			SetWindowLongPtr(hDlg, GWLP_USERDATA, lParam);
		break;

		case WM_INITDIALOG:
			{
				s = (DXDlgSettings *)lParam;
				hWnd = hDlg;
				SetWindowLongPtr(hDlg, GWLP_USERDATA, lParam);
				InitDialog(s->port, s->dev_type);
			}break;
		case  WM_CTLCOLORSTATIC:
			{
				if((HWND)lParam==GetDlgItem(hWnd,IDC_GROUP1) || (HWND)lParam==GetDlgItem(hWnd,IDC_GROUP2) || (HWND)lParam==GetDlgItem(hWnd,IDC_GROUP3) )
				{
					SetTextColor((HDC) wParam, RGB(79,97,117));
					SetBkMode((HDC) wParam, TRANSPARENT);
					return (INT_PTR) GetStockObject(NULL_BRUSH);
				}
				break;
			}
		case WM_TIMER:
			{
				
				switch(wParam)
				{
					case 22:
						{
							s = (DXDlgSettings*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
							if(listening) ListenForControl(s->port);
							ControlTest(s->port);
							break;
						}
					
				}
				break;
			}

		case WM_COMMAND:
			s = (DXDlgSettings*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
			switch(LOWORD(wParam))
			{
				case IDC_COMBO1:
					switch(HIWORD(wParam))
					{
						case CBN_SELCHANGE:
							LoadFilter(s->port);
							break;
					}			
					break;
				case IDC_COMBO3:
					switch(HIWORD(wParam))
					{
						case CBN_SELCHANGE:
							DefaultFilters(s->port, SendMessage(GetDlgItem(hWnd,IDC_COMBO3), CB_GETCURSEL, 0, 0));
							SendMessage(GetDlgItem(hWnd,IDC_COMBO3), CB_SETCURSEL, -1, 0);
							break;
					}
					break;
				case IDC_COMBO4:
					switch(HIWORD(wParam))
					{
						case LBN_SELCHANGE:
							//selectedJoy[0] = SendDlgItemMessage(hWnd, IDC_COMBO4, CB_GETCURSEL, 0, 0);
							break;
					}
					break;

				case IDOK:
					{
						INVERTFORCES[s->port] = SendDlgItemMessage(hWnd, IDC_CHECK1, BM_GETCHECK, 0, 0);
						BYPASSCAL = SendDlgItemMessage(hWnd, IDC_CHECK2, BM_GETCHECK, 0, 0);
						useRamp = !!SendDlgItemMessage(hWnd, IDC_CHECK3, BM_GETCHECK, 0, 0);
						GAINZ[s->port][0] = SendMessage(GetDlgItem(hWnd, IDC_SLIDER4), TBM_GETPOS, 0, 0);
						FFMULTI[s->port][0] = SendMessage(GetDlgItem(hWnd, IDC_SLIDER5), TBM_GETPOS, 0, 0);
						SaveDInputConfig(s->port, s->dev_type);
						SaveConfig(); // Force save to ini file
						//Seems to create some dead locks
						//SendMessage(hWnd, WM_CLOSE, 0, 0);
						//return TRUE;
						dialogOpen = false;
						FreeDirectInput();
						EndDialog(hWnd, TRUE);
						return TRUE;
						
					}
					//break; //Fall through
				case IDCANCEL:
					{
						//Seems to create some dead locks
						//SendMessage(hWnd, WM_CLOSE, 0, 0);
						dialogOpen=false;
						FreeDirectInput();
						EndDialog(hWnd, FALSE);
						return TRUE;
						
					}
					break;
				case IDC_BUTTON1:
					{
						//MessageBeep(MB_ICONEXCLAMATION);
						TestForce(s->port);
					}
					break;

				case IDC_ASS0: { StartListen(CID_STEERING); break; }
				case IDC_ASS1: { StartListen(CID_STEERING_R); break; }
				case IDC_ASS2: { StartListen(CID_THROTTLE); break; }
				case IDC_ASS3: { StartListen(CID_BRAKE); break; }
				case IDC_ASS4: { StartListen(CID_HATUP); break; }
				case IDC_ASS5: { StartListen(CID_HATDOWN); break; }
				case IDC_ASS6: { StartListen(CID_HATLEFT); break; }
				case IDC_ASS7: { StartListen(CID_HATRIGHT); break; }
				case IDC_ASS8: { StartListen(CID_SQUARE); break; }
				case IDC_ASS9: { StartListen(CID_TRIANGLE); break; }
				case IDC_ASS10: { StartListen(CID_CROSS); break; }
				case IDC_ASS11: { StartListen(CID_CIRCLE); break; }
				case IDC_ASS12: { StartListen(CID_L1); break; }
				case IDC_ASS13: { StartListen(CID_R1); break; }
				case IDC_ASS14: { StartListen(CID_L2); break; }
				case IDC_ASS15: { StartListen(CID_R2); break; }
				case IDC_ASS16: { StartListen(CID_L3); break; }
				case IDC_ASS17: { StartListen(CID_R3); break; }
				case IDC_ASS18: { StartListen(CID_SELECT); break; }
				case IDC_ASS19: { StartListen(CID_START); break; }
				case IDC_DEL0: { DeleteControl(s->port, CID_STEERING); break; }
				case IDC_DEL1: { DeleteControl(s->port, CID_STEERING_R); break; }
				case IDC_DEL2: { DeleteControl(s->port, CID_THROTTLE); break; }
				case IDC_DEL3: { DeleteControl(s->port, CID_BRAKE); break; }
				case IDC_DEL4: { DeleteControl(s->port, CID_HATUP); break; }
				case IDC_DEL5: { DeleteControl(s->port, CID_HATDOWN); break; }
				case IDC_DEL6: { DeleteControl(s->port, CID_HATLEFT); break; }
				case IDC_DEL7: { DeleteControl(s->port, CID_HATRIGHT); break; }
				case IDC_DEL8: { DeleteControl(s->port, CID_SQUARE); break; }
				case IDC_DEL9: { DeleteControl(s->port, CID_TRIANGLE); break; }
				case IDC_DEL10: { DeleteControl(s->port, CID_CROSS); break; }
				case IDC_DEL11: { DeleteControl(s->port, CID_CIRCLE); break; }
				case IDC_DEL12: { DeleteControl(s->port, CID_L1); break; }
				case IDC_DEL13: { DeleteControl(s->port, CID_R1); break; }
				case IDC_DEL14: { DeleteControl(s->port, CID_L2); break; }
				case IDC_DEL15: { DeleteControl(s->port, CID_R2); break; }
				case IDC_DEL16: { DeleteControl(s->port, CID_L3); break; }
				case IDC_DEL17: { DeleteControl(s->port, CID_R3); break; }
				case IDC_DEL18: { DeleteControl(s->port, CID_SELECT); break; }
				case IDC_DEL19: { DeleteControl(s->port, CID_START); break; }


				case IDC_PICTURELINK1:{ShellExecuteA(NULL, "open", "http://www.ecsimhardware.com",NULL, NULL, SW_SHOWNORMAL);break;}
				case IDC_PICTURELINK2:{ShellExecuteA(NULL, "open", "http://www.ecsimshop.com",NULL, NULL, SW_SHOWNORMAL);break;}
				case IDC_PICTURELINK3:{ShellExecuteA(NULL, "open", "http://www.tocaedit.com",NULL, NULL, SW_SHOWNORMAL);break;}
			}
			break;

		case WM_CLOSE:
			{
				dialogOpen=false;
				FreeDirectInput();
				EndDialog(hWnd, 0);
			}
			break;

		case WM_DESTROY:
			//PostQuitMessage(0);
			return TRUE;
			break;
		case WM_HSCROLL:
			s = (DXDlgSettings*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
			ApplyFilter(s->port);
			break;
		case WM_PAINT:
			s = (DXDlgSettings*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
			OnPaint(s->port);
			break;
	}

	return FALSE;
}

void SaveDInputConfig(int port, const char *dev_type)
{
	SaveSetting(_T("dinput"), _T("BYPASSCAL"), BYPASSCAL);
	//SaveSetting(_T("dinput"), _T("controllers"), (int32_t)g_pJoysticks.size());

	wchar_t section[256];
	swprintf_s(section, L"%S dinput %d", dev_type, port);

	ClearSection(section);
	SaveSetting(section, _T("INVERTFORCES"), INVERTFORCES[port]);

	for (auto& control : g_Controls[port])
	{
		int cid = control.first;
		const InputMapped& im = control.second;
		auto joy = g_pJoysticks[im.index];

		//TODO all of it
		std::stringstream ss;
		ss << joy->GetGUID();
		//swprintf_s(section, _T("%" SFMTs " dinput %d - {%" SFMTs "}"), dev_type, port, ss.str().c_str());
		swprintf_s(section, _T("%" SFMTs " dinput %d"), dev_type, port);

		//SaveSetting(section, _T("ProductName"), joy->Product());

		swprintf_s(text, _T("GUID %i"), cid);
		SaveSetting(section, text, ss.str());
		swprintf_s(text, _T("MAPPINGTYPE %i"), cid);
		SaveSetting(section, text, im.type);
		swprintf_s(text, _T("CONTROL %i"), cid);
		SaveSetting(section, text, im.mapped);

		if (joy->GetControlType() == CT_JOYSTICK) {
			swprintf_s(text, _T("INVERTED %i"), cid);
			SaveSetting(section, text, im.INVERTED);
			swprintf_s(text, _T("HALF %i"), cid);
			SaveSetting(section, text, im.HALF);
			swprintf_s(text, _T("LINEAR %i"), cid);
			SaveSetting(section, text, LINEAR[port][cid]);
			swprintf_s(text, _T("OFFSET %i"), cid);
			SaveSetting(section, text, OFFSET[port][cid]);
			swprintf_s(text, _T("DEADZONE %i"), cid);
			SaveSetting(section, text, DEADZONE[port][cid]);
		}
	}

	SaveSetting(section, _T("GAINZ"), GAINZ[port][0]);
	SaveSetting(section, _T("FFMULTI"), FFMULTI[port][0]);
	//only for config dialog
	SaveSetting(section, _T("UseRamp"), useRamp);
}

void LoadDInputConfig(int port, const char* dev_type)
{
	LoadSetting(_T("dinput"), _T("BYPASSCAL"), BYPASSCAL);

	wchar_t section[256];
	swprintf_s(section, L"%S dinput %d", dev_type, port);

	LoadSetting(section, _T("INVERTFORCES"), INVERTFORCES[port]);
	if (!LoadSetting(section, _T("GAINZ"), GAINZ[port][0]))
		GAINZ[port][0] = 10000;

	if (!LoadSetting(section, _T("FFMULTI"), FFMULTI[port][0]))
		FFMULTI[port][0] = 0;

	//for (size_t i = 0; i < g_pJoysticks.size(); i++)
	{
		//TODO all of it
		//std::stringstream ss;
		//ss << g_pJoysticks[i]->GetGUID();

		//swprintf_s(section, _T("%" SFMTs " dinput %d - {%" SFMTs "}"), dev_type, port, ss.str().c_str());
		swprintf_s(section, _T("%" SFMTs " dinput %d"), dev_type, port);

		for (int cid = 0; cid < CID_COUNT; cid++)
		{
			InputMapped im = {};
			bool found = false;
			std::string guid;

			swprintf_s(text, _T("GUID %i"), cid);
			if (!LoadSetting(section, text, guid))
				continue;

			for (size_t i = 0; i < g_pJoysticks.size(); i++) {
				std::stringstream ss;
				ss << g_pJoysticks[i]->GetGUID();
				if (ss.str() == guid) {
					im.index = i;
					found = true;
					break;
				}
			}

			if (!found)
				continue;

			swprintf_s(text, _T("MAPPINGTYPE %i"), cid);
			if (!LoadSetting(section, text, (int32_t&)im.type))
				continue;

			if (im.type == MT_NONE) {
				OSDebugOut(_T("Skipping control %d (%s), mapping type is None\n"), cid, g_pJoysticks[im.index]->Product().c_str());
				continue;
			}

			swprintf_s(text, _T("CONTROL %i"), cid);
			if (!LoadSetting(section, text, im.mapped))
				continue;

			if (g_pJoysticks[im.index]->GetControlType() == CT_JOYSTICK)
			{
				swprintf_s(text, _T("INVERTED %i"), cid);
				LoadSetting(section, text, im.INVERTED);
				swprintf_s(text, _T("HALF %i"), cid);
				LoadSetting(section, text, im.HALF);
				swprintf_s(text, _T("LINEAR %i"), cid);
				LoadSetting(section, text, im.LINEAR);
				swprintf_s(text, _T("OFFSET %i"), cid);
				LoadSetting(section, text, im.OFFSET);
				swprintf_s(text, _T("DEADZONE %i"), cid);
				LoadSetting(section, text, im.DEADZONE);
			}

			//if (cid <= CID_BRAKE)
			{
				LINEAR[port][cid] = im.LINEAR;
				OFFSET[port][cid] = im.OFFSET;
				DEADZONE[port][cid] = im.DEADZONE;
			}


			AddInputMap(port, (ControlID)cid, im);
		}
	}

	LoadSetting(section, _T("UseRamp"), useRamp);
}


int DInputPad::Configure(int port, const char* dev_type, void *data)
{
	Win32Handles h = *(Win32Handles*)data;
	struct DXDlgSettings s;
	s.port = port;
	s.dev_type = dev_type;
	return DialogBoxParam(h.hInst, MAKEINTRESOURCE(IDD_DIALOG1), h.hWnd, DxDialogProc, (LPARAM)&s);
}

}} //namespace
#pragma warning (pop)
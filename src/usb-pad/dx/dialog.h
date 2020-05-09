#pragma warning (push)
// floats to int
#pragma warning (disable : 4244)

namespace usb_pad { namespace dx {

struct DXDlgSettings
{
	int port;
	const char* dev_type;
};

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


	filtercontrol = SendMessage(GetDlgItem(hWnd,IDC_COMBO1), CB_GETCURSEL, 0, 0);
	if(filtercontrol==-1)return;
	//slider
	SendMessage(GetDlgItem(hWnd,IDC_SLIDER1), TBM_SETPOS, 1, LINEAR[port][filtercontrol]+50 * PRECMULTI);
	SendMessage(GetDlgItem(hWnd,IDC_SLIDER2), TBM_SETPOS, 1, OFFSET[port][filtercontrol]+50 * PRECMULTI);
	SendMessage(GetDlgItem(hWnd,IDC_SLIDER3), TBM_SETPOS, 1, DEADZONE[port][filtercontrol]+50 * PRECMULTI);
	SendMessage(GetDlgItem(hWnd,IDC_SLIDER4), TBM_SETPOS, 1, GAINZ[port][0]);
	SendMessage(GetDlgItem(hWnd,IDC_SLIDER5), TBM_SETPOS, 1, FFMULTI[port][0]);

	ApplyFilter(port);

}

void ControlTest(int port) //thread: waits for window
{
	filtercontrol = SendMessage(GetDlgItem(hWnd,IDC_COMBO1), CB_GETCURSEL, 0, 0);
	if(filtercontrol>=0 && listening==false)
	{
		PollDevices();
		TESTV=ReadAxis(AXISID[port][filtercontrol], INVERT[port][filtercontrol], HALF[port][filtercontrol]);
		TESTVF=FilterControl(ReadAxis(AXISID[port][filtercontrol], INVERT[port][filtercontrol], HALF[port][filtercontrol]), LINEAR[port][filtercontrol], OFFSET[port][filtercontrol], DEADZONE[port][filtercontrol]);
		GetClientRect(GetDlgItem(hWnd,IDC_PICTURE), &rect);
		MapWindowPoints(GetDlgItem(hWnd,IDC_PICTURE), hWnd, (POINT *) &rect, 2);
		InvalidateRect( hWnd, &rect, TRUE );
	}else{
		TESTV=0;
	}

	return;
}

void ListenForControl(int port)
{
	LONG inv=0;
	LONG ini=0;
	LONG ax=0;
	LONG but=0;
	LONG ret=0;

	inv=0;
	ini=0;
	ret = FindControl(port, ax,inv,ini,but);

	if(ret){
		AXISID[port][CID] = ax;
		INVERT[port][CID] = inv;
		HALF[port][CID] = ini;
		BUTTON[port][CID] = but;
		
	}

	if(listening){
		swprintf_s(text, L"Listening... %u", GetListenTimeout()/1000+1);
		SetWindowText(GetDlgItem(hWnd,LABELS[CID]),text);
	}else{
		swprintf_s(text, L"%i/%i/%i/%i", AXISID[port][CID], INVERT[port][CID], HALF[port][CID], BUTTON[port][CID]);
		SetWindowText(GetDlgItem(hWnd,LABELS[CID]),text);
	}

}

void StartListen(char controlid)
{
	if(listening)return;

	CID = controlid;
	swprintf_s(text, L"Listening...");SetWindowText(GetDlgItem(hWnd,LABELS[CID]),text);
	OSDebugOut(TEXT("Begin Listen %i\n"), numj);
	ListenAxis();
}

void DeleteControl(int port, char controlid)
{
	CID = controlid;
	AXISID[port][CID] = -1;
	INVERT[port][CID] = -1;
	HALF[port][CID] = -1;
	BUTTON[port][CID] = -1;
	swprintf_s(text, L"%i/%i/%i/%i", AXISID[port][CID], INVERT[port][CID], HALF[port][CID], BUTTON[port][CID]);
	SetWindowText(GetDlgItem(hWnd,LABELS[CID]),text);
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
	if (px <= 0 || py <= 0) {
		return;
	}
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
	LoadMain(port, dev_type);

	InitDirectInput(hWnd, port);

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

	for(int i = 0;i<numc;i++)
	{
		swprintf_s(text, L"%i/%i/%i/%i", AXISID[port][i], INVERT[port][i], HALF[port][i], BUTTON[port][i]);
		SetWindowText(GetDlgItem(hWnd,LABELS[i]),text);
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
				if ((HWND)lParam == GetDlgItem(hWnd, IDC_BZ_CTL1_LBL1)
				|| (HWND)lParam == GetDlgItem(hWnd, IDC_BZ_CTL2_LBL1)
				|| (HWND)lParam == GetDlgItem(hWnd, IDC_BZ_CTL3_LBL1)
				|| (HWND)lParam == GetDlgItem(hWnd, IDC_BZ_CTL4_LBL1)) {
					SetTextColor((HDC)wParam, RGB(255, 0, 0));
					SetBkMode((HDC)wParam, TRANSPARENT);
					return (INT_PTR)GetStockObject(NULL_BRUSH);
				}
				if ((HWND)lParam == GetDlgItem(hWnd, IDC_BZ_CTL1_LBL2)
				|| (HWND)lParam == GetDlgItem(hWnd, IDC_BZ_CTL2_LBL2)
				|| (HWND)lParam == GetDlgItem(hWnd, IDC_BZ_CTL3_LBL2)
				|| (HWND)lParam == GetDlgItem(hWnd, IDC_BZ_CTL4_LBL2)) {
					SetTextColor((HDC)wParam, RGB(0, 192, 255));
					SetBkMode((HDC)wParam, TRANSPARENT);
					return (INT_PTR)GetStockObject(NULL_BRUSH);
				}
				if ((HWND)lParam == GetDlgItem(hWnd, IDC_BZ_CTL1_LBL3)
				|| (HWND)lParam == GetDlgItem(hWnd, IDC_BZ_CTL2_LBL3)
				|| (HWND)lParam == GetDlgItem(hWnd, IDC_BZ_CTL3_LBL3)
				|| (HWND)lParam == GetDlgItem(hWnd, IDC_BZ_CTL4_LBL3)) {
					SetTextColor((HDC)wParam, RGB(255, 165, 0));
					SetBkMode((HDC)wParam, TRANSPARENT);
					return (INT_PTR)GetStockObject(NULL_BRUSH);
				}
				if ((HWND)lParam == GetDlgItem(hWnd, IDC_BZ_CTL1_LBL4)
				|| (HWND)lParam == GetDlgItem(hWnd, IDC_BZ_CTL2_LBL4)
				|| (HWND)lParam == GetDlgItem(hWnd, IDC_BZ_CTL3_LBL4)
				|| (HWND)lParam == GetDlgItem(hWnd, IDC_BZ_CTL4_LBL4)) {
					SetTextColor((HDC)wParam, RGB(0, 140, 0));
					SetBkMode((HDC)wParam, TRANSPARENT);
					return (INT_PTR)GetStockObject(NULL_BRUSH);
				}
				if ((HWND)lParam == GetDlgItem(hWnd, IDC_BZ_CTL1_LBL5)
				|| (HWND)lParam == GetDlgItem(hWnd, IDC_BZ_CTL2_LBL5)
				|| (HWND)lParam == GetDlgItem(hWnd, IDC_BZ_CTL3_LBL5)
				|| (HWND)lParam == GetDlgItem(hWnd, IDC_BZ_CTL4_LBL5)) {
					SetTextColor((HDC)wParam, RGB(204, 204, 0));
					SetBkMode((HDC)wParam, TRANSPARENT);
					return (INT_PTR)GetStockObject(NULL_BRUSH);
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
						SaveMain(s->port, s->dev_type);
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

				case IDC_ASS0:{StartListen(0);break;}
				case IDC_ASS1:{StartListen(1);break;}
				case IDC_ASS2:{StartListen(2);break;}
				case IDC_ASS3:{StartListen(3);break;}
				case IDC_ASS4:{StartListen(4);break;}
				case IDC_ASS5:{StartListen(5);break;}
				case IDC_ASS6:{StartListen(6);break;}
				case IDC_ASS7:{StartListen(7);break;}
				case IDC_ASS8:{StartListen(8);break;}
				case IDC_ASS9:{StartListen(9);break;}
				case IDC_ASS10:{StartListen(10);break;}
				case IDC_ASS11:{StartListen(11);break;}
				case IDC_ASS12:{StartListen(12);break;}
				case IDC_ASS13:{StartListen(13);break;}
				case IDC_ASS14:{StartListen(14);break;}
				case IDC_ASS15:{StartListen(15);break;}
				case IDC_ASS16:{StartListen(16);break;}
				case IDC_ASS17:{StartListen(17);break;}
				case IDC_ASS18:{StartListen(18);break;}
				case IDC_ASS19:{StartListen(19);break;}
				case IDC_DEL0:{DeleteControl(s->port, 0);break;}
				case IDC_DEL1:{DeleteControl(s->port, 1);break;}
				case IDC_DEL2:{DeleteControl(s->port, 2);break;}
				case IDC_DEL3:{DeleteControl(s->port, 3);break;}
				case IDC_DEL4:{DeleteControl(s->port, 4);break;}
				case IDC_DEL5:{DeleteControl(s->port, 5);break;}
				case IDC_DEL6:{DeleteControl(s->port, 6);break;}
				case IDC_DEL7:{DeleteControl(s->port, 7);break;}
				case IDC_DEL8:{DeleteControl(s->port, 8);break;}
				case IDC_DEL9:{DeleteControl(s->port, 9);break;}
				case IDC_DEL10:{DeleteControl(s->port, 10);break;}
				case IDC_DEL11:{DeleteControl(s->port, 11);break;}
				case IDC_DEL12:{DeleteControl(s->port, 12);break;}
				case IDC_DEL13:{DeleteControl(s->port, 13);break;}
				case IDC_DEL14:{DeleteControl(s->port, 14);break;}
				case IDC_DEL15:{DeleteControl(s->port, 15);break;}
				case IDC_DEL16:{DeleteControl(s->port, 16);break;}
				case IDC_DEL17:{DeleteControl(s->port, 17);break;}
				case IDC_DEL18:{DeleteControl(s->port, 18);break;}
				case IDC_DEL19:{DeleteControl(s->port, 19);break;}


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

}} //namespace
#pragma warning (pop)
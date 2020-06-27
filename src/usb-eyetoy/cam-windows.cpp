

#include "videodev.h"
#include "cam-windows.h"
#include "usb-eyetoy-webcam.h"
#include "jo_mpeg.h"

#include "../Win32/Config.h"
#include "../Win32/resource.h"

namespace usb_eyetoy
{
namespace windows_api
{

HRESULT DirectShow::CallbackHandler::SampleCB(double time, IMediaSample *sample) {
	HRESULT hr;
	unsigned char* buffer;

	hr = sample->GetPointer((BYTE**)&buffer);
	if (hr != S_OK) return S_OK;

	if (callback) callback(buffer, sample->GetActualDataLength(), BITS_PER_PIXEL);
	return S_OK;
}

HRESULT DirectShow::CallbackHandler::QueryInterface(REFIID iid, LPVOID *ppv) {
	if( iid == IID_ISampleGrabberCB || iid == IID_IUnknown ) {
		*ppv = (void *) static_cast<ISampleGrabberCB*>( this );
		return S_OK;
	}
	return E_NOINTERFACE;
}

std::vector<std::wstring> getDevList() {
	std::vector<std::wstring> devList;

	ICreateDevEnum *pCreateDevEnum = 0;
	HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pCreateDevEnum));
	if (FAILED(hr)) {
		fprintf(stderr, "Error Creating Device Enumerator");
		return devList;
	}

	IEnumMoniker *pEnum = 0;
	hr = pCreateDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, NULL);
	if (FAILED(hr)) {
		fprintf(stderr, "You have no video capture hardware");
		return devList;
	};

	IMoniker *pMoniker = NULL;
	while (pEnum->Next(1, &pMoniker, NULL) == S_OK) {
		IPropertyBag *pPropBag;
		HRESULT hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
		if (FAILED(hr)) {
			pMoniker->Release();
			continue;
		}

		VARIANT var;
		VariantInit(&var);
		hr = pPropBag->Read(L"Description", &var, 0);
		if (FAILED(hr)) {
			hr = pPropBag->Read(L"FriendlyName", &var, 0);
		}
		if (SUCCEEDED(hr)) {
			devList.push_back(var.bstrVal);
			VariantClear(&var);
		}

		pPropBag->Release();
		pMoniker->Release();
	}

	pEnum->Release();
	CoUninitialize();

	return devList;
}

int DirectShow::InitializeDevice(std::wstring selectedDevice) {

	// Create the Capture Graph Builder.
	HRESULT hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pGraphBuilder));
	if (FAILED(hr)) {
		fprintf(stderr, "CoCreateInstance CLSID_CaptureGraphBuilder2 err : %x\n", hr);
		return -1;
	}

	// Create the Filter Graph Manager.
	hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pGraph));
	if (FAILED(hr)) {
		fprintf(stderr, "CoCreateInstance CLSID_FilterGraph err : %x\n", hr);
		return -1;
	}

	hr = pGraphBuilder->SetFiltergraph(pGraph);
	if (FAILED(hr)) {
		fprintf(stderr, "SetFiltergraph err : %x\n", hr);
		return -1;
	}

	hr = pGraph->QueryInterface(IID_IMediaControl, (void **)&pControl);
	if (FAILED(hr)) {
		fprintf(stderr, "QueryInterface IID_IMediaControl err : %x\n", hr);
		return -1;
	}

	// enumerate all video capture devices
	ICreateDevEnum *pCreateDevEnum = 0;
	hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pCreateDevEnum));
	if (FAILED(hr)) {
		fprintf(stderr, "Error Creating Device Enumerator");
		return -1;
	}

	IEnumMoniker *pEnum = 0;
	hr = pCreateDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, NULL);
	if (FAILED(hr)) {
		fprintf(stderr, "You have no video capture hardware");
		return -1;
	};

	pEnum->Reset();

	IMoniker *pMoniker;
	while (pEnum->Next(1, &pMoniker, NULL) == S_OK && sourcefilter == NULL) {
		IPropertyBag *pPropBag = 0;
		hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
		if (FAILED(hr)) {
			fprintf(stderr, "BindToStorage err : %x\n", hr);
			goto freeMoniker;
		}

		VARIANT var;
		VariantInit(&var);

		hr = pPropBag->Read(L"Description", &var, 0);
		if (FAILED(hr)) {
			hr = pPropBag->Read(L"FriendlyName", &var, 0);
		}
		if (FAILED(hr)) {
			fprintf(stderr, "Read name err : %x\n", hr);
			goto freeVar;
		}
		fprintf(stderr, "Camera: '%ls'\n", var.bstrVal);
		if (!selectedDevice.empty() && selectedDevice != var.bstrVal) {
			goto freeVar;
		}

		//add a filter for the device
		hr = pGraph->AddSourceFilterForMoniker(pMoniker, NULL, L"sourcefilter", &sourcefilter);
		if (FAILED(hr)) {
			fprintf(stderr, "AddSourceFilterForMoniker err : %x\n", hr);
			goto freeVar;
		}

		hr = pGraphBuilder->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, sourcefilter, IID_IAMStreamConfig, (void **)&pSourceConfig);
		if (SUCCEEDED(hr)) {
			int iCount = 0, iSize = 0;
			hr = pSourceConfig->GetNumberOfCapabilities(&iCount, &iSize);

			// Check the size to make sure we pass in the correct structure.
			if (iSize == sizeof(VIDEO_STREAM_CONFIG_CAPS)) {
				// Use the video capabilities structure.
				for (int iFormat = 0; iFormat < iCount; iFormat++) {
					VIDEO_STREAM_CONFIG_CAPS scc;
					AM_MEDIA_TYPE *pmtConfig;
					hr = pSourceConfig->GetStreamCaps(iFormat, &pmtConfig, (BYTE *)&scc);
					fprintf(stderr, "GetStreamCaps min=%dx%d max=%dx%d, fmt=%x\n",
							scc.MinOutputSize.cx, scc.MinOutputSize.cy,
							scc.MaxOutputSize.cx, scc.MaxOutputSize.cy,
							pmtConfig->subtype);

					if (SUCCEEDED(hr)) {
						if ((pmtConfig->majortype == MEDIATYPE_Video) &&
							(pmtConfig->formattype == FORMAT_VideoInfo) &&
							(pmtConfig->cbFormat >= sizeof(VIDEOINFOHEADER)) &&
							(pmtConfig->pbFormat != NULL)) {

							VIDEOINFOHEADER *pVih = (VIDEOINFOHEADER *)pmtConfig->pbFormat;
							pVih->bmiHeader.biWidth = 320;
							pVih->bmiHeader.biHeight = 240;
							pVih->bmiHeader.biSizeImage = DIBSIZE(pVih->bmiHeader);
							hr = pSourceConfig->SetFormat(pmtConfig);
						}
						//DeleteMediaType(pmtConfig);
					}
				}
			}
		}

		// Create the Sample Grabber filter.
		hr = CoCreateInstance(CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&samplegrabberfilter));
		if (FAILED(hr)) {
			fprintf(stderr, "CoCreateInstance CLSID_SampleGrabber err : %x\n", hr);
			goto freeVar;
		}

		hr = pGraph->AddFilter(samplegrabberfilter, L"samplegrabberfilter");
		if (FAILED(hr)) {
			fprintf(stderr, "AddFilter samplegrabberfilter err : %x\n", hr);
			goto freeVar;
		}

		//set mediatype on the samplegrabber
		hr = samplegrabberfilter->QueryInterface(IID_PPV_ARGS(&samplegrabber));
		if (FAILED(hr)) {
			fprintf(stderr, "QueryInterface err : %x\n", hr);
			goto freeVar;
		}

		AM_MEDIA_TYPE mt;
		ZeroMemory(&mt, sizeof(mt));
		mt.majortype = MEDIATYPE_Video;
		mt.subtype = MEDIASUBTYPE_RGB24;
		hr = samplegrabber->SetMediaType(&mt);
		if (FAILED(hr)) {
			fprintf(stderr, "SetMediaType err : %x\n", hr);
			goto freeVar;
		}

		//add the callback to the samplegrabber
		hr = samplegrabber->SetCallback(callbackhandler, 0);
		if (hr != S_OK) {
			fprintf(stderr, "SetCallback err : %x\n", hr);
			goto freeVar;
		}

		//set the null renderer
		hr = CoCreateInstance(CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&nullrenderer));
		if (FAILED(hr)) {
			fprintf(stderr, "CoCreateInstance CLSID_NullRenderer err : %x\n", hr);
			goto freeVar;
		}

		hr = pGraph->AddFilter(nullrenderer, L"nullrenderer");
		if (FAILED(hr)) {
			fprintf(stderr, "AddFilter nullrenderer err : %x\n", hr);
			goto freeVar;
		}

		//set the render path
		hr = pGraphBuilder->RenderStream(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, sourcefilter, samplegrabberfilter, nullrenderer);
		if (FAILED(hr)) {
			fprintf(stderr, "RenderStream err : %x\n", hr);
			goto freeVar;
		}

		// if the stream is started, start capturing immediatly
		LONGLONG start = 0, stop = MAXLONGLONG;
		hr = pGraphBuilder->ControlStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, sourcefilter, &start, &stop, 1, 2);
		if (FAILED(hr)) {
			fprintf(stderr, "ControlStream err : %x\n", hr);
			goto freeVar;
		}

	freeVar:
		VariantClear(&var);
		pPropBag->Release();

	freeMoniker:
		pMoniker->Release();
	}
	pEnum->Release();
	if (sourcefilter == NULL) {
		return -1;
	}
	return 0;
}

void DirectShow::Start() {
	HRESULT hr = nullrenderer->Run(0);
	if (FAILED(hr)) throw hr;

	hr = samplegrabberfilter->Run(0);
	if (FAILED(hr)) throw hr;

	hr = sourcefilter->Run(0);
	if (FAILED(hr)) throw hr;
}

void DirectShow::Stop() {
	HRESULT hr = sourcefilter->Stop();
	if (FAILED(hr)) throw hr;

	hr = samplegrabberfilter->Stop();
	if (FAILED(hr)) throw hr;

	hr = nullrenderer->Stop();
	if (FAILED(hr)) throw hr;
}

buffer_t mpeg_buffer = { NULL, 0 };
std::mutex mpeg_mutex;

void store_mpeg_frame(unsigned char *data, unsigned int len) {
	mpeg_mutex.lock();
	memcpy(mpeg_buffer.start, data, len);
	mpeg_buffer.length = len;
	mpeg_mutex.unlock();
}

void dshow_callback(unsigned char *data, int len, int bitsperpixel) {
	if (bitsperpixel == 24) {
		unsigned char *mpegData = (unsigned char *)calloc(1, 320 * 240 * 2);
		int mpegLen = jo_write_mpeg(mpegData, data, 320, 240, JO_RGB24, JO_FLIP_X, JO_FLIP_Y);
		store_mpeg_frame(mpegData, mpegLen);
		free(mpegData);
	} else {
		fprintf(stderr, "dshow_callback: unk format: len=%d bpp=%d\n", len, bitsperpixel);
	}
}

void create_dummy_frame() {
	const int width = 320;
	const int height = 240;
	const int bytesPerPixel = 3;

	unsigned char *rgbData = (unsigned char*) calloc(1, width * height * bytesPerPixel);
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			unsigned char *ptr = rgbData + (y*width+x) * bytesPerPixel;
			ptr[0] = 255-y;
			ptr[1] = y;
			ptr[2] = 255-y;
		}
	}
	unsigned char *mpegData = (unsigned char*) calloc(1, width * height * bytesPerPixel);
	int mpegLen = jo_write_mpeg(mpegData, rgbData, width, height, JO_RGB24, JO_NONE, JO_NONE);
	free(rgbData);

	store_mpeg_frame(mpegData, mpegLen);
	free(mpegData);
}

DirectShow::DirectShow(int port) {
	mPort = port;
	pGraphBuilder = NULL;
	pGraph = NULL;
	pControl = NULL;
	sourcefilter = NULL;
	samplegrabberfilter = NULL;
	nullrenderer = NULL;
	pSourceConfig = NULL;
	samplegrabber = NULL;
	callbackhandler = new CallbackHandler();
	CoInitialize(NULL);
}

int DirectShow::Open() {
	mpeg_buffer.start = calloc(1, 320 * 240 * 2);
	create_dummy_frame();

	std::wstring selectedDevice;
	LoadSetting(EyeToyWebCamDevice::TypeName(), Port(), APINAME, N_DEVICE, selectedDevice);

	int ret = InitializeDevice(selectedDevice);
	if (ret < 0) {
		fprintf(stderr, "Camera: cannot find '%ls'\n", selectedDevice.c_str());
		return -1;
	}

	pControl->Run();
	this->Stop();
	this->SetCallback(dshow_callback);
	this->Start();

	return 0;
};

int DirectShow::Close() {
	if (sourcefilter != NULL) {
		this->Stop();
		pControl->Stop();

		sourcefilter->Release();
		pSourceConfig->Release();
		samplegrabberfilter->Release();
		samplegrabber->Release();
		nullrenderer->Release();
	}

	pGraphBuilder->Release();
	pGraph->Release();
	pControl->Release();

	if (mpeg_buffer.start != NULL) {
		free(mpeg_buffer.start);
		mpeg_buffer.start = NULL;
	}
	return 0;
};

int DirectShow::GetImage(uint8_t *buf, int len) {
	mpeg_mutex.lock();
	int len2 = mpeg_buffer.length;
	if (len < mpeg_buffer.length) len2 = len;
	memcpy(buf, mpeg_buffer.start, len2);
	mpeg_mutex.unlock();
	return len2;
};

BOOL CALLBACK DirectShowDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	int port;

	switch (uMsg) {
		case WM_CREATE:
			SetWindowLongPtr(hW, GWLP_USERDATA, (LONG)lParam);
			break;
		case WM_INITDIALOG: {
			port = (int)lParam;
			SetWindowLongPtr(hW, GWLP_USERDATA, (LONG)lParam);

			std::wstring selectedDevice;
			LoadSetting(EyeToyWebCamDevice::TypeName(), port, APINAME, N_DEVICE, selectedDevice);
			SendDlgItemMessage(hW, IDC_COMBO1, CB_RESETCONTENT, 0, 0);

			std::vector<std::wstring> devList = getDevList();
			for (auto i = 0; i != devList.size(); i++) {
				SendDlgItemMessageW(hW, IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)devList[i].c_str());
				if (selectedDevice == devList.at(i)) {
					SendDlgItemMessage(hW, IDC_COMBO1, CB_SETCURSEL, i, i);
				}
			}
			return TRUE;
		}
		case WM_COMMAND:
			if (HIWORD(wParam) == BN_CLICKED) {
				switch (LOWORD(wParam)) {
					case IDOK: {
						INT_PTR res = RESULT_OK;
						static wchar_t selectedDevice[500] = {0};
						GetWindowTextW(GetDlgItem(hW, IDC_COMBO1), selectedDevice, countof(selectedDevice));
						port = (int)GetWindowLongPtr(hW, GWLP_USERDATA);
						if (!SaveSetting<std::wstring>(EyeToyWebCamDevice::TypeName(), port, APINAME, N_DEVICE, selectedDevice)) {
							res = RESULT_FAILED;
						}
						EndDialog(hW, res);
						return TRUE;
					}
					case IDCANCEL:
						EndDialog(hW, RESULT_CANCELED);
						return TRUE;
				}
			}
	}
	return FALSE;
}

int DirectShow::Configure(int port, const char *dev_type, void *data) {
	Win32Handles handles = *(Win32Handles *)data;
	return DialogBoxParam(handles.hInst,
						  MAKEINTRESOURCE(IDD_DLG_EYETOY),
						  handles.hWnd,
						  (DLGPROC)DirectShowDlgProc, port);
};

} // namespace windows_api
} // namespace usb_eyetoy

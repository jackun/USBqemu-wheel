#include <mutex>

#include "videodev.h"
#include "cam-windows.h"
#include "jo_mpeg.h"


DShowVideoDevice::DShowVideoDevice() {
	friendlyname = (char*) calloc(1, MAX_DEVICE_NAME * sizeof(char));
	filtername =  (WCHAR*) calloc(1, MAX_DEVICE_NAME * sizeof(WCHAR));

	id					= -1;
	sourcefilter		= 0;
	samplegrabberfilter = 0;
	nullrenderer		= 0;
	callbackhandler		= new CallbackHandler(this);
}

DShowVideoDevice::~DShowVideoDevice() {
	free(friendlyname);
	free(filtername);
}

int DShowVideoDevice::GetId() {
	return id;
}

const char *DShowVideoDevice::GetFriendlyName() {
	return friendlyname;
}

void DShowVideoDevice::SetCallback(DShowVideoCaptureCallback cb) {
	callbackhandler->SetCallback(cb);
}

void DShowVideoDevice::Start() {
	HRESULT hr = nullrenderer->Run(0);
	if (FAILED(hr)) throw hr;

	hr = samplegrabberfilter->Run(0);
	if (FAILED(hr)) throw hr;

	hr = sourcefilter->Run(0);
	if (FAILED(hr)) throw hr;
}

void DShowVideoDevice::Stop() {
	HRESULT hr = sourcefilter->Stop();
	if (FAILED(hr)) throw hr;

	hr = samplegrabberfilter->Stop();
	if (FAILED(hr)) throw hr;

	hr = nullrenderer->Stop();
	if (FAILED(hr)) throw hr;
}

DShowVideoDevice::CallbackHandler::CallbackHandler(DShowVideoDevice *vd) {
	callback = 0;
	parent = vd;
}

DShowVideoDevice::CallbackHandler::~CallbackHandler() {}

void DShowVideoDevice::CallbackHandler::SetCallback(DShowVideoCaptureCallback cb) {
	callback = cb;
}

HRESULT DShowVideoDevice::CallbackHandler::SampleCB(double time, IMediaSample *sample) {
	HRESULT hr;
	unsigned char* buffer;

	hr = sample->GetPointer((BYTE**)&buffer);
	if (hr != S_OK) return S_OK;

	if (callback) callback(buffer, sample->GetActualDataLength(), BITS_PER_PIXEL, parent);
	return S_OK;
}

HRESULT DShowVideoDevice::CallbackHandler::BufferCB(double time, BYTE *buffer, long len) {
	return S_OK;
}

HRESULT DShowVideoDevice::CallbackHandler::QueryInterface(REFIID iid, LPVOID *ppv) {
	if( iid == IID_ISampleGrabberCB || iid == IID_IUnknown ) {
		*ppv = (void *) static_cast<ISampleGrabberCB*>( this );
		return S_OK;
	}
	return E_NOINTERFACE;
}

ULONG DShowVideoDevice::CallbackHandler::AddRef() {
	return 1;
}

ULONG DShowVideoDevice::CallbackHandler::Release() {
	return 2;
}

DShowVideoCapture::DShowVideoCapture() {
	CoInitialize(NULL);

	playing = false;
	current = 0;
	devices = new DShowVideoDevice[MAX_DEVICES];

	InitializeGraph();
	InitializeVideo();

	pControl->Run();
	for (int i = 0; i < num_devices; i++) {
		devices[i].Stop();
	}
}

DShowVideoCapture::~DShowVideoCapture() {
	delete[] devices;
}

DShowVideoDevice *DShowVideoCapture::GetDevices() {
	return devices;
}

int DShowVideoCapture::NumDevices() {
	return num_devices;
}

int DShowVideoCapture::InitializeGraph() {
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
	return 0;
}

int DShowVideoCapture::InitializeVideo() {
	HRESULT hr;

	// enumerate all video capture devices
	ICreateDevEnum *pCreateDevEnum = 0;
	hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pCreateDevEnum));
	if (FAILED(hr)) {
		fprintf(stderr, "Error Creating Device Enumerator");
		return -1;
	}

	IEnumMoniker *pEm = 0;
	hr = pCreateDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEm, NULL);
	if (FAILED(hr)) {
		fprintf(stderr, "You have no video capture hardware");
		return -1;
	};

	pEm->Reset();
	IMoniker *pM;

	num_devices = 0;
	while (hr = pEm->Next(1, &pM, 0), hr == S_OK) {
		IPropertyBag *pBag = 0;

		hr = pM->BindToStorage(0, 0, IID_IPropertyBag, (void**)&pBag);
		if (FAILED(hr)) {
			fprintf(stderr, "BindToStorage err : %x\n", hr);
			goto freeMoniker;
		}

		VARIANT var;
		VariantInit(&var);
		hr = pBag->Read(L"Description", &var, 0);
		if (FAILED(hr)) {
			hr = pBag->Read(L"FriendlyName", &var, 0);
		}
		if (FAILED(hr)) {
			fprintf(stderr, "Read name err : %x\n", hr);
			goto freeVar;
		}

		DShowVideoDevice *dev = devices + num_devices++;
		BSTR ptr = var.bstrVal;

		for (int c = 0; *ptr; c++, ptr++) {
			dev->filtername[c] = *ptr;
			dev->friendlyname[c] = *ptr & 0xFF;
		}
		fprintf(stderr, "Camera %d: %s\n", num_devices, dev->friendlyname);

		//add a filter for the device
		hr = pGraph->AddSourceFilterForMoniker(pM, NULL, dev->filtername, &dev->sourcefilter);
		if (FAILED(hr)) {
			fprintf(stderr, "AddSourceFilterForMoniker err : %x\n", hr);
			goto freeVar;
		}

		// Create the Sample Grabber filter.
		hr = CoCreateInstance(CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dev->samplegrabberfilter));
		if (FAILED(hr)) {
			fprintf(stderr, "CoCreateInstance CLSID_SampleGrabber err : %x\n", hr);
			goto freeVar;
		}

		//set mediatype on the samplegrabber
		hr = dev->samplegrabberfilter->QueryInterface(IID_PPV_ARGS(&dev->samplegrabber));
		if (FAILED(hr)) {
			fprintf(stderr, "QueryInterface err : %x\n", hr);
			goto freeVar;
		}

		WCHAR filtername[MAX_DEVICE_NAME + 2];
		wcscpy(filtername, L"SG ");
		wcscpy(filtername+3, dev->filtername);
		pGraph->AddFilter(dev->samplegrabberfilter, filtername);

		hr = pGraphBuilder->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, dev->sourcefilter, IID_IAMStreamConfig, (void **)&dev->pStreamConfig);
		if (SUCCEEDED(hr)) {
			int iCount = 0, iSize = 0;
			hr = dev->pStreamConfig->GetNumberOfCapabilities(&iCount, &iSize);

			// Check the size to make sure we pass in the correct structure.
			if (iSize == sizeof(VIDEO_STREAM_CONFIG_CAPS)) {
				// Use the video capabilities structure.
				for (int iFormat = 0; iFormat < iCount; iFormat++) {
					VIDEO_STREAM_CONFIG_CAPS scc;
					AM_MEDIA_TYPE *pmtConfig;
					hr = dev->pStreamConfig->GetStreamCaps(iFormat, &pmtConfig, (BYTE *)&scc);
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
							hr = dev->pStreamConfig->SetFormat(pmtConfig);
						}
						//DeleteMediaType(pmtConfig);
					}
				}
			}
		}

		AM_MEDIA_TYPE mt;
		ZeroMemory(&mt, sizeof(mt));
		mt.majortype = MEDIATYPE_Video;
		mt.subtype = MEDIASUBTYPE_RGB24;
		hr = dev->samplegrabber->SetMediaType(&mt);
		if (FAILED(hr)) {
			fprintf(stderr, "SetMediaType err : %x\n", hr);
			goto freeVar;
		}

		//add the callback to the samplegrabber
		hr = dev->samplegrabber->SetCallback(dev->callbackhandler, 0);
		if (hr != S_OK) {
			fprintf(stderr, "SetCallback err : %x\n", hr);
			goto freeVar;
		}

		//set the null renderer
		hr = CoCreateInstance(CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dev->nullrenderer));
		if (FAILED(hr)) {
			fprintf(stderr, "CoCreateInstance CLSID_NullRenderer err : %x\n", hr);
			goto freeVar;
		}

		wcscpy(filtername, L"NR ");
		wcscpy(filtername+3, dev->filtername);
		pGraph->AddFilter(dev->nullrenderer, filtername);

		//set the render path
		hr = pGraphBuilder->RenderStream(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, dev->sourcefilter, dev->samplegrabberfilter, dev->nullrenderer);
		if (FAILED(hr)) {
			fprintf(stderr, "RenderStream err : %x\n", hr);
			goto freeVar;
		}

		//if the stream is started, start capturing immediatly
		LONGLONG start = 0, stop = MAXLONGLONG;
		hr = pGraphBuilder->ControlStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, dev->sourcefilter, &start, &stop, 1, 2);
		if (FAILED(hr)) {
			fprintf(stderr, "ControlStream err : %x\n", hr);
			goto freeVar;
		}

		//reference the graph
		dev->pGraph = pGraph;
		dev->id = num_devices;

	freeVar:
		VariantClear(&var);
		pBag->Release();

	freeMoniker:
		pM->Release();
	}
	return 0;
}

/////////////////////////////////////////////////////////

DShowVideoCapture *vc = nullptr;
DShowVideoDevice *devices = nullptr;

buffer_t mpeg_buffer;
std::mutex mpeg_mutex;

static void store_mpeg_frame(unsigned char *data, unsigned int len) {
	mpeg_mutex.lock();
	memcpy(mpeg_buffer.start, data, len);
	mpeg_buffer.length = len;
	mpeg_mutex.unlock();
}

void dshow_callback(unsigned char *data, int len, int bpp, DShowVideoDevice* dev) {
	if (bpp == 24) {
		unsigned char *mpegData = (unsigned char *)calloc(1, 320 * 240 * 2);
		int mpegLen = jo_write_mpeg(mpegData, data, 320, 240, JO_RGB24, JO_FLIP_X, JO_FLIP_Y);
		store_mpeg_frame(mpegData, mpegLen);
		free(mpegData);
	} else {
		fprintf(stderr, "dshow_callback: unk format: len=%d bpp=%d\n", len, bpp);
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

namespace usb_eyetoy
{
namespace windows_api
{


int DirectShow::Open() {
	mpeg_buffer.start = calloc(1, 320 * 240 * 2);
	create_dummy_frame();

	vc = new DShowVideoCapture();
	devices = vc->GetDevices();
	int num_devices = vc->NumDevices();

	devices[0].SetCallback(dshow_callback);
	devices[0].Start();

	return 0;
};

int DirectShow::Close() {
	devices[0].Stop();
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

int DirectShow::Reset() {
	return 0;
};

} // namespace windows_api
} // namespace usb_eyetoy

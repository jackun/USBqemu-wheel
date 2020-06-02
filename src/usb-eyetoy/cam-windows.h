#include "videodev.h"


#pragma comment(lib, "strmiids")

#include <windows.h>
#include <dshow.h>

extern "C" {
	extern GUID IID_ISampleGrabberCB;
	extern GUID IID_ISampleGrabber;
	extern GUID CLSID_SampleGrabber;
	extern GUID CLSID_NullRenderer;
}

#pragma region qedit.h
struct __declspec(uuid("0579154a-2b53-4994-b0d0-e773148eff85"))
ISampleGrabberCB : IUnknown {
	virtual HRESULT __stdcall SampleCB (double SampleTime, struct IMediaSample *pSample) = 0;
	virtual HRESULT __stdcall BufferCB (double SampleTime, unsigned char *pBuffer, long BufferLen) = 0;
};

struct __declspec(uuid("6b652fff-11fe-4fce-92ad-0266b5d7c78f"))
ISampleGrabber : IUnknown {
	virtual HRESULT __stdcall SetOneShot (long OneShot) = 0;
	virtual HRESULT __stdcall SetMediaType (struct _AMMediaType *pType) = 0;
	virtual HRESULT __stdcall GetConnectedMediaType (struct _AMMediaType *pType) = 0;
	virtual HRESULT __stdcall SetBufferSamples (long BufferThem) = 0;
	virtual HRESULT __stdcall GetCurrentBuffer (long *pBufferSize, long *pBuffer) = 0;
	virtual HRESULT __stdcall GetCurrentSample (struct IMediaSample **ppSample) = 0;
	virtual HRESULT __stdcall SetCallback (struct ISampleGrabberCB *pCallback, long WhichMethodToCallback) = 0;
};

struct __declspec(uuid("c1f400a0-3f08-11d3-9f0b-006008039e37"))
SampleGrabber;

#pragma endregion


#ifndef MAXLONGLONG
#define MAXLONGLONG 0x7FFFFFFFFFFFFFFF
#endif

#ifndef MAX_DEVICES
#define MAX_DEVICES 8
#endif

#ifndef MAX_DEVICE_NAME
#define MAX_DEVICE_NAME 80
#endif

#ifndef BITS_PER_PIXEL
#define BITS_PER_PIXEL 24
#endif

class DShowVideoDevice;
typedef void (*DShowVideoCaptureCallback)(unsigned char *data, int len, int bitsperpixel, DShowVideoDevice *dev);

class DShowVideoDevice {
public:
	DShowVideoDevice();
	~DShowVideoDevice();

	int GetId();
	const char *GetFriendlyName();
	void SetCallback(DShowVideoCaptureCallback cb);

	void Start();
	void Stop();

private:
	int	id;
	char *friendlyname;
	WCHAR *filtername;
	
	IBaseFilter *sourcefilter;
	IBaseFilter *samplegrabberfilter;
	IBaseFilter *nullrenderer;
	IAMStreamConfig *pStreamConfig;

	ISampleGrabber *samplegrabber;

	IFilterGraph2 *pGraph;

	class CallbackHandler : public ISampleGrabberCB {
	public:
		CallbackHandler(DShowVideoDevice *parent);
		~CallbackHandler();

		void SetCallback(DShowVideoCaptureCallback cb);

		virtual HRESULT __stdcall SampleCB(double time, IMediaSample *sample);
		virtual HRESULT __stdcall BufferCB(double time, BYTE *buffer, long len);
		virtual HRESULT __stdcall QueryInterface(REFIID iid, LPVOID *ppv);
		virtual ULONG	__stdcall AddRef();
		virtual ULONG	__stdcall Release();

	private:
		DShowVideoCaptureCallback callback;
		DShowVideoDevice *parent;

	} *callbackhandler;

	friend class DShowVideoCapture;
};

class DShowVideoCapture {
public:
	DShowVideoCapture();
	~DShowVideoCapture();

	DShowVideoDevice *GetDevices();
	int NumDevices();

protected:
	void InitializeGraph();
	void InitializeVideo();

private:
	IFilterGraph2 *pGraph;
	ICaptureGraphBuilder2 *pGraphBuilder;
	IMediaControl *pControl;

	bool playing;

	DShowVideoDevice *devices;
	DShowVideoDevice *current;

	int num_devices;
};


typedef struct {
	void *start = NULL;
	size_t length;
} buffer_t;

namespace usb_eyetoy
{
namespace windows_api
{

static const char *APINAME = "DirectShow";

class DirectShow : public VideoDevice {
public:
	DirectShow(int port) : mPort(port) {};
	~DirectShow(){};
	int Open();
	int Close();
	int GetImage(uint8_t *buf, int len);
	int Reset();

	static const TCHAR *Name() {
		return TEXT("DirectShow");
	}
	static int Configure(int port, const char *dev_type, void *data) {
		return 0;
	};

	int Port() { return mPort; }
	void Port(int port) { mPort = port; }

protected:
	int mPort;
};

} // namespace windows_api
} // namespace usb_eyetoy

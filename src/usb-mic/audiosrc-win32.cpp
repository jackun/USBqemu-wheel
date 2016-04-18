// Used OBS as example

#include "../USB.h"
#include "mic-audiodefs.h"
#include "../libsamplerate/samplerate.h"

#include <assert.h>
#include <Mmdeviceapi.h>
#include <Audioclient.h>
#include <propsys.h>
#include <Functiondiscoverykeys_devpkey.h>

#define SafeRelease(x) if(x){x->Release(); x = NULL;}
#define ConvertMSTo100NanoSec(ms) (ms*1000*10) //1000 microseconds, then 10 "100nanosecond" segments

static FILE* file = nullptr;

LARGE_INTEGER clockFreq = { 0 };
__declspec(thread) LONGLONG lastQPCTime = 0;
LONGLONG lastTimeMS = 0;
LONGLONG GetQPCTimeMS()
{
	LARGE_INTEGER currentTime;
	QueryPerformanceCounter(&currentTime);

	if (currentTime.QuadPart < lastQPCTime)
		OSDebugOut(TEXT("GetQPCTimeMS: WTF, clock went backwards! %I64d < %I64d"), currentTime.QuadPart, lastQPCTime);

	lastQPCTime = currentTime.QuadPart;

	LONGLONG timeVal = currentTime.QuadPart;
	timeVal *= 1000;
	timeVal /= clockFreq.QuadPart;

	return timeVal;
}

LONGLONG GetQPCTime100NS()
{
	LARGE_INTEGER currentTime;
	QueryPerformanceCounter(&currentTime);

	if (currentTime.QuadPart < lastQPCTime)
		OSDebugOut(TEXT("GetQPCTime100NS: WTF, clock went backwards! %I64d < %I64d"), currentTime.QuadPart, lastQPCTime);

	lastQPCTime = currentTime.QuadPart;

	double timeVal = double(currentTime.QuadPart);
	timeVal *= 10000000.0;
	timeVal /= double(clockFreq.QuadPart);

	return LONGLONG(timeVal);
}

bool AudioInit()
{
	QueryPerformanceFrequency(&clockFreq);
	HRESULT hr = CoInitialize(0);// Ex(nullptr, COINIT_APARTMENTTHREADED);
	if (S_OK != hr && S_FALSE != hr /* already inited */)
	{
		OSDebugOut(TEXT("Com initialization failed with %d\n"), hr);
		return false;
	}
	//mComDealloc = new FunctionDeallocator< void(__stdcall*)(void) >(CoUninitialize);

	//CoInitialize(0);
	return true;
}

void AudioDeinit()
{
	CoUninitialize();
}

void GetAudioDevices(std::vector<AudioDeviceInfo> &devices)
{
    const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
    const IID IID_IMMDeviceEnumerator    = __uuidof(IMMDeviceEnumerator);
    IMMDeviceEnumerator *mmEnumerator;
    HRESULT err;

    err = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&mmEnumerator);
    if(FAILED(err))
    {
        OSDebugOut(TEXT("GetAudioDevices: Could not create IMMDeviceEnumerator\n"));
        return;
    }

    IMMDeviceCollection *collection;
    EDataFlow audioDeviceType = eCapture;
    DWORD flags = DEVICE_STATE_ACTIVE;
    //if (!bConnectedOnly)
        flags |= DEVICE_STATE_UNPLUGGED;

    err = mmEnumerator->EnumAudioEndpoints(audioDeviceType, flags, &collection);
    if(FAILED(err))
    {
        OSDebugOut(TEXT("GetAudioDevices: Could not enumerate audio endpoints\n"));
        SafeRelease(mmEnumerator);
        return;
    }

    UINT count;
    if(SUCCEEDED(collection->GetCount(&count)))
    {
        for(UINT i=0; i<count; i++)
        {
            IMMDevice *device;
            if(SUCCEEDED(collection->Item(i, &device)))
            {
                const WCHAR *wstrID;
                if(SUCCEEDED(device->GetId((LPWSTR*)&wstrID)))
                {
                    IPropertyStore *store;
                    if(SUCCEEDED(device->OpenPropertyStore(STGM_READ, &store)))
                    {
                        PROPVARIANT varName;

                        PropVariantInit(&varName);
                        if(SUCCEEDED(store->GetValue(PKEY_Device_FriendlyName, &varName)))
                        {
                            const WCHAR *wstrName = varName.pwszVal;

                            AudioDeviceInfo info;
                            info.strID = wstrID;
                            info.strName = wstrName;
							devices.push_back(info);
                        }
                    }

                    CoTaskMemFree((LPVOID)wstrID);
                }

                SafeRelease(device);
            }
        }
    }

    //-------------------------------------------------------

    SafeRelease(collection);
    SafeRelease(mmEnumerator);
}

template<typename T>
class QueueBuffer
{
public:
	QueueBuffer()
	: mBuffer(NULL)
	, mPos(0)
	, mLen(1<<20)
	, mPosPtr(0)
	{
		mBuffer = (T*)malloc(sizeof(T) * mLen);
	}

	~QueueBuffer()
	{
		free(mBuffer);
	}

	// Add silence
	void Add(int32_t len)
	{
		assert(mPos < INT_MAX - len);

		if (!len)
			return;

		if (mPos > INT_MAX - len)
			throw new std::exception("Too much data");

		if (mPos + len > mLen)
		{
			mLen = mPos + (1<<20);
			mBuffer = (T*)realloc(mBuffer, sizeof(T) * mLen);
		}

		memset(mBuffer + mPos, 0, sizeof(T) * len);
		mPos += len;
	}

	// Add actual data
	void Add(T *ptr, int32_t len)
	{
		assert(ptr);
		assert(mPos < INT_MAX - len);

		if(!len)
			return;

		if(mPos > INT_MAX - len)
			throw new std::exception("Too much data");

		if(mPos + len > mLen)
		{
			mLen = mPos + (1<<20);
			mBuffer = (T*)realloc(mBuffer, sizeof(T) * mLen);
		}

		memcpy(mBuffer + mPos, ptr, sizeof(T) * len);
		mPos += len;
	}

	T* Ptr()
	{
		return mBuffer;// +mPosPtr;
	}

	void Remove(int32_t len)
	{
		//mPosPtr += len;
		//return;
		// Keep old buffer size, should have less memory fragmentation,
		// but resize if it is larger than 1MB.
		bool bRealloc = false;
		if(len >= mLen)
		{
			mPos = 0;
			if(mLen * sizeof(T) > (1<<20))
			{
				mLen = (1 << 20) / sizeof(T);
				bRealloc = true;
			}
		}
		else if(len > 0)
		{
			if(mLen * sizeof(T) > (1<<20))
				bRealloc = true;
			mPos = MAX(mPos - len, 0);
			mLen -= len;
			//mLen = MAX(mLen - len, 1<<20);
			if (mLen * sizeof(T) < (1 << 20) && bRealloc)
				bRealloc = true;
			else
				bRealloc = false;
			memmove(mBuffer, mBuffer + len, mLen * sizeof(T));
		}

		if (bRealloc)
		{
			mLen = 1 << 20;
			mBuffer = (T*)realloc(mBuffer, sizeof(T) * mLen);
		}
	}

	uint32_t Capacity(){ return mLen; }
	uint32_t Size(){ return mPos - mPosPtr; }
	void Reset(){ Remove(mLen); }


private:
	T *mBuffer;
	int32_t mPos;
	int32_t mLen;
	int32_t mPosPtr;

};

class MMAudioSource : public AudioSource
{
public:
	MMAudioSource(AudioDeviceInfo &dev)
	: mDevInfo(dev)
	, mmCapture(NULL)
	, mmClient(NULL)
	, mmDevice(NULL)
	, mmClock(NULL)
	, mmEnumerator(NULL)
	, mResampler(NULL)
	, mDeviceLost(true)
	, mResample(false)
	, mFirstSamples(true)
	, mOutputSamplesPerSec(48000)
	{
	}

	~MMAudioSource()
	{
		FreeData();
		SafeRelease(mmEnumerator);
		mResampler = src_delete(mResampler);
		if (file)
			fclose(file);
		file = nullptr;
	}

	void FreeData()
	{
		SafeRelease(mmCapture);
		SafeRelease(mmClient);
		SafeRelease(mmDevice);
		SafeRelease(mmClock);
		//clear mBuffer
	}

	bool Init()
	{
		const IID IID_IMMDeviceEnumerator    = __uuidof(IMMDeviceEnumerator);
		const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);

		HRESULT err = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&mmEnumerator);
		if(FAILED(err))
		{
			SysMessage(TEXT("MMAudioSource::Init(): Could not create IMMDeviceEnumerator = %08lX\n"), err);
			return false;
		}

		return Reinitialize();
	}

	bool Reinitialize()
	{
		const IID IID_IAudioClient           = __uuidof(IAudioClient);
		const IID IID_IAudioCaptureClient    = __uuidof(IAudioCaptureClient);
		HRESULT err;

		if(!mDeviceLost && mmClock)
			return true;
		else
		{
			if (GetQPCTimeMS() - lastTimeMS < 1000)
				return false;
			lastTimeMS = GetQPCTimeMS();
		}

		err = mmEnumerator->GetDevice(mDevInfo.strID.c_str(), &mmDevice);

		if(FAILED(err))
		{
			if (!mDeviceLost) 
				SysMessage(TEXT("MMAudioSource::Reinitialize(): Could not create IMMDevice = %08lX\n"), err);
			return false;
		}

		err = mmDevice->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&mmClient);
		if(FAILED(err))
		{
			if (!mDeviceLost) 
				SysMessage(TEXT("MMAudioSource::Reinitialize(): Could not create IAudioClient = %08lX\n"), err);
			return false;
		}

		// get name

		IPropertyStore *store;
		if(SUCCEEDED(mmDevice->OpenPropertyStore(STGM_READ, &store)))
		{
			PROPVARIANT varName;

			PropVariantInit(&varName);
			if(SUCCEEDED(store->GetValue(PKEY_Device_FriendlyName, &varName)))
			{
				const WCHAR* wstrName = varName.pwszVal;
				mDeviceName = wstrName;
			}

			store->Release();
		}

		// get format

		WAVEFORMATEX *pwfx;
		err = mmClient->GetMixFormat(&pwfx);
		if(FAILED(err))
		{
			if (!mDeviceLost)
				SysMessage(TEXT("MMAudioSource::Reinitialize(): Could not get mix format from audio client = %08lX\n"), err);
			return false;
		}

		WAVEFORMATEXTENSIBLE *wfext = NULL;

		if(pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
		{
			wfext = (WAVEFORMATEXTENSIBLE*)pwfx;
			mInputChannelMask = wfext->dwChannelMask;

			if(wfext->SubFormat != KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
			{
				if (!mDeviceLost)
					SysMessage(TEXT("MMAudioSource::Reinitialize(): Unsupported wave format\n"));
				CoTaskMemFree(pwfx);
				return false;
			}
		}
		else if(pwfx->wFormatTag != WAVE_FORMAT_IEEE_FLOAT)
		{
			if (!mDeviceLost)
				SysMessage(TEXT("MMAudioSource::Reinitialize(): Unsupported wave format\n"));
			CoTaskMemFree(pwfx);
			return false;
		}

		mFloat                 = true;
		mInputChannels         = pwfx->nChannels;
		mInputBitsPerSample    = 32;
		mInputBlockSize        = pwfx->nBlockAlign;
		mInputSamplesPerSec    = pwfx->nSamplesPerSec;
		//sampleWindowSize      = (inputSamplesPerSec/100);

		DWORD flags = 0;//useInputDevice ? 0 : AUDCLNT_STREAMFLAGS_LOOPBACK;

		//Random limit of 1ms to 1 seconds
		if(conf.MicBuffering == 0)
			conf.MicBuffering = 50;
		conf.MicBuffering = MIN(MAX(conf.MicBuffering, 1), 1000);
		OSDebugOut(TEXT("Mic buffering: %d\n"), conf.MicBuffering);

		err = mmClient->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, ConvertMSTo100NanoSec(conf.MicBuffering), 0, pwfx, NULL);
		//err = AUDCLNT_E_UNSUPPORTED_FORMAT;

		if(FAILED(err))
		{
			if (!mDeviceLost)
				SysMessage(TEXT("MMAudioSource::Reinitialize(): Could not initialize audio client, result = %08lX\n"), err);
			CoTaskMemFree(pwfx);
			return false;
		}

		// acquire services

		err = mmClient->GetService(IID_IAudioCaptureClient, (void**)&mmCapture);
		if(FAILED(err))
		{
			if (!mDeviceLost)
				SysMessage(TEXT("MMAudioSource::Reinitialize(): Could not get audio capture client, result = %08lX\n"), err);
			CoTaskMemFree(pwfx);
			return false;
		}

		err = mmClient->GetService(__uuidof(IAudioClock), (void**)&mmClock);
		if(FAILED(err))
		{
			if (!mDeviceLost)
				SysMessage(TEXT("MMAudioSource::Reinitialize(): Could not get audio capture clock, result = %08lX\n"), err);
			CoTaskMemFree(pwfx);
			return false;
		}

		CoTaskMemFree(pwfx);

		// Setup resampler
		int converterType = SRC_SINC_FASTEST;
		int errVal = 0;

		if (mResampler)
			mResampler = src_delete(mResampler);

		mResampler = src_new(converterType, mInputChannels, &errVal);
		if(!mResampler)
		{
			OSDebugOut(TEXT("Failed to create resampler: error %08lX\n"), errVal);
#ifndef _DEBUG
			SysMessage(TEXT("USBqemu: Failed to create resampler: error %08lX"), errVal);
#endif
			return false;
		}

		if (mDeviceLost && !mFirstSamples) //TODO really lost and just first run. Call Start() from ctor always anyway?
			this->Start();

		mDeviceLost = false;

		return true;
	}

	void Start()
	{
		mQBuffer.Reset();
		if(mmClient)
			mmClient->Start();
	}

	void Stop()
	{
		if(mmClient)
			mmClient->Stop();
		mQBuffer.Reset();
	}

	virtual bool GetFrames(uint32_t *size)
	{
		UINT32 pkSize = 0;
		if (!Reinitialize())
			return false;

		HRESULT hRes = mmCapture->GetNextPacketSize(&pkSize);

		if (FAILED(hRes)) {
			if (hRes == AUDCLNT_E_DEVICE_INVALIDATED) {
				FreeData();
				mDeviceLost = true;
				OSDebugOut(TEXT("Audio device has been lost, attempting to reinitialize\n"));
				return false;
			}
			return false;
		}

		pkSize += mQBuffer.Size() / mInputChannels;

		if(mResample)
			pkSize = UINT32(double(pkSize) * mResampleRatio);
		
		*size = pkSize * mInputChannels;

		return true;
	}

	virtual uint32_t GetBuffer(int16_t *outBuf, uint32_t outFrames)
	{
		if (mFirstSamples) //TODO clean out initially buffered samples, unnecessery?
		{
			while (GetMMBuffer()); //TODO Is MSVC gonna 'optimize' this?
			mQBuffer.Remove(mQBuffer.Size() - uint32_t(outFrames / mResampleRatio));
			mFirstSamples = false;
		}
		//Argh, makes shit play fast, because missing samples duh
		//else if (mQBuffer.Size() < (conf.MicBuffering * (mInputSamplesPerSec/1000.0) * mInputChannels)) //outLen) // because forever increasing buffer otherwise
			GetMMBuffer();

		if (!mQBuffer.Size())
			return 0;

#if _DEBUG
		if (!file)
		{
			char name[1024] = { 0 };
			sprintf_s(name, "audiosrc_output_%dch_%dHz.raw", mInputChannels, mOutputSamplesPerSec);
			fopen_s(&file, name, "wb");
		}
#endif
		static LONGLONG time = 0;
		static int samples = 0;
		static double timeAdjust = 1.0;

		samples += outFrames;
		time = GetQPCTime100NS();
		if (lastTimeMS == 0) lastTimeMS = time;
		if (time - lastTimeMS >= 1e7)
		{
			LONGLONG diff = time - lastTimeMS;
			OSDebugOut(TEXT("timespan: %" PRId64 " sampling: %f\n"), diff, float(samples) / diff * 1e7);
			//timeAdjust = diff / 1e7;
			timeAdjust = (samples / (diff / 1e7)) / mOutputSamplesPerSec;
			lastTimeMS = time;
			samples = 0;
		}

		//TODO separate thread
		if(mResample)
		{
			std::vector<float> rebuf(outFrames + 1); //TODO check std::vector elements' alignment

			SRC_DATA data;
			memset(&data, 0, sizeof(SRC_DATA));
			data.data_in = mQBuffer.Ptr();
			data.input_frames = mQBuffer.Size() / mInputChannels;
			data.data_out = &rebuf[0];
			data.output_frames = outFrames / mInputChannels;
			data.src_ratio = mResampleRatio * timeAdjust;

			src_process(mResampler, &data);

			uint32_t len = data.output_frames_gen * mInputChannels;
			src_float_to_short_array(&rebuf[0], (short*)outBuf, len);

			OSDebugOut(TEXT("Resampler: in %d out %d used %d gen %d\n"),
				data.input_frames, data.output_frames,
				data.input_frames_used, data.output_frames_gen);

#if _DEBUG
			if (file)
				fwrite(outBuf, sizeof(short), len, file);
#endif

			mQBuffer.Remove(data.input_frames_used * mInputChannels);
			OSDebugOut(TEXT("Resampled Queue size: %d  Outlen: %d\n"), mQBuffer.Size(), len);
			return len;
		}

		uint32_t totalLen = MIN(outFrames, mQBuffer.Size());
		src_float_to_short_array(mQBuffer.Ptr(), (short*)outBuf, totalLen);

#if _DEBUG
		if (file)
			fwrite(outBuf, sizeof(short), totalLen, file);
#endif

		mQBuffer.Remove(totalLen);
		OSDebugOut(TEXT("Queue plain: %d OutLen: %d\n"), mQBuffer.Size(), totalLen);
		return totalLen;
	}

	/*
		Returns read frame count.
	*/
	uint32_t GetMMBuffer()
	{
		UINT64 devPosition, qpcTimestamp;
		LPBYTE captureBuffer;
		UINT32 numFramesRead;
		DWORD dwFlags = 0;

		if(mDeviceLost)
		{
			if(Reinitialize())
			{
				OSDebugOut(TEXT("Device reacquired.\n"));
				Start();
			}
		}

		UINT32 captureSize = 0;
		HRESULT hRes = mmCapture->GetNextPacketSize(&captureSize);

		if (FAILED(hRes)) {
			if (hRes == AUDCLNT_E_DEVICE_INVALIDATED) {
				FreeData();
				mDeviceLost = true;
				OSDebugOut(TEXT("Audio device has been lost, attempting to reinitialize\n"));
			}
			return 0;
		}

		if (!captureSize)
			return 0;

		if (SUCCEEDED(mmCapture->GetBuffer(&captureBuffer, &numFramesRead, &dwFlags, &devPosition, &qpcTimestamp)))
		{
			UINT totalLen = numFramesRead * mInputChannels;
			if (dwFlags == AUDCLNT_BUFFERFLAGS_SILENT)
				mQBuffer.Add(totalLen);
			else
				mQBuffer.Add((float*)captureBuffer, totalLen);

			mmCapture->ReleaseBuffer(numFramesRead);
		}

		return numFramesRead;
	}

	virtual void SetResampling(int samplerate)
	{
		if(mInputSamplesPerSec == samplerate)
		{
			mResample = false;
			return;
		}
		mOutputSamplesPerSec = samplerate;
		mResampleRatio = double(samplerate) / double(mInputSamplesPerSec);
		mResample = true;
	}

	virtual uint32_t GetChannels()
	{
		return mInputChannels;
	}

private:
	IMMDeviceEnumerator *mmEnumerator;

	IMMDevice           *mmDevice;
	IAudioClient        *mmClient;
	IAudioCaptureClient *mmCapture;
	IAudioClock         *mmClock;

	bool  mResample;
	bool  mFloat;
	bool  mFirstSamples; //On the first call, empty the buffer to lower latency
	UINT  mInputChannels;
	UINT  mInputSamplesPerSec;
	UINT  mOutputSamplesPerSec;
	UINT  mInputBitsPerSample;
	UINT  mInputBlockSize;
	DWORD mInputChannelMask;

	AudioDeviceInfo mDevInfo;
	bool mDeviceLost;
	std::wstring mDeviceName;

	SRC_STATE *mResampler;
	double mResampleRatio;

	QueueBuffer<float> mQBuffer;
};

AudioSource *CreateNewAudioSource(AudioDeviceInfo &dev)
{
	MMAudioSource *src = new MMAudioSource(dev);
	if(src->Init())
		return src;

	delete src;
	return NULL;
}
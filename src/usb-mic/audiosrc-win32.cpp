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

bool AudioInit()
{
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
	, mLen(1024)
	{
		mBuffer = (T*)malloc(sizeof(T) * mLen);
	}

	~QueueBuffer()
	{
		free(mBuffer);
	}

	// Add silence
	void Add(uint32_t len)
	{
		assert(mPos < INT_MAX - len);

		if (!len)
			return;

		if (mPos > INT_MAX - len)
			throw new std::exception("Too much data");

		if (mPos + len > mLen)
		{
			mLen = mPos + len;
			mBuffer = (T*)realloc(mBuffer, sizeof(T) * mLen);
		}

		memset(mBuffer + mPos, 0, sizeof(T) * len);
		mPos += len;
	}

	// Add actual data
	void Add(T *ptr, uint32_t len)
	{
		assert(ptr);
		assert(mPos < INT_MAX - len);

		if(!len)
			return;

		if(mPos > INT_MAX - len)
			throw new std::exception("Too much data");

		if(mPos + len > mLen)
		{
			mLen = mPos + len;
			mBuffer = (T*)realloc(mBuffer, sizeof(T) * mLen);
		}

		memcpy(mBuffer + mPos, ptr, sizeof(T) * len);
		mPos += len;
	}

	T* Ptr()
	{
		return mBuffer;
	}

	void Remove(uint32_t len)
	{
		// Keep old buffer size, should have less memory fragmentation,
		// but resize if it is larger than 1MB.
		bool bRealloc = false;
		if(len >= mLen)
		{
			mPos = 0;
			if(mLen * sizeof(T) > (1<<20))
			{
				mLen = 1024;
				bRealloc = true;
			}
		}
		else if(len > 0)
		{
			if(mLen * sizeof(T) > (1<<20))
				bRealloc = true;
			mPos -= len;
			mLen -= len;
			memmove(mBuffer, mBuffer + len, mLen * sizeof(T));
		}

		if(bRealloc)
			mBuffer = (T*)realloc(mBuffer, sizeof(T) * mLen);
	}

	uint32_t Capacity(){ return mLen; }
	uint32_t Size(){ return mPos; }
	void Reset(){ Remove(mLen); }


private:
	T *mBuffer;
	uint32_t mPos;
	uint32_t mLen;
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
	{
	}

	~MMAudioSource()
	{
		FreeData();
		SafeRelease(mmEnumerator);
		mResampler = src_delete(mResampler);
		if (!file)
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
			OSDebugOut(TEXT("MMDeviceAudioSource::Init(): Could not create IMMDeviceEnumerator = %08lX\n"), err);
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

		err = mmEnumerator->GetDevice(mDevInfo.strID.c_str(), &mmDevice);

		if(FAILED(err))
		{
			if (!mDeviceLost) 
				OSDebugOut(TEXT("MMDeviceAudioSource::Reinitialize(): Could not create IMMDevice = %08lX\n"), err);
			return false;
		}

		err = mmDevice->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&mmClient);
		if(FAILED(err))
		{
			if (!mDeviceLost) 
				OSDebugOut(TEXT("MMDeviceAudioSource::Reinitialize(): Could not create IAudioClient = %08lX\n"), err);
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
				OSDebugOut(TEXT("MMDeviceAudioSource::Reinitialize(): Could not get mix format from audio client = %08lX\n"), err);
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
					OSDebugOut(TEXT("MMDeviceAudioSource::Reinitialize(): Unsupported wave format\n"));
				CoTaskMemFree(pwfx);
				return false;
			}
		}
		else if(pwfx->wFormatTag != WAVE_FORMAT_IEEE_FLOAT)
		{
			if (!mDeviceLost)
				OSDebugOut(TEXT("MMDeviceAudioSource::Reinitialize(): Unsupported wave format\n"));
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
		UINT buffering = MIN(MAX(conf.MicBuffering, 1), 1000);
		OSDebugOut(TEXT("Mic buffering: %d\n"), buffering);

		err = mmClient->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, ConvertMSTo100NanoSec(buffering), 0, pwfx, NULL);
		//err = AUDCLNT_E_UNSUPPORTED_FORMAT;

		if(FAILED(err))
		{
			if (!mDeviceLost)
				OSDebugOut(TEXT("MMDeviceAudioSource::Reinitialize(): Could not initialize audio client, result = %08lX\n"), err);
			CoTaskMemFree(pwfx);
			return false;
		}

		// acquire services

		err = mmClient->GetService(IID_IAudioCaptureClient, (void**)&mmCapture);
		if(FAILED(err))
		{
			if (!mDeviceLost) OSDebugOut(TEXT("MMDeviceAudioSource::Reinitialize(): Could not get audio capture client, result = %08lX\n"), err);
			CoTaskMemFree(pwfx);
			return false;
		}

		err = mmClient->GetService(__uuidof(IAudioClock), (void**)&mmClock);
		if(FAILED(err))
		{
			if (!mDeviceLost) OSDebugOut(TEXT("MMDeviceAudioSource::Reinitialize(): Could not get audio capture clock, result = %08lX\n"), err);
			CoTaskMemFree(pwfx);
			return false;
		}

		CoTaskMemFree(pwfx);

		// Setup resampler
		int converterType = SRC_SINC_FASTEST;
		int errVal = 0;
		mResampler = src_new(converterType, mInputChannels, &errVal);
		if(!mResampler)
		{
			OSDebugOut(TEXT("Failed to create resampler: error %08lX\n"), errVal);
			return false;
		}

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
		Reinitialize();
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

		pkSize = MAX(pkSize, mQBuffer.Size() / mInputChannels);

		if(mResample)
			pkSize = UINT32((double(pkSize) * mResampleRatio) + 1.0);
		
		*size = pkSize * mInputChannels;// * sizeof(uint16_t);

		return true;
	}

	//TODO @param outLen : bytes to read (samples * channels), turn into samples to read?
	virtual uint32_t GetBuffer(int16_t *outBuf, uint32_t outLen)
	{
		if (mFirstSamples) //TODO clean out initially buffered samples, unnecessery?
		{
			while (GetMMBuffer()); //TODO Is MSVC gonna 'optimize' this?
			mQBuffer.Remove(mQBuffer.Size() - outLen);
			mFirstSamples = false;
		}
		else if (mQBuffer.Size() < outLen) // because forever increasing buffer otherwise
			GetMMBuffer();

		if(mResample)
		{
			std::vector<float> rebuf(outLen + 1); //TODO check std::vector elements' alignment

			SRC_DATA data;
			memset(&data, 0, sizeof(SRC_DATA));
			data.data_in = mQBuffer.Ptr();
			data.input_frames = mQBuffer.Size() / mInputChannels;
			data.data_out = &rebuf[0];
			data.output_frames = outLen / mInputChannels;
			data.src_ratio = mResampleRatio;

			src_process(mResampler, &data);

			uint32_t len = data.output_frames_gen * mInputChannels;
			src_float_to_short_array(&rebuf[0], (short*)outBuf, len);

			OSDebugOut(TEXT("Resampler: in %d out %d used %d gen %d\n"),
				data.input_frames, data.output_frames,
				data.input_frames_used, data.output_frames_gen);

			
			if (!file)
			{
				file = fopen("output.raw", "wb");
			}
			else
				fwrite(outBuf, sizeof(short), len, file);

			mQBuffer.Remove(data.input_frames_used * mInputChannels);
			OSDebugOut(TEXT("Queue resampled: %d  Outlen: %d\n"), mQBuffer.Size(), len);
			return len;
		}

		uint32_t totalLen = MIN(outLen, mQBuffer.Size());
		src_float_to_short_array(mQBuffer.Ptr(), (short*)outBuf, totalLen);
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
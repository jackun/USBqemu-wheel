// Used OBS as example

#include "../USB.h"
#include "mic-audiodefs.h"
#include "../libsamplerate/samplerate.h"

#include <Mmdeviceapi.h>
#include <Audioclient.h>
#include <propsys.h>
#include <Functiondiscoverykeys_devpkey.h>

#define SafeRelease(x) if(x){x->Release(); x = NULL;}
inline UINT ConvertMSTo100NanoSec(UINT ms)
{
	return ms*1000*10; //1000 microseconds, then 10 "100nanosecond" segments
}

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
	{
	}

	~MMAudioSource()
	{
		FreeData();
		SafeRelease(mmEnumerator);
		mResampler = src_delete(mResampler);
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

		err = mmClient->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, ConvertMSTo100NanoSec(50), 0, pwfx, NULL);
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
		if(mmClient)
			mmClient->Start();
	}

	void Stop()
	{
		if(mmClient)
			mmClient->Stop();
	}

	virtual bool GetBufferSize(uint32_t *size)
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

		if(mResample)
			pkSize = UINT32((double(pkSize) * mResampleRatio) + 1.0);
		
		*size = pkSize;// * mInputChannels;// * sizeof(uint16_t);

		return true;
	}

	//TODO Overall todo and how accurate the 'stream' continuation needs to be for singstar games?
	virtual uint32_t GetBuffer(uint16_t *outBuf, uint32_t outLen)
	{
		UINT64 devPosition, qpcTimestamp;
		LPBYTE captureBuffer;
		UINT32 numFramesRead;
		DWORD dwFlags = 0;
		std::vector<float> buf;

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
				return false;
			}
			return false;
		}

		if (!captureSize)
			return false;

		hRes = mmCapture->GetBuffer(&captureBuffer, &numFramesRead, &dwFlags, &devPosition, &qpcTimestamp);
		UINT totalLen = numFramesRead;// * mInputChannels;

		//TODO buffer management
		if(mResample)
		{
			buf.resize(totalLen);
			memcpy(&buf[0], captureBuffer, totalLen * sizeof(float));
		}
		else
		{
			totalLen = MIN(totalLen, outLen);
			src_float_to_short_array(&buf[0], (short*)outBuf, totalLen);
		}

		mmCapture->ReleaseBuffer(numFramesRead);

		if(mResample)
		{
			UINT newBufFrames = UINT((double(numFramesRead) * mResampleRatio) + 1.0);
			std::vector<float> rebuf(newBufFrames/* * mInputChannels*/);

			SRC_DATA data;
			memset(&data, 0, sizeof(SRC_DATA));
			data.data_in = &buf[0];
			data.input_frames = numFramesRead;
			data.data_out = &rebuf[0];
			data.output_frames = newBufFrames;
			data.src_ratio = mResampleRatio;

			src_process(mResampler, &data);

			int len = MIN(outLen / sizeof(uint16_t), data.output_frames_gen/* * mInputChannels*/);
			src_float_to_short_array(&rebuf[0], (short*)outBuf, len);

			return len;
			//TODO: check data.input_frames_used and data.output_frames_gen
		}

		return totalLen;
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

	std::queue<float> mBuffer;
};

AudioSource *CreateNewAudioSource(AudioDeviceInfo &dev)
{
	MMAudioSource *src = new MMAudioSource(dev);
	if(src->Init())
		return src;

	delete src;
	return NULL;
}
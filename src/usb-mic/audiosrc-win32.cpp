// Used OBS as example

#include "../USB.h"
//#include "audiosrc.h"
#include "audiosourceproxy.h"
#include "../libsamplerate/samplerate.h"
#include "../Win32/Config-win32.h"

#include <assert.h>
#include <Mmdeviceapi.h>
#include <Audioclient.h>
#include <propsys.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <typeinfo>

#define APINAME "wasapi"
#define APINAMEW TEXT(APINAME)

#define SafeRelease(x) if(x){x->Release(); x = NULL;}
#define ConvertMSTo100NanoSec(ms) (ms*1000*10) //1000 microseconds, then 10 "100nanosecond" segments

static FILE* file = nullptr;
static AudioDeviceInfoList audioDevs;
//Config dlg temporaries
static std::wstring micDev[2];
static std::string micApi[2];
static BOOL CALLBACK MicDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam);

LARGE_INTEGER clockFreq = { 0 };
__declspec(thread) LONGLONG lastQPCTime = 0;
LONGLONG lastTimeMS = 0;
LONGLONG lastTimeNS = 0;
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
			assert(mPos - len >= 0);
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
	MMAudioSource(int port, int mic)
	: mPort(port)
	, mMic(mic)
	, mmCapture(NULL)
	, mmClient(NULL)
	, mmDevice(NULL)
	, mmClock(NULL)
	, mmEnumerator(NULL)
	, mResampler(NULL)
	, mDeviceLost(false)
	, mResample(false)
	, mFirstSamples(true)
	, mOutputSamplesPerSec(48000)
	, mResampleRatio(1.0)
	, mTimeAdjust(1.0)
	, mThread(INVALID_HANDLE_VALUE)
	, mEvent(INVALID_HANDLE_VALUE)
	, mQuit(false)
	, mPaused(true)
	, mLastGetBufferMS(0)
	, mBuffering(50)
	{
		mEvent = CreateEvent(NULL, FALSE, FALSE, TEXT("ResamplerThread"));
		mMutex = CreateMutex(NULL, FALSE, TEXT("ResampledQueueMutex"));
		if(!Init())
			throw AudioSourceError("MMAudioSource:: WASAPI init failed!");
	}

	~MMAudioSource()
	{
		mQuit = true;
		if (mThread != INVALID_HANDLE_VALUE)
		{
			SetEvent(mEvent);
			if (WaitForSingleObject(mThread, 30000) != WAIT_OBJECT_0)
				TerminateThread(mThread, 0);
		}

		FreeData();
		SafeRelease(mmEnumerator);
		mResampler = src_delete(mResampler);
		if (file)
			fclose(file);
		file = nullptr;

		CloseHandle(mThread);
		mThread = INVALID_HANDLE_VALUE;
		CloseHandle(mEvent);
		mEvent = INVALID_HANDLE_VALUE;
		CloseHandle(mMutex);
		mMutex = INVALID_HANDLE_VALUE;
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

		{
			CONFIGVARIANT var(mMic ? N_AUDIO_DEVICE1 : N_AUDIO_DEVICE1, CONFIG_TYPE_WCHAR);
			if (!LoadSetting(mPort, APINAME, var))
			{
				return false;
			}
			mDevID = var.wstrValue;
		}

		{
			CONFIGVARIANT var(N_BUFFER_LEN, CONFIG_TYPE_INT);
			if (LoadSetting(mPort, APINAME, var))
				mBuffering = var.intValue;
		}


		HRESULT err = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&mmEnumerator);
		if(FAILED(err))
		{
			SysMessage(TEXT("MMAudioSource::Init(): Could not create IMMDeviceEnumerator = %08lX\n"), err);
			return false;
		}
		//TODO Not starting thread here unnecesserily
		//mThread = CreateThread(NULL, 0, MMAudioSource::Thread, this, 0, 0);
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

		err = mmEnumerator->GetDevice(mDevID.c_str(), &mmDevice);

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

		/*IPropertyStore *store;
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
		}*/

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
		if(mBuffering == 0)
			mBuffering = 50;
		mBuffering = MIN(MAX(mBuffering, 1), 1000);
		OSDebugOut(TEXT("Mic buffering: %d\n"), mBuffering);

		err = mmClient->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, ConvertMSTo100NanoSec(mBuffering), 0, pwfx, NULL);
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

		//random, reserve enough memory for 5 seconds
		mQBuffer.reserve(mInputChannels * mInputSamplesPerSec * 5);
		mResampledBuffer.reserve(mInputChannels * mOutputSamplesPerSec * 5);

		if (mDeviceLost && !mFirstSamples) //TODO really lost and just first run. Call Start() from ctor always anyway?
			this->Start();

		mDeviceLost = false;

		return true;
	}

	void Start()
	{
		ResetBuffers();
		src_reset(mResampler);
		if(mmClient)
			mmClient->Start();
		mPaused = false;
	}

	void Stop()
	{
		mPaused = true;
		if(mmClient)
			mmClient->Stop();
	}

	void ResetBuffers()
	{
		WaitForSingleObject(mMutex, INFINITE);
		mQBuffer.resize(0);
		mResampledBuffer.resize(0);
		ReleaseMutex(mMutex);
	}

	//TODO or just return samples count in mResampledBuffer?
	virtual bool GetFrames(uint32_t *size)
	{
		UINT32 pkSize = 0;
		if (mDeviceLost)
			return false;

		HRESULT hRes = mmCapture->GetNextPacketSize(&pkSize);

		if (FAILED(hRes)) {
			//TODO Threading; can't release mmCapture cause thread calls GetMMBuffer()
			if (hRes == AUDCLNT_E_DEVICE_INVALIDATED) {
				mDeviceLost = true;
				//FreeData();
				OSDebugOut(TEXT("Audio device has been lost, attempting to reinitialize\n"));
				return false;
			}
			return false;
		}

		if (mResample)
			pkSize = UINT32(double(pkSize) * mResampleRatio);

		//TODO
		WaitForSingleObject(mMutex, INFINITE);
		pkSize += mResampledBuffer.size() / mInputChannels;
		ReleaseMutex(mMutex);

		*size = pkSize;
		return true;
	}

	static DWORD WINAPI Thread(LPVOID ptr)
	{
		MMAudioSource *src = (MMAudioSource*)ptr;
		std::vector<float> rebuf;
		int ret = 1;
		bool bThreadComInitialized = false;

		//TODO APARTMENTTHREADED instead?
		HRESULT hr = CoInitializeEx( NULL, COINIT_MULTITHREADED );
		if ((S_OK != hr) && (S_FALSE != hr) /* already inited */ && (hr != RPC_E_CHANGED_MODE))
		{
			OSDebugOut(TEXT("Com initialization failed with %d\n"), hr);
			return 1;
		}

		if (hr != RPC_E_CHANGED_MODE)
			bThreadComInitialized = true;

		if (WaitForSingleObject(src->mEvent, INFINITE) != WAIT_OBJECT_0)
		{
			OSDebugOut(TEXT("Failed to for event: %08X\n"), GetLastError());
			goto error;
		}

#if _DEBUG
		if (!file)
		{
			char name[1024] = { 0 };
			sprintf_s(name, "audiosrc_output_%dch_%dHz.raw", src->mInputChannels, src->mOutputSamplesPerSec);
			fopen_s(&file, name, "wb");
		}
#endif
		//Call mmClient->Start() here instead?
		src->ResetBuffers(); //reset, maybe thread died previously
		src->mLastGetBufferMS = GetQPCTimeMS(); //fall through first time check

		while (!src->mQuit)
		{
			//Too long since last GetBuffer(), USB subsystem is not initialized properly probably
			if (!src->mDeviceLost && (GetQPCTimeMS() - src->mLastGetBufferMS > 5000))
			{
				ret = 2;
				goto quit;
			}

			while (src->mPaused)
			{
				Sleep(100);
				if (src->mQuit)
					goto quit;
			}

			src->GetMMBuffer();
			if (src->mQBuffer.size())
			{
				size_t resampled = static_cast<size_t>(src->mQBuffer.size() * src->mResampleRatio * src->mTimeAdjust) * src->mInputChannels;
				if (resampled == 0)
					resampled = src->mQBuffer.size();
				rebuf.resize(resampled);

				OSDebugOut(TEXT("--------------------------\n"));
				OSDebugOut(TEXT("Resampled size: %zd\n"), resampled);

				SRC_DATA data;
				memset(&data, 0, sizeof(SRC_DATA));
				data.data_in = &src->mQBuffer[0];
				data.input_frames = src->mQBuffer.size() / src->mInputChannels;
				data.data_out = &rebuf[0];
				data.output_frames = resampled / src->mInputChannels;
				data.src_ratio = src->mResampleRatio * src->mTimeAdjust;

				src_process(src->mResampler, &data);

				DWORD resMutex = WaitForSingleObject(src->mMutex, INFINITE);
				if (resMutex != WAIT_OBJECT_0)
				{
					OSDebugOut(TEXT("Mutex wait failed: %d\n"), resMutex);
					goto error;
				}

				uint32_t len = data.output_frames_gen * src->mInputChannels;
				size_t size = src->mResampledBuffer.size();
				if (len > 0)
				{
					//too long, drop samples, caused by saving/loading savestates and random stutters
					int sizeInMS = (((src->mResampledBuffer.size() + len) * 1000 / src->mInputChannels) / src->mOutputSamplesPerSec);
					int threshold = src->mBuffering > 25 ? src->mBuffering : 25;
					if (sizeInMS > threshold)
					{
						size = 0;
						src->mResampledBuffer.resize(len);
					}
					else
						src->mResampledBuffer.resize(size + len);
					src_float_to_short_array(&rebuf[0], &(src->mResampledBuffer[size]), len);
				}

				OSDebugOut(TEXT("Resampler: in %d out %d used %d gen %d, rb: %zd\n"),
					data.input_frames, data.output_frames,
					data.input_frames_used, data.output_frames_gen, src->mResampledBuffer.size());

#if _DEBUG
				if (file && len)
					fwrite(&(src->mResampledBuffer[size]), sizeof(short), len, file);
#endif

				uint32_t sizeBefore = src->mQBuffer.size();
				auto remSize = data.input_frames_used * src->mInputChannels;
				src->mQBuffer.erase(src->mQBuffer.begin(), src->mQBuffer.begin() + remSize);

				OSDebugOut(TEXT("Sample Queue size: %zd - %d -> %zd\n"),
					sizeBefore, remSize, src->mQBuffer.size());

				if (!ReleaseMutex(src->mMutex))
				{
					OSDebugOut(TEXT("Mutex release failed\n"));
					goto error;
				}
			}
			Sleep(src->mDeviceLost ? 1000 : 1);
		}

quit:
		ret = 0;
error:
		if (bThreadComInitialized == true)
			CoUninitialize();

		return ret;
	}

	virtual uint32_t GetBuffer(int16_t *outBuf, uint32_t outFrames)
	{
		mLastGetBufferMS = GetQPCTimeMS();
		static LONGLONG time = 0;
		static int samples = 0;

		if(!mQuit && (mThread == INVALID_HANDLE_VALUE ||
				WaitForSingleObject(mThread, 0) == WAIT_OBJECT_0)) //Thread got killed prematurely
			mThread = CreateThread(NULL, 0, MMAudioSource::Thread, this, 0, 0);

		SetEvent(mEvent);

		DWORD resMutex = WaitForSingleObject(mMutex, INFINITE);
		if (resMutex != WAIT_OBJECT_0)
		{
			OSDebugOut(TEXT("Mutex wait failed: %d\n"), resMutex);
			return 0;
		}

		samples += outFrames;
		time = GetQPCTime100NS();
		if (lastTimeNS == 0) lastTimeNS = time;
		LONGLONG diff = time - lastTimeNS;
		if (diff >= LONGLONG(1e7))
		{
			mTimeAdjust = (samples / (diff / 1e7)) / mOutputSamplesPerSec;
			//if(mTimeAdjust > 1.0) mTimeAdjust = 1.0; //If game is in 'turbo mode', just return zero samples or...?
			OSDebugOut(TEXT("timespan: %") TEXT(PRId64) TEXT(" sampling: %f adjust: %f\n"), diff, float(samples) / diff * 1e7, mTimeAdjust);
			lastTimeNS = time;
			samples = 0;
		}

		uint32_t totalLen = MIN(outFrames * mInputChannels, mResampledBuffer.size());
		OSDebugOut(TEXT("Resampled buffer size: %zd, copy: %zd\n"), mResampledBuffer.size(), totalLen);
		if (totalLen > 0)
		{
			memcpy(outBuf, &mResampledBuffer[0], sizeof(short) * totalLen);
			mResampledBuffer.erase(mResampledBuffer.begin(), mResampledBuffer.begin() + totalLen);
		}

		if (!ReleaseMutex(mMutex))
		{
			OSDebugOut(TEXT("Mutex release failed\n"));
		}

		return totalLen / mInputChannels;
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
			FreeData();
			if(Reinitialize())
			{
				OSDebugOut(TEXT("Device reacquired.\n"));
				Start();
			}
			else
			{
				return 0;
			}
		}

		UINT32 captureSize = 0;
		HRESULT hRes = mmCapture->GetNextPacketSize(&captureSize);

		if (FAILED(hRes)) {
			if (hRes == AUDCLNT_E_DEVICE_INVALIDATED) {
				mDeviceLost = true;
				FreeData();
				OSDebugOut(TEXT("Audio device has been lost, attempting to reinitialize\n"));
			}
			return 0;
		}

		if (!captureSize)
			return 0;

		if (SUCCEEDED(mmCapture->GetBuffer(&captureBuffer, &numFramesRead, &dwFlags, &devPosition, &qpcTimestamp)))
		{
			UINT totalLen = numFramesRead * mInputChannels;
			if (dwFlags & AUDCLNT_BUFFERFLAGS_SILENT)
				mQBuffer.resize(mQBuffer.size() + totalLen);
			else
				mQBuffer.assign((float*)captureBuffer, (float*)captureBuffer + totalLen);

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

	virtual MicMode GetMicMode(AudioSource* compare)
	{
		if (compare && typeid(compare) != typeid(this))
			return MIC_MODE_SEPARATE; //atleast, if not single altogether

		if (compare)
		{
			MMAudioSource *src = dynamic_cast<MMAudioSource *>(compare);
			if (mDevID == src->mDevID)
				return MIC_MODE_SHARED;
		}

		CONFIGVARIANT var(mMic ? N_AUDIO_DEVICE0 : N_AUDIO_DEVICE1, CONFIG_TYPE_WCHAR);
		if (LoadSetting(mPort, APINAME, var) && var.wstrValue == mDevID)
			return MIC_MODE_SHARED;

		return MIC_MODE_SEPARATE;
	}

	static const wchar_t* Name()
	{
		return L"WASAPI";
	}

	static bool AudioInit()
	{
		QueryPerformanceFrequency(&clockFreq);
		return true;
	}

	static void AudioDeinit()
	{
	}

	static void AudioDevices(std::vector<AudioDeviceInfo> &devices)
	{
		const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
		const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
		IMMDeviceEnumerator *mmEnumerator;
		HRESULT err;

		err = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&mmEnumerator);
		if (FAILED(err))
		{
			SysMessage(TEXT("AudioDevices: Could not create IMMDeviceEnumerator\n"));
			return;
		}

		IMMDeviceCollection *collection;
		EDataFlow audioDeviceType = eCapture;
		DWORD flags = DEVICE_STATE_ACTIVE;
		//if (!bConnectedOnly)
		flags |= DEVICE_STATE_UNPLUGGED;

		err = mmEnumerator->EnumAudioEndpoints(audioDeviceType, flags, &collection);
		if (FAILED(err))
		{
			SysMessage(TEXT("AudioDevices: Could not enumerate audio endpoints\n"));
			SafeRelease(mmEnumerator);
			return;
		}

		UINT count;
		if (SUCCEEDED(collection->GetCount(&count)))
		{
			for (UINT i = 0; i<count; i++)
			{
				IMMDevice *device;
				if (SUCCEEDED(collection->Item(i, &device)))
				{
					const WCHAR *wstrID;
					if (SUCCEEDED(device->GetId((LPWSTR*)&wstrID)))
					{
						IPropertyStore *store;
						if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &store)))
						{
							PROPVARIANT varName;

							PropVariantInit(&varName);
							if (SUCCEEDED(store->GetValue(PKEY_Device_FriendlyName, &varName)))
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

		SafeRelease(collection);
		SafeRelease(mmEnumerator);
	}

	static int Configure(int port, void *data)
	{
		Win32Handles h = *(Win32Handles*)data;
		return DialogBoxParam(h.hInst,
			MAKEINTRESOURCE(IDD_DLGMIC),
			h.hWnd,
			(DLGPROC)MicDlgProc, port);
	}

	static std::vector<CONFIGVARIANT> GetSettings()
	{
		//TODO GetSettings()
		return std::vector<CONFIGVARIANT>();
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

	std::wstring mDevID;
	bool mDeviceLost;
	std::wstring mDeviceName;
	int mMic;
	int mPort;
	int mBuffering;

	SRC_STATE *mResampler;
	double mResampleRatio;
	// Speed up or slow down audio
	double mTimeAdjust;
	std::vector<short> mResampledBuffer;
	std::vector<float> mQBuffer;
	HANDLE mThread;
	HANDLE mEvent;
	HANDLE mMutex;
	bool mQuit;
	bool mPaused;
	LONGLONG mLastGetBufferMS;

	//QueueBuffer<float> mQBuffer;
};

static void RefreshAudioList(HWND hW, LRESULT idx)
{
	audioDevs.clear();

	SendDlgItemMessage(hW, IDC_COMBOMIC1, CB_RESETCONTENT, 0, 0);

	SendDlgItemMessageW(hW, IDC_COMBOMIC1, CB_ADDSTRING, 0, (LPARAM)L"None");
	SendDlgItemMessageW(hW, IDC_COMBOMIC2, CB_ADDSTRING, 0, (LPARAM)L"None");

	SendDlgItemMessage(hW, IDC_COMBOMIC1, CB_SETCURSEL, 0, 0);
	SendDlgItemMessage(hW, IDC_COMBOMIC2, CB_SETCURSEL, 0, 0);

	MMAudioSource::AudioDevices(audioDevs);
	AudioDeviceInfoList::iterator it;
	int i = 0;
	for (it = audioDevs.begin(); it != audioDevs.end(); it++)
	{
		SendDlgItemMessageW(hW, IDC_COMBOMIC1, CB_ADDSTRING, 0, (LPARAM)it->strName.c_str());
		SendDlgItemMessageW(hW, IDC_COMBOMIC2, CB_ADDSTRING, 0, (LPARAM)it->strName.c_str());

		i++;
		if (it->strID == micDev[0])
			SendDlgItemMessage(hW, IDC_COMBOMIC1, CB_SETCURSEL, i, i);
		if (it->strID == micDev[1])
			SendDlgItemMessage(hW, IDC_COMBOMIC2, CB_SETCURSEL, i, i);
	}
}

static BOOL CALLBACK MicDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	int tmp = 0, port = 0;
	static auto audioProxyMap = RegisterAudioSource::instance().Map();

	switch (uMsg) {
	case WM_CREATE:
		SetWindowLong(hW, GWL_USERDATA, (LONG)lParam);
		break;
	case WM_INITDIALOG:
	{
		port = (int)lParam;
		SetWindowLong(hW, GWL_USERDATA, (LONG)lParam);
		int buffering = 20;
		{
			CONFIGVARIANT var(N_BUFFER_LEN, CONFIG_TYPE_INT);
			if (LoadSetting(port, APINAME, var))
				buffering = var.intValue;
		}
		SendDlgItemMessage(hW, IDC_MICSLIDER, TBM_SETRANGEMIN, TRUE, 1);
		SendDlgItemMessage(hW, IDC_MICSLIDER, TBM_SETRANGEMAX, TRUE, 1000);
		SendDlgItemMessage(hW, IDC_MICSLIDER, TBM_SETPOS, TRUE, buffering);
		SetDlgItemInt(hW, IDC_MICBUF, buffering, FALSE);
		{
			for (int i = 0; i < 2; i++)
			{
				CONFIGVARIANT var0(i ? N_AUDIO_DEVICE1 : N_AUDIO_DEVICE0, CONFIG_TYPE_WCHAR);
				if (LoadSetting(port, APINAME, var0))
					micDev[i] = var0.wstrValue;
			}
		}
		RefreshAudioList(hW, -1);
		return TRUE;
	}
	case WM_HSCROLL:
		if ((HWND)lParam == GetDlgItem(hW, IDC_MICSLIDER))
		{
			int pos = SendDlgItemMessage(hW, IDC_MICSLIDER, TBM_GETPOS, 0, 0);
			SetDlgItemInt(hW, IDC_MICBUF, pos, FALSE);
			break;
		}
		break;

	case WM_COMMAND:
		switch (HIWORD(wParam))
		{
		case EN_CHANGE:
		{
			switch (LOWORD(wParam))
			{
			case IDC_MICBUF:
				CHECKED_SET_MAX_INT(tmp, hW, IDC_MICBUF, FALSE, 1, 1000);
				SendDlgItemMessage(hW, IDC_MICSLIDER, TBM_SETPOS, TRUE, tmp);
				break;
			}
		}
		break;
		case BN_CLICKED:
		{
			switch (LOWORD(wParam)) {
			case IDOK:
			{
				int p1, p2;
				INT_PTR res = RESULT_OK;
				p1 = SendDlgItemMessage(hW, IDC_COMBOMIC1, CB_GETCURSEL, 0, 0);
				p2 = SendDlgItemMessage(hW, IDC_COMBOMIC2, CB_GETCURSEL, 0, 0);

				if (p1 > 0)
					micDev[0] = (audioDevs.begin() + p1 - 1)->strID;
				else
					micDev[0] = L"";
				if (p2 > 0)
					micDev[1] = (audioDevs.begin() + p2 - 1)->strID;
				else
					micDev[1] = L"";

				port = (int)GetWindowLong(hW, GWL_USERDATA);

				for (int i = 0; i < 2; i++)
				{
					CONFIGVARIANT var(i ? N_AUDIO_DEVICE1 : N_AUDIO_DEVICE0, CONFIG_TYPE_WCHAR);
					var.wstrValue = micDev[i];
					if (!SaveSetting(port, APINAME, var))
						res = RESULT_FAILED;
				}

				{
					CONFIGVARIANT var(N_BUFFER_LEN, CONFIG_TYPE_INT);
					var.intValue = SendDlgItemMessage(hW, IDC_MICSLIDER, TBM_GETPOS, 0, 0);
					if (!SaveSetting(port, APINAME, var))
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
		break;
		}
	}
	return FALSE;
}

REGISTER_AUDIOSRC(APINAME, MMAudioSource);
#undef APINAME
#undef APINAMEW

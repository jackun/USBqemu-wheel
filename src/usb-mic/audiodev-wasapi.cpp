// Used OBS as example

#include "../USB.h"
//#include "audiodev.h"
#include "audiodeviceproxy.h"
#include "../libsamplerate/samplerate.h"
#include "../Win32/Config-win32.h"
#include "../Win32/resource.h"

#include <assert.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <propsys.h>
#include <typeinfo>
#include <functiondiscoverykeys_devpkey.h>

#define APINAME "wasapi"
#define APINAMEW TEXT(APINAME)

#define SafeRelease(x) if(x){x->Release(); x = NULL;}
#define ConvertMSTo100NanoSec(ms) (ms*1000*10) //1000 microseconds, then 10 "100nanosecond" segments

namespace audiodev_wasapi {

static FILE* file = nullptr;

//Config dlg temporaries
struct WASAPISettings
{
	int port;
	AudioDeviceInfoList sourceDevs;
	AudioDeviceInfoList sinkDevs;
	std::wstring selectedDev[3];
};

static BOOL CALLBACK WASAPIDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam);

LARGE_INTEGER clockFreq = { 0 };
__declspec(thread) LONGLONG lastQPCTime = 0;

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

class MMAudioDevice : public AudioDevice
{
public:
	MMAudioDevice(int port, int device, AudioDir dir)
	: mPort(port)
	, mDevice(device)
	, mmCapture(NULL)
	, mmRender(NULL)
	, mmClient(NULL)
	, mmDevice(NULL)
	, mmClock(NULL)
	, mmEnumerator(NULL)
	, mResampler(NULL)
	, mDeviceLost(false)
	, mResample(false)
	, mFirstSamples(true)
	, mSamplesPerSec(48000)
	, mResampleRatio(1.0)
	, mTimeAdjust(1.0)
	, mThread(INVALID_HANDLE_VALUE)
	, mEvent(INVALID_HANDLE_VALUE)
	, mQuit(false)
	, mPaused(true)
	, mLastGetBufferMS(0)
	, mBuffering(50)
	, mAudioDir(dir)
	{
		mEvent = CreateEvent(NULL, FALSE, FALSE, TEXT("ResamplerThread"));
		mMutex = CreateMutex(NULL, FALSE, TEXT("ResampledQueueMutex"));
		if(!Init())
			throw AudioDeviceError("MMAudioDevice:: WASAPI init failed!");
	}

	~MMAudioDevice()
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
		SafeRelease(mmRender);
		SafeRelease(mmClient);
		SafeRelease(mmDevice);
		SafeRelease(mmClock);
		//clear mBuffer
	}

	bool Init()
	{
		const IID IID_IMMDeviceEnumerator    = __uuidof(IMMDeviceEnumerator);
		const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);

		if (mAudioDir == AUDIODIR_SOURCE)
		{
			CONFIGVARIANT var(mDevice ? N_AUDIO_SOURCE1 : N_AUDIO_SOURCE0, CONFIG_TYPE_WCHAR);
			if (!LoadSetting(mPort, APINAME, var))
			{
				return false;
			}
			mDevID = var.wstrValue;
		}
		else
		{
			CONFIGVARIANT var(mDevice ? N_AUDIO_SINK1 : N_AUDIO_SINK0, CONFIG_TYPE_WCHAR);
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
			SysMessage(TEXT("MMAudioDevice::Init(): Could not create IMMDeviceEnumerator = %08lX\n"), err);
			return false;
		}
		//TODO Not starting thread here unnecesserily
		//mThread = CreateThread(NULL, 0, MMAudioDevice::CaptureThread, this, 0, 0);
		return Reinitialize();
	}

	bool Reinitialize()
	{
		const IID IID_IAudioClient           = __uuidof(IAudioClient);
		const IID IID_IAudioCaptureClient    = __uuidof(IAudioCaptureClient);
		const IID IID_IAudioRenderClient     = __uuidof(IAudioRenderClient);
		HRESULT err;

		if(!mDeviceLost && mmClock)
			return true;
		else
		{
			if (GetQPCTimeMS() - mLastTimeMS < 1000)
				return false;
			mLastTimeMS = GetQPCTimeMS();
		}

		err = mmEnumerator->GetDevice(mDevID.c_str(), &mmDevice);

		if(FAILED(err))
		{
			if (!mDeviceLost) 
				SysMessage(TEXT("MMAudioDevice::Reinitialize(): Could not create IMMDevice = %08lX\n"), err);
			return false;
		}

		err = mmDevice->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&mmClient);
		if(FAILED(err))
		{
			if (!mDeviceLost) 
				SysMessage(TEXT("MMAudioDevice::Reinitialize(): Could not create IAudioClient = %08lX\n"), err);
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
				SysMessage(TEXT("MMAudioDevice::Reinitialize(): Could not get mix format from audio client = %08lX\n"), err);
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
					SysMessage(TEXT("MMAudioDevice::Reinitialize(): Unsupported wave format\n"));
				CoTaskMemFree(pwfx);
				return false;
			}
		}
		else if(pwfx->wFormatTag != WAVE_FORMAT_IEEE_FLOAT)
		{
			if (!mDeviceLost)
				SysMessage(TEXT("MMAudioDevice::Reinitialize(): Unsupported wave format\n"));
			CoTaskMemFree(pwfx);
			return false;
		}

		mFloat            = true;
		mDeviceChannels         = pwfx->nChannels;
		mDeviceBitsPerSample    = 32;
		mDeviceBlockSize        = pwfx->nBlockAlign;
		mDeviceSamplesPerSec    = pwfx->nSamplesPerSec;
		//sampleWindowSize      = (inputSamplesPerSec/100);

		DWORD flags = 0;//useInputDevice ? 0 : AUDCLNT_STREAMFLAGS_LOOPBACK;

		//Random limit of 1ms to 1 seconds
		if(mBuffering == 0)
			mBuffering = 50;
		mBuffering = std::min(std::max(mBuffering, 1), 1000);
		OSDebugOut(TEXT("Mic buffering: %d\n"), mBuffering);

		err = mmClient->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, ConvertMSTo100NanoSec(mBuffering), 0, pwfx, NULL);
		//err = AUDCLNT_E_UNSUPPORTED_FORMAT;

		if(FAILED(err))
		{
			if (!mDeviceLost)
				SysMessage(TEXT("MMAudioDevice::Reinitialize(): Could not initialize audio client, result = %08lX\n"), err);
			CoTaskMemFree(pwfx);
			return false;
		}

		// acquire services
		if (mAudioDir == AUDIODIR_SOURCE)
			err = mmClient->GetService(IID_IAudioCaptureClient, (void**)&mmCapture);
		else
			err = mmClient->GetService(IID_IAudioRenderClient, (void**)&mmRender);

		if(FAILED(err))
		{
			if (!mDeviceLost)
				SysMessage(TEXT("MMAudioDevice::Reinitialize(): Could not get audio %s client, result = %08lX\n"),
					(mAudioDir == AUDIODIR_SOURCE ? TEXT("capture") : TEXT("render")), err);
			CoTaskMemFree(pwfx);
			return false;
		}

		err = mmClient->GetService(__uuidof(IAudioClock), (void**)&mmClock);

		if(FAILED(err))
		{
			if (!mDeviceLost)
				SysMessage(TEXT("MMAudioDevice::Reinitialize(): Could not get audio capture clock, result = %08lX\n"), err);
			CoTaskMemFree(pwfx);
			return false;
		}

		CoTaskMemFree(pwfx);

		// Setup resampler
		int converterType = SRC_SINC_FASTEST;
		int errVal = 0;

		mResampler = src_delete(mResampler);
		mResampler = src_new(converterType, mDeviceChannels, &errVal);

		if(!mResampler)
		{
			OSDebugOut(TEXT("Failed to create resampler: error %08lX\n"), errVal);
#ifndef _DEBUG
			SysMessage(TEXT("USBqemu: Failed to create resampler: error %08lX"), errVal);
#endif
			return false;
		}

		//random, reserve enough memory for 5 seconds
		mFloatBuffer.reserve(mDeviceChannels * mDeviceSamplesPerSec * 5);
		mShortBuffer.reserve(mDeviceChannels * mSamplesPerSec * 5);

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
		mFloatBuffer.resize(0);
		mShortBuffer.resize(0);
		ReleaseMutex(mMutex);
	}

	//TODO or just return samples count in mShortBuffer?
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
		pkSize += mShortBuffer.size() / mDeviceChannels;
		ReleaseMutex(mMutex);

		*size = pkSize;
		return true;
	}

	static DWORD WINAPI CaptureThread(LPVOID ptr)
	{
		MMAudioDevice *src = (MMAudioDevice*)ptr;
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

		src->ResetBuffers(); //reset, maybe thread died previously

		if (WaitForSingleObject(src->mEvent, INFINITE) != WAIT_OBJECT_0)
		{
			OSDebugOut(TEXT("Failed to for event: %08X\n"), GetLastError());
			goto error;
		}

#if _DEBUG
		if (!file)
		{
			char name[1024] = { 0 };
			sprintf_s(name, "audiosrc_output_%dch_%dHz.raw", src->mDeviceChannels, src->mSamplesPerSec);
			fopen_s(&file, name, "wb");
		}
#endif
		//Call mmClient->Start() here instead?
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
			if (src->mFloatBuffer.size())
			{
				size_t resampled = static_cast<size_t>(src->mFloatBuffer.size() * src->mResampleRatio * src->mTimeAdjust);
				if (resampled == 0)
					resampled = src->mFloatBuffer.size();
				rebuf.resize(resampled);

				SRC_DATA srcData;
				memset(&srcData, 0, sizeof(SRC_DATA));
				srcData.data_in = src->mFloatBuffer.data();
				srcData.input_frames = src->mFloatBuffer.size() / src->mDeviceChannels;
				srcData.data_out = rebuf.data();
				srcData.output_frames = rebuf.size() / src->mDeviceChannels;
				srcData.src_ratio = src->mResampleRatio * src->mTimeAdjust;

				src_process(src->mResampler, &srcData);

				DWORD resMutex = WaitForSingleObject(src->mMutex, 30000);
				if (resMutex != WAIT_OBJECT_0)
				{
					OSDebugOut(TEXT("Mutex wait failed: %d\n"), resMutex);
					goto error;
				}

				uint32_t len = srcData.output_frames_gen * src->mDeviceChannels;
				size_t size = src->mShortBuffer.size();
				if (len > 0)
				{
					//too long, drop samples, caused by saving/loading savestates and random stutters
					int sizeInMS = (((src->mShortBuffer.size() + len) * 1000 / src->mDeviceChannels) / src->mSamplesPerSec);
					int threshold = src->mBuffering > 25 ? src->mBuffering : 25;
					if (sizeInMS > threshold)
					{
						size = 0;
						src->mShortBuffer.resize(len);
					}
					else
						src->mShortBuffer.resize(size + len);
					src_float_to_short_array(&rebuf[0], &(src->mShortBuffer[size]), len);
				}

				OSDebugOut(TEXT("Resampler: in %d out %d used %d gen %d, rb: %zd\n"),
					srcData.input_frames, srcData.output_frames,
					srcData.input_frames_used, srcData.output_frames_gen, src->mShortBuffer.size());

#if _DEBUG
				if (file && len)
					fwrite(&(src->mShortBuffer[size]), sizeof(short), len, file);
#endif

				uint32_t sizeBefore = src->mFloatBuffer.size();
				auto remSize = srcData.input_frames_used * src->mDeviceChannels;
				src->mFloatBuffer.erase(src->mFloatBuffer.begin(), src->mFloatBuffer.begin() + remSize);

				OSDebugOut(TEXT("Sample Queue size: %zd - %d -> %zd\n"),
					sizeBefore, remSize, src->mFloatBuffer.size());

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

	static DWORD WINAPI RenderThread(LPVOID ptr)
	{
		MMAudioDevice *src = (MMAudioDevice*)ptr;
		std::vector<float> buffer;
		UINT32 bufferFrameCount, numFramesPadding, numFramesAvailable;
		BYTE *pData;
		SRC_DATA srcData;
		int ret = 1;
		HRESULT hr = 0;
		bool bThreadComInitialized = false;

		//TODO APARTMENTTHREADED instead?
		hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
		if ((S_OK != hr) && (S_FALSE != hr) /* already inited */ && (hr != RPC_E_CHANGED_MODE))
		{
			OSDebugOut(TEXT("Com initialization failed with %d\n"), hr);
			return 1;
		}

		if (hr != RPC_E_CHANGED_MODE)
			bThreadComInitialized = true;

		src->ResetBuffers(); //reset, maybe thread died previously

		if (WaitForSingleObject(src->mEvent, INFINITE) != WAIT_OBJECT_0)
		{
			OSDebugOut(TEXT("Failed to for event: %08X\n"), GetLastError());
			goto error;
		}

#if _DEBUG
		if (!file)
		{
			char name[1024] = { 0 };
			sprintf_s(name, "audiosink_%dch_%dHz.raw", src->mDeviceChannels, src->mDeviceSamplesPerSec);
			fopen_s(&file, name, "wb");
		}
#endif
		//Call mmClient->Start() here instead?
		src->mLastGetBufferMS = GetQPCTimeMS(); //fall through first time check

		while (!src->mQuit)
		{
			while (src->mPaused)
			{
				Sleep(100);
				if (src->mQuit)
					goto quit;
			}

			if (src->mDeviceLost && !src->Reinitialize())
			{
				Sleep(1000);
				continue;
			}

			DWORD resMutex = WaitForSingleObject(src->mMutex, 5000);
			if (resMutex != WAIT_OBJECT_0)
			{
				OSDebugOut(TEXT("Mutex wait failed: %d\n"), resMutex);
				goto error;
			}

			hr = src->mmClient->GetBufferSize(&bufferFrameCount);
			if (FAILED(hr))
				goto device_error;

			hr = src->mmClient->GetCurrentPadding(&numFramesPadding);
			if (FAILED(hr))
				goto device_error;

			numFramesAvailable = bufferFrameCount - numFramesPadding;

			if (src->mShortBuffer.size())
			{
				buffer.resize(src->mShortBuffer.size());
				src_short_to_float_array(src->mShortBuffer.data(), buffer.data(), buffer.size());

				if (numFramesAvailable > 0)
				{
					hr = src->mmRender->GetBuffer(numFramesAvailable, &pData);
					if (FAILED(hr))
						goto device_error;

					memset(&srcData, 0, sizeof(SRC_DATA));
					srcData.data_in = buffer.data();
					srcData.input_frames = buffer.size() / src->mDeviceChannels;
					srcData.data_out = (float*)pData;
					srcData.output_frames = numFramesAvailable;
					srcData.src_ratio = src->mResampleRatio * src->mTimeAdjust;

					src_process(src->mResampler, &srcData);

					auto frames_used = srcData.input_frames_used * src->mDeviceChannels;
					if (frames_used > 0)
						src->mShortBuffer.erase(src->mShortBuffer.begin(), src->mShortBuffer.begin() + frames_used);

#if _DEBUG
					uint32_t len = srcData.output_frames_gen * src->mDeviceChannels;
					if (file && len)
						fwrite(pData, sizeof(float), len, file);
#endif

					hr = src->mmRender->ReleaseBuffer(srcData.output_frames_gen, 0);
					if (FAILED(hr))
						goto device_error;

					OSDebugOut(TEXT("Resampler: in %d out %d used %d gen %d, rb: %zd\n"),
						srcData.input_frames, srcData.output_frames,
						srcData.input_frames_used, srcData.output_frames_gen, src->mShortBuffer.size());
				}
			}
			else
			{
				hr = src->mmRender->GetBuffer(numFramesAvailable, &pData);
				if (FAILED(hr))
					goto device_error;
				hr = src->mmRender->ReleaseBuffer(numFramesAvailable, AUDCLNT_BUFFERFLAGS_SILENT);
			}

		device_error:
			if (!ReleaseMutex(src->mMutex))
			{
				OSDebugOut(TEXT("Mutex release failed\n"));
				goto error;
			}

			if (FAILED(hr)) {
				if (hr == AUDCLNT_E_DEVICE_INVALIDATED) {
					src->mDeviceLost = true;
					OSDebugOut(TEXT("Audio device has been lost, attempting to reinitialize\n"));
				}
				else
					goto error;
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

		if (!mQuit && (mThread == INVALID_HANDLE_VALUE ||
			WaitForSingleObject(mThread, 0) == WAIT_OBJECT_0)) //Thread got killed prematurely
		{
			mThread = CreateThread(NULL, 0, MMAudioDevice::CaptureThread, this, 0, 0);
		}
		SetEvent(mEvent);

		DWORD resMutex = WaitForSingleObject(mMutex, 1000);
		if (resMutex != WAIT_OBJECT_0)
		{
			OSDebugOut(TEXT("Mutex wait failed: %d\n"), resMutex);
			return 0;
		}

		mSamples += outFrames;
		mTime = GetQPCTime100NS();
		if (mLastTimeNS == 0) mLastTimeNS = mTime;
		LONGLONG diff = mTime - mLastTimeNS;
		if (diff >= LONGLONG(1e7))
		{
			mTimeAdjust = (mSamples / (diff / 1e7)) / mSamplesPerSec;
			//if(mTimeAdjust > 1.0) mTimeAdjust = 1.0; //If game is in 'turbo mode', just return zero samples or...?
			OSDebugOut(TEXT("timespan: %I64d sampling: %f adjust: %f\n"), diff, float(mSamples) / diff * 1e7, mTimeAdjust);
			mLastTimeNS = mTime;
			mSamples = 0;
		}

		uint32_t totalLen = std::min(outFrames * mDeviceChannels, mShortBuffer.size());
		OSDebugOut(TEXT("Resampled buffer size: %zd, copy: %zd\n"), mShortBuffer.size(), totalLen);
		if (totalLen > 0)
		{
			memcpy(outBuf, &mShortBuffer[0], sizeof(short) * totalLen);
			mShortBuffer.erase(mShortBuffer.begin(), mShortBuffer.begin() + totalLen);
		}

		if (!ReleaseMutex(mMutex))
		{
			OSDebugOut(TEXT("Mutex release failed\n"));
		}

		return totalLen / mDeviceChannels;
	}

	virtual uint32_t SetBuffer(int16_t *inBuf, uint32_t inFrames)
	{
		mLastGetBufferMS = GetQPCTimeMS();

		if (!mQuit && (mThread == INVALID_HANDLE_VALUE ||
			WaitForSingleObject(mThread, 0) == WAIT_OBJECT_0)) //Thread got killed prematurely
		{
			mThread = CreateThread(NULL, 0, MMAudioDevice::RenderThread, this, 0, 0);
		}
		SetEvent(mEvent);

		DWORD resMutex = WaitForSingleObject(mMutex, 1000);
		if (resMutex != WAIT_OBJECT_0)
		{
			OSDebugOut(TEXT("Mutex wait failed: %d\n"), resMutex);
			return 0;
		}

		size_t old_size = mShortBuffer.size();
		size_t nshort = inFrames * mDeviceChannels;
		mShortBuffer.resize(old_size + nshort);
		memcpy(mShortBuffer.data() + old_size, inBuf, nshort * sizeof(int16_t));

		if (!ReleaseMutex(mMutex))
		{
			OSDebugOut(TEXT("Mutex release failed\n"));
		}

		return inFrames;
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
			UINT totalLen = numFramesRead * mDeviceChannels;
			if (dwFlags & AUDCLNT_BUFFERFLAGS_SILENT)
				mFloatBuffer.resize(mFloatBuffer.size() + totalLen);
			else
				mFloatBuffer.assign((float*)captureBuffer, (float*)captureBuffer + totalLen);

			mmCapture->ReleaseBuffer(numFramesRead);
		}

		return numFramesRead;
	}

	virtual void SetResampling(int samplerate)
	{
		if(mDeviceSamplesPerSec == samplerate)
		{
			mResample = false;
			return;
		}
		mSamplesPerSec = samplerate;
		if (mAudioDir == AUDIODIR_SOURCE)
			mResampleRatio = double(samplerate) / double(mDeviceSamplesPerSec);
		else
			mResampleRatio = double(mDeviceSamplesPerSec) / double(samplerate);
		mResample = true;
		ResetBuffers();
	}

	virtual uint32_t GetChannels()
	{
		return mDeviceChannels;
	}

	virtual MicMode GetMicMode(AudioDevice* compare)
	{
		if (compare && typeid(compare) != typeid(this))
			return MIC_MODE_SEPARATE; //atleast, if not single altogether

		if (compare)
		{
			MMAudioDevice *src = dynamic_cast<MMAudioDevice *>(compare);
			if (src && mDevID == src->mDevID)
				return MIC_MODE_SHARED;
			return MIC_MODE_SEPARATE;
		}

		CONFIGVARIANT var(mDevice ? N_AUDIO_SOURCE0 : N_AUDIO_SOURCE1, CONFIG_TYPE_WCHAR);
		if (LoadSetting(mPort, APINAME, var) && var.wstrValue == mDevID)
			return MIC_MODE_SHARED;

		return MIC_MODE_SINGLE;
	}

	static const TCHAR* Name()
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

	static void AudioDevices(std::vector<AudioDeviceInfo> &devices, AudioDir dir)
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
		EDataFlow audioDeviceType = (dir == AUDIODIR_SOURCE ? eCapture : eRender);
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
		WASAPISettings settings;
		settings.port = port;

		return DialogBoxParam(h.hInst,
			MAKEINTRESOURCE(IDD_DLGWASAPI),
			h.hWnd,
			(DLGPROC)WASAPIDlgProc, (LPARAM)&settings);
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
	IAudioRenderClient  *mmRender;
	IAudioClock         *mmClock;

	bool  mResample;
	bool  mFloat;
	bool  mFirstSamples; //On the first call, empty the buffer to lower latency
	UINT  mDeviceChannels;
	UINT  mDeviceSamplesPerSec;
	UINT  mSamplesPerSec;
	UINT  mDeviceBitsPerSample;
	UINT  mDeviceBlockSize;
	DWORD mInputChannelMask;

	std::wstring mDevID;
	bool mDeviceLost;
	std::wstring mDeviceName;
	int mDevice;
	int mPort;
	int mBuffering;
	AudioDir mAudioDir;

	SRC_STATE *mResampler;
	double mResampleRatio;
	// Speed up or slow down audio
	double mTimeAdjust;
	std::vector<short> mShortBuffer;
	std::vector<float> mFloatBuffer;
	HANDLE mThread;
	HANDLE mEvent;
	HANDLE mMutex;
	bool mQuit;
	bool mPaused;
	LONGLONG mLastGetBufferMS;

	LONGLONG mTime = 0;
	int mSamples = 0;
	LONGLONG mLastTimeMS = 0;
	LONGLONG mLastTimeNS = 0;
};

static void RefreshInputAudioList(HWND hW, LRESULT idx, WASAPISettings *settings)
{
	settings->sourceDevs.clear();

	SendDlgItemMessage(hW, IDC_COMBO1, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage(hW, IDC_COMBO2, CB_RESETCONTENT, 0, 0);

	SendDlgItemMessageW(hW, IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)L"None");
	SendDlgItemMessageW(hW, IDC_COMBO2, CB_ADDSTRING, 0, (LPARAM)L"None");

	SendDlgItemMessage(hW, IDC_COMBO1, CB_SETCURSEL, 0, 0);
	SendDlgItemMessage(hW, IDC_COMBO2, CB_SETCURSEL, 0, 0);

	MMAudioDevice::AudioDevices(settings->sourceDevs, AUDIODIR_SOURCE);
	AudioDeviceInfoList::iterator it;
	int i = 0;
	for (it = settings->sourceDevs.begin(); it != settings->sourceDevs.end(); it++)
	{
		SendDlgItemMessageW(hW, IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)it->strName.c_str());
		SendDlgItemMessageW(hW, IDC_COMBO2, CB_ADDSTRING, 0, (LPARAM)it->strName.c_str());

		i++;
		if (it->strID == settings->selectedDev[0])
			SendDlgItemMessage(hW, IDC_COMBO1, CB_SETCURSEL, i, i);
		if (it->strID == settings->selectedDev[1])
			SendDlgItemMessage(hW, IDC_COMBO2, CB_SETCURSEL, i, i);
	}
}

static void RefreshOutputAudioList(HWND hW, LRESULT idx, WASAPISettings *settings)
{
	settings->sinkDevs.clear();

	SendDlgItemMessage(hW, IDC_COMBO3, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessageW(hW, IDC_COMBO3, CB_ADDSTRING, 0, (LPARAM)L"None");
	SendDlgItemMessage(hW, IDC_COMBO3, CB_SETCURSEL, 0, 0);

	MMAudioDevice::AudioDevices(settings->sinkDevs, AUDIODIR_SINK);
	AudioDeviceInfoList::iterator it;
	int i = 0;
	for (it = settings->sinkDevs.begin(); it != settings->sinkDevs.end(); it++)
	{
		SendDlgItemMessageW(hW, IDC_COMBO3, CB_ADDSTRING, 0, (LPARAM)it->strName.c_str());

		i++;
		if (it->strID == settings->selectedDev[2])
			SendDlgItemMessage(hW, IDC_COMBO3, CB_SETCURSEL, i, i);
	}
}

static BOOL CALLBACK WASAPIDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	int tmp = 0;
	WASAPISettings *s;
	static auto audioProxyMap = RegisterAudioDevice::instance().Map();

	switch (uMsg) {
	case WM_CREATE:
		SetWindowLong(hW, GWL_USERDATA, (LONG)lParam);
		break;
	case WM_INITDIALOG:
	{
		s = (WASAPISettings *)lParam;
		SetWindowLong(hW, GWL_USERDATA, (LONG)lParam);
		int buffering = 20;
		{
			CONFIGVARIANT var(N_BUFFER_LEN, CONFIG_TYPE_INT);
			if (LoadSetting(s->port, APINAME, var))
				buffering = var.intValue;
		}
		SendDlgItemMessage(hW, IDC_SLIDER1, TBM_SETRANGEMIN, TRUE, 1);
		SendDlgItemMessage(hW, IDC_SLIDER1, TBM_SETRANGEMAX, TRUE, 1000);
		SendDlgItemMessage(hW, IDC_SLIDER1, TBM_SETPOS, TRUE, buffering);
		SetDlgItemInt(hW, IDC_BUFFER1, buffering, FALSE);
		{
			for (int i = 0; i < 2; i++)
			{
				CONFIGVARIANT var0(i ? N_AUDIO_SOURCE1 : N_AUDIO_SOURCE0, CONFIG_TYPE_WCHAR);
				if (LoadSetting(s->port, APINAME, var0))
					s->selectedDev[i] = var0.wstrValue;
			}

			{
				CONFIGVARIANT var1(N_AUDIO_SINK0, CONFIG_TYPE_WCHAR);
				if (LoadSetting(s->port, APINAME, var1))
					s->selectedDev[2] = var1.wstrValue;
			}
		}
		RefreshInputAudioList(hW, -1, s);
		RefreshOutputAudioList(hW, -1, s);
		return TRUE;
	}
	case WM_HSCROLL:
		if ((HWND)lParam == GetDlgItem(hW, IDC_SLIDER1))
		{
			int pos = SendDlgItemMessage(hW, IDC_SLIDER1, TBM_GETPOS, 0, 0);
			SetDlgItemInt(hW, IDC_BUFFER1, pos, FALSE);
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
			case IDC_BUFFER1:
				CHECKED_SET_MAX_INT(tmp, hW, IDC_BUFFER1, FALSE, 1, 1000);
				SendDlgItemMessage(hW, IDC_SLIDER1, TBM_SETPOS, TRUE, tmp);
				break;
			}
		}
		break;
		case BN_CLICKED:
		{
			switch (LOWORD(wParam)) {
			case IDOK:
			{
				int p[3];
				s = (WASAPISettings *)GetWindowLong(hW, GWL_USERDATA);
				INT_PTR res = RESULT_OK;
				p[0] = SendDlgItemMessage(hW, IDC_COMBO1, CB_GETCURSEL, 0, 0);
				p[1] = SendDlgItemMessage(hW, IDC_COMBO2, CB_GETCURSEL, 0, 0);
				p[2] = SendDlgItemMessage(hW, IDC_COMBO3, CB_GETCURSEL, 0, 0);

				for (int i = 0; i < 3; i++)
				{
					s->selectedDev[i] = L"";

					if (p[i] > 0)
						s->selectedDev[i] = ((i < 2 ? s->sourceDevs.begin() : s->sinkDevs.begin()) + p[i] - 1)->strID;
				}

				const wchar_t *n[] = { N_AUDIO_SOURCE0, N_AUDIO_SOURCE1, N_AUDIO_SINK0 };
				for (int i = 0; i < 3; i++)
				{
					CONFIGVARIANT var(n[i], s->selectedDev[i]);
					if (!SaveSetting(s->port, APINAME, var))
						res = RESULT_FAILED;
				}

				{
					CONFIGVARIANT var(N_BUFFER_LEN, (int32_t)SendDlgItemMessage(hW, IDC_SLIDER1, TBM_GETPOS, 0, 0));
					if (!SaveSetting(s->port, APINAME, var))
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

REGISTER_AUDIODEV(APINAME, MMAudioDevice);
};
#undef APINAME
#undef APINAMEW

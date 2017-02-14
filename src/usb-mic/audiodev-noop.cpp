#include "audiodeviceproxy.h"

#define APINAME "noop"
#define APINAMEW TEXT(APINAME)

class NoopAudioDevice : public AudioDevice
{
public:
	NoopAudioDevice(int port, int mic, AudioDir dir) {}
	~NoopAudioDevice() {}
	void Start() {}
	void Stop() {}
	virtual bool GetFrames(uint32_t *size)
	{
		return true;
	}
	virtual uint32_t GetBuffer(int16_t *outBuf, uint32_t outFrames)
	{
		return outFrames;
	}
	virtual uint32_t SetBuffer(int16_t *inBuf, uint32_t inFrames)
	{
		return inFrames;
	}
	virtual void SetResampling(int samplerate) {}
	virtual uint32_t GetChannels()
	{
		return 1;
	}

	virtual MicMode GetMicMode(AudioDevice* compare)
	{
		return MIC_MODE_SINGLE;
	}

	static const TCHAR* Name()
	{
		return TEXT("NOOP");
	}

	static bool AudioInit()
	{
		return true;
	}

	static void AudioDeinit()
	{
	}

	static void AudioDevices(std::vector<AudioDeviceInfo> &devices)
	{
		AudioDeviceInfo info;
		info.strID = TEXT("silence");
		info.strName = TEXT("Silence");
		devices.push_back(info);
	}

	static int Configure(int port, void *data)
	{
		return RESULT_OK;
	}

	static std::vector<CONFIGVARIANT> GetSettings()
	{
		//TODO GetSettings()
		return std::vector<CONFIGVARIANT>();
	}
};

REGISTER_AUDIODEV(APINAME, NoopAudioDevice);
#undef APINAME
#undef APINAMEW

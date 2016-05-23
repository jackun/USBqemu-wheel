#include "audiosourceproxy.h"

#define APINAME "noop"
#define APINAMEW TEXT(APINAME)

class NoopAudioSource : public AudioSource
{
public:
	NoopAudioSource(int port, int mic) {}
	~NoopAudioSource() {}
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
	virtual void SetResampling(int samplerate) {}
	virtual uint32_t GetChannels()
	{
		return 1;
	}

	virtual MicMode GetMicMode(AudioSource* compare)
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

REGISTER_AUDIOSRC(APINAME, NoopAudioSource);
#undef APINAME
#undef APINAMEW

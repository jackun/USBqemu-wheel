#pragma once
#include "midideviceproxy.h"

namespace mididev_noop {

static const char *APINAME = "noop";

class NoopMidiDevice : public MidiDevice
{
public:
	NoopMidiDevice(int port, const char* dev_type, int mic, MidiDir dir): MidiDevice(port, dev_type, mic, dir) {}
	~NoopMidiDevice() {}
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

	virtual bool Compare(MidiDevice* compare)
	{
		return false;
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

	static void MidiDevices(std::vector<MidiDeviceInfo> &devices, MidiDir )
	{
		MidiDeviceInfo info;
		info.strID = TEXT("silence");
		info.strName = TEXT("Silence");
		devices.push_back(info);
	}

	static int Configure(int port, const char* dev_type, void *data)
	{
		return RESULT_OK;
	}
};

}
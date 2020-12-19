// Used OBS as example

#include "../midideviceproxy.h"
#include "libsamplerate/samplerate.h"
#include "shared/ringbuffer.h"

#include <mmdeviceapi.h>
#include <audioclient.h>

namespace usb_midi { namespace mididev_devices {

static const char *APINAME = "mididevs";

class Win32MidiDevice : public MidiDevice
{
public:
	Win32MidiDevice(int port, const char* dev_type):
	MidiDevice(port, dev_type)
	{
		if(!Init())
			throw usb_midi::MidiDeviceError("Win32MidiDevice:: device name is empty, skipping");
		if(!Reinitialize())
			throw usb_midi::MidiDeviceError("Win32MidiDevice:: Devices init failed!");
	}

	~Win32MidiDevice();
	bool Init();
	bool Reinitialize();
	void Start();
	void Stop();
	uint32_t PopMidiCommand();

	virtual bool Compare(MidiDevice* compare)
	{
		if (compare)
		{
			Win32MidiDevice *src = static_cast<Win32MidiDevice *>(compare);
			if (src && mDevID == src->mDevID)
				return true;
		}
		return false;
	}

	static const char* TypeName()
	{
		return APINAME;
	}

	static const TCHAR* Name()
	{
		return TEXT("MIDI Input Device");
	}

	static bool AudioInit();

	static void AudioDeinit()
	{
	}

	static void MidiDevices(std::vector<MidiDeviceInfo> &devices);
	static int Configure(int port, const char* dev_type, void *data);

private:
	std::wstring mDevID;
	std::wstring mDeviceName;

	std::queue<unsigned int> midiBuffer;
	int32_t midiOffset;
};

}} // namespace
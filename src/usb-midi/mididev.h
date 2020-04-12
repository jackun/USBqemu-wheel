//
// Types to shared by platforms and config. dialog.
//

#ifndef MIDIDEV_H
#define MIDIDEV_H

#include <string>
#include <vector>
#include <queue>

#define S_AUDIO_SOURCE0	TEXT("Audio source 1")
#define N_AUDIO_SOURCE0	TEXT("audio_src_0")
#define KEY_OFFSET TEXT("key_offset")

//TODO sufficient for linux too?
struct MidiDeviceInfoA
{
	//int intID; //optional ID
	std::string strID;
	std::string strName; //gui name
};

struct MidiDeviceInfoW
{
	//int intID; //optional ID
	std::wstring strID;
	std::wstring strName; //gui name
};

#if _WIN32
#define MidiDeviceInfo MidiDeviceInfoW
#else
#define MidiDeviceInfo MidiDeviceInfoA
#endif

class MidiDevice
{
public:
	MidiDevice(int port, const char* dev_type): mPort(port)
	, mDevType(dev_type)
	{

	}

	virtual ~MidiDevice() {}

	virtual void Start() = 0;
	virtual void Stop() = 0;
	virtual uint32_t PopMidiCommand() = 0;

	// Compare if another instance is using the same device
	virtual bool Compare(MidiDevice* compare) = 0;

	//Remember to add to your class
	//static const wchar_t* GetName();

protected:
	int mPort;
	const char *mDevType;
};

typedef std::vector<MidiDeviceInfo> MidiDeviceInfoList;
#endif
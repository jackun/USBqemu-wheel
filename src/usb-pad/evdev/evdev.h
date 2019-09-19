#pragma once
#include "linux/util.h"
#include <unistd.h>
//#include <dirent.h> //gtk.h pulls in?
#include <thread>
#include <array>
#include <atomic>
#include "evdev-ff.h"
#include "shared.h"
#include "readerwriterqueue/readerwriterqueue.h"

namespace usb_pad { namespace evdev {

#define test_bit(nr, addr) \
	(((1UL << ((nr) % (sizeof(long) * 8))) & ((addr)[(nr) / (sizeof(long) * 8)])) != 0)
#define NBITS(x) ((((x)-1)/(sizeof(long) * 8))+1)

void EnumerateDevices(vstring& list);

struct axis_correct
{
	int used;
	int coef[3];
};

struct device_data
{
	int fd;
	std::string name;
	uint8_t axis_map[ABS_MAX + 1];
	bool axis_inverted[3];
	//uint8_t
	uint16_t btn_map[KEY_MAX + 1];
	struct axis_correct abs_correct[ABS_MAX];
	int axes = 0;
	int buttons = 0;
	std::vector<uint16_t> mappings;
	bool is_gamepad; //xboxish gamepad
	bool is_dualanalog; // tricky, have to read the AXIS_RZ somehow and
					// determine if its unpressed value is zero
};


class EvDevPad : public Pad
{
public:
	EvDevPad(int port, const char* dev_type): Pad(port, dev_type)
	, mUseRawFF(false)
	, mHidHandle(-1)
	, mWriterThreadIsRunning(false)
	{
	}

	~EvDevPad() { Close(); }
	int Open();
	int Close();
	int TokenIn(uint8_t *buf, int len);
	int TokenOut(const uint8_t *data, int len);
	int Reset() { return 0; }

	static const TCHAR* Name()
	{
		return "Evdev";
	}

	static int Configure(int port, const char* dev_type, void *data);
protected:
	void PollAxesValues(const device_data& device);
	void SetAxis(const device_data& device, int code, int value);
	static void WriterThread(void *ptr);

	int mHidHandle;
	EvdevFF *mEvdevFF;
	struct wheel_data_t mWheelData;
	std::vector<device_data> mDevices;
	bool mUseRawFF;
	std::thread mWriterThread;
	std::atomic<bool> mWriterThreadIsRunning;
	moodycamel::BlockingReaderWriterQueue<std::array<uint8_t, 8>, 32> mFFData;
};

template< size_t _Size >
bool GetEvdevName(const std::string& path, char (&name)[_Size])
{
	int fd = 0;
	if ((fd = open(path.c_str(), O_RDONLY)) < 0)
	{
		fprintf(stderr, "Cannot open %s\n", path.c_str());
	}
	else
	{
		if (ioctl(fd, EVIOCGNAME(_Size), name) < -1)
		{
			fprintf(stderr, "Cannot get controller's name\n");
			close(fd);
			return false;
		}
		close(fd);
		return true;
	}
	return false;
}

// SDL2
// convert values into 16 bit range
static int AxisCorrect(const axis_correct& correct, int value)
{
	if (correct.used) {
		value *= 2;
		if (value > correct.coef[0]) {
			if (value < correct.coef[1]) {
				return 0;
			}
			value -= correct.coef[1];
		} else {
			value -= correct.coef[0];
		}
		value *= correct.coef[2];
		value >>= 13;
	}

	/* Clamp and return */
	if (value < -32768)
		return -32768;
	if (value > 32767)
		return 32767;

	return value;
}

}} //namespace

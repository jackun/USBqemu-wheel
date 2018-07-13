#pragma once
#include "linux/util.h"
#include <linux/input.h>
#include <unistd.h>
#include <thread>
#include <array>
#include <atomic>
#include "evdev-ff.h"
#include "shared.h"
#include "readerwriterqueue/readerwriterqueue.h"

#define test_bit(nr, addr) \
	(((1UL << ((nr) % (sizeof(long) * 8))) & ((addr)[(nr) / (sizeof(long) * 8)])) != 0)
#define NBITS(x) ((((x)-1)/(sizeof(long) * 8))+1)

struct axis_correct
{
	int used;
	int coef[3];
};

class EvDevPad : public Pad
{
public:
	EvDevPad(int port): Pad(port)
	, mIsGamepad(false)
	, mIsDualAnalog(false)
	, mUseRawFF(false)
	, mEvdevFF(nullptr)
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

	static int Configure(int port, void *data);
protected:
	void PollAxesValues();
	void SetAxis(int code, int value);
	static void WriterThread(void *ptr);

	int mHandle;
	int mHidHandle;
	EvdevFF *mEvdevFF;
	struct wheel_data_t mWheelData;
	uint8_t  mAxisMap[ABS_MAX + 1];
	uint8_t  mAxisInverted[ABS_MAX + 1];
	//uint8_t
	uint16_t mBtnMap[KEY_MAX + 1];
	struct axis_correct mAbsCorrect[ABS_MAX];

	int mAxisCount;
	int mButtonCount;

	std::vector<uint16_t> mMappings;

	bool mIsGamepad; //xboxish gamepad
	bool mIsDualAnalog; // tricky, have to read the AXIS_RZ somehow and
						// determine if its unpressed value is zero
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
static int AxisCorrect(axis_correct& correct, int value)
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

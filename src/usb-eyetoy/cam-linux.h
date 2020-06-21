#include "videodev.h"

namespace usb_eyetoy
{
namespace linux_api
{

static const char *APINAME = "V4L2";

class V4L2 : public VideoDevice
{
public:
	V4L2(int port) : mPort(port) {};
	~V4L2(){};
	int Open();
	int Close();
	int GetImage(uint8_t *buf, int len);
	int Reset();

	static const TCHAR *Name() {
		return TEXT("V4L2");
	}
	static int Configure(int port, const char *dev_type, void *data){
		return 0;
	};

	int Port() { return mPort; }
	void Port(int port) { mPort = port; }

protected:
	int mPort;
};

} // namespace linux_api
} // namespace usb_eyetoy

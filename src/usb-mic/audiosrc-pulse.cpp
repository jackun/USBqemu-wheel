#include <cstdint>
#include "audiosourceproxy.h"
#include <pulse/pulseaudio.h>
#include <typeinfo>

#define APINAME "pulse"

static void pa_context_state_cb(pa_context *c, void *userdata)
{
	pa_context_state_t state;
	int *pa_ready = (int *)userdata;

	state = pa_context_get_state(c);
	switch (state) {
		// There are just here for reference
		case PA_CONTEXT_UNCONNECTED:
		case PA_CONTEXT_CONNECTING:
		case PA_CONTEXT_AUTHORIZING:
		case PA_CONTEXT_SETTING_NAME:
		default:
			break;
		case PA_CONTEXT_FAILED:
		case PA_CONTEXT_TERMINATED:
			*pa_ready = 2;
			break;
		case PA_CONTEXT_READY:
			*pa_ready = 1;
			break;
	}
}

static void pa_sourcelist_cb(pa_context *c, const pa_source_info *l, int eol, void *userdata)
{
	AudioDeviceInfoList *devicelist = static_cast<AudioDeviceInfoList *>(userdata);

	if (eol > 0) {
		return;
	}

	AudioDeviceInfo dev;
	dev.strID = l->name;
	dev.strName = l->description;
	//dev.intID = l->index;
	devicelist->push_back(dev);
}

static int pa_get_devicelist(AudioDeviceInfoList& input)
{
	pa_mainloop *pa_ml;
	pa_mainloop_api *pa_mlapi;
	pa_operation *pa_op;
	pa_context *pa_ctx;

	int state = 0;
	int pa_ready = 0;

	pa_ml = pa_mainloop_new();
	pa_mlapi = pa_mainloop_get_api(pa_ml);
	pa_ctx = pa_context_new(pa_mlapi, "USBqemu-devicelist");

	pa_context_connect(pa_ctx, NULL, PA_CONTEXT_NOFLAGS, NULL);

	pa_context_set_state_callback(pa_ctx, pa_context_state_cb, &pa_ready);

	for (;;) {

		if (pa_ready == 0)
		{
			pa_mainloop_iterate(pa_ml, 1, NULL);
			continue;
		}

		// Connection failed
		if (pa_ready == 2)
		{
			pa_context_disconnect(pa_ctx);
			pa_context_unref(pa_ctx);
			pa_mainloop_free(pa_ml);
			return -1;
		}

		switch (state)
		{
			case 0:
				pa_op = pa_context_get_source_info_list(pa_ctx,
						pa_sourcelist_cb,
						&input);
				state++;
				break;
			case 1:
				if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE)
				{
					pa_operation_unref(pa_op);
					pa_context_disconnect(pa_ctx);
					pa_context_unref(pa_ctx);
					pa_mainloop_free(pa_ml);
					return 0;
				}
				break;
			default:
				return -1;
		}
		pa_mainloop_iterate(pa_ml, 1, NULL);
	}
}

class PulseAudioSource : public AudioSource
{
public:
	PulseAudioSource(int port, int mic): mPort(port), mMic(mic)
	{
		CONFIGVARIANT var(mic ? N_AUDIO_DEVICE1 : N_AUDIO_DEVICE0, CONFIG_TYPE_CHAR);
		if(LoadSetting(mPort, APINAME, var) && !var.strValue.empty())
		{
			mDevice = var.strValue;
			//TODO open device etc.
		}
		else
			throw AudioSourceError(APINAME ": failed to load device settings");
	}
	~PulseAudioSource() {}

	uint32_t GetBuffer(int16_t *buff, uint32_t len)
	{
		return 0;
	}

	bool GetFrames(uint32_t *size)
	{
		return false;
	}

	void SetResampling(int samplerate)
	{
	}
	
	uint32_t GetChannels()
	{
		return 0;
	}

	void Start() {}
	void Stop() {}

	virtual MicMode GetMicMode(AudioSource* compare)
	{
		if(compare && typeid(compare) != typeid(this))
			return MIC_MODE_SEPARATE; //atleast, if not single altogether

		if (compare)
		{
			PulseAudioSource *src = dynamic_cast<PulseAudioSource *>(compare);
			if (mDevice == src->mDevice)
				return MIC_MODE_SHARED;
		}

		CONFIGVARIANT var(mMic ? N_AUDIO_DEVICE0 : N_AUDIO_DEVICE1, CONFIG_TYPE_CHAR);
		if(LoadSetting(mPort, APINAME, var) && var.strValue == mDevice)
			return MIC_MODE_SHARED;

		return MIC_MODE_SEPARATE;
	}

	static const wchar_t* Name()
	{
		return L"PulseAudio";
	}

	static bool Configure(int port, void *data)
	{
		return false;
	}

	static void AudioDevices(std::vector<AudioDeviceInfo> &devices)
	{
		pa_get_devicelist(devices);
	}

	static bool AudioInit() { return false; }
	static void AudioDeinit() {}
	static std::vector<CONFIGVARIANT> GetSettings()
	{
		return std::vector<CONFIGVARIANT>();
	}

protected:
	int mPort;
	int mMic;
	int mChannels;
	std::string mDevice;
};

REGISTER_AUDIOSRC(APINAME, PulseAudioSource);
#undef APINAME

#include <cstdint>
#include "audiosourceproxy.h"
//#include <pulse/pulseaudio.h>
#include "../dynlink/pulse.h"
#include <typeinfo>

#define APINAME "pulse"

static void pa_context_state_cb(pa_context *c, void *userdata)
{
	pa_context_state_t state;
	int *pa_ready = (int *)userdata;

	state = pf_pa_context_get_state(c);
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

	pa_ml = pf_pa_mainloop_new();
	pa_mlapi = pf_pa_mainloop_get_api(pa_ml);
	pa_ctx = pf_pa_context_new(pa_mlapi, "USBqemu-devicelist");

	pf_pa_context_connect(pa_ctx, NULL, PA_CONTEXT_NOFLAGS, NULL);

	pf_pa_context_set_state_callback(pa_ctx, pa_context_state_cb, &pa_ready);

	for (;;) {

		if (pa_ready == 0)
		{
			pf_pa_mainloop_iterate(pa_ml, 1, NULL);
			continue;
		}

		// Connection failed
		if (pa_ready == 2)
		{
			pf_pa_context_disconnect(pa_ctx);
			pf_pa_context_unref(pa_ctx);
			pf_pa_mainloop_free(pa_ml);
			return -1;
		}

		switch (state)
		{
			case 0:
				pa_op = pf_pa_context_get_source_info_list(pa_ctx,
						pa_sourcelist_cb,
						&input);
				state++;
				break;
			case 1:
				if (pf_pa_operation_get_state(pa_op) == PA_OPERATION_DONE)
				{
					pf_pa_operation_unref(pa_op);
					pf_pa_context_disconnect(pa_ctx);
					pf_pa_context_unref(pa_ctx);
					pf_pa_mainloop_free(pa_ml);
					return 0;
				}
				break;
			default:
				return -1;
		}
		pf_pa_mainloop_iterate(pa_ml, 1, NULL);
	}
}

static void context_notify_cb (pa_context *c, void *userdata)
{
	printf("context_notify\n");
	pa_context_state_t state;
	int *pa_ready = (int *)userdata;

	state = pf_pa_context_get_state(c);
	switch (state) {
			// These are just here for reference
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

static void stream_read_cb (pa_stream *p, size_t nbytes, void *userdata)
{
	const void* data = NULL;
	printf("stream_read_callback %d bytes\n", nbytes);
	int ret = pf_pa_stream_peek(p, &data, &nbytes);

	printf("pa_stream_peek %zu %s\n", nbytes, pf_pa_strerror(ret));

	//memcpy(dst, data, nbytes);

	//if copy succeeded, drop samples at pulse's side
	ret = pf_pa_stream_drop(p);
	printf("pa_stream_drop %s\n", pf_pa_strerror(ret));
}

class PulseAudioSource : public AudioSource
{
public:
	PulseAudioSource(int port, int mic): mPort(port)
	, mMic(mic)
	, mBuffering(50)
	, mQuit(false)
	, mPMainLoop(nullptr)
	, mPContext(nullptr)
	, mStream(nullptr)
	, mPAready(0)
	{
		CONFIGVARIANT var(mic ? N_AUDIO_DEVICE1 : N_AUDIO_DEVICE0, CONFIG_TYPE_CHAR);
		if(LoadSetting(mPort, APINAME, var) && !var.strValue.empty())
		{
			mDevice = var.strValue;
			//TODO open device etc.
		}
		else
			throw AudioSourceError(APINAME ": failed to load device settings");

		{
			CONFIGVARIANT var(N_BUFFER_LEN, CONFIG_TYPE_INT);
			if(LoadSetting(mPort, APINAME, var))
				mBuffering = var.intValue;
		}

		if (!AudioInit())
			throw AudioSourceError(APINAME ": failed to init API");
	}

	~PulseAudioSource()
	{
		mQuit = true;
		AudioDeinit();
	}

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

	void Uninit()
	{
		int ret;
		if (mStream) {
			ret = pa_stream_disconnect(mStream);
			pa_stream_unref(mStream); //needed?
			mStream = nullptr;
		}
		if (mPContext) {
			pa_context_disconnect(mPContext);
			pa_context_unref(mPContext);
			mPContext = nullptr;
		}
		if (mPMainLoop) {
			pa_threaded_mainloop_stop(mPMainLoop);
			pa_threaded_mainloop_free(mPMainLoop);
			mPMainLoop = nullptr;
		}
	}

	bool Init()
	{
		int ret;
		char *server = NULL; //TODO add server selector?

		mPMainLoop = pf_pa_threaded_mainloop_new();
		pa_mainloop_api *mlapi = pf_pa_threaded_mainloop_get_api(mPMainLoop);

		mPContext = pf_pa_context_new (mlapi, "USBqemu-pulse");

		ret = pf_pa_context_connect (mPContext,
			server,
			PA_CONTEXT_NOFLAGS,
			NULL
		);

		printf("pa_context_connect %s\n", pf_pa_strerror(ret));
		if (ret)
			goto error;

		pf_pa_context_set_state_callback(mPContext,
			context_notify_cb,
			&mPAready
		);

		pa_threaded_mainloop_start(mPMainLoop);

		// wait for context_notify_cb
		for(;;)
		{
			if(mPAready == 1) break;
			if(mPAready == 2 || mQuit) goto error;
		}

		pa_sample_spec ss;
		ss.format =  PA_SAMPLE_FLOAT32LE; //PA_SAMPLE_S16LE;
		ss.channels = 2;
		ss.rate = 48000;

		mStream = pf_pa_stream_new(mPContext,
			"USBqemu-pulse",
			&ss,
			NULL
		);

		pf_pa_stream_set_read_callback(mStream,
			stream_read_cb,
			this
		);

		// Sets individual read callback fragsize but recording itself
		// still "lags" ~1sec (read_cb is called in bursts) without 
		// PA_STREAM_ADJUST_LATENCY
		pa_buffer_attr buffer_attr;
		buffer_attr.maxlength = (uint32_t) -1;
		buffer_attr.tlength = (uint32_t) -1;
		buffer_attr.prebuf = (uint32_t) -1;
		buffer_attr.minreq = (uint32_t) -1;
		buffer_attr.fragsize = pa_usec_to_bytes(mBuffering * 1000, &ss);
		printf("usec_to_bytes %zu\n", buffer_attr.fragsize);

		ret = pf_pa_stream_connect_record(mStream,
			mDevice.c_str(),
			&buffer_attr,
			PA_STREAM_ADJUST_LATENCY
		);

		printf("pa_stream_connect_record %s\n", pf_pa_strerror(ret));
		if (ret)
			goto error;

		return true;
	error:
		Uninit();
		return false;
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

	static bool AudioInit()
	{
		return DynLoadPulse();
	}
	static void AudioDeinit()
	{
		DynUnloadPulse();
	}
	static std::vector<CONFIGVARIANT> GetSettings()
	{
		return std::vector<CONFIGVARIANT>();
	}

protected:
	int mPort;
	int mMic;
	int mChannels;
	int mBuffering;
	std::string mDevice;

	bool mQuit;
	int mPAready;
	pa_threaded_mainloop *mPMainLoop;
	pa_context *mPContext;
	pa_stream  *mStream;
};

REGISTER_AUDIOSRC(APINAME, PulseAudioSource);
#undef APINAME

#include <cstdint>
#include <cstring>
#include "../osdebugout.h"
#include "audiosourceproxy.h"
//#include <pulse/pulseaudio.h>
#include "../dynlink/pulse.h"
#include "../libsamplerate/samplerate.h"
#include <typeinfo>
//#include <thread>
#include <mutex>
#include <chrono>

using hrc = std::chrono::high_resolution_clock;
using ms = std::chrono::milliseconds;
using us = std::chrono::microseconds;
using ns = std::chrono::nanoseconds;
using sec = std::chrono::seconds;

#define APINAME "pulse"

static void pa_context_state_cb(pa_context *c, void *userdata)
{
	pa_context_state_t state;
	int *pa_ready = (int *)userdata;

	state = pf_pa_context_get_state(c);
	switch (state) {
		// There are just here for reference
		case PA_CONTEXT_UNCONNECTED:
			*pa_ready = 3;
			break;
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

class PulseAudioSource : public AudioSource
{
public:
	PulseAudioSource(int port, int mic): mPort(port)
	, mMic(mic)
	, mBuffering(50)
	, mPaused(true)
	, mQuit(false)
	, mPMainLoop(nullptr)
	, mPContext(nullptr)
	, mStream(nullptr)
	, mServer(nullptr)
	, mPAready(0)
	, mResampleRatio(1.0)
	, mTimeAdjust(1.0)
	, mOutputSamplesPerSec(48000)
	, mResampler(nullptr)
	, mOutSamples(0)
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
			throw AudioSourceError(APINAME ": failed to bind pulseaudio library");

		mSSpec.format =  PA_SAMPLE_FLOAT32LE; //PA_SAMPLE_S16LE;
		mSSpec.channels = 2;
		mSSpec.rate = 48000;

		if (!Init())
			throw AudioSourceError(APINAME ": failed to init");
	}

	~PulseAudioSource()
	{
		mQuit = true;
		Uninit();
		AudioDeinit();
		mResampler = src_delete(mResampler);
	}

	uint32_t GetBuffer(int16_t *buff, uint32_t len)
	{
		auto now = hrc::now();
		auto dur = std::chrono::duration_cast<ms>(now-mLastGetBuffer).count();
		//Disconnected, try reconnect after every 1sec, hopefully game retries to read samples
		if (mPAready == 3 && dur >= 1000)
		{
			mLastGetBuffer = now;
			int ret = pf_pa_context_connect (mPContext,
				mServer,
				PA_CONTEXT_NOFLAGS,
				NULL
			);

			OSDebugOut("pa_context_connect %s\n", pf_pa_strerror(ret));
		}
		else
			mLastGetBuffer = now;

		// Something cocked up and game didn't poll usb over 5 secs
		if (dur > 5000)
			ResetBuffers();

		mOutSamples += len;

		// init time point
		if (mLastOut.time_since_epoch().count() == 0)
			mLastOut = now;

		auto diff = std::chrono::duration_cast<us>(now-mLastOut).count();

		if (diff >= int64_t(1e6))
		{
			mTimeAdjust = (mOutSamples / (diff / 1e6)) / mOutputSamplesPerSec;
			//if(mTimeAdjust > 1.0) mTimeAdjust = 1.0; //If game is in 'turbo mode', just return zero samples or...?
			OSDebugOut("timespan: %" PRId64 " sampling: %f adjust: %f\n", diff, float(mOutSamples) / diff * 1e6, mTimeAdjust);
			mLastOut = now;
			mOutSamples = 0;
		}

		std::lock_guard<std::mutex> lk(mMutex);
		uint32_t totalLen = MIN(len * mSSpec.channels, mResampledBuffer.size());
		OSDebugOut("Resampled buffer size: %zd, copy: %zd\n", mResampledBuffer.size(), totalLen);
		if (totalLen > 0)
		{
			memcpy(buff, &mResampledBuffer[0], sizeof(short) * totalLen);
			mResampledBuffer.erase(mResampledBuffer.begin(), mResampledBuffer.begin() + totalLen);
		}

		return totalLen / mSSpec.channels;
	}

	bool GetFrames(uint32_t *size)
	{
		std::lock_guard<std::mutex> lk(mMutex);
		*size = mResampledBuffer.size() / mSSpec.channels;
		return true;
	}

	void SetResampling(int samplerate)
	{
		mOutputSamplesPerSec = samplerate;
		mResampleRatio = double(samplerate) / double(mSSpec.rate);
		//mResample = true;
	}

	uint32_t GetChannels()
	{
		return mSSpec.channels;
	}

	void Start()
	{
		ResetBuffers();
		mPaused = false;
	}

	void Stop() { mPaused = true; }

	virtual MicMode GetMicMode(AudioSource* compare)
	{
		if(compare && typeid(compare) != typeid(this))
			return MIC_MODE_SEPARATE; //atleast, if not single altogether

		if (compare)
		{
			PulseAudioSource *src = dynamic_cast<PulseAudioSource *>(compare);
			if (src && mDevice == src->mDevice)
				return MIC_MODE_SHARED;
			return MIC_MODE_SEPARATE;
		}

		CONFIGVARIANT var(mMic ? N_AUDIO_DEVICE0 : N_AUDIO_DEVICE1, CONFIG_TYPE_CHAR);
		if(LoadSetting(mPort, APINAME, var) && var.strValue == mDevice)
			return MIC_MODE_SHARED;

		return MIC_MODE_SINGLE;
	}

	void Uninit()
	{
		int ret;
		if (mStream) {
			ret = pf_pa_stream_disconnect(mStream);
			pf_pa_stream_unref(mStream); //needed?
			mStream = nullptr;
		}
		if (mPContext) {
			pf_pa_context_disconnect(mPContext);
			pf_pa_context_unref(mPContext);
			mPContext = nullptr;
		}
		if (mPMainLoop) {
			pf_pa_threaded_mainloop_stop(mPMainLoop);
			pf_pa_threaded_mainloop_free(mPMainLoop);
			mPMainLoop = nullptr;
		}
	}

	bool Init()
	{
		int ret = 0;

		mPMainLoop = pf_pa_threaded_mainloop_new();
		pa_mainloop_api *mlapi = pf_pa_threaded_mainloop_get_api(mPMainLoop);

		mPContext = pf_pa_context_new (mlapi, "USBqemu-pulse");

		ret = pf_pa_context_connect (mPContext,
			mServer,
			PA_CONTEXT_NOFLAGS,
			NULL
		);

		OSDebugOut("pa_context_connect %s\n", pf_pa_strerror(ret));
		if (ret != PA_OK)
			goto error;

		pf_pa_context_set_state_callback(mPContext,
			pa_context_state_cb,
			&mPAready
		);

		pf_pa_threaded_mainloop_start(mPMainLoop);

		// wait for pa_context_state_cb
		for(;;)
		{
			if(mPAready == 1) break;
			if(mPAready == 2 || mQuit) goto error;
		}

		mStream = pf_pa_stream_new(mPContext,
			"USBqemu-pulse",
			&mSSpec,
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
		buffer_attr.fragsize = pf_pa_usec_to_bytes(mBuffering * 1000, &mSSpec);
		OSDebugOut("usec_to_bytes %zu\n", buffer_attr.fragsize);

		ret = pf_pa_stream_connect_record(mStream,
			mDevice.c_str(),
			&buffer_attr,
			PA_STREAM_ADJUST_LATENCY
		);

		OSDebugOut("pa_stream_connect_record %s\n", pf_pa_strerror(ret));
		if (ret != PA_OK)
			goto error;

		// Setup resampler
		if (mResampler)
			mResampler = src_delete(mResampler);

		mResampler = src_new(SRC_SINC_FASTEST, mSSpec.channels, &ret);
		if (!mResampler)
		{
			OSDebugOut("Failed to create resampler: error %08lX\n", ret);
			goto error;
		}

		mLastGetBuffer = hrc::now();
		return true;
	error:
		Uninit();
		return false;
	}

	void ResetBuffers()
	{
		std::lock_guard<std::mutex> lk(mMutex);
		pa_sample_spec ss(mSSpec);
		ss.rate = mOutputSamplesPerSec;

		size_t bytes = pf_pa_bytes_per_second(&mSSpec) * 5;
		mQBuffer.resize(0);
		mQBuffer.reserve(bytes);

		bytes = pf_pa_bytes_per_second(&ss) * 5;
		mResampledBuffer.resize(0);
		mResampledBuffer.reserve(bytes);
		src_reset(mResampler);
	}

	static const TCHAR* Name()
	{
		return "PulseAudio";
	}

	static int Configure(int port, void *data)
	{
		return RESULT_CANCELED;
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

	static void stream_read_cb (pa_stream *p, size_t nbytes, void *userdata);

protected:
	int mPort;
	int mMic;
	int mChannels;
	int mBuffering;
	std::string mDevice;
	int mOutputSamplesPerSec;
	pa_sample_spec mSSpec;

	SRC_STATE *mResampler;
	double mResampleRatio;
	// Speed up or slow down audio
	double mTimeAdjust;
	std::vector<short> mResampledBuffer;
	std::vector<float> mQBuffer;
	//std::thread mThread;
	//std::condition_variable mEvent;
	std::mutex mMutex;
	bool mQuit;
	bool mPaused;
	hrc::time_point mLastGetBuffer;

	int mPAready;
	pa_threaded_mainloop *mPMainLoop;
	pa_context *mPContext;
	pa_stream  *mStream;
	char *mServer; //TODO add server selector?

	int mOutSamples;
	hrc::time_point mLastOut;
};

void PulseAudioSource::stream_read_cb (pa_stream *p, size_t nbytes, void *userdata)
{
	PulseAudioSource *src = (PulseAudioSource *) userdata;
	const void* padata = NULL;
	if (src->mQuit)
		return;

	OSDebugOut("stream_read_callback %d bytes\n", nbytes);

	int ret = pf_pa_stream_peek(p, &padata, &nbytes);
	OSDebugOut("pa_stream_peek %zu %s\n", nbytes, pf_pa_strerror(ret));

	if (ret != PA_OK)
		return;

	auto dur = std::chrono::duration_cast<ms>(hrc::now() - src->mLastGetBuffer).count();
	if (src->mPaused || dur > 50000) {
		ret = pf_pa_stream_drop(p);
		if (ret != PA_OK)
			OSDebugOut("pa_stream_drop %s\n", pf_pa_strerror(ret));
		return;
	}

	{
		size_t old_size = src->mQBuffer.size();
		size_t nfloats = nbytes / sizeof(float);
		src->mQBuffer.resize(old_size + nfloats);
		memcpy(&src->mQBuffer[old_size], padata, nbytes);
		//if copy succeeded, drop samples at pulse's side
		ret = pf_pa_stream_drop(p);
		if (ret != PA_OK)
			OSDebugOut("pa_stream_drop %s\n", pf_pa_strerror(ret));
	}

	size_t resampled = static_cast<size_t>(src->mQBuffer.size() * src->mResampleRatio * src->mTimeAdjust);// * src->mSSpec.channels;
	if (resampled == 0)
		resampled = src->mQBuffer.size();

	std::vector<float> rebuf(resampled);

	SRC_DATA data;
	memset(&data, 0, sizeof(SRC_DATA));
	data.data_in = &src->mQBuffer[0];
	data.input_frames = src->mQBuffer.size() / src->mSSpec.channels;
	data.data_out = &rebuf[0];
	data.output_frames = resampled / src->mSSpec.channels;
	data.src_ratio = src->mResampleRatio * src->mTimeAdjust;

	src_process(src->mResampler, &data);

	std::lock_guard<std::mutex> lock(src->mMutex);

	uint32_t len = data.output_frames_gen * src->mSSpec.channels;
	size_t size = src->mResampledBuffer.size();
	if (len > 0)
	{
		//too long, drop samples, caused by saving/loading savestates and random stutters
		int sizeInMS = (((src->mResampledBuffer.size() + len) * 1000 / src->mSSpec.channels) / src->mOutputSamplesPerSec);
		int threshold = src->mBuffering > 25 ? src->mBuffering : 25;
		if (sizeInMS > threshold)
		{
			size = 0;
			src->mResampledBuffer.resize(len);
		}
		else
			src->mResampledBuffer.resize(size + len);
		src_float_to_short_array(&rebuf[0], &(src->mResampledBuffer[size]), len);
	}

//#if _DEBUG
//	if (file && len)
//		fwrite(&(src->mResampledBuffer[size]), sizeof(short), len, file);
//#endif

	auto remSize = data.input_frames_used * src->mSSpec.channels;
	src->mQBuffer.erase(src->mQBuffer.begin(), src->mQBuffer.begin() + remSize);

	OSDebugOut("Resampler: in %d out %d used %d gen %d, rb: %zd, qb: %zd\n",
		data.input_frames, data.output_frames,
		data.input_frames_used, data.output_frames_gen,
		src->mResampledBuffer.size(), src->mQBuffer.size());
}

REGISTER_AUDIOSRC(APINAME, PulseAudioSource);
#undef APINAME

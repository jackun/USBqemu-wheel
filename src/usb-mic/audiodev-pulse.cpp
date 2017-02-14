#include <cstdint>
#include <cstring>
#include "../osdebugout.h"
#include "audiodeviceproxy.h"
#include "../libsamplerate/samplerate.h"
#include <typeinfo>
//#include <thread>
#include <mutex>
#include <chrono>
#include <gtk/gtk.h>

#ifndef DYNLINK_PULSE
#include <pulse/pulseaudio.h>
#else
#include "../dynlink/pulse.h"
#endif

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

	state = pa_context_get_state(c);
	OSDebugOut("pa_context_get_state() %d\n", state);
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

static void pa_sinklist_cb(pa_context *c, const pa_sink_info *l, int eol, void *userdata)
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

static int pa_get_devicelist(AudioDeviceInfoList& list, AudioDir dir)
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

	OSDebugOut("pa_get_devicelist\n");
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
				if (dir == AUDIODIR_SOURCE)
					pa_op = pa_context_get_source_info_list(pa_ctx,
						pa_sourcelist_cb,
						&list);
				else
					pa_op = pa_context_get_sink_info_list(pa_ctx,
						pa_sinklist_cb,
						&list);
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

// GTK+ config. dialog stuff
GtkWidget *new_combobox(const char* label, GtkWidget *vbox); // src/linux/config-gtk.cpp
static void populateDeviceWidget(GtkComboBox *widget, const std::string& devName, const AudioDeviceInfoList& devs)
{
	gtk_list_store_clear (GTK_LIST_STORE (gtk_combo_box_get_model (widget)));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), "None");
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);

	int i = 1;
	for (auto& dev: devs)
	{
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), dev.strName.c_str());
		if (!devName.empty() && devName == dev.strID)
			gtk_combo_box_set_active (GTK_COMBO_BOX (widget), i);
		i++;
	}
}

static void deviceChanged (GtkComboBox *widget, gpointer data)
{
	*(int*) data = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
}

static int GtkConfigure(int port, void *data)
{
	GtkWidget *ro_frame, *ro_label, *rs_hbox, *rs_label, *rs_cb, *vbox;

	int dev_idxs[] = {0, 0, 0, 0};

	AudioDeviceInfoList srcDevs;
	if (pa_get_devicelist(srcDevs, AUDIODIR_SOURCE) != 0)
	{
		OSDebugOut("pa_get_devicelist failed\n");
		return RESULT_FAILED;
	}

	AudioDeviceInfoList sinkDevs;
	if (pa_get_devicelist(sinkDevs, AUDIODIR_SINK) != 0)
	{
		OSDebugOut("pa_get_devicelist failed\n");
		return RESULT_FAILED;
	}

	GtkWidget *dlg = gtk_dialog_new_with_buttons (
		"PulseAudio Settings", GTK_WINDOW (data), GTK_DIALOG_MODAL,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OK, GTK_RESPONSE_OK,
		NULL);
	gtk_window_set_position (GTK_WINDOW (dlg), GTK_WIN_POS_CENTER);
	gtk_window_set_resizable (GTK_WINDOW (dlg), TRUE);
	GtkWidget *dlg_area_box = gtk_dialog_get_content_area (GTK_DIALOG (dlg));

	ro_frame = gtk_frame_new (NULL);
	gtk_box_pack_start (GTK_BOX (dlg_area_box), ro_frame, TRUE, FALSE, 5);

	GtkWidget *main_vbox = gtk_vbox_new (FALSE, 5);
	gtk_container_add (GTK_CONTAINER (ro_frame), main_vbox);

	const char* labels[] = {"Source 1", "Source 2", "Sink 1", "Sink 2"};
	for (int i=0; i<2; i++)
	{
		std::string devName;
		CONFIGVARIANT var(i ? N_AUDIO_SOURCE1 : N_AUDIO_SOURCE0, CONFIG_TYPE_CHAR);
		if (LoadSetting(port, APINAME, var))
			devName = var.strValue;

		GtkWidget *cb = new_combobox(labels[i], main_vbox);
		g_signal_connect (G_OBJECT (cb), "changed", G_CALLBACK (deviceChanged), (gpointer)&dev_idxs[i]);
		populateDeviceWidget (GTK_COMBO_BOX (cb), devName, srcDevs);
	}

	for (int i=2; i<4; i++)
	{
		std::string devName;
		CONFIGVARIANT var(i-2 ? N_AUDIO_SINK1 : N_AUDIO_SINK0, CONFIG_TYPE_CHAR);
		if (LoadSetting(port, APINAME, var))
			devName = var.strValue;

		GtkWidget *cb = new_combobox(labels[i], main_vbox);
		g_signal_connect (G_OBJECT (cb), "changed", G_CALLBACK (deviceChanged), (gpointer)&dev_idxs[i]);
		populateDeviceWidget (GTK_COMBO_BOX (cb), devName, sinkDevs);
	}

	gtk_widget_show_all (dlg);
	gint result = gtk_dialog_run (GTK_DIALOG (dlg));

	gtk_widget_destroy (dlg);

	// Wait for all gtk events to be consumed ...
	while (gtk_events_pending ())
		gtk_main_iteration_do (FALSE);

	if (result == GTK_RESPONSE_OK)
	{
		for (int i=0; i<2; i++)
		{
			int idx = dev_idxs[i];
			{
				CONFIGVARIANT var(i ? N_AUDIO_SOURCE1 : N_AUDIO_SOURCE0, "");

				if (idx > 0)
					var.strValue = srcDevs[idx - 1].strID;

				if (!SaveSetting(port, APINAME, var))
						return RESULT_FAILED;
			}

			idx = dev_idxs[i+2];
			{
				CONFIGVARIANT var(i ? N_AUDIO_SINK1 : N_AUDIO_SINK0, "");

				if (idx > 0)
					var.strValue = sinkDevs[idx - 1].strID;

				if (!SaveSetting(port, APINAME, var))
						return RESULT_FAILED;
			}
		}
		return RESULT_OK;
	}

	return RESULT_CANCELED;
}

class PulseAudioDevice : public AudioDevice
{
public:
	PulseAudioDevice(int port, int device, AudioDir dir): mPort(port)
	, mDevice(device)
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
	, mAudioDir(dir)
	{
		CONFIGVARIANT var(device ? N_AUDIO_SOURCE1 : N_AUDIO_SOURCE0, CONFIG_TYPE_CHAR);
		if(LoadSetting(mPort, APINAME, var) && !var.strValue.empty())
		{
			mDeviceName = var.strValue;
			//TODO open device etc.
		}
		else
			throw AudioDeviceError(APINAME ": failed to load device settings");

		{
			CONFIGVARIANT var(N_BUFFER_LEN, CONFIG_TYPE_INT);
			if(LoadSetting(mPort, APINAME, var))
				mBuffering = var.intValue;
		}

		if (!AudioInit())
			throw AudioDeviceError(APINAME ": failed to bind pulseaudio library");

		mSSpec.format =  PA_SAMPLE_FLOAT32LE; //PA_SAMPLE_S16LE;
		mSSpec.channels = 2;
		mSSpec.rate = 48000;

		if (!Init())
			throw AudioDeviceError(APINAME ": failed to init");
	}

	~PulseAudioDevice()
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
			int ret = pa_context_connect (mPContext,
				mServer,
				PA_CONTEXT_NOFLAGS,
				NULL
			);

			OSDebugOut("pa_context_connect %s\n", pa_strerror(ret));
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

	uint32_t SetBuffer(int16_t *buff, uint32_t len)
	{
		return len;
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
		ResetBuffers();
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

	virtual MicMode GetMicMode(AudioDevice* compare)
	{
		if(compare && typeid(compare) != typeid(this))
			return MIC_MODE_SEPARATE; //atleast, if not single altogether

		if (compare)
		{
			PulseAudioDevice *src = dynamic_cast<PulseAudioDevice *>(compare);
			if (src && mDeviceName == src->mDeviceName)
				return MIC_MODE_SHARED;
			return MIC_MODE_SEPARATE;
		}

		CONFIGVARIANT var(mDevice ? N_AUDIO_SOURCE0 : N_AUDIO_SOURCE1, CONFIG_TYPE_CHAR);
		if(LoadSetting(mPort, APINAME, var) && var.strValue == mDeviceName)
			return MIC_MODE_SHARED;

		return MIC_MODE_SINGLE;
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
		int ret = 0;

		mPMainLoop = pa_threaded_mainloop_new();
		pa_mainloop_api *mlapi = pa_threaded_mainloop_get_api(mPMainLoop);

		mPContext = pa_context_new (mlapi, "USBqemu-pulse");

		ret = pa_context_connect (mPContext,
			mServer,
			PA_CONTEXT_NOFLAGS,
			NULL
		);

		OSDebugOut("pa_context_connect %s\n", pa_strerror(ret));
		if (ret != PA_OK)
			goto error;

		pa_context_set_state_callback(mPContext,
			pa_context_state_cb,
			&mPAready
		);

		pa_threaded_mainloop_start(mPMainLoop);

		// wait for pa_context_state_cb
		for(;;)
		{
			if(mPAready == 1) break;
			if(mPAready == 2 || mQuit) goto error;
		}

		mStream = pa_stream_new(mPContext,
			"USBqemu-pulse",
			&mSSpec,
			NULL
		);

		// Sets individual read callback fragsize but recording itself
		// still "lags" ~1sec (read_cb is called in bursts) without
		// PA_STREAM_ADJUST_LATENCY
		pa_buffer_attr buffer_attr;
		buffer_attr.maxlength = (uint32_t) -1;
		buffer_attr.tlength = (uint32_t) -1;
		buffer_attr.prebuf = (uint32_t) -1;
		buffer_attr.minreq = (uint32_t) -1;
		buffer_attr.fragsize = pa_usec_to_bytes(mBuffering * 1000, &mSSpec);
		OSDebugOut("usec_to_bytes %zu\n", buffer_attr.fragsize);

		if (mAudioDir == AUDIODIR_SOURCE)
		{
			pa_stream_set_read_callback(mStream,
				stream_read_cb,
				this
			);

			ret = pa_stream_connect_record(mStream,
				mDeviceName.c_str(),
				&buffer_attr,
				PA_STREAM_ADJUST_LATENCY
			);
			OSDebugOut("pa_stream_connect_record %s\n", pa_strerror(ret));
		}
		else
		{
			pa_stream_set_write_callback(mStream,
				stream_write_cb,
				this
			);

			ret = pa_stream_connect_playback(mStream,
				mDeviceName.c_str(),
				&buffer_attr,
				PA_STREAM_ADJUST_LATENCY,
				nullptr,
				nullptr
			);
			OSDebugOut("pa_stream_connect_playback %s\n", pa_strerror(ret));
		}

		if (ret != PA_OK)
			goto error;

		// Setup resampler
		mResampler = src_delete(mResampler);

		mResampler = src_new(SRC_SINC_FASTEST, mSSpec.channels, &ret);
		if (!mResampler)
		{
			OSDebugOut("Failed to create resampler: error %08X\n", ret);
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

		size_t bytes = pa_bytes_per_second(&mSSpec) * 5;
		mQBuffer.resize(0);
		mQBuffer.reserve(bytes);

		bytes = pa_bytes_per_second(&ss) * 5;
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
		int ret = RESULT_FAILED;
		if (PulseAudioDevice::AudioInit())
		{
			ret = GtkConfigure(port, data);
			PulseAudioDevice::AudioDeinit();
		}
		return ret;
	}

	static void AudioDevices(std::vector<AudioDeviceInfo> &devices)
	{
		pa_get_devicelist(devices, AUDIODIR_SOURCE);
	}

	static bool AudioInit()
	{
#ifdef DYNLINK_PULSE
		return DynLoadPulse();
#else
		return true;
#endif
	}
	static void AudioDeinit()
	{
#ifdef DYNLINK_PULSE
		DynUnloadPulse();
#endif
	}
	static std::vector<CONFIGVARIANT> GetSettings()
	{
		return std::vector<CONFIGVARIANT>();
	}

	static void stream_read_cb (pa_stream *p, size_t nbytes, void *userdata);
	static void stream_write_cb (pa_stream *p, size_t nbytes, void *userdata);

protected:
	int mPort;
	int mDevice;
	int mChannels;
	int mBuffering;
	std::string mDeviceName;
	int mOutputSamplesPerSec;
	pa_sample_spec mSSpec;
	AudioDir mAudioDir;

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

void PulseAudioDevice::stream_read_cb (pa_stream *p, size_t nbytes, void *userdata)
{
	PulseAudioDevice *src = (PulseAudioDevice *) userdata;
	const void* padata = NULL;
	if (src->mQuit)
		return;

	//OSDebugOut("stream_read_callback %d bytes\n", nbytes);

	int ret = pa_stream_peek(p, &padata, &nbytes);
	//OSDebugOut("pa_stream_peek %zu %s\n", nbytes, pa_strerror(ret));

	if (ret != PA_OK)
		return;

	auto dur = std::chrono::duration_cast<ms>(hrc::now() - src->mLastGetBuffer).count();
	if (src->mPaused || dur > 5000) {
		ret = pa_stream_drop(p);
		if (ret != PA_OK)
			OSDebugOut("pa_stream_drop %d: %s\n", ret, pa_strerror(ret));
		return;
	}

	{
		size_t old_size = src->mQBuffer.size();
		size_t nfloats = nbytes / sizeof(float);
		src->mQBuffer.resize(old_size + nfloats);
		memcpy(&src->mQBuffer[old_size], padata, nbytes);
		//if copy succeeded, drop samples at pulse's side
		ret = pa_stream_drop(p);
		if (ret != PA_OK)
			OSDebugOut("pa_stream_drop %s\n", pa_strerror(ret));
	}

	size_t resampled = static_cast<size_t>(src->mQBuffer.size() * src->mResampleRatio * src->mTimeAdjust);// * src->mSSpec.channels;
	if (resampled == 0)
		resampled = src->mQBuffer.size();

	std::vector<float> rebuf(resampled);

	SRC_DATA data;
	memset(&data, 0, sizeof(SRC_DATA));
	data.data_in = src->mQBuffer.data();
	data.input_frames = src->mQBuffer.size() / src->mSSpec.channels;
	data.data_out = rebuf.data();
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
		src_float_to_short_array(rebuf.data(), &(src->mResampledBuffer[size]), len);
	}

//#if _DEBUG
//	if (file && len)
//		fwrite(&(src->mResampledBuffer[size]), sizeof(short), len, file);
//#endif

	auto remSize = data.input_frames_used * src->mSSpec.channels;
	src->mQBuffer.erase(src->mQBuffer.begin(), src->mQBuffer.begin() + remSize);

	//OSDebugOut("Resampler: in %ld out %ld used %ld gen %ld, rb: %zd, qb: %zd\n",
		//data.input_frames, data.output_frames,
		//data.input_frames_used, data.output_frames_gen,
		//src->mResampledBuffer.size(), src->mQBuffer.size());
}

void PulseAudioDevice::stream_write_cb (pa_stream *p, size_t nbytes, void *userdata)
{
}

REGISTER_AUDIODEV(APINAME, PulseAudioDevice);
#undef APINAME

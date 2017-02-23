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
#include <pulse/pulseaudio.h>

#ifdef DYNLINK_PULSE
#include "../dynlink/pulse.h"
#endif

using hrc = std::chrono::high_resolution_clock;
using ms = std::chrono::milliseconds;
using us = std::chrono::microseconds;
using ns = std::chrono::nanoseconds;
using sec = std::chrono::seconds;

GtkWidget *new_combobox(const char* label, GtkWidget *vbox); // src/linux/config-gtk.cpp

#define APINAME "pulse"

namespace audiodev_pulse {

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
	, mSamplesPerSec(48000)
	, mResampler(nullptr)
	, mOutSamples(0)
	, mAudioDir(dir)
	{
		int i = dir == AUDIODIR_SOURCE ? 0 : 2;
		const char* var_names[] = {
			N_AUDIO_SOURCE0,
			N_AUDIO_SOURCE1,
			N_AUDIO_SINK0,
			N_AUDIO_SINK1
		};

		CONFIGVARIANT var(device ? var_names[i + 1] : var_names[i], CONFIG_TYPE_CHAR);
		if(LoadSetting(mPort, APINAME, var) && !var.strValue.empty())
		{
			mDeviceName = var.strValue;
		}
		else
			throw AudioDeviceError(APINAME ": failed to load device settings");

		{
			CONFIGVARIANT var(N_BUFFER_LEN, CONFIG_TYPE_INT);
			if(LoadSetting(mPort, APINAME, var))
				mBuffering = MAX(25, var.intValue);
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
		if (file) fclose(file);
	}

	uint32_t GetBuffer(int16_t *buff, uint32_t frames)
	{
		auto now = hrc::now();
		auto dur = std::chrono::duration_cast<ms>(now-mLastGetBuffer).count();

		// init time point
		if (mLastOut.time_since_epoch().count() == 0)
			mLastOut = now;

		//Disconnected, try reconnect after every 1sec, hopefully game retries to read samples
		if (mPAready == 3 && dur >= 1000)
		{
			mLastGetBuffer = now;
			int ret = pa_context_connect (mPContext,
				mServer,
				PA_CONTEXT_NOFLAGS,
				NULL
			);

			//TODO reconnect stream as well?

			OSDebugOut("pa_context_connect %s\n", pa_strerror(ret));
		}
		else
			mLastGetBuffer = now;

		//FIXME Can't use it like this. Some games only poll for data when needed.
		// Something cocked up and game didn't poll usb over 5 secs
		//if (dur > 5000)
		//	ResetBuffers();

		//auto diff = std::chrono::duration_cast<us>(now-mLastOut).count();
		//mOutSamples += frames;

		//if (diff >= int64_t(1e6))
		//{
			//mTimeAdjust = (mOutSamples / (diff / 1e6)) / mSamplesPerSec;
			////if(mTimeAdjust > 1.0) mTimeAdjust = 1.0; //If game is in 'turbo mode', just return zero samples or...?
			//OSDebugOut("timespan: %" PRId64 " sampling: %f adjust: %f\n", diff, float(mOutSamples) / diff * 1e6, mTimeAdjust);
			//mLastOut = now;
			//mOutSamples = 0;
		//}

		std::lock_guard<std::mutex> lk(mMutex);
		uint32_t totalFrames = MIN(frames * mSSpec.channels, mShortBuffer.size());//TODO double check, remove?
		//OSDebugOut("Resampled buffer size: %zd, sent: %zd\n", mShortBuffer.size(), totalFrames / mSSpec.channels);
		if (totalFrames > 0)
		{
			memcpy(buff, mShortBuffer.data(), sizeof(short) * totalFrames);
			mShortBuffer.erase(mShortBuffer.begin(), mShortBuffer.begin() + totalFrames);
		}

		return totalFrames / mSSpec.channels;
	}

	uint32_t SetBuffer(int16_t *buff, uint32_t frames)
	{
		auto now = hrc::now();
		auto dur = std::chrono::duration_cast<ms>(now-mLastGetBuffer).count();

		// init time point
		if (mLastOut.time_since_epoch().count() == 0)
			mLastOut = now;

		//Disconnected, try reconnect after every 1sec
		if (mPAready == 3 && dur >= 1000)
		{
			mLastGetBuffer = now;
			int ret = pa_context_connect (mPContext,
				mServer,
				PA_CONTEXT_NOFLAGS,
				NULL
			);

			//TODO reconnect stream as well?

			OSDebugOut("pa_context_connect %s\n", pa_strerror(ret));
			if (ret != PA_OK)
				return frames;
		}
		else
			mLastGetBuffer = now;

		std::lock_guard<std::mutex> lk(mMutex);
		size_t old_size = mShortBuffer.size();
		size_t nshort = frames * mSSpec.channels;
		mShortBuffer.resize(old_size + nshort);
		memcpy(mShortBuffer.data() + old_size, buff, nshort * sizeof(int16_t));

#if 0
		if (!file)
		{
			char name[1024] = { 0 };
			snprintf(name, sizeof(name), "headset_out_s16le_%dch_%dHz.raw", mSSpec.channels, mSSpec.rate);
			file = fopen(name, "wb");
		}

		if (file)
			fwrite(mShortBuffer.data() + old_size, 1, nshort * sizeof(int16_t), file);
#endif
		return frames;
	}

	bool GetFrames(uint32_t *size)
	{
		std::lock_guard<std::mutex> lk(mMutex);
		*size = mShortBuffer.size() / mSSpec.channels;
		return true;
	}

	void SetResampling(int samplerate)
	{
		mSamplesPerSec = samplerate;
		if (mAudioDir == AUDIODIR_SOURCE)
			mResampleRatio = double(samplerate) / double(mSSpec.rate);
		else
			mResampleRatio = double(mSSpec.rate) / double(samplerate);
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
		if (mStream)
		{
			pa_threaded_mainloop_lock(mPMainLoop);
			if (pa_stream_is_corked(mStream) > 0)
			{
				pa_operation *op = pa_stream_cork(mStream, 0, stream_success_cb, this);
				if (op)
					pa_operation_unref(op);
			}
			pa_threaded_mainloop_unlock(mPMainLoop);
		}
	}

	void Stop()
	{
		mPaused = true;
		if (mStream)
		{
			pa_threaded_mainloop_lock(mPMainLoop);
			if (!pa_stream_is_corked(mStream))
			{
				pa_operation *op = pa_stream_cork(mStream, 1, stream_success_cb, this);
				if (op)
					pa_operation_unref(op);
			}
			pa_threaded_mainloop_unlock(mPMainLoop);
		}
	}

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
		pa_operation* pa_op = nullptr;

		mPMainLoop = pa_threaded_mainloop_new();
		pa_mainloop_api *mlapi = pa_threaded_mainloop_get_api(mPMainLoop);

		mPContext = pa_context_new (mlapi, "USBqemu");

		pa_context_set_state_callback(mPContext,
			context_state_cb,
			this
		);

		// Lock the mainloop so that it does not run and crash before the context is ready
		pa_threaded_mainloop_lock(mPMainLoop);
		pa_threaded_mainloop_start(mPMainLoop);

		ret = pa_context_connect (mPContext,
			mServer,
			PA_CONTEXT_NOFLAGS,
			NULL
		);

		OSDebugOut("pa_context_connect %s\n", pa_strerror(ret));
		if (ret != PA_OK)
			goto error;

		// wait for pa_context_state_cb
		for(;;)
		{
			if(mPAready == 1) break;
			if(mPAready == 2 || mQuit) goto error;
			pa_threaded_mainloop_wait(mPMainLoop);
		}

		mStream = pa_stream_new(mPContext,
			"USBqemu-pulse",
			&mSSpec,
			NULL
		);

		if (!mStream)
			goto error;

		pa_stream_set_state_callback(mStream, stream_state_cb, this);

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

			buffer_attr.maxlength = pa_bytes_per_second(&mSSpec);
			buffer_attr.prebuf = 0; // Don't stop on underrun but then
									// stream also only starts manually with uncorking.
			buffer_attr.tlength = pa_usec_to_bytes(mBuffering * 1000, &mSSpec);
			pa_stream_flags_t flags = (pa_stream_flags_t)
				(PA_STREAM_INTERPOLATE_TIMING |
				PA_STREAM_NOT_MONOTONIC |
				PA_STREAM_AUTO_TIMING_UPDATE |
				//PA_STREAM_VARIABLE_RATE |
				PA_STREAM_ADJUST_LATENCY);

			ret = pa_stream_connect_playback(mStream,
				mDeviceName.c_str(),
				&buffer_attr,
				flags,
				nullptr,
				nullptr
			);
			OSDebugOut("pa_stream_connect_playback %s\n", pa_strerror(ret));
		}

		if (ret != PA_OK)
			goto error;

		// Wait for the stream to be ready
		for(;;) {
			pa_stream_state_t stream_state = pa_stream_get_state(mStream);
			assert(PA_STREAM_IS_GOOD(stream_state));
			if (stream_state == PA_STREAM_READY) break;
			if (stream_state == PA_STREAM_FAILED) goto error;
			pa_threaded_mainloop_wait(mPMainLoop);
		}

		OSDebugOut("pa_stream_is_corked %d\n", pa_stream_is_corked(mStream));
		OSDebugOut("pa_stream_is_suspended %d\n", pa_stream_is_suspended (mStream));
		pa_op = pa_stream_cork(mStream, 0, stream_success_cb, this);
		if (pa_op)
			pa_operation_unref(pa_op);

		pa_threaded_mainloop_unlock(mPMainLoop);

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
		pa_threaded_mainloop_unlock(mPMainLoop);
		Uninit();
		return false;
	}

	void ResetBuffers()
	{
		std::lock_guard<std::mutex> lk(mMutex);
		pa_sample_spec ss(mSSpec);
		ss.rate = mSamplesPerSec;

		size_t bytes = pa_bytes_per_second(&mSSpec) * 5;
		mFloatBuffer.resize(0);
		mFloatBuffer.reserve(bytes);

		bytes = pa_bytes_per_second(&ss) * 5;
		mShortBuffer.resize(0);
		mShortBuffer.reserve(bytes);
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

	static void context_state_cb(pa_context *c, void *userdata);
	static void stream_state_cb(pa_stream *s, void *userdata);
	static void stream_read_cb (pa_stream *p, size_t nbytes, void *userdata);
	static void stream_write_cb (pa_stream *p, size_t nbytes, void *userdata);
	static void stream_success_cb (pa_stream *p, int success, void *userdata) {}

protected:
	int mPort;
	int mDevice;
	int mChannels;
	int mBuffering;
	std::string mDeviceName;
	int mSamplesPerSec;
	pa_sample_spec mSSpec;
	AudioDir mAudioDir;

	SRC_STATE *mResampler;
	double mResampleRatio;
	// Speed up or slow down audio
	double mTimeAdjust;
	std::vector<short> mShortBuffer;
	std::vector<float> mFloatBuffer;
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
	FILE* file = nullptr;
};

void PulseAudioDevice::context_state_cb(pa_context *c, void *userdata)
{
	pa_context_state_t state;
	PulseAudioDevice *padev = (PulseAudioDevice *)userdata;

	state = pa_context_get_state(c);
	OSDebugOut("pa_context_get_state %d\n", state);
	switch (state) {
		case PA_CONTEXT_CONNECTING:
		case PA_CONTEXT_AUTHORIZING:
		case PA_CONTEXT_SETTING_NAME:
		default:
			break;
		case PA_CONTEXT_UNCONNECTED:
			padev->mPAready = 3;
			break;
		case PA_CONTEXT_FAILED:
		case PA_CONTEXT_TERMINATED:
			padev->mPAready = 2;
			break;
		case PA_CONTEXT_READY:
			padev->mPAready = 1;
			break;
	}

	pa_threaded_mainloop_signal(padev->mPMainLoop, 0);
}

void PulseAudioDevice::stream_state_cb(pa_stream *s, void *userdata)
{
	PulseAudioDevice *padev = (PulseAudioDevice *)userdata;
	pa_threaded_mainloop_signal(padev->mPMainLoop, 0);
}

void PulseAudioDevice::stream_read_cb (pa_stream *p, size_t nbytes, void *userdata)
{
	PulseAudioDevice *padev = (PulseAudioDevice *) userdata;
	const void* padata = NULL;
	if (padev->mQuit)
		return;

	//OSDebugOut("stream_read_callback %d bytes\n", nbytes);

	int ret = pa_stream_peek(p, &padata, &nbytes);
	//OSDebugOut("pa_stream_peek %zu %s\n", nbytes, pa_strerror(ret));

	if (ret != PA_OK)
		return;

	auto dur = std::chrono::duration_cast<ms>(hrc::now() - padev->mLastGetBuffer).count();
	if (padev->mPaused /*|| dur > 5000*/) {
		ret = pa_stream_drop(p);
		if (ret != PA_OK)
			OSDebugOut("pa_stream_drop %d: %s\n", ret, pa_strerror(ret));
		return;
	}

	{
		size_t old_size = padev->mFloatBuffer.size();
		size_t nfloats = nbytes / sizeof(float);
		padev->mFloatBuffer.resize(old_size + nfloats);
		memcpy(&padev->mFloatBuffer[old_size], padata, nbytes);
		//if copy succeeded, drop samples at pulse's side
		ret = pa_stream_drop(p);
		if (ret != PA_OK)
			OSDebugOut("pa_stream_drop %s\n", pa_strerror(ret));
	}

	size_t resampled = static_cast<size_t>(padev->mFloatBuffer.size() * padev->mResampleRatio * padev->mTimeAdjust);// * padev->mSSpec.channels;
	if (resampled == 0)
		resampled = padev->mFloatBuffer.size();

	std::vector<float> rebuf(resampled);

	SRC_DATA data;
	memset(&data, 0, sizeof(SRC_DATA));
	data.data_in = padev->mFloatBuffer.data();
	data.input_frames = padev->mFloatBuffer.size() / padev->mSSpec.channels;
	data.data_out = rebuf.data();
	data.output_frames = resampled / padev->mSSpec.channels;
	data.src_ratio = padev->mResampleRatio * padev->mTimeAdjust;

	src_process(padev->mResampler, &data);

	std::lock_guard<std::mutex> lock(padev->mMutex);

	uint32_t len = data.output_frames_gen * padev->mSSpec.channels;
	size_t size = padev->mShortBuffer.size();
	if (len > 0)
	{
		//too long, drop samples, caused by saving/loading savestates and random stutters
		int sizeInMS = (((padev->mShortBuffer.size() + len) * 1000 / padev->mSSpec.channels) / padev->mSamplesPerSec);
		int threshold = padev->mBuffering > 25 ? padev->mBuffering : 25;
		if (sizeInMS > threshold)
		{
			size = 0;
			padev->mShortBuffer.resize(len);
		}
		else
			padev->mShortBuffer.resize(size + len);
		src_float_to_short_array(rebuf.data(), &(padev->mShortBuffer[size]), len);
	}

	auto remSize = data.input_frames_used * padev->mSSpec.channels;
	if (remSize > 0)
		padev->mFloatBuffer.erase(padev->mFloatBuffer.begin(), padev->mFloatBuffer.begin() + remSize);

	//OSDebugOut("Resampler: in %ld out %ld used %ld gen %ld, rb: %zd, qb: %zd\n",
		//data.input_frames, data.output_frames,
		//data.input_frames_used, data.output_frames_gen,
		//padev->mShortBuffer.size(), padev->mFloatBuffer.size());
}

void PulseAudioDevice::stream_write_cb (pa_stream *p, size_t nbytes, void *userdata)
{
	void *pa_buffer = NULL;
	size_t pa_bytes, old_size;
	// The length of the data to write in bytes, must be in multiples of the stream's sample spec frame size
	ssize_t remaining_bytes = nbytes;
	size_t floats_written = 0;
	int ret = PA_OK;
	std::vector<float> float_samples;
	SRC_DATA data;
	memset(&data, 0, sizeof(SRC_DATA));

	PulseAudioDevice *padev = (PulseAudioDevice *) userdata;
	if (padev->mQuit)
		return;

	std::lock_guard<std::mutex> lock(padev->mMutex);

	size_t resampled = static_cast<size_t>(padev->mShortBuffer.size() * padev->mResampleRatio * padev->mTimeAdjust);
	if (resampled == 0)
		resampled = padev->mShortBuffer.size() * (padev->mResampleRatio > 1.0 ? padev->mResampleRatio : 1.0);

	old_size = padev->mFloatBuffer.size();
	padev->mFloatBuffer.resize(old_size + resampled - resampled % padev->mSSpec.channels);

	//OSDebugOut("buffer old size: %zu, new size: %zu resampled: %zu requested: %zu\n",
	//	old_size, padev->mFloatBuffer.size(), resampled, nbytes);

	// Convert short samples to float and to final output sample rate
	if (padev->mShortBuffer.size() > 0)
	{
		float_samples.resize(padev->mShortBuffer.size());
		src_short_to_float_array(padev->mShortBuffer.data(),
				float_samples.data(), padev->mShortBuffer.size());

		data.data_in = float_samples.data();
		data.input_frames = float_samples.size() / padev->mSSpec.channels;
		data.data_out = padev->mFloatBuffer.data() + old_size;
		data.output_frames = resampled / padev->mSSpec.channels;
		data.src_ratio = padev->mResampleRatio * padev->mTimeAdjust;

		src_process(padev->mResampler, &data);

		uint32_t new_len = data.output_frames_gen * padev->mSSpec.channels;
		padev->mFloatBuffer.resize(old_size + new_len);

		auto remSize = data.input_frames_used * padev->mSSpec.channels;
		if (remSize > 0)
			padev->mShortBuffer.erase(padev->mShortBuffer.begin(), padev->mShortBuffer.begin() + remSize);
	}

	// Write converted float samples or silence to PulseAudio stream
	while (remaining_bytes > 0)
	{
		pa_bytes = remaining_bytes;

		ret = pa_stream_begin_write(padev->mStream, &pa_buffer, &pa_bytes);
		if (ret != PA_OK)
		{
			OSDebugOut("pa_stream_begin_write %d: %s\n", ret, pa_strerror(ret));
			goto exit;
		}

		//OSDebugOut("offset %zu %zd %zu\n", floats_written, remaining_bytes, pa_bytes);

		size_t final_bytes = 0;
		if (padev->mFloatBuffer.size() > 0)
		{

			final_bytes = MIN(pa_bytes, padev->mFloatBuffer.size() * sizeof(float));
			memcpy(pa_buffer, padev->mFloatBuffer.data(), final_bytes);
			floats_written += final_bytes / sizeof(float);
#if 0
			if (!padev->file)
			{
				char name[1024] = { 0 };
				snprintf(name, sizeof(name), "headset_float32le_%dch_%dHz.raw", padev->mSSpec.channels, padev->mSSpec.rate);
				padev->file = fopen(name, "wb");
			}

			if (padev->file)
				fwrite(padev->mFloatBuffer.data(), 1, final_bytes, padev->file);
#endif
		}

		if (pa_bytes > final_bytes)
			memset((uint8_t*)pa_buffer + final_bytes, 0, pa_bytes - final_bytes);

		ret = pa_stream_write(padev->mStream, pa_buffer, pa_bytes, NULL, 0LL, PA_SEEK_RELATIVE);
		if (ret != PA_OK)
		{
			OSDebugOut("pa_stream_write %d: %s\n", ret, pa_strerror(ret));
			pa_stream_cancel_write(padev->mStream); //TODO needed?
			goto exit;
		}

		remaining_bytes -= pa_bytes;
	}

exit:
	//OSDebugOut("Resampler: in %ld out %ld used %ld gen %ld, rb: %zd, qb: %zd written: %zu\n",
		//data.input_frames, data.output_frames,
		//data.input_frames_used, data.output_frames_gen,
		//padev->mShortBuffer.size(), padev->mFloatBuffer.size(), floats_written * 4);

	if (floats_written > 0)
		padev->mFloatBuffer.erase(padev->mFloatBuffer.begin(), padev->mFloatBuffer.begin() + floats_written);
}

REGISTER_AUDIODEV(APINAME, PulseAudioDevice);
};
#undef APINAME

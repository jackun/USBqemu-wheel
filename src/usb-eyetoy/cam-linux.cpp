#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <fcntl.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <linux/videodev2.h>

#include "gtk.h"

#include "cam-linux.h"
#include "usb-eyetoy-webcam.h"
#include "jpgd/jpgd.h"
#include "jo_mpeg.h"

GtkWidget *new_combobox(const char* label, GtkWidget *vbox); // src/linux/config-gtk.cpp

#define CLEAR(x) memset(&(x), 0, sizeof(x))

namespace usb_eyetoy
{
namespace linux_api
{

static pthread_t _eyetoy_thread;
static pthread_t *eyetoy_thread = &_eyetoy_thread;
static unsigned char eyetoy_running = 0;

static int           fd = -1;
buffer_t             *buffers;
static unsigned int  n_buffers;
static int           out_buf;
static unsigned int  pixelformat;

buffer_t             mpeg_buffer;
std::mutex           mpeg_mutex;

static int xioctl(int fh, unsigned long int request, void *arg) {
	int r;
	do {
		r = ioctl(fh, request, arg);
	} while (-1 == r && EINTR == errno);
	return r;
}

static void store_mpeg_frame(unsigned char *data, unsigned int len) {
	mpeg_mutex.lock();
	memcpy(mpeg_buffer.start, data, len);
	mpeg_buffer.length = len;
	mpeg_mutex.unlock();
}

static void process_image(const unsigned char *ptr, int size) {
	if (pixelformat == V4L2_PIX_FMT_YUYV) {
		unsigned char *mpegData = (unsigned char*) calloc(1, 320 * 240 * 2);
		int mpegLen = jo_write_mpeg(mpegData, ptr, 320, 240, JO_YUYV, JO_FLIP_X, JO_NONE);
		store_mpeg_frame(mpegData, mpegLen);
		free(mpegData);
	} else if (pixelformat == V4L2_PIX_FMT_JPEG) {
		int width, height, actual_comps;
		unsigned char *rgbData = jpgd::decompress_jpeg_image_from_memory(ptr, size, &width, &height, &actual_comps, 3);
		unsigned char *mpegData = (unsigned char*) calloc(1, 320 * 240 * 2);
		int mpegLen = jo_write_mpeg(mpegData, rgbData, 320, 240, JO_RGB24, JO_FLIP_X, JO_NONE);
		free(rgbData);
		store_mpeg_frame(mpegData, mpegLen);
		free(mpegData);
	} else {
		fprintf(stderr, "unk format %c%c%c%c\n", pixelformat, pixelformat>>8, pixelformat>>16, pixelformat>>24);
	}
}

static int read_frame() {
	struct v4l2_buffer buf;
	CLEAR(buf);
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
		switch (errno) {
		case EAGAIN:
			return 0;

		case EIO:
		default:
			fprintf(stderr, "%s error %d, %s\n", "VIDIOC_DQBUF", errno, strerror(errno));
			return -1;
		}
	}

	assert(buf.index < n_buffers);

	process_image((const unsigned char*) buffers[buf.index].start, buf.bytesused);

	if (-1 == xioctl(fd, VIDIOC_QBUF, &buf)) {
		fprintf(stderr, "%s error %d, %s\n", "VIDIOC_QBUF", errno, strerror(errno));
		return -1;
	}

	return 1;
}

std::vector<std::string> getDevList() {
	std::vector<std::string> devList;
	char dev_name[64];
	int fd;
	struct v4l2_capability cap;

	for (int index = 0; index < 64; index++) {
		snprintf(dev_name, sizeof(dev_name), "/dev/video%d", index);

		if ((fd = open(dev_name, O_RDONLY)) < 0) {
			continue;
		}

		if(ioctl(fd, VIDIOC_QUERYCAP, &cap) >= 0) {
			devList.push_back((char*)cap.card);
		}

		close(fd);
	}
	return devList;
}

static int v4l_open(std::string selectedDevice) {
	char dev_name[64];
	struct v4l2_capability cap;

	fd = -1;
	for (int index = 0; index < 64; index++) {
		snprintf(dev_name, sizeof(dev_name), "/dev/video%d", index);

		if ((fd = open(dev_name, O_RDWR | O_NONBLOCK, 0)) < 0) {
			continue;
		}

		CLEAR(cap);
		if(ioctl(fd, VIDIOC_QUERYCAP, &cap) >= 0) {
			fprintf(stderr, "Camera: %s / %s\n", dev_name, (char*)cap.card);
			if (!selectedDevice.empty() && strcmp(selectedDevice.c_str(), (char*)cap.card) == 0) {
				goto cont;
			}
		}

		close(fd);
		fd = -1;
	}

	if (fd < 0) {
		snprintf(dev_name, sizeof(dev_name), "/dev/video0");
		fd = open(dev_name, O_RDWR | O_NONBLOCK, 0);
		if (-1 == fd) {
			fprintf(stderr, "Cannot open '%s': %d, %s\n", dev_name, errno, strerror(errno));
			return -1;
		}
	}

cont:

	CLEAR(cap);
	if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap)) {
		if (EINVAL == errno) {
			fprintf(stderr, "%s is no V4L2 device\n", dev_name);
			return -1;
		} else {
			fprintf(stderr, "%s error %d, %s\n", "VIDIOC_QUERYCAP", errno, strerror(errno));
			return -1;
		}
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		fprintf(stderr, "%s is no video capture device\n", dev_name);
		return -1;
	}

	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		fprintf(stderr, "%s does not support streaming i/o\n", dev_name);
		return -1;
	}

	struct v4l2_cropcap cropcap;
	CLEAR(cropcap);
	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (0 == xioctl(fd, VIDIOC_CROPCAP, &cropcap)) {
		struct v4l2_crop crop;
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect;

		if (-1 == xioctl(fd, VIDIOC_S_CROP, &crop)) {
			switch (errno) {
			case EINVAL:
				break;
			default:
				break;
			}
		}
	}

	struct v4l2_format fmt;
	CLEAR(fmt);
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width       = 320;
	fmt.fmt.pix.height      = 240;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;

	if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt)) {
		fprintf(stderr, "%s error %d, %s\n", "VIDIOC_S_FMT", errno, strerror(errno));
		return -1;
	}
	pixelformat = fmt.fmt.pix.pixelformat;
	fprintf(stderr, "VIDIOC_S_FMT res=%dx%d, fmt=%c%c%c%c\n", fmt.fmt.pix.width, fmt.fmt.pix.height,
		pixelformat, pixelformat>>8, pixelformat>>16, pixelformat>>24
	);

	struct v4l2_requestbuffers req;
	CLEAR(req);
	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			fprintf(stderr, "%s does not support memory mapping\n", dev_name);
			return -1;
		} else {
			fprintf(stderr, "%s error %d, %s\n", "VIDIOC_REQBUFS", errno, strerror(errno));
			return -1;
		}
	}

	if (req.count < 2) {
		fprintf(stderr, "Insufficient buffer memory on %s\n", dev_name);
		return -1;
	}

	buffers = (buffer_t*) calloc(req.count, sizeof(*buffers));

	if (!buffers) {
		fprintf(stderr, "Out of memory\n");
		return -1;
	}

	for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
		struct v4l2_buffer buf;

		CLEAR(buf);
		buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index  = n_buffers;

		if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf)) {
			fprintf(stderr, "%s error %d, %s\n", "VIDIOC_QUERYBUF", errno, strerror(errno));
			return -1;
		}

		buffers[n_buffers].length = buf.length;
		buffers[n_buffers].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);

		if (MAP_FAILED == buffers[n_buffers].start) {
			fprintf(stderr, "%s error %d, %s\n", "mmap", errno, strerror(errno));
			return -1;
		}
	}

	for (unsigned int i = 0; i < n_buffers; ++i) {
		struct v4l2_buffer buf;
		CLEAR(buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		if (-1 == xioctl(fd, VIDIOC_QBUF, &buf)) {
			fprintf(stderr, "%s error %d, %s\n", "VIDIOC_QBUF", errno, strerror(errno));
			return -1;
		}
	}

	enum v4l2_buf_type type;
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == xioctl(fd, VIDIOC_STREAMON, &type)) {
		fprintf(stderr, "%s error %d, %s\n", "VIDIOC_STREAMON", errno, strerror(errno));
		return -1;
	}
	return 0;
}


static void* v4l_thread(void *arg) {
	while(eyetoy_running) {
		for (;;) {
			fd_set fds;

			FD_ZERO(&fds);
			FD_SET(fd, &fds);

			struct timeval timeout = {2, 0}; // 2sec
			int ret = select(fd + 1, &fds, NULL, NULL, &timeout);

			if (ret < 0) {
				if (errno == EINTR)
					continue;
				fprintf(stderr, "%s error %d, %s\n", "select", errno, strerror(errno));
				break;
			}

			if (ret == 0) {
				fprintf(stderr, "select timeout\n");
				break;
			}

			if (read_frame())
				break;
		}
	}
	return NULL;
}

static int v4l_close() {
	enum v4l2_buf_type type;
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type)) {
		fprintf(stderr, "%s error %d, %s\n", "VIDIOC_STREAMOFF", errno, strerror(errno));
		return -1;
	}

	for (unsigned int i = 0; i < n_buffers; ++i) {
		if (-1 == munmap(buffers[i].start, buffers[i].length)) {
			fprintf(stderr, "%s error %d, %s\n", "munmap", errno, strerror(errno));
			return -1;
		}
	}
	free(buffers);

	if (-1 == close(fd)) {
		fprintf(stderr, "%s error %d, %s\n", "close", errno, strerror(errno));
		return -1;
	}
	fd = -1;
	return 0;
}

void create_dummy_frame() {
	const int width = 320;
	const int height = 240;
	const int bytesPerPixel = 3;

	unsigned char *rgbData = (unsigned char*) calloc(1, width * height * bytesPerPixel);
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			unsigned char *ptr = rgbData + (y*width+x) * bytesPerPixel;
			ptr[0] = 255-y;
			ptr[1] = y;
			ptr[2] = 255-y;
		}
	}
	unsigned char *mpegData = (unsigned char*) calloc(1, width * height * bytesPerPixel);
	int mpegLen = jo_write_mpeg(mpegData, rgbData, width, height, JO_RGB24, JO_NONE, JO_NONE);
	free(rgbData);

	store_mpeg_frame(mpegData, mpegLen);
	free(mpegData);
}

int V4L2::Open() {
	mpeg_buffer.start = calloc(1, 320 * 240 * 2);
	create_dummy_frame();
	if (eyetoy_running) {
		eyetoy_running = 0;
		pthread_join(*eyetoy_thread, NULL);
		v4l_close();
	}
	eyetoy_running = 1;
	std::string selectedDevice;
	LoadSetting(EyeToyWebCamDevice::TypeName(), mPort, APINAME, N_DEVICE, selectedDevice);
	if (v4l_open(selectedDevice) != 0)
		return -1;
	pthread_create(eyetoy_thread, NULL, &v4l_thread, NULL);
	return 0;
};

int V4L2::Close() {
	if (eyetoy_running) {
		eyetoy_running = 0;
		pthread_join(*eyetoy_thread, NULL);
		v4l_close();
	}
	return 0;
};

int V4L2::GetImage(uint8_t *buf, int len) {
	mpeg_mutex.lock();
	int len2 = mpeg_buffer.length;
	if (len < mpeg_buffer.length) len2 = len;
	memcpy(buf, mpeg_buffer.start, len2);
	mpeg_mutex.unlock();
	return len2;
};

static void deviceChanged(GtkComboBox *widget, gpointer data) {
	*(int*) data = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
}

int GtkConfigure(int port, const char* dev_type, void *data) {
	GtkWidget *ro_frame, *ro_label, *rs_hbox, *rs_label;

	std::string selectedDevice;
	LoadSetting(dev_type, port, APINAME, N_DEVICE, selectedDevice);

	GtkWidget *dlg = gtk_dialog_new_with_buttons(
		"V4L2 Settings", GTK_WINDOW(data), GTK_DIALOG_MODAL,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OK, GTK_RESPONSE_OK,
		NULL);
	gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER);
	gtk_window_set_resizable(GTK_WINDOW(dlg), TRUE);
	gtk_window_set_default_size(GTK_WINDOW(dlg), 320, 75);

	GtkWidget *dlg_area_box = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
	GtkWidget *main_hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(dlg_area_box), main_hbox);
	GtkWidget *right_vbox = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(main_hbox), right_vbox, TRUE, TRUE, 5);

	GtkWidget *rs_cb = new_combobox("Device:", right_vbox);

	std::vector<std::string> devList = getDevList();
	int sel_idx = 0;
	for (auto idx = 0; idx < devList.size(); idx++) {
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(rs_cb), devList.at(idx).c_str());
		if (!selectedDevice.empty() && selectedDevice == devList.at(idx)) {
			gtk_combo_box_set_active(GTK_COMBO_BOX(rs_cb), idx);
			sel_idx = idx;
		}
	}

	int sel_new;
	g_signal_connect(G_OBJECT(rs_cb), "changed", G_CALLBACK(deviceChanged), (gpointer)&sel_new);

	gtk_widget_show_all(dlg);
	gint result = gtk_dialog_run(GTK_DIALOG(dlg));

	int ret = RESULT_OK;
	if (result == GTK_RESPONSE_OK) {
		if (sel_new != sel_idx) {
			if (!SaveSetting(dev_type, port, APINAME, N_DEVICE, devList.at(sel_new))) {
				ret = RESULT_FAILED;
			}
		}
	} else {
		ret = RESULT_CANCELED;
	}

	gtk_widget_destroy(dlg);
	return ret;
}

int V4L2::Configure(int port, const char *dev_type, void *data) {
	return GtkConfigure(port, dev_type, data);
};

} // namespace linux_api
} // namespace usb_eyetoy

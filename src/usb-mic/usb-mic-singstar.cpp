/*
 * QEMU USB HID devices
 * 
 * Copyright (c) 2005 Fabrice Bellard
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

// Most stuff is based on Qemu 1.7 USB soundcard passthrough code.

#include "../USB.h"
#include "../qemu-usb/vl.h"
#include <assert.h>

/* HID interface requests */
#define GET_REPORT   0xa101
#define GET_IDLE     0xa102
#define GET_PROTOCOL 0xa103
#define SET_IDLE     0x210a
#define SET_PROTOCOL 0x210b

#define USB_MOUSE  1
#define USB_TABLET 2

//#include "type.h"

#include "usb.h"
#include "audio.h"
#include "mic-audiodefs.h"
//#include "usbcfg.h"
//#include "usbdesc.h"

/*
 * A Basic Audio Device uses these specific values
 */
#define USBAUDIO_PACKET_SIZE     200 //192
#define USBAUDIO_SAMPLE_RATE     48000
#define USBAUDIO_PACKET_INTERVAL 1

/*
 * A USB audio device supports an arbitrary number of alternate
 * interface settings for each interface.  Each corresponds to a block
 * diagram of parameterized blocks.  This can thus refer to things like
 * number of channels, data rates, or in fact completely different
 * block diagrams.  Alternative setting 0 is always the null block diagram,
 * which is used by a disabled device.
 */
enum usb_audio_altset {
    ALTSET_OFF  = 0x00,         /* No endpoint */
    ALTSET_ON   = 0x01,         /* Single endpoint */
};

/*
 * Class-specific control requests
 */
#define CR_SET_CUR      0x01
#define CR_GET_CUR      0x81
#define CR_SET_MIN      0x02
#define CR_GET_MIN      0x82
#define CR_SET_MAX      0x03
#define CR_GET_MAX      0x83
#define CR_SET_RES      0x04
#define CR_GET_RES      0x84
#define CR_SET_MEM      0x05
#define CR_GET_MEM      0x85
#define CR_GET_STAT     0xff

/*
 * Feature Unit Control Selectors
 */
#define MUTE_CONTROL                    0x01
#define VOLUME_CONTROL                  0x02
#define BASS_CONTROL                    0x03
#define MID_CONTROL                     0x04
#define TREBLE_CONTROL                  0x05
#define GRAPHIC_EQUALIZER_CONTROL       0x06
#define AUTOMATIC_GAIN_CONTROL          0x07
#define DELAY_CONTROL                   0x08
#define BASS_BOOST_CONTROL              0x09
#define LOUDNESS_CONTROL                0x0a

#define USB_DEVICE_DESC_SIZE        18
#define USB_CONFIGUARTION_DESC_SIZE 9
#define USB_INTERFACE_DESC_SIZE     9
#define USB_ENDPOINT_DESC_SIZE      7

/*
 * buffering
 */

struct streambuf {
    uint8_t *data;
    uint32_t size;
    uint32_t prod;
    uint32_t cons;
};

typedef struct SINGSTARMICState {
    USBDevice dev;
    int port;
    AudioSource *audsrc[2];
    // Use same source for both player or
    // left channel for P1 and right for P2 if stereo.
    bool isCombined;

    /* state */
    struct {
        enum usb_audio_altset altset;
        bool mute;
        uint8_t vol[2];
        struct streambuf buf;
    } out;

    /* properties */
    uint32_t debug;
    uint32_t buffer;
    uint32_t srate[2]; //two mics
    //uint8_t  fifo[2][200]; //on-chip 400byte fifo
    streambuf fifo[2];
} SINGSTARMICState;

/* descriptor dumped from a real singstar MIC adapter */
static const uint8_t singstar_mic_dev_descriptor[] = {
    /* bLength             */ 0x12, //(18)
    /* bDescriptorType     */ 0x01, //(1)
    /* bcdUSB              */ WBVAL(0x0110), //(272)
    /* bDeviceClass        */ 0x00, //(0)
    /* bDeviceSubClass     */ 0x00, //(0)
    /* bDeviceProtocol     */ 0x00, //(0)
    /* bMaxPacketSize0     */ 0x08, //(8)
    /* idVendor            */ WBVAL(0x1415), //(5141)
    /* idProduct           */ WBVAL(0x0000), //(0)
    /* bcdDevice           */ WBVAL(0x0001), //(1)
    /* iManufacturer       */ 0x01, //(1)
    /* iProduct            */ 0x02, //(2)
    /* iSerialNumber       */ 0x00, //(0)
    /* bNumConfigurations  */ 0x01, //(1)

};

static const uint8_t singstar_mic_config_descriptor[] = {

/* Configuration 1 */
  0x09,   /* bLength */
  USB_CONFIGURATION_DESCRIPTOR_TYPE,    /* bDescriptorType */
  WBVAL(0x00b1),                        /* wTotalLength */
  0x02,                                 /* bNumInterfaces */
  0x01,                                 /* bConfigurationValue */
  0x00,                                 /* iConfiguration */
  USB_CONFIG_BUS_POWERED,               /* bmAttributes */
  USB_CONFIG_POWER_MA(90),              /* bMaxPower */

/* Interface 0, Alternate Setting 0, Audio Control */
  USB_INTERFACE_DESC_SIZE,              /* bLength */
  USB_INTERFACE_DESCRIPTOR_TYPE,        /* bDescriptorType */
  0x00,                                 /* bInterfaceNumber */
  0x00,                                 /* bAlternateSetting */
  0x00,                                 /* bNumEndpoints */
  USB_DEVICE_CLASS_AUDIO,               /* bInterfaceClass */
  AUDIO_SUBCLASS_AUDIOCONTROL,          /* bInterfaceSubClass */
  AUDIO_PROTOCOL_UNDEFINED,             /* bInterfaceProtocol */
  0x00,                                 /* iInterface */

/* Audio Control Interface */
  AUDIO_CONTROL_INTERFACE_DESC_SZ(1),   /* bLength */
  AUDIO_INTERFACE_DESCRIPTOR_TYPE,      /* bDescriptorType */
  AUDIO_CONTROL_HEADER,                 /* bDescriptorSubtype */
  WBVAL(0x0100), /* 1.00 */             /* bcdADC */
  WBVAL(0x0028),                        /* wTotalLength */
  0x01,                                 /* bInCollection */
  0x01,                                 /* baInterfaceNr */

/* Audio Input Terminal */
  AUDIO_INPUT_TERMINAL_DESC_SIZE,       /* bLength */
  AUDIO_INTERFACE_DESCRIPTOR_TYPE,      /* bDescriptorType */
  AUDIO_CONTROL_INPUT_TERMINAL,         /* bDescriptorSubtype */
  0x01,                                 /* bTerminalID */
  WBVAL(AUDIO_TERMINAL_MICROPHONE),     /* wTerminalType */
  0x02,                                 /* bAssocTerminal */
  0x02,                                 /* bNrChannels */
  WBVAL(AUDIO_CHANNEL_L
       |AUDIO_CHANNEL_R),				/* wChannelConfig */
  0x00,                                 /* iChannelNames */
  0x00,                                 /* iTerminal */

/* Audio Output Terminal */
  AUDIO_OUTPUT_TERMINAL_DESC_SIZE,      /* bLength */
  AUDIO_INTERFACE_DESCRIPTOR_TYPE,      /* bDescriptorType */
  AUDIO_CONTROL_OUTPUT_TERMINAL,        /* bDescriptorSubtype */
  0x02,                                 /* bTerminalID */
  WBVAL(AUDIO_TERMINAL_USB_STREAMING),  /* wTerminalType */
  0x01,                                 /* bAssocTerminal */
  0x03,                                 /* bSourceID */
  0x00,                                 /* iTerminal */

/* Audio Feature Unit */
  AUDIO_FEATURE_UNIT_DESC_SZ(2,1),      /* bLength */
  AUDIO_INTERFACE_DESCRIPTOR_TYPE,      /* bDescriptorType */
  AUDIO_CONTROL_FEATURE_UNIT,           /* bDescriptorSubtype */
  0x03,                                 /* bUnitID */
  0x01,                                 /* bSourceID */
  0x01,                                 /* bControlSize */
  0x01,                                 /* bmaControls(0) */
  0x02,                                 /* bmaControls(1) */
  0x02,                                 /* bmaControls(2) */
  0x00,                                 /* iTerminal */

/* Interface 1, Alternate Setting 0, Audio Streaming - Zero Bandwith */
  USB_INTERFACE_DESC_SIZE,              /* bLength */
  USB_INTERFACE_DESCRIPTOR_TYPE,        /* bDescriptorType */
  0x01,                                 /* bInterfaceNumber */
  0x00,                                 /* bAlternateSetting */
  0x00,                                 /* bNumEndpoints */
  USB_DEVICE_CLASS_AUDIO,               /* bInterfaceClass */
  AUDIO_SUBCLASS_AUDIOSTREAMING,        /* bInterfaceSubClass */
  AUDIO_PROTOCOL_UNDEFINED,             /* bInterfaceProtocol */
  0x00,                                 /* iInterface */

/* Interface 1, Alternate Setting 1, Audio Streaming - Operational */
  USB_INTERFACE_DESC_SIZE,              /* bLength */
  USB_INTERFACE_DESCRIPTOR_TYPE,        /* bDescriptorType */
  0x01,                                 /* bInterfaceNumber */
  0x01,                                 /* bAlternateSetting */
  0x01,                                 /* bNumEndpoints */
  USB_DEVICE_CLASS_AUDIO,               /* bInterfaceClass */
  AUDIO_SUBCLASS_AUDIOSTREAMING,        /* bInterfaceSubClass */
  AUDIO_PROTOCOL_UNDEFINED,             /* bInterfaceProtocol */
  0x00,                                 /* iInterface */

/* Audio Streaming Interface */
  AUDIO_STREAMING_INTERFACE_DESC_SIZE,  /* bLength */
  AUDIO_INTERFACE_DESCRIPTOR_TYPE,      /* bDescriptorType */
  AUDIO_STREAMING_GENERAL,              /* bDescriptorSubtype */
  0x02,                                 /* bTerminalLink */
  0x01,                                 /* bDelay */
  WBVAL(AUDIO_FORMAT_PCM),              /* wFormatTag */

/* Audio Type I Format */
  AUDIO_FORMAT_TYPE_I_DESC_SZ(5),       /* bLength */
  AUDIO_INTERFACE_DESCRIPTOR_TYPE,      /* bDescriptorType */
  AUDIO_STREAMING_FORMAT_TYPE,          /* bDescriptorSubtype */
  AUDIO_FORMAT_TYPE_I,                  /* bFormatType */
  0x01,                                 /* bNrChannels */
  0x02,                                 /* bSubFrameSize */
  0x10,                                 /* bBitResolution */
  0x05,                                 /* bSamFreqType */
  B3VAL(8000),                          /* tSamFreq 1 */
  B3VAL(11025),                         /* tSamFreq 2 */
  B3VAL(22050),                         /* tSamFreq 3 */
  B3VAL(44100),                         /* tSamFreq 4 */
  B3VAL(48000),                         /* tSamFreq 5 */

/* Endpoint - Standard Descriptor */
  AUDIO_STANDARD_ENDPOINT_DESC_SIZE,    /* bLength */
  USB_ENDPOINT_DESCRIPTOR_TYPE,         /* bDescriptorType */
  USB_ENDPOINT_OUT(0x81),               /* bEndpointAddress */
  USB_ENDPOINT_TYPE_ISOCHRONOUS
  | USB_ENDPOINT_SYNC_ASYNCHRONOUS,     /* bmAttributes */
  WBVAL(0x0064),                        /* wMaxPacketSize */
  0x01,                                 /* bInterval */
  0x00,                                 /* bRefresh */
  0x00,                                 /* bSynchAddress */

/* Endpoint - Audio Streaming */
  AUDIO_STREAMING_ENDPOINT_DESC_SIZE,   /* bLength */
  AUDIO_ENDPOINT_DESCRIPTOR_TYPE,       /* bDescriptorType */
  AUDIO_ENDPOINT_GENERAL,               /* bDescriptor */
  0x01,                                 /* bmAttributes */
  0x00,                                 /* bLockDelayUnits */
  WBVAL(0x0000),                        /* wLockDelay */

/* Interface 1, Alternate Setting 2, Audio Streaming - ? */
  USB_INTERFACE_DESC_SIZE,              /* bLength */
  USB_INTERFACE_DESCRIPTOR_TYPE,        /* bDescriptorType */
  0x01,                                 /* bInterfaceNumber */
  0x02,                                 /* bAlternateSetting */
  0x01,                                 /* bNumEndpoints */
  USB_DEVICE_CLASS_AUDIO,               /* bInterfaceClass */
  AUDIO_SUBCLASS_AUDIOSTREAMING,        /* bInterfaceSubClass */
  AUDIO_PROTOCOL_UNDEFINED,             /* bInterfaceProtocol */
  0x00,                                 /* iInterface */

/* Audio Streaming Interface */
  AUDIO_STREAMING_INTERFACE_DESC_SIZE,  /* bLength */
  AUDIO_INTERFACE_DESCRIPTOR_TYPE,      /* bDescriptorType */
  AUDIO_STREAMING_GENERAL,              /* bDescriptorSubtype */
  0x02,                                 /* bTerminalLink */
  0x01,                                 /* bDelay */
  WBVAL(AUDIO_FORMAT_PCM),              /* wFormatTag */

/* Audio Type I Format */
  AUDIO_FORMAT_TYPE_I_DESC_SZ(5),       /* bLength */
  AUDIO_INTERFACE_DESCRIPTOR_TYPE,      /* bDescriptorType */
  AUDIO_STREAMING_FORMAT_TYPE,          /* bDescriptorSubtype */
  AUDIO_FORMAT_TYPE_I,                  /* bFormatType */
  0x02,                                 /* bNrChannels */
  0x02,                                 /* bSubFrameSize */
  0x10,                                 /* bBitResolution */
  0x05,                                 /* bSamFreqType */
  B3VAL(8000),                          /* tSamFreq 1 */
  B3VAL(11025),                         /* tSamFreq 2 */
  B3VAL(22050),                         /* tSamFreq 3 */
  B3VAL(44100),                         /* tSamFreq 4 */
  B3VAL(48000),                         /* tSamFreq 5 */

/* Endpoint - Standard Descriptor */
  AUDIO_STANDARD_ENDPOINT_DESC_SIZE,    /* bLength */
  USB_ENDPOINT_DESCRIPTOR_TYPE,         /* bDescriptorType */
  USB_ENDPOINT_OUT(0x81),               /* bEndpointAddress */
  USB_ENDPOINT_TYPE_ISOCHRONOUS
  | USB_ENDPOINT_SYNC_ASYNCHRONOUS,     /* bmAttributes */
  WBVAL(0x00c8),                        /* wMaxPacketSize */
  0x01,                                 /* bInterval */
  0x00,                                 /* bRefresh */
  0x00,                                 /* bSynchAddress */

/* Endpoint - Audio Streaming */
  AUDIO_STREAMING_ENDPOINT_DESC_SIZE,   /* bLength */
  AUDIO_ENDPOINT_DESCRIPTOR_TYPE,       /* bDescriptorType */
  AUDIO_ENDPOINT_GENERAL,               /* bDescriptor */
  0x01,                                 /* bmAttributes */
  0x00,                                 /* bLockDelayUnits */
  WBVAL(0x0000),                        /* wLockDelay */

/* Terminator */
  0                                     /* bLength */
};


static void singstar_mic_handle_reset(USBDevice *dev)
{
    /* XXX: do it */
	return;
}

/*
 * Note: we arbitrarily map the volume control range onto -inf..+8 dB
 */
#define ATTRIB_ID(cs, attrib, idif)     \
    (((cs) << 24) | ((attrib) << 16) | (idif))


static void streambuf_init(struct streambuf *buf, uint32_t size)
{
    free(buf->data);
    buf->size = size - (size % USBAUDIO_PACKET_SIZE);
    buf->data = (uint8_t*)malloc(buf->size);
    buf->prod = 0;
    buf->cons = 0;
}

static void streambuf_fini(struct streambuf *buf)
{
    free(buf->data);
    buf->data = NULL;
}

static int streambuf_put(struct streambuf *buf, uint8_t *p)
{
    uint32_t free = buf->size - (buf->prod - buf->cons);

    if (!free) {
        return 0;
    }
    assert(free >= USBAUDIO_PACKET_SIZE);
	//TODO
    memcpy(buf->data + (buf->prod % buf->size), p,
                    USBAUDIO_PACKET_SIZE);
    buf->prod += USBAUDIO_PACKET_SIZE;
    return USBAUDIO_PACKET_SIZE;
}

static uint8_t *streambuf_get(struct streambuf *buf)
{
    uint32_t used = buf->prod - buf->cons;
    uint8_t *data;

    if (!used) {
        return NULL;
    }
    assert(used >= USBAUDIO_PACKET_SIZE);
    data = buf->data + (buf->cons % buf->size);
    buf->cons += USBAUDIO_PACKET_SIZE;
    return data;
}

//0x0300 - feature bUnitID 0x03
static int usb_audio_get_control(SINGSTARMICState *s, uint8_t attrib,
                                 uint16_t cscn, uint16_t idif,
                                 int length, uint8_t *data)
{
    uint8_t cs = cscn >> 8;
    uint8_t cn = cscn - 1;      /* -1 for the non-present master control */
    uint32_t aid = ATTRIB_ID(cs, attrib, idif);
    int ret = USB_RET_STALL;

    switch (aid) {
    case ATTRIB_ID(MUTE_CONTROL, CR_GET_CUR, 0x0300):
        data[0] = s->out.mute;
        ret = 1;
        break;
    case ATTRIB_ID(VOLUME_CONTROL, CR_GET_CUR, 0x0300):
        if (cn < 2) {
            //uint16_t vol = (s->out.vol[cn] * 0x8800 + 127) / 255 + 0x8000;
            uint16_t vol = (s->out.vol[cn] * 0x8800 + 127) / 255 + 0x8000;
            data[0] = (uint8_t)(vol & 0xFF);
            data[1] = vol >> 8;
            ret = 2;
        }
        break;
    case ATTRIB_ID(VOLUME_CONTROL, CR_GET_MIN, 0x0300):
        if (cn < 2) {
            data[0] = 0x01;
            data[1] = 0x80;
            //data[0] = 0x00;
            //data[1] = 0xE1; //0xE100 -31dB
            ret = 2;
        }
        break;
    case ATTRIB_ID(VOLUME_CONTROL, CR_GET_MAX, 0x0300):
        if (cn < 2) {
            data[0] = 0x00;
            data[1] = 0x08;
            //data[0] = 0x00;
            //data[1] = 0x18; //0x1800 +24dB
            ret = 2;
        }
        break;
    case ATTRIB_ID(VOLUME_CONTROL, CR_GET_RES, 0x0300):
        if (cn < 2) {
            data[0] = 0x88;
            data[1] = 0x00;
            //data[0] = 0x00;
            //data[1] = 0x01; //0x0100 1.0 dB
            ret = 2;
        }
        break;
    }

    return ret;
}

static int usb_audio_set_control(SINGSTARMICState *s, uint8_t attrib,
                                 uint16_t cscn, uint16_t idif,
                                 int length, uint8_t *data)
{
    uint8_t cs = cscn >> 8;
    uint8_t cn = cscn - 1;      /* -1 for the non-present master control */
    uint32_t aid = ATTRIB_ID(cs, attrib, idif);
    int ret = USB_RET_STALL;
    bool set_vol = false;

    switch (aid) {
    case ATTRIB_ID(MUTE_CONTROL, CR_SET_CUR, 0x0300):
        s->out.mute = data[0] & 1;
        set_vol = true;
        ret = 0;
        break;
    case ATTRIB_ID(VOLUME_CONTROL, CR_SET_CUR, 0x0300):
        if (cn < 2) {
            uint16_t vol = data[0] + (data[1] << 8);

			//qemu usb audiocard formula, singstar has a bit different range
            vol -= 0x8000;
            vol = (vol * 255 + 0x4400) / 0x8800;
            if (vol > 255) {
                vol = 255;
            }

            if (s->out.vol[cn] != vol) {
				s->out.vol[cn] = (uint8_t)vol;
				set_vol = true;
			}
            ret = 0;
        }
        break;
    }

    if (set_vol) {
        //if (s->debug) {
            fprintf(stderr, "singstar: mute %d, lvol %3d, rvol %3d\n",
                    s->out.mute, s->out.vol[0], s->out.vol[1]);
			OSDebugOut(TEXT("singstar: mute %d, lvol %3d, rvol %3d\n"),
                    s->out.mute, s->out.vol[0], s->out.vol[1]);
        //}
        //AUD_set_volume_out(s->out.voice, s->out.mute,
        //                   s->out.vol[0], s->out.vol[1]);
    }

    return ret;
}

static int usb_audio_ep_control(SINGSTARMICState *s, uint8_t attrib,
                                 uint16_t cscn, uint16_t ep,
                                 int length, uint8_t *data)
{
    uint8_t cs = cscn >> 8;
    uint8_t cn = cscn - 1;      /* -1 for the non-present master control */
    uint32_t aid = ATTRIB_ID(cs, attrib, ep);
    int ret = USB_RET_STALL;

	//cs 1 cn 0xFF, ep 0x81 attrib 1
	fprintf(stderr, "singstar: ep control cs %x, cn %X, %X %X data:", cs, cn, attrib, ep);
	OSDebugOut(TEXT("singstar: ep control cs %x, cn %X, attr: %02X ep: %04X\n"), cs, cn, attrib, ep);
	for(int i=0; i<length; i++)
		fprintf(stderr, "%02X ", data[i]);
	fprintf(stderr, "\n");

    switch (aid) {
    case ATTRIB_ID(AUDIO_SAMPLING_FREQ_CONTROL, CR_SET_CUR, 0x81):
		if( cn == 0xFF) {
			s->srate[0] = data[0] | (data[1] << 8) | (data[2] << 16);
			s->srate[1] = s->srate[0];

			if(s->audsrc[0])
				s->audsrc[0]->SetResampling(s->srate[0]);

			if(s->audsrc[1])
				s->audsrc[1]->SetResampling(s->srate[1]);

			OSDebugOut(TEXT("singstar: set sampling to %d\n"), s->srate[0]);
		} else if( cn < 2) {

			s->srate[cn] = data[0] | (data[1] << 8) | (data[2] << 16);
			OSDebugOut(TEXT("singstar: set cn %d sampling to %d\n"), cn, s->srate[cn]);
			if(s->audsrc[cn])
				s->audsrc[cn]->SetResampling(s->srate[cn]);
		}
        ret = 0;
        break;
    case ATTRIB_ID(AUDIO_SAMPLING_FREQ_CONTROL, CR_GET_CUR, 0x81):
        data[0] = s->srate[0] & 0xFF;
		data[1] = (s->srate[0] >> 8) & 0xFF;
		data[2] = (s->srate[0] >> 16) & 0xFF;
        ret = 3;
        break;
    }

    return ret;
}

static int singstar_mic_handle_control(USBDevice *dev, int request, int value,
                                  int index, int length, uint8_t *data)
{
    SINGSTARMICState *s = (SINGSTARMICState *)dev;
    int ret = 0;

	//OSDebugOut("singstar: req %04X val: %04X idx: %04X len: %d\n", request, value, index, length);

    switch(request) {
    /*
    * Audio device specific request
    */
    case ClassInterfaceRequest | CR_GET_CUR:
    case ClassInterfaceRequest | CR_GET_MIN:
    case ClassInterfaceRequest | CR_GET_MAX:
    case ClassInterfaceRequest | CR_GET_RES:
        ret = usb_audio_get_control(s, request & 0xff, value, index,
                                    length, data);
        if (ret < 0) {
            //if (s->debug) {
                fprintf(stderr, "singstar: fail: get control\n");
            //}
            goto fail;
        }
        break;

    case ClassInterfaceOutRequest | CR_SET_CUR:
    case ClassInterfaceOutRequest | CR_SET_MIN:
    case ClassInterfaceOutRequest | CR_SET_MAX:
    case ClassInterfaceOutRequest | CR_SET_RES:
        ret = usb_audio_set_control(s, request & 0xff, value, index,
                                    length, data);
        if (ret < 0) {
            //if (s->debug) {
                fprintf(stderr, "singstar: fail: set control\n data:");
            //}
            goto fail;
        }
        break;

    case ClassEndpointRequest | CR_GET_CUR:
    case ClassEndpointRequest | CR_GET_MIN:
    case ClassEndpointRequest | CR_GET_MAX:
    case ClassEndpointRequest | CR_GET_RES:
    case ClassEndpointOutRequest | CR_SET_CUR:
    case ClassEndpointOutRequest | CR_SET_MIN:
    case ClassEndpointOutRequest | CR_SET_MAX:
    case ClassEndpointOutRequest | CR_SET_RES:
        ret = usb_audio_ep_control(s, request & 0xff, value, index,
                                    length, data);
        if (ret < 0) goto fail;
        break;
    /*
    * Generic usb request
    */
    case DeviceRequest | USB_REQ_GET_STATUS:
        data[0] = (dev->remote_wakeup << USB_DEVICE_REMOTE_WAKEUP);
        data[1] = 0x00;
        ret = 2;
        break;
    case DeviceOutRequest | USB_REQ_CLEAR_FEATURE:
        if (value == USB_DEVICE_REMOTE_WAKEUP) {
            dev->remote_wakeup = 0;
        } else {
            goto fail;
        }
        ret = 0;
        break;
    case DeviceOutRequest | USB_REQ_SET_FEATURE:
        if (value == USB_DEVICE_REMOTE_WAKEUP) {
            dev->remote_wakeup = 1;
        } else {
            goto fail;
        }
        ret = 0;
        break;
    case DeviceOutRequest | USB_REQ_SET_ADDRESS:
        dev->addr = value;
        ret = 0;
        break;
    case DeviceRequest | USB_REQ_GET_DESCRIPTOR:

		//TODO qemu has internal buffer of 8KB, but PS2 games' usb driver
		//first calls with buffer size like 4 or 8 bytes
		//and then re-requests with 'ret'-urned size buffer.
        switch(value >> 8) {
        case USB_DT_DEVICE:
            memcpy(data, singstar_mic_dev_descriptor, 
                   sizeof(singstar_mic_dev_descriptor));
            ret = sizeof(singstar_mic_dev_descriptor);
            break;
        case USB_DT_CONFIG:
            memcpy(data, singstar_mic_config_descriptor, 
                sizeof(singstar_mic_config_descriptor));
            ret = sizeof(singstar_mic_config_descriptor);
            break;
		//Probably ignored most of the time
        case USB_DT_STRING:
            switch(value & 0xff) {
            case 0:
                /* language ids */
                data[0] = 4;
                data[1] = 3;
                data[2] = 0x09;
                data[3] = 0x04;
                ret = 4;
                break;
            case 1:// TODO iSerial = 0
                /* serial number */
                ret = set_usb_string(data, "3X0420811");
                break;
            case 2:// TODO iProduct = 2
                /* product description */
                ret = set_usb_string(data, "USBMIC");
                break;
            case 3:// TODO iManufacturer = 1
                /* vendor description */
                ret = set_usb_string(data, "Nam Tai E&E Products Ltd.");
                break;
            default:
                goto fail;
            }
            break;
        default:
            goto fail;
        }
        break;
    case DeviceRequest | USB_REQ_GET_CONFIGURATION:
        data[0] = 1;
        ret = 1;
        break;
    case DeviceOutRequest | USB_REQ_SET_CONFIGURATION:
        ret = 0;
        break;
    case DeviceRequest | USB_REQ_GET_INTERFACE:
        data[0] = 0;
        ret = 1;
        break;
	case InterfaceOutRequest | USB_REQ_SET_INTERFACE:
    case DeviceOutRequest | USB_REQ_SET_INTERFACE:
        ret = 0;
        break;
        /* hid specific requests */
    case InterfaceRequest | USB_REQ_GET_DESCRIPTOR:
        // Probably never comes here
        goto fail;
        break;
    case GET_REPORT:
        ret = 0;
        break;
    case SET_IDLE:
        ret = 0;
        break;
    default:
    fail:
        ret = USB_RET_STALL;
        break;
    }
    return ret;
}

static int singstar_mic_handle_data(USBDevice *dev, int pid, 
                               uint8_t devep, uint8_t *data, int len)
{
    SINGSTARMICState *s = (SINGSTARMICState *)dev;
    int ret = 0;

    switch(pid) {
    case USB_TOKEN_IN:
        //fprintf(stderr, "token in ep: %d len: %d\n", devep, len);
		OSDebugOut(TEXT("token in ep: %d len: %d\n"), devep, len);
        if (devep == 1) {

			//TODO
			uint16_t *buffer[2] = {0};
			uint32_t bufSize, outlen[2] = {0};
			uint16_t *src1, *src2;
			uint16_t *dst = (uint16_t *)data;
			uint32_t forLen = len / (2 * sizeof(uint16_t));

			for(int i = 0; i<2; i++)
			{
				if(s->audsrc[i] && 
					s->audsrc[i]->GetBufferSize(&bufSize))
				{
					buffer[i] = new uint16_t[bufSize];
					outlen[i] = s->audsrc[i]->GetBuffer(buffer[i], bufSize);
				}
			}

			//TODO cache coherence
			//TODO well, it is 16bit interleaved, right?
			if(s->isCombined && (buffer[0] || buffer[1]))
			{
				uint32_t ch = s->audsrc[0]->GetChannels();
				if(buffer[0])
					src1 = (uint16_t *)buffer[0];
				else
					src1 = (uint16_t *)buffer[1];

				int i = 0;
				for(; i < (outlen[0] / ch) && i < forLen; i++)
				{
					dst[i * 2] = src1[i * ch];
					if(ch == 1)
						dst[i * 2 + 1] = src2[i * ch];
					else
						dst[i * 2 + 1] = src2[i * ch + 1];
				}

				ret =i;
			}
			else if(buffer[0] && buffer[1])
			{
				uint32_t cn1 = s->audsrc[0]->GetChannels();
				uint32_t cn2 = s->audsrc[1]->GetChannels();
				uint32_t minLen = MIN(outlen[0] / cn1, outlen[1] / cn2);

				src1 = (uint16_t *)buffer[0];
				src2 = (uint16_t *)buffer[1];

				int i = 0;
				for(; i < minLen && i < forLen; i++)
				{
					dst[i * 2] = src1[i * cn1];
					dst[i * 2 + 1] = src2[i * cn2];
				}

				ret = i;
			}
			else //Could ignore this and use isCombined case
			{
				int i = 0;
				uint32_t cn = 1;
				uint32_t minLen = -1;

				for(int k=0; k < 2; k++)
				{
					if(s->audsrc[i])
					{
						cn = s->audsrc[i]->GetChannels();
						minLen = outlen[i] / cn;
						src1 = buffer[i];
						break;
					}
				}

				for(; i < minLen && i < forLen; i++)
				{
					if(s->audsrc[0])
					{
						dst[i * 2] = src1[i * cn];
						dst[i * 2 + 1] = 0;
					}
					else
					{
						dst[i * 2] = 0;
						dst[i * 2 + 1] = src1[i * cn];
					}
				}
				ret = i;
			}

			delete[] buffer[0];
			delete[] buffer[1];
			return ret;
        }
        break;
    case USB_TOKEN_OUT:
        printf("token out ep: %d\n", devep);
		OSDebugOut(TEXT("token out ep: %d len: %d\n"), devep, len);
    default:
    fail:
        ret = USB_RET_STALL;
        break;
    }
    return ret;
}


static void singstar_mic_handle_destroy(USBDevice *dev)
{
    SINGSTARMICState *s = (SINGSTARMICState *)dev;

	if(s->audsrc[0])
	{
		s->audsrc[0]->Stop();
		delete s->audsrc[0];
		s->audsrc[0] = NULL;
	}

	if(s->audsrc[1])
	{
		s->audsrc[1]->Stop();
		delete s->audsrc[1];
		s->audsrc[1] = NULL;
	}

    free(s);
	AudioDeinit();
}

int singstar_mic_handle_packet(USBDevice *s, int pid, 
                              uint8_t devaddr, uint8_t devep,
                              uint8_t *data, int len)
{
	//fprintf(stderr,"usb-singstar_mic: packet received with pid=%x, devaddr=%x, devep=%x and len=%x\n",pid,devaddr,devep,len);
	return usb_generic_handle_packet(s,pid,devaddr,devep,data,len);
}

USBDevice *singstar_mic_init(int port, STDSTR *devs)
{
    SINGSTARMICState *s;

    s = (SINGSTARMICState *)qemu_mallocz(sizeof(SINGSTARMICState));
    if (!s)
        return NULL;

	AudioInit();
	if(!devs[0].empty() && !devs[1].empty() 
		&& (devs[0] == devs[1]))
	{
		s->isCombined = true;
	}

	AudioDeviceInfo info;

	if(!devs[0].empty())
	{
		info.strID = devs[0];
		s->audsrc[0] = CreateNewAudioSource(info);
		if(s->audsrc[0])
			s->audsrc[0]->Start();
	}

	if(!s->isCombined && !devs[1].empty())
	{
		info.strID = devs[1];
		s->audsrc[1] = CreateNewAudioSource(info);
		if(s->audsrc[1])
			s->audsrc[1]->Start();
	}

	if(!s->audsrc[0] && !s->audsrc[1])
	{
		free(s);
		AudioDeinit();
		return NULL;
	}

	//if(!s->audsrc[0] || !s->audsrc[1])
	//	s->isCombined = true;

    s->dev.speed = USB_SPEED_FULL;
    s->dev.handle_packet  = singstar_mic_handle_packet;
    s->dev.handle_reset   = singstar_mic_handle_reset;
    s->dev.handle_control = singstar_mic_handle_control;
    s->dev.handle_data    = singstar_mic_handle_data;
    s->dev.handle_destroy = singstar_mic_handle_destroy;
    s->port = port;

    strncpy(s->dev.devname, "USBMIC", sizeof(s->dev.devname));

    return (USBDevice *)s;

}

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

#include "../qemu-usb/vl.h"
#include "../qemu-usb/desc.h"
#include "usb-midi-ux16.h"
#include <assert.h>

#include "../usb-mic/audio.h"

namespace usb_midi {
typedef struct PC300KBDState {
    USBDevice dev;

    USBDesc desc;
    USBDescDevice desc_dev;
} PC300KBDState;

static const USBDescStrings desc_strings = {
    "",
};

static const uint8_t pc300_kbd_dev_descriptor[] = {
    /* bLength             */ 0x12, //(18)
    /* bDescriptorType     */ 0x01, //(1)
    /* bcdUSB              */ WBVAL(0x0110), //(272)
    /* bDeviceClass        */ 0xff, //(255)
    /* bDeviceSubClass     */ 0x00, //(0)
    /* bDeviceProtocol     */ 0x00, //(0)
    /* bMaxPacketSize0     */ 0x08, //(8)
    /* idVendor            */ WBVAL(0x0582), //(1410)
    /* idProduct           */ WBVAL(0x0008), //(8)
    /* bcdDevice           */ WBVAL(0x0110), //(272)
    /* iManufacturer       */ 0x01, //(1)
    /* iProduct            */ 0x00, //(0)
    /* iSerialNumber       */ 0x00, //(0) unused
    /* bNumConfigurations  */ 0x01, //(1)

};

static const uint8_t pc300_kbd_config_descriptor[] = {

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
        USB_CLASS_AUDIO,                      /* bInterfaceClass */
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
            | AUDIO_CHANNEL_R),				/* wChannelConfig */
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
        USB_CLASS_AUDIO,                      /* bInterfaceClass */
        AUDIO_SUBCLASS_AUDIOSTREAMING,        /* bInterfaceSubClass */
        AUDIO_PROTOCOL_UNDEFINED,             /* bInterfaceProtocol */
        0x00,                                 /* iInterface */

    /* Interface 1, Alternate Setting 1, Audio Streaming - Operational */
        USB_INTERFACE_DESC_SIZE,              /* bLength */
        USB_INTERFACE_DESCRIPTOR_TYPE,        /* bDescriptorType */
        0x01,                                 /* bInterfaceNumber */
        0x01,                                 /* bAlternateSetting */
        0x01,                                 /* bNumEndpoints */
        USB_CLASS_AUDIO,                      /* bInterfaceClass */
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
        USB_ENDPOINT_IN(1),                   /* bEndpointAddress */
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
        USB_CLASS_AUDIO,                      /* bInterfaceClass */
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
        USB_ENDPOINT_IN(1),                   /* bEndpointAddress */
        USB_ENDPOINT_TYPE_BULK,               /* bmAttributes */
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

USBDevice* MidiPc300Device::CreateDevice(int port)
{
    std::string api;
    if (!LoadSetting(nullptr, port, TypeName(), N_DEVICE_API, api))
        return nullptr;

    USBDevice* dev = MidiUx16Device::CreateDevice(port, api);
    if (!dev)
        return nullptr;

    PC300KBDState* s = (PC300KBDState*)dev;
    s->desc = {};
    s->desc_dev = {};

    s->desc.full = &s->desc_dev;
    s->desc.str = desc_strings;

    if (usb_desc_parse_dev(pc300_kbd_dev_descriptor, sizeof(pc300_kbd_dev_descriptor), s->desc, s->desc_dev) < 0) {
        OSDebugOut(TEXT("Failed usb_desc_parse_dev\n"));
        goto fail;
    }

    if (usb_desc_parse_config(pc300_kbd_config_descriptor, sizeof(pc300_kbd_config_descriptor), s->desc_dev) < 0) {
        OSDebugOut(TEXT("Failed usb_desc_parse_config\n"));
        goto fail;
    }

    s->dev.klass.usb_desc = &s->desc;
    s->dev.klass.product_desc = desc_strings[0];

    usb_desc_init(&s->dev);
    SysMessage(TEXT("pc300: Started"));

    return dev;

fail:
    s->dev.klass.unrealize(dev);
    return nullptr;
}

}
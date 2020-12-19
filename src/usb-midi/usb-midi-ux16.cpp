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
typedef struct UX16KBDState {
    USBDevice dev;

    USBDesc desc;
    USBDescDevice desc_dev;

    MidiDevice* midisrc;
    usb_midi::MidiDeviceProxyBase* midisrcproxy;

    struct freeze {
        int port;
        int intf;
    } f; //freezable

    /* properties */
    uint32_t debug;
} UX16KBDState;

static const USBDescStrings desc_strings = {
    "",
};

static const uint8_t ux16_kbd_dev_descriptor[] = {
    /* bLength             */ 0x12, //(18)
    /* bDescriptorType     */ 0x01, //(1)
    /* bcdUSB              */ WBVAL(0x0110), //(272)
    /* bDeviceClass        */ 0xFF, //(255)
    /* bDeviceSubClass     */ 0x00, //(0)
    /* bDeviceProtocol     */ 0xFF, //(255)
    /* bMaxPacketSize0     */ 0x08, //(8)
    /* idVendor            */ WBVAL(0x0499), //(1177)
    /* idProduct           */ WBVAL(0x1009), //(4105)
    /* bcdDevice           */ WBVAL(0x0102), //(102)
    /* iManufacturer       */ 0x00, //(0)
    /* iProduct            */ 0x00, //(0)
    /* iSerialNumber       */ 0x00, //(0) unused
    /* bNumConfigurations  */ 0x01, //(1)

};


static const uint8_t ux16_kbd_config_descriptor[] = {
    /* Configuration 1 */
    USB_CONFIGURATION_DESC_SIZE,          /* bLength */
    USB_CONFIGURATION_DESCRIPTOR_TYPE,    /* bDescriptorType */
    WBVAL(0x0036),                        /* wTotalLength */
    0x01,                                 /* bNumInterfaces */
    0x01,                                 /* bConfigurationValue */
    0x00,                                 /* iConfiguration */
    USB_CONFIG_BUS_POWERED,               /* bmAttributes */
    USB_CONFIG_POWER_MA(80),              /* bMaxPower */

    /* Interface Descriptor */
    USB_INTERFACE_DESC_SIZE,              /* bLength */
    USB_INTERFACE_DESCRIPTOR_TYPE,        /* bDescriptorType */
    0x00,                                 /* bInterfaceNumber */
    0x00,                                 /* bAlternateSetting */
    0x02,                                 /* bNumEndpoints */
    USB_CLASS_VENDOR_SPEC,                /* bInterfaceClass */
    AUDIO_SUBCLASS_UNDEFINED,             /* bInterfaceSubClass */
    0xff,                                 /* bInterfaceProtocol */
    0x00,                                 /* iInterface */

    /* Audio Control Interface? */
    0x07,                                 /* bLength */
    AUDIO_INTERFACE_DESCRIPTOR_TYPE,      /* bDescriptorType */
    AUDIO_CONTROL_HEADER,                 /* bDescriptorSubtype */
    0x00, 0x01, 0x16, 0x00,

    /* Audio Input Terminal? */
    0x06,                                 /* bLength */
    AUDIO_INTERFACE_DESCRIPTOR_TYPE,      /* bDescriptorType */
    AUDIO_CONTROL_INPUT_TERMINAL,         /* bDescriptorSubtype */
    0x02, 0x01, 0x00,

    /* Audio Output Terminal */
    0x09,                                 /* bLength */
    AUDIO_INTERFACE_DESCRIPTOR_TYPE,      /* bDescriptorType */
    AUDIO_CONTROL_OUTPUT_TERMINAL,        /* bDescriptorSubtype */
    0x02,                                 /* bTerminalID */
    WBVAL(AUDIO_TERMINAL_USB_STREAMING),  /* wTerminalType */
    0x01,                                 /* bAssocTerminal */
    0x01,                                 /* bSourceID */
    0x00,                                 /* iTerminal */

    /* Endpoint - Standard Descriptor */
    0x07,                                 /* bLength */
    USB_ENDPOINT_DESCRIPTOR_TYPE,         /* bDescriptorType */
    USB_ENDPOINT_OUT(1),                  /* bEndpointAddress */
    USB_ENDPOINT_TYPE_BULK,               /* bmAttributes */
    WBVAL(0x0040),                        /* wMaxPacketSize */
    0x01,                                 /* bInterval */

    /* Endpoint - Standard Descriptor */
    0x07,                                 /* bLength */
    USB_ENDPOINT_DESCRIPTOR_TYPE,         /* bDescriptorType */
    USB_ENDPOINT_IN(2),                   /* bEndpointAddress */
    USB_ENDPOINT_TYPE_BULK,               /* bmAttributes */
    WBVAL(0x0040),                        /* wMaxPacketSize */
    0x01,                                 /* bInterval */

    /* Terminator */
    0                                     /* bLength */
};


static void ux16_kbd_handle_reset(USBDevice* dev)
{
    /* XXX: do it */
    return;
}

static void ux16_kbd_set_interface(USBDevice* dev, int intf,
    int alt_old, int alt_new)
{
    UX16KBDState* s = (UX16KBDState*)dev;
    s->f.intf = alt_new;
    OSDebugOut(TEXT("ux16: intf:%d alt:%d -> %d\n"), intf, alt_old, alt_new);
}

static void ux16_kbd_handle_control(USBDevice* dev, USBPacket* p, int request, int value,
    int index, int length, uint8_t* data)
{
    UX16KBDState* s = (UX16KBDState*)dev;
    int ret = 0;

    OSDebugOut(TEXT("ux16: req %04X val: %04X idx: %04X len: %d\n"), request, value, index, length);

    ret = usb_desc_handle_control(dev, p, request, value, index, length, data);
    if (ret >= 0) {
        return;
    }

    switch (request) {
    default:
    fail:
        p->status = USB_RET_STALL;
        break;
    }
}

static void ux16_kbd_handle_data(USBDevice* dev, USBPacket* p)
{
    UX16KBDState* s = (UX16KBDState*)dev;
    int ret = 0;
    uint8_t devep = p->ep->nr;
    switch (p->pid) {
    case USB_TOKEN_IN: {
        int32_t* dst = nullptr;
        std::vector<int32_t> dst_alloc(0);
        size_t len = p->iov.size;

        if (p->iov.niov == 1) {
            dst = (int32_t*)p->iov.iov[0].iov_base;
        }
        else {
            dst_alloc.resize(len / sizeof(int32_t));
            dst = dst_alloc.data();
        }

        memset(dst, 0, len);

        // The game can't process packets quick enough if you send a ton at once
        // so instead just return 1 MIDI command per update
        uint32_t curValue = 0xffffffff;
        if (s->midisrc) {
            curValue = s->midisrc->PopMidiCommand();
        }

        if (curValue != 0xffffffff) {
            dst[0] = (curValue << 8) | ((curValue & 0xf0) >> 4);
            ret += 4;
        }
        else {
            ret = 0;
        }

        if (p->iov.niov > 1)
        {
            usb_packet_copy(p, dst_alloc.data(), ret);
        }
        else {
            p->actual_length = ret;
        }
        break;
    }
    case USB_TOKEN_OUT:
        printf("token out ep: %d\n", devep);
        OSDebugOut(TEXT("token out ep: %d len: %d\n"), devep, p->actual_length);
    default:
    fail:
        p->status = USB_RET_STALL;
        break;
    }
}


static void ux16_kbd_handle_destroy(USBDevice* dev)
{
    UX16KBDState* s = (UX16KBDState*)dev;

    if (s) {
        if (s->midisrc) {
            s->midisrc->Stop();
            s->midisrc = NULL;
        }

        // TODO: Why does this throw an exception?
        // if(s->midisrcproxy) {
        //   s->midisrcproxy->AudioDeinit();
        // }
    }

    delete s;
}

static int ux16_kbd_handle_open(USBDevice* dev)
{
    UX16KBDState* s = (UX16KBDState*)dev;

    OSDebugOut(TEXT("Called ux16_kbd_handle_open\n"));

    if (s)
    {
        if (s->midisrc) {
            s->midisrc->Start();
        }
    }
    return 0;
}

static void ux16_kbd_handle_close(USBDevice* dev)
{
    UX16KBDState* s = (UX16KBDState*)dev;
    if (s)
    {
        if (s->midisrc) {
            s->midisrc->Stop();
        }
    }
}

//USBDevice *ux16_kbd_init(int port, TSTDSTRING *devs)
USBDevice* MidiUx16Device::CreateDevice(int port)
{
    std::string api;
    LoadSetting(nullptr, port, MidiUx16Device::TypeName(), N_DEVICE_API, api);
    return MidiUx16Device::CreateDevice(port, api);
}

USBDevice* MidiUx16Device::CreateDevice(int port, const std::string& api)
{
    UX16KBDState* s;

    s = new UX16KBDState();

    s->midisrcproxy = usb_midi::RegisterMidiDevice::instance().Proxy(api);
    if (!s->midisrcproxy)
    {
        SysMessage(TEXT("ux16: Invalid MIDI API: '%") TEXT(SFMTs) TEXT("'\n"), api.c_str());
        delete s;
        return NULL;
    }

    s->midisrcproxy->AudioInit();
    s->midisrc = s->midisrcproxy->CreateObject(port, TypeName());

    s->desc.full = &s->desc_dev;
    s->desc.str = desc_strings;

    if (usb_desc_parse_dev(ux16_kbd_dev_descriptor, sizeof(ux16_kbd_dev_descriptor), s->desc, s->desc_dev) < 0) {
        OSDebugOut(TEXT("Failed usb_desc_parse_dev\n"));
        goto fail;
    }

    if (usb_desc_parse_config(ux16_kbd_config_descriptor, sizeof(ux16_kbd_config_descriptor), s->desc_dev) < 0) {
        OSDebugOut(TEXT("Failed usb_desc_parse_config\n"));
        goto fail;
    }

    s->dev.speed = USB_SPEED_FULL;
    s->dev.klass.handle_attach = usb_desc_attach;
    s->dev.klass.handle_reset = ux16_kbd_handle_reset;
    s->dev.klass.handle_control = ux16_kbd_handle_control;
    s->dev.klass.handle_data = ux16_kbd_handle_data;
    s->dev.klass.set_interface = ux16_kbd_set_interface;
    s->dev.klass.unrealize = ux16_kbd_handle_destroy;
    s->dev.klass.open = ux16_kbd_handle_open;
    s->dev.klass.close = ux16_kbd_handle_close;
    s->dev.klass.usb_desc = &s->desc;
    s->dev.klass.product_desc = desc_strings[0];

    usb_desc_init(&s->dev);
    usb_ep_init(&s->dev);
    ux16_kbd_handle_reset((USBDevice*)s);

    return (USBDevice*)s;

fail:
    ux16_kbd_handle_destroy((USBDevice*)s);
    return NULL;
}

int MidiUx16Device::Configure(int port, const std::string& api, void* data)
{
    auto proxy = usb_midi::RegisterMidiDevice::instance().Proxy(api);
    if (proxy)
        return proxy->Configure(port, TypeName(), data);
    return RESULT_CANCELED;
}

int MidiUx16Device::Freeze(int mode, USBDevice* dev, void* data)
{
    UX16KBDState* s = (UX16KBDState*)dev;
    switch (mode)
    {
    case FREEZE_LOAD:
        if (!s) return -1;
        s->f = *(UX16KBDState::freeze*)data;
        return sizeof(UX16KBDState::freeze);
    case FREEZE_SAVE:
        if (!s) return -1;
        *(UX16KBDState::freeze*)data = s->f;
        return sizeof(UX16KBDState::freeze);
    case FREEZE_SIZE:
        return sizeof(UX16KBDState::freeze);
    default:
        break;
    }
    return -1;
}

}

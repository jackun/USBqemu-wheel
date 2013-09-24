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

#include <limits.h>
#include <string.h>
#include <stddef.h>

#include "../qemu-usb/vl.h"
#include <setupapi.h>
extern "C"{
#include "../ddk/hidsdi.h"
}

//Hardcoded Logitech MOMO racing wheel, idea is that gamepad etc. would be selectable instead
#define PAD_VID			0x046D
#define PAD_PID			0xCA03
#define DF_PID			0xC294 //generic PID
#define DFP_PID			0xC298
#define MAX_BUTTONS		128

bool doPassthrough = false;
uint32_t reportInSize = 0;
uint32_t reportOutSize = 0;
HIDP_CAPS caps;
HIDD_ATTRIBUTES attr;
USAGE usage[MAX_BUTTONS];
ULONG value = 0;
USHORT capsLength, numberOfButtons = 0;

//extern OHCIState *qemu_ohci;
//unsigned char comp_data[6];
HANDLE usb_pad=(HANDLE)-1;
HANDLE readData=(HANDLE)-1;
OVERLAPPED ovl;

/* HID interface requests */
#define GET_REPORT   0xa101
#define GET_IDLE     0xa102
#define GET_PROTOCOL 0xa103
#define SET_IDLE     0x210a
#define SET_PROTOCOL 0x210b

#include "../usb-mic/usb.h"
#include "../usb-mic/usbcfg.h"
#include "../usb-mic/usbdesc.h"

//
// http://stackoverflow.com/questions/3534535/whats-a-time-efficient-algorithm-to-copy-unaligned-bit-arrays
//

#define PREPARE_FIRST_COPY()                                      \
    do {                                                          \
    if (src_len >= (CHAR_BIT - dst_offset_modulo)) {              \
        *dst     &= reverse_mask[dst_offset_modulo];              \
        src_len -= CHAR_BIT - dst_offset_modulo;                  \
    } else {                                                      \
        *dst     &= reverse_mask[dst_offset_modulo]               \
              | reverse_mask_xor[dst_offset_modulo + src_len + 1];\
         c       &= reverse_mask[dst_offset_modulo + src_len    ];\
        src_len = 0;                                              \
    } } while (0)

//But copies bits in reverse?
static void
bitarray_copy(const uint8_t*src_org, int src_offset, int src_len,
                    uint8_t*dst_org, int dst_offset)
{
    static const unsigned char mask[] =
        { 0x55, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff };
    static const unsigned char reverse_mask[] =
        { 0x55, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff };
    static const unsigned char reverse_mask_xor[] =
        { 0xff, 0x7f, 0x3f, 0x1f, 0x0f, 0x07, 0x03, 0x01, 0x00 };

    if (src_len) {
        const unsigned char *src;
              unsigned char *dst;
        int                  src_offset_modulo,
                             dst_offset_modulo;

        src = src_org + (src_offset / CHAR_BIT);
        dst = dst_org + (dst_offset / CHAR_BIT);

        src_offset_modulo = src_offset % CHAR_BIT;
        dst_offset_modulo = dst_offset % CHAR_BIT;

        if (src_offset_modulo == dst_offset_modulo) {
            int              byte_len;
            int              src_len_modulo;
            if (src_offset_modulo) {
                unsigned char   c;

                c = reverse_mask_xor[dst_offset_modulo]     & *src++;

                PREPARE_FIRST_COPY();
                *dst++ |= c;
            }

            byte_len = src_len / CHAR_BIT;
            src_len_modulo = src_len % CHAR_BIT;

            if (byte_len) {
                memcpy(dst, src, byte_len);
                src += byte_len;
                dst += byte_len;
            }
            if (src_len_modulo) {
                *dst     &= reverse_mask_xor[src_len_modulo];
                *dst |= reverse_mask[src_len_modulo]     & *src;
            }
        } else {
            int             bit_diff_ls,
                            bit_diff_rs;
            int             byte_len;
            int             src_len_modulo;
            unsigned char   c;
            /*
             * Begin: Line things up on destination. 
             */
            if (src_offset_modulo > dst_offset_modulo) {
                bit_diff_ls = src_offset_modulo - dst_offset_modulo;
                bit_diff_rs = CHAR_BIT - bit_diff_ls;

                c = *src++ << bit_diff_ls;
                c |= *src >> bit_diff_rs;
                c     &= reverse_mask_xor[dst_offset_modulo];
            } else {
                bit_diff_rs = dst_offset_modulo - src_offset_modulo;
                bit_diff_ls = CHAR_BIT - bit_diff_rs;

                c = *src >> bit_diff_rs     &
                    reverse_mask_xor[dst_offset_modulo];
            }
            PREPARE_FIRST_COPY();
            *dst++ |= c;

            /*
             * Middle: copy with only shifting the source. 
             */
            byte_len = src_len / CHAR_BIT;

            while (--byte_len >= 0) {
                c = *src++ << bit_diff_ls;
                c |= *src >> bit_diff_rs;
                *dst++ = c;
            }

            /*
             * End: copy the remaing bits; 
             */
            src_len_modulo = src_len % CHAR_BIT;
            if (src_len_modulo) {
                c = *src++ << bit_diff_ls;
                c |= *src >> bit_diff_rs;
                c     &= reverse_mask[src_len_modulo];

                *dst     &= reverse_mask_xor[src_len_modulo];
                *dst |= c;
            }
        }
    }
}

typedef struct PADState {
	USBDevice dev;
	//nothing yet
} PADState;

/**
  linux hid-lg4ff.c
  http://www.spinics.net/lists/linux-input/msg16570.html
  Every Logitech wheel reports itself as generic Logitech Driving Force wheel (VID 046d, PID c294). This is done to ensure that the 
  wheel will work on every USB HID-aware system even when no Logitech driver is available. It however limits the capabilities of the 
  wheel - range is limited to 200 degrees, G25/G27 don't report the clutch pedal and there is only one combined axis for throttle and 
  brake. The switch to native mode is done via hardware-specific command which is different for each wheel. When the wheel 
  receives such command, it simulates reconnect and reports to the OS with its actual PID.
  Currently not emulating reattachment. Any games that expect to?
**/

//GT4 seems to disregard report desc.
//#define pad_hid_report_descriptor pad_driving_force_pro_hid_report_descriptor
//#define pad_hid_report_descriptor pad_momo_hid_report_descriptor
#define pad_hid_report_descriptor pad_driving_force_hid_report_descriptor

/* descriptor Logitech Driving Force Pro */
static /*const*/ uint8_t pad_dev_descriptor[] = {
	/* bLength             */ 0x12, //(18)
	/* bDescriptorType     */ 0x01, //(1)
	/* bcdUSB              */ WBVAL(0x0110), //(272) //USB 1.1
	/* bDeviceClass        */ 0x00, //(0)
	/* bDeviceSubClass     */ 0x00, //(0)
	/* bDeviceProtocol     */ 0x00, //(0)
	/* bMaxPacketSize0     */ 0x08, //(8)
	/* idVendor            */ WBVAL(0x046d),
	/* idProduct           */ WBVAL(DF_PID), //WBVAL(0xc294), 0xc298 dfp
	/* bcdDevice           */ WBVAL(0x0001), //(1)
	/* iManufacturer       */ 0x01, //(1)
	/* iProduct            */ 0x02, //(2)
	/* iSerialNumber       */ 0x00, //(0)
	/* bNumConfigurations  */ 0x01, //(1)

};

#define DESC_CONFIG_WORD(a) (a&0xFF),((a>>8)&0xFF)

static const uint8_t momo_config_descriptor[] = {
	0x09,   /* bLength */
	USB_CONFIGURATION_DESCRIPTOR_TYPE,    /* bDescriptorType */
	WBVAL(41),                        /* wTotalLength */
	0x01,                                 /* bNumInterfaces */
	0x01,                                 /* bConfigurationValue */
	0x00,                                 /* iConfiguration */
	USB_CONFIG_BUS_POWERED,               /* bmAttributes */
	USB_CONFIG_POWER_MA(80),              /* bMaxPower */

	/* Interface Descriptor */
	0x09,//sizeof(USB_INTF_DSC),   // Size of this descriptor in bytes
	0x04,                   // INTERFACE descriptor type
	0,                      // Interface Number
	0,                      // Alternate Setting Number
	2,                      // Number of endpoints in this intf
	USB_CLASS_HID,               // Class code
	0,     // Subclass code
	0,     // Protocol code
	0,                      // Interface string index

	/* HID Class-Specific Descriptor */
	0x09,//sizeof(USB_HID_DSC)+3,    // Size of this descriptor in bytes RRoj hack
	0x21,                // HID descriptor type
	DESC_CONFIG_WORD(0x0111),                 // HID Spec Release Number in BCD format (1.11)
	0x21,                   // Country Code (0x00 for Not supported, 0x21 for US)
	1,                      // Number of class descriptors, see usbcfg.h
	0x22,//DSC_RPT,                // Report descriptor type
	DESC_CONFIG_WORD(87),          // Size of the report descriptor

	/* Endpoint Descriptor */
	0x07,/*sizeof(USB_EP_DSC)*/
	0x05, //USB_DESCRIPTOR_ENDPOINT,    //Endpoint Descriptor
	0x1|0x80, //HID_EP | _EP_IN,        //EndpointAddress
	0x03, //_INTERRUPT,                 //Attributes
	DESC_CONFIG_WORD(16),        //size
	0xFF,                       //Interval, shouldn't this be infinite and updates get pushed as they happen?

	/* Endpoint Descriptor */
	0x07,/*sizeof(USB_EP_DSC)*/
	0x05, //USB_DESCRIPTOR_ENDPOINT,    //Endpoint Descriptor
	0x1|0x0, //HID_EP | _EP_OUT,        //EndpointAddress
	0x03, //_INTERRUPT,                 //Attributes
	DESC_CONFIG_WORD(16),        //size
	0xFF,                        //Interval
	//41 bytes
/* Terminator */
	0	/* bLength */
};

//https://lkml.org/lkml/2011/5/28/140
//https://github.com/torvalds/linux/blob/master/drivers/hid/hid-lg.c
// separate axes version
static const uint8_t pad_driving_force_hid_report_descriptor[] = {
	0x05, 0x01, /* Usage Page (Desktop), */
	0x09, 0x04, /* Usage (Joystik), */
	0xA1, 0x01, /* Collection (Application), */
	0xA1, 0x02, /* Collection (Logical), */
	0x95, 0x01, /* Report Count (1), */
	0x75, 0x0A, /* Report Size (10), */
	0x14, /* Logical Minimum (0), */
	0x26, 0xFF, 0x03, /* Logical Maximum (1023), */
	0x34, /* Physical Minimum (0), */
	0x46, 0xFF, 0x03, /* Physical Maximum (1023), */
	0x09, 0x30, /* Usage (X), */
	0x81, 0x02, /* Input (Variable), */
	0x95, 0x0C, /* Report Count (12), */
	0x75, 0x01, /* Report Size (1), */
	0x25, 0x01, /* Logical Maximum (1), */
	0x45, 0x01, /* Physical Maximum (1), */
	0x05, 0x09, /* Usage (Buttons), */
	0x19, 0x01, /* Usage Minimum (1), */
	0x29, 0x0c, /* Usage Maximum (12), */
	0x81, 0x02, /* Input (Variable), */
	0x95, 0x02, /* Report Count (2), */
	0x06, 0x00, 0xFF, /* Usage Page (Vendor: 65280), */
	0x09, 0x01, /* Usage (?: 1), */
	0x81, 0x02, /* Input (Variable), */
	0x05, 0x01, /* Usage Page (Desktop), */
	0x26, 0xFF, 0x00, /* Logical Maximum (255), */
	0x46, 0xFF, 0x00, /* Physical Maximum (255), */
	0x95, 0x01, /* Report Count (1), */
	0x75, 0x08, /* Report Size (8), */
	0x81, 0x02, /* Input (Variable), */
	0x25, 0x07, /* Logical Maximum (7), */
	0x46, 0x3B, 0x01, /* Physical Maximum (315), */
	0x75, 0x04, /* Report Size (4), */
	0x65, 0x14, /* Unit (Degrees), */
	0x09, 0x39, /* Usage (Hat Switch), */
	0x81, 0x42, /* Input (Variable, Null State), */
	0x75, 0x01, /* Report Size (1), */
	0x95, 0x04, /* Report Count (4), */
	0x65, 0x00, /* Unit (none), */
	0x06, 0x00, 0xFF, /* Usage Page (Vendor: 65280), */
	0x09, 0x01, /* Usage (?: 1), */
	0x25, 0x01, /* Logical Maximum (1), */
	0x45, 0x01, /* Physical Maximum (1), */
	0x81, 0x02, /* Input (Variable), */
	0x05, 0x01, /* Usage Page (Desktop), */
	0x95, 0x01, /* Report Count (1), */
	0x75, 0x08, /* Report Size (8), */
	0x26, 0xFF, 0x00, /* Logical Maximum (255), */
	0x46, 0xFF, 0x00, /* Physical Maximum (255), */
	0x09, 0x31, /* Usage (Y), */
	0x81, 0x02, /* Input (Variable), */
	0x09, 0x35, /* Usage (Rz), */
	0x81, 0x02, /* Input (Variable), */
	0xC0, /* End Collection, */
	0xA1, 0x02, /* Collection (Logical), */
	0x26, 0xFF, 0x00, /* Logical Maximum (255), */
	0x46, 0xFF, 0x00, /* Physical Maximum (255), */
	0x95, 0x07, /* Report Count (7), */
	0x75, 0x08, /* Report Size (8), */
	0x09, 0x03, /* Usage (?: 3), */
	0x91, 0x02, /* Output (Variable), */
	0xC0, /* End Collection, */
	0xC0 /* End Collection */
};

static const uint8_t pad_driving_force_pro_hid_report_descriptor[] = {
	0x05, 0x01, /* Usage Page (Desktop), */
	0x09, 0x04, /* Usage (Joystik), */
	0xA1, 0x01, /* Collection (Application), */
	0xA1, 0x02, /* Collection (Logical), */
	0x95, 0x01, /* Report Count (1), */
	0x75, 0x0E, /* Report Size (14), */
	0x14, /* Logical Minimum (0), */
	0x26, 0xFF, 0x3F, /* Logical Maximum (16383), */
	0x34, /* Physical Minimum (0), */
	0x46, 0xFF, 0x3F, /* Physical Maximum (16383), */
	0x09, 0x30, /* Usage (X), */
	0x81, 0x02, /* Input (Variable), */
	0x95, 0x0E, /* Report Count (14), */
	0x75, 0x01, /* Report Size (1), */
	0x25, 0x01, /* Logical Maximum (1), */
	0x45, 0x01, /* Physical Maximum (1), */
	0x05, 0x09, /* Usage Page (Button), */
	0x19, 0x01, /* Usage Minimum (01h), */
	0x29, 0x0E, /* Usage Maximum (0Eh), */
	0x81, 0x02, /* Input (Variable), */
	0x05, 0x01, /* Usage Page (Desktop), */
	0x95, 0x01, /* Report Count (1), */
	0x75, 0x04, /* Report Size (4), */
	0x25, 0x07, /* Logical Maximum (7), */
	0x46, 0x3B, 0x01, /* Physical Maximum (315), */
	0x65, 0x14, /* Unit (Degrees), */
	0x09, 0x39, /* Usage (Hat Switch), */
	0x81, 0x42, /* Input (Variable, Nullstate), */
	0x65, 0x00, /* Unit, */
	0x26, 0xFF, 0x00, /* Logical Maximum (255), */
	0x46, 0xFF, 0x00, /* Physical Maximum (255), */
	0x75, 0x08, /* Report Size (8), */
	0x81, 0x01, /* Input (Constant), */
	0x09, 0x31, /* Usage (Y), */
	0x81, 0x02, /* Input (Variable), */
	0x09, 0x35, /* Usage (Rz), */
	0x81, 0x02, /* Input (Variable), */
	0x81, 0x01, /* Input (Constant), */
	0xC0, /* End Collection, */
	0xA1, 0x02, /* Collection (Logical), */
	0x09, 0x02, /* Usage (02h), */
	0x95, 0x07, /* Report Count (7), */
	0x91, 0x02, /* Output (Variable), */
	0xC0, /* End Collection, */
	0xC0 /* End Collection */
};

static const uint8_t pad_momo_hid_report_descriptor[] = {
	0x05, 0x01, /* Usage Page (Desktop), */
	0x09, 0x04, /* Usage (Joystik), */
	0xA1, 0x01, /* Collection (Application), */
	0xA1, 0x02, /* Collection (Logical), */
	0x95, 0x01, /* Report Count (1), */
	0x75, 0x0A, /* Report Size (10), */
	0x14, 0x00, /* Logical Minimum (0), */
	0x25, 0xFF, 0x03, /* Logical Maximum (1023), */
	0x35, 0x00, /* Physical Minimum (0), */
	0x46, 0xFF, 0x03, /* Physical Maximum (1023), */
	0x09, 0x30, /* Usage (X), */
	0x81, 0x02, /* Input (Variable), */
	0x95, 0x08, /* Report Count (8), */
	0x75, 0x01, /* Report Size (1), */
	0x25, 0x01, /* Logical Maximum (1), */
	0x45, 0x01, /* Physical Maximum (1), */
	0x05, 0x09, /* Usage Page (Button), */
	0x19, 0x01, /* Usage Minimum (01h), */
	0x29, 0x08, /* Usage Maximum (08h), */
	0x81, 0x02, /* Input (Variable), */
	0x06, 0x00, 0xFF, /* Usage Page (FF00h), */
	0x75, 0x0E, /* Report Size (14), */
	0x95, 0x01, /* Report Count (1), */
	0x26, 0xFF, 0x00, /* Logical Maximum (255), */
	0x46, 0xFF, 0x00, /* Physical Maximum (255), */
	0x09, 0x00, /* Usage (00h), */
	0x81, 0x02, /* Input (Variable), */
	0x05, 0x01, /* Usage Page (Desktop), */
	0x75, 0x08, /* Report Size (8), */
	0x09, 0x31, /* Usage (Y), */
	0x81, 0x02, /* Input (Variable), */
	0x09, 0x32, /* Usage (Z), */
	0x81, 0x02, /* Input (Variable), */
	0x06, 0x00, 0xFF, /* Usage Page (FF00h), */
	0x09, 0x01, /* Usage (01h), */
	0x81, 0x02, /* Input (Variable), */
	0xC0, /* End Collection, */
	0xA1, 0x02, /* Collection (Logical), */
	0x09, 0x02, /* Usage (02h), */
	0x95, 0x07, /* Report Count (7), */
	0x91, 0x02, /* Output (Variable), */
	0xC0, /* End Collection, */
	0xC0 /* End Collection */
};

struct df_data_t
{
	uint32_t pad0 : 8;//report id?
	uint32_t axis_x : 10;
	uint32_t buttons : 12;
	uint32_t padding0 : 2; //??

	uint32_t vendor_stuff : 8;
	uint32_t hatswitch : 4;
	uint32_t something : 4; //??
	uint32_t axis_y : 8;
	uint32_t axis_rz : 8; //+56??
} df_data;

struct dfp_data_t
{
	//uint32_t pad0 : 8;
	uint32_t axis_x_1 : 14;
	uint32_t buttons : 12; //14
	// 32
	//uint32_t buttons_high : 4;
	uint32_t hatswitch : 4;
	uint32_t paddd : 2;

	uint32_t paddsd : 16;
	uint32_t axis_y : 8;
	uint32_t axis_rz : 8;
	
	//uint32_t axis_x : 8;
	//uint32_t pad : 2;
	
	//uint32_t axis_y : 10;
	//uint32_t axis_rz : 8;
	
} dfp_data;

struct dfp_buttons_t
{
	uint16_t cross : 1;
	uint16_t square : 1;
	uint16_t circle : 1;
	uint16_t triangle : 1;
	uint16_t rpaddle_R1 : 1;
	uint16_t lpaddle_L1 : 1;
	uint16_t R2 : 1;
	uint16_t L2 : 1;
	uint16_t select : 1;
	uint16_t start : 1;
	uint16_t R3 : 1;
	uint16_t L3 : 1;
	uint16_t shifter_back : 1;
	uint16_t shifter_fwd : 1;
	uint16_t padding : 2;
} dfp_buttons;

struct dfgt_buttons_t
{
	uint16_t cross : 1;
	uint16_t square : 1;
	uint16_t circle : 1;
	uint16_t triangle : 1;
	uint16_t rpaddle_R1 : 1;
	uint16_t lpaddle_L1 : 1;
	uint16_t R2 : 1;
	uint16_t L2 : 1;
	uint16_t select : 1;
	uint16_t start : 1;
	uint16_t R3 : 1;
	uint16_t L3 : 1;
	uint16_t shifter_back : 1;
	uint16_t shifter_fwd : 1;
	uint16_t dial_center : 1;
	uint16_t dial_cw : 1;

	uint16_t dial_ccw : 1;
	uint16_t rocker_minus : 1;
	uint16_t horn : 1;
	uint16_t ps_button : 1;
	uint16_t padding: 12;
} dfgt_buttons;

//fucking bitfields, keep types/size equal or vars start from beginning again
struct momo_data_t
{
	uint32_t pad0 : 8;//report id probably
	uint32_t axis_x : 10;
	uint32_t buttons : 10;
	uint32_t padding0 : 4;//32

	uint32_t padding1 : 8;
	uint32_t axis_y : 8;
	uint32_t axis_z : 8;
	uint32_t padding2 : 8;//32
} momo_data;

// convert momo to 'dumb-wheel' aka 0xC294
struct generic_data_t
{
	uint32_t axis_x : 10;
	uint32_t buttons : 10;
	uint32_t pad0 : 12;// 4bits to buttons, 8bits to axis??

	uint32_t axis_rz : 8;//clutch??
	uint32_t axis_y : 8;
	uint32_t axis_z : 8;
	uint32_t pad2 : 8;
} generic_data;

static int usb_pad_poll(PADState *s, uint8_t *buf, int len)
{
	uint8_t data[64];
	DWORD waitRes;
	
	//fprintf(stderr,"usb-pad: poll len=%li\n", len);
	if(doPassthrough)
	{
		//ZeroMemory(buf, len);
		ReadFile(usb_pad, buf, reportInSize, 0, &ovl);
		waitRes = WaitForSingleObject(ovl.hEvent, 30);
		if(waitRes == WAIT_TIMEOUT || waitRes == WAIT_ABANDONED)
			CancelIo(usb_pad);
		return len;
	}
	else
	{
		ZeroMemory(data, 64);
		// Be sure to read 'reportInSize' bytes or you get interleaved reads of data and garbage or something
		ReadFile(usb_pad, data, reportInSize, 0, &ovl);
		waitRes = WaitForSingleObject(ovl.hEvent, 30);
		// if the transaction timed out, then we have to manually cancel the request
		if(waitRes == WAIT_TIMEOUT || waitRes == WAIT_ABANDONED)
			CancelIo(usb_pad);
	}
	//fprintf(stderr, "\tData 0:%02X 8:%02X 16:%02X 24:%02X 32:%02X %02X %02X %02X\n", data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);

	/** try to get DFP working, faaail, only buttons **/
	/**
	ZeroMemory(buf, len);
	//bitarray_copy(data, 8, 10, buf, 0);
	//bitarray_copy(data, 28, 10, buf, 14 + 2); //buttons, 10 bits -> 12 bits
	int wheel = (data[1] | (data[2] & 0x3) << 8) * (0x3FFF/0x3FF) ;
	buf[0] = 0;//wheel & 0xFF;
	buf[1] = (wheel >> 8) & 0xFF;

	// Axis X
	buf[3] = 0x0;
	buf[4] = 0x0;
	buf[5] = 0x0;
	buf[6] = 0x0;
	buf[7] = 0x0;
	buf[8] = 0x0;
	buf[9] = 0x0;
	buf[10] = 0x0;
	buf[11] = 0x0;
	buf[12] = 0x0;
	buf[13] = 0x0;
	buf[14] = 0x0;
	buf[15] = 0x0;

	// MOMO to DFP buttons
	buf[1] |= (data[2] << 4) & 0xC0; //2 bits 2..3 -> 6..7
	buf[2] |= (data[2] >> 4) & 0xF; //4 bits 4..7 -> 0..3
	buf[2] |= (data[3] << 4) & 0xF0; //4 bits 0..3 -> 4..7
	**/

	//buf[1] = 1<<4; // 
	//buf[1] = 1<<5; // ??
	//buf[1] |= 1<<6; // cross 15
	//buf[1] = 1<<7; // square 16
	//buf[2] = 1<<0; // circle 17
	//buf[2] = 1<<1; // triangle 18
	//buf[2] |= 1<<2; // R1 gear up 19
	//buf[2] |= 1<<3; // L1 gear down 20
	//buf[2] |= 1<<4; // R2 21
	//buf[2] |= 1<<5; // L2 22
	//buf[2] |= 1<<6; // select 23
	//buf[2] |= 1<<7; // start??? 24
	
	//buf[3] = 1<<0; //25
	//buf[3] = 1<<1;//shift?? //26
	//buf[3] = 1<<2;//shift?? //27
	//buf[3] = 1<<3;//shift up?? //28
	//buf[3] |= 1<<4;//view right //29
	//buf[3] |= 1<<5;//view FR?? //30 // 4|5 bits view RL
	//buf[3] = 1<<6;//view back R2?? //31
	//buf[3] |= 1<<7;//view ahead?? //32
	//buf[4] = 0xff;//???

	//buf[5] = 0x0;//data[5]; //y -> y
	//buf[6] = 0xFF ;//data[6]; //z -> rz
	//buf[7] = 1<<4; //steer left
	//buf[10] = 0xFF;

	//fprintf(stderr, "\tData %02X %02X %02X %02X %02X %02X %02X %02X\n", data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
	//memcpy(&momo_data, data, sizeof(momo_data_t));
	//fprintf(stderr, "\tmomo_data: %d %04X %02X %02X\n",momo_data.axis_x, momo_data.buttons, momo_data.axis_y, momo_data.axis_z);
	//memcpy(buf, &data, len);
	//memset(&dfp_data, 0, sizeof(dfp_data_t));
	//dfp_data.buttons = momo_data.buttons;

	//dfp_data.axis_x = momo_data.axis_x ;// * (0x3fff/0x3ff);
	//dfp_data.axis_y = momo_data.axis_y;
	//dfp_data.axis_rz = momo_data.axis_z;
	
	//fprintf(stderr, "\tdfp_data: %04X %02X %02X\n", dfp_data.axis_x, dfp_data.axis_y, dfp_data.axis_rz);

	//memcpy(buf, &dfp_data, sizeof(dfp_data_t));

	/*df_data.axis_x = momo_data.axis_x;
	df_data.axis_y = momo_data.axis_y;
	df_data.axis_rz = momo_data.axis_z;
	df_data.buttons = momo_data.buttons;
	memcpy(buf, &df_data, sizeof(df_data));*/
	
	/*** raw version, MOMO to generic 0xC294, works kinda ***/
	/*ZeroMemory(&generic_data, sizeof(generic_data));
	memcpy(&momo_data, data, sizeof(momo_data_t));
	//memcpy(&generic_data, data, sizeof(momo_data_t));
	generic_data.buttons = momo_data.buttons;
	generic_data.axis_x = momo_data.axis_x;
	generic_data.axis_y = momo_data.axis_y;
	generic_data.axis_z = momo_data.axis_z;
	generic_data.axis_rz = 0xff; //set to 0xFF aka not pressed
	memcpy(buf, &generic_data, sizeof(generic_data_t));*/

	/** More generic version **/
	PHIDP_PREPARSED_DATA pPreparsedData;
	
	ZeroMemory(&generic_data, sizeof(generic_data));
	
	// Setting to unpressed
	generic_data.axis_y = 0xFF;
	generic_data.axis_z = 0xFF;
	generic_data.axis_rz = 0xFF;

	HidD_GetPreparsedData(usb_pad, &pPreparsedData);
	//HidP_GetCaps(pPreparsedData, &caps);

	/// Get button values
	HANDLE heap = GetProcessHeap();
	PHIDP_BUTTON_CAPS pButtonCaps = 
		(PHIDP_BUTTON_CAPS)HeapAlloc(heap, 0, sizeof(HIDP_BUTTON_CAPS) * caps.NumberInputButtonCaps);

	capsLength = caps.NumberInputButtonCaps;
	if(HidP_GetButtonCaps(HidP_Input, pButtonCaps, &capsLength, pPreparsedData) == HIDP_STATUS_SUCCESS )
		numberOfButtons = pButtonCaps->Range.UsageMax - pButtonCaps->Range.UsageMin + 1;

	ULONG usageLength = numberOfButtons;
	if(HidP_GetUsages(
			HidP_Input, pButtonCaps->UsagePage, 0, usage, &usageLength, pPreparsedData,
			(PCHAR)data, caps.InputReportByteLength) == HIDP_STATUS_SUCCESS )
	{
		// 10 from generic_data_t.buttons, maybe bring it to 12 bits
		for(ULONG i = 0; i < usageLength && i < 10; i++)
			generic_data.buttons |=  1 << (usage[i] - pButtonCaps->Range.UsageMin);
		//fprintf(stderr, "Buttons: %04X\n", generic_data.buttons);
	}


	/// Get axes values
	PHIDP_VALUE_CAPS pValueCaps
		= (PHIDP_VALUE_CAPS)HeapAlloc(heap, 0, sizeof(HIDP_VALUE_CAPS) * caps.NumberInputValueCaps);

	capsLength = caps.NumberInputValueCaps;
	if(HidP_GetValueCaps(HidP_Input, pValueCaps, &capsLength, pPreparsedData) == HIDP_STATUS_SUCCESS )
	{
		for(USHORT i = 0; i < capsLength; i++)
		{
			if(HidP_GetUsageValue(
					HidP_Input, pValueCaps[i].UsagePage, 0, 
					pValueCaps[i].Range.UsageMin, &value, pPreparsedData,
					(PCHAR)data, caps.InputReportByteLength
				) != HIDP_STATUS_SUCCESS )
			{
				continue; // if here then maybe something is up with HIDP_CAPS.NumberInputValueCaps
			}

			//fprintf(stderr, "Min/max %d/%d\t", pValueCaps[i].LogicalMin, pValueCaps[i].LogicalMax);
			switch(pValueCaps[i].Range.UsageMin)
			{
				case 0x30: // X-axis
					//lAxisX = (LONG)value - 128;
					//fprintf(stderr, "X: %d\n", value);
					generic_data.axis_x = value; // * (pValueCaps[i].LogicalMax / 1023);
					break;

				case 0x31: // Y-axis
					//lAxisY = (LONG)value - 128;
					//fprintf(stderr, "Y: %d\n", value);
					if(!(attr.VendorID == 0x046D && attr.ProductID == 0xCA03))
						generic_data.axis_y = value;
					break;

				case 0x32: // Z-axis
					//lAxisZ = (LONG)value - 128;
					//fprintf(stderr, "Z: %d\n", value);
					if(attr.VendorID == 0x046D && attr.ProductID == 0xCA03)
						generic_data.axis_y = value;//FIXME with MOMO for some reason :S
					else
						generic_data.axis_z = value;
					break;

				case 0x33: // Rotate-X
					//lAxisRx = (LONG)value - 128;
					//fprintf(stderr, "Rx: %d\n", value);
					break;
					
				case 0x34: // Rotate-Y
					//lAxisRy = (LONG)value - 128;
					//fprintf(stderr, "Ry: %d\n", value);
					break;

				case 0x35: // Rotate-Z
					//lAxisRz = (LONG)value - 128;
					//fprintf(stderr, "Rz: %d\n", value);
					if(attr.VendorID == 0x046D && attr.ProductID == 0xCA03)
						generic_data.axis_z = value;//FIXME with MOMO for some reason :S
					else
						generic_data.axis_rz = value;
					break;

				case 0x39: // Hat Switch
					//lHat = value;
					//fprintf(stderr, "Hat: %02X\n", value);
					break;
			}
		}
	}

	memcpy(buf, &generic_data, sizeof(generic_data_t));

	HeapFree(heap, 0, pButtonCaps);
	HeapFree(heap, 0, pValueCaps);
	HidD_FreePreparsedData(pPreparsedData);
	return len;
}

static int pad_handle_data(USBDevice *dev, int pid, 
							uint8_t devep, uint8_t *data, int len)
{
	PADState *s = (PADState *)dev;
	int ret = 0; 
	DWORD out = 0, err = 0, waitRes = 0;
	BOOL res;
	uint8_t outbuf[65];
	
	switch(pid) {
	case USB_TOKEN_IN:
		if (devep == 1 && usb_pad!=INVALID_HANDLE_VALUE) {
			ret = usb_pad_poll(s, data, len);
		}
		else{
			goto fail;
		}
		break;
	case USB_TOKEN_OUT:
		//fprintf(stderr,"usb-pad: data token out len=0x%X\n",len);
		//If i'm reading it correctly MOMO report size for output has Report Size(8) and Report Count(7), so that's 7 bytes
		//Now move that 7 bytes over by one and add report id of 0 (right?). Supposedly mandatory for HIDs.
		memcpy(outbuf + 1, data, len - 1);
		outbuf[0] = 0;
		//CancelIo(usb_pad); //FIXME overlapped gives ERROR_IO_PENDING, breaks FF ofcourse
		res = WriteFile(usb_pad, outbuf, reportOutSize, &out, &ovl);
		waitRes = WaitForSingleObject(ovl.hEvent, 30);
		if(waitRes == WAIT_TIMEOUT || waitRes == WAIT_ABANDONED)
			CancelIo(usb_pad);
		//err = GetLastError();
		//fprintf(stderr,"usb-pad: wrote %d, res: %d, err: %d\n", out, res, err);
		break;
	default:
	fail:
		ret = USB_RET_STALL;
		break;
	}
	return ret;
}

static void pad_handle_reset(USBDevice *dev)
{
	/* XXX: do it */
	return;
}

static int pad_handle_control(USBDevice *dev, int request, int value,
								  int index, int length, uint8_t *data)
{
	PADState *s = (PADState *)dev;
	int ret = 0;

	switch(request) {
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
		switch(value >> 8) {
		case USB_DT_DEVICE:
			if(doPassthrough)
			{
				pad_dev_descriptor[10] = 0xC2;
				pad_dev_descriptor[11] = 0x98;
			}

			memcpy(data, pad_dev_descriptor, 
				   sizeof(pad_dev_descriptor));
			ret = sizeof(pad_dev_descriptor);
			break;
		case USB_DT_CONFIG:
			memcpy(data, momo_config_descriptor, 
				sizeof(momo_config_descriptor));
			ret = sizeof(momo_config_descriptor);
			break;
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
			case 1:
				/* serial number */
				ret = set_usb_string(data, "");
				break;
			case 2:
				/* product description */
				ret = set_usb_string(data, "Driving Force Pro");
				break;
			case 3:
				/* vendor description */
				ret = set_usb_string(data, "Logitech");
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
	case DeviceOutRequest | USB_REQ_SET_INTERFACE:
		ret = 0;
		break;
		/* hid specific requests */
	case InterfaceRequest | USB_REQ_GET_DESCRIPTOR:
		fprintf(stderr, "InterfaceRequest | USB_REQ_GET_DESCRIPTOR %d\n", value>>8);
		switch(value >> 8) {
		case 0x22:
			fprintf(stderr, "Sending hid report desc.\n");
			memcpy(data, pad_hid_report_descriptor, 
				sizeof(pad_hid_report_descriptor));
			ret = sizeof(pad_hid_report_descriptor);
			break;
		default:
			goto fail;
		}
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

static void pad_handle_destroy(USBDevice *dev)
{
	PADState *s = (PADState *)dev;

	free(s);
}

int pad_handle_packet(USBDevice *s, int pid, 
							uint8_t devaddr, uint8_t devep,
							uint8_t *data, int len)
{
	//fprintf(stderr,"usb-pad: packet received with pid=%x, devaddr=%x, devep=%x and len=%x\n",pid,devaddr,devep,len);
	return usb_generic_handle_packet(s,pid,devaddr,devep,data,len);
}

USBDevice *pad_init()
{
	PADState *s;

	s = (PADState *)qemu_mallocz(sizeof(PADState));
	if (!s)
		return NULL;
	s->dev.speed = USB_SPEED_FULL;
	s->dev.handle_packet  = pad_handle_packet;
	s->dev.handle_reset   = pad_handle_reset;
	s->dev.handle_control = pad_handle_control;
	s->dev.handle_data    = pad_handle_data;
	s->dev.handle_destroy = pad_handle_destroy;

	// GT4 doesn't seem to care for a proper name?
	strncpy(s->dev.devname, "Driving Force Pro", sizeof(s->dev.devname));

	int i=0;
	DWORD needed=0;
	unsigned char buf[8];
	HDEVINFO devInfo;
	GUID guid;
	SP_DEVICE_INTERFACE_DATA diData;
	PSP_DEVICE_INTERFACE_DETAIL_DATA didData;

	readData=CreateEvent(0, 0, 0, 0);
	memset(&ovl, 0, sizeof(OVERLAPPED));
	ovl.hEvent=readData;
	ovl.Offset=0;
	ovl.OffsetHigh=0;

	HidD_GetHidGuid(&guid);
	
	devInfo=SetupDiGetClassDevs(&guid, 0, 0, DIGCF_DEVICEINTERFACE);
	if(!devInfo)return 0;
	
	diData.cbSize=sizeof(diData);

	while(SetupDiEnumDeviceInterfaces(devInfo, 0, &guid, i, &diData)){
		if(usb_pad!=INVALID_HANDLE_VALUE)CloseHandle(usb_pad);

		SetupDiGetDeviceInterfaceDetail(devInfo, &diData, 0, 0, &needed, 0);

		didData=(PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(needed);
		didData->cbSize=sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
		if(!SetupDiGetDeviceInterfaceDetail(devInfo, &diData, didData, needed, 0, 0)){
			free(didData);
			break;
		}

		usb_pad=CreateFile(didData->DevicePath, GENERIC_READ|GENERIC_WRITE, 
			FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
		if(usb_pad==INVALID_HANDLE_VALUE){
			fprintf(stderr,"Could not open device %i\n", i);
			free(didData);
			i++;
			continue;
		}

		HidD_GetAttributes(usb_pad, &attr);
		PHIDP_PREPARSED_DATA pp_data;
		HidD_GetPreparsedData(usb_pad, &pp_data);

		HidP_GetCaps(pp_data, &caps);
		reportInSize = caps.InputReportByteLength;
		reportOutSize = caps.OutputReportByteLength;
		ULONG node_len = caps.NumberLinkCollectionNodes * sizeof(HIDP_LINK_COLLECTION_NODE);
		PHIDP_LINK_COLLECTION_NODE pnode = (PHIDP_LINK_COLLECTION_NODE)malloc(node_len);
			
		HidP_GetLinkCollectionNodes(pnode, &node_len, pp_data);

		bool gotit = false;
		if(caps.UsagePage == HID_USAGE_PAGE_GENERIC &&
			caps.Usage == HID_USAGE_GENERIC_JOYSTICK)
			gotit = true;

		free(pnode);
		HidD_FreePreparsedData(pp_data);

		fprintf(stderr, "Device %i : VID %04X PID %04X\n", i, attr.VendorID, attr.ProductID);

		//if((attr.VendorID==PAD_VID) && 
		//	(attr.ProductID==PAD_PID || attr.ProductID==DFP_PID))
		if(gotit)
		{
			if(attr.ProductID==DFP_PID)
				doPassthrough = true;
			free(didData);
			fprintf(stderr, "Wheel found !!! %04X:%04X\n", attr.VendorID, attr.ProductID);
			break;
		}
		i++;
	}

	if(usb_pad==INVALID_HANDLE_VALUE)
		fprintf(stderr, "Could not find wheels\n");
	return (USBDevice *)s;

}

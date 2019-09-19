/*
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

#include "videodeviceproxy.h"
#include "../osdebugout.h"
#include "usb-eyetoy-webcam.h"
#include "ov519.h"
#include "../qemu-usb/desc.h"

namespace usb_eyetoy {

#include "../../cut2.cpp"
const int mjpg_frame_size = sizeof(mjpg_frame);

static const USBDescStrings desc_strings = {
	"",
	"Sony corporation",
	"EyeToy USB camera Namtai",
};

typedef struct EYETOYState {
	USBDevice dev;
	USBDesc desc;
	USBDescDevice desc_dev;

	VideoDevice *videodev;
//	struct freeze {
	uint8_t regs[0xFF]; //OV519
	uint8_t i2c_regs[0xFF]; //OV764x

	int frame_offset;
	uint8_t alts[3];
	uint8_t filter_log;
//	} f;
} EYETOYState;

void EyeToyWebCamDevice::Initialize()
{
	RegisterVideoDevice::Initialize();
}

std::list<std::string> EyeToyWebCamDevice::ListAPIs()
{
	return RegisterVideoDevice::instance().Names();
}

const TCHAR* EyeToyWebCamDevice::LongAPIName(const std::string& name)
{
	return RegisterVideoDevice::instance().Proxy(name)->Name();
}

/*
	Manufacturer:   OmniVision Technologies, Inc.
	Product ID:     0x8519
	Vendor ID:      0x05a9

	Device VendorID/ProductID:   0x054C/0x0155   (Sony Corporation)
	Device Version Number:   0x0100
	Number of Configurations:   1
	Manufacturer String:   1 "Sony corporation"
	Product String:   2 "EyeToy USB camera Namtai"
*/

static const uint8_t eyetoy_dev_descriptor[] = {
	0x12, /* bLength */
	0x01, /* bDescriptorType */
	WBVAL(0x0110), /* bcdUSB */
	0x00, /* bDeviceClass */
	0x00, /* bDeviceSubClass */
	0x00, /* bDeviceProtocol */
	0x08, /* bMaxPacketSize0 */
	WBVAL(0x054c), /* idVendor */
	WBVAL(0x0155), /* idProduct */
	WBVAL(0x0100), /* bcdDevice */
	0x01, /* iManufacturer */
	0x02, /* iProduct */
	0x00, /* iSerialNumber */
	0x01, /* bNumConfigurations */
};

/* XXX: patch interrupt size */
static const uint8_t eyetoy_config_descriptor[] = {
	0x09,        // bLength
	0x02,        // bDescriptorType (Configuration)
	0xB4, 0x00,  // wTotalLength 180
	0x03,        // bNumInterfaces 3
	0x01,        // bConfigurationValue
	0x00,        // iConfiguration (String Index)
	0x90,        // bmAttributes
	0xFA,        // bMaxPower 500mA

	0x09,        // bLength
	0x04,        // bDescriptorType (Interface)
	0x00,        // bInterfaceNumber 0
	0x00,        // bAlternateSetting
	0x01,        // bNumEndpoints 1
	0xFF,        // bInterfaceClass
	0x00,        // bInterfaceSubClass
	0x00,        // bInterfaceProtocol
	0x00,        // iInterface (String Index)

	0x07,        // bLength
	0x05,        // bDescriptorType (Endpoint)
	0x81,        // bEndpointAddress (IN/D2H)
	0x01,        // bmAttributes (Isochronous, No Sync, Data EP)
	0x00, 0x00,  // wMaxPacketSize 0
	0x01,        // bInterval 1 (unit depends on device speed)

	0x09,        // bLength
	0x04,        // bDescriptorType (Interface)
	0x00,        // bInterfaceNumber 0
	0x01,        // bAlternateSetting
	0x01,        // bNumEndpoints 1
	0xFF,        // bInterfaceClass
	0x00,        // bInterfaceSubClass
	0x00,        // bInterfaceProtocol
	0x00,        // iInterface (String Index)

	0x07,        // bLength
	0x05,        // bDescriptorType (Endpoint)
	0x81,        // bEndpointAddress (IN/D2H)
	0x01,        // bmAttributes (Isochronous, No Sync, Data EP)
	0x80, 0x01,  // wMaxPacketSize 384
	0x01,        // bInterval 1 (unit depends on device speed)

	0x09,        // bLength
	0x04,        // bDescriptorType (Interface)
	0x00,        // bInterfaceNumber 0
	0x02,        // bAlternateSetting
	0x01,        // bNumEndpoints 1
	0xFF,        // bInterfaceClass
	0x00,        // bInterfaceSubClass
	0x00,        // bInterfaceProtocol
	0x00,        // iInterface (String Index)

	0x07,        // bLength
	0x05,        // bDescriptorType (Endpoint)
	0x81,        // bEndpointAddress (IN/D2H)
	0x01,        // bmAttributes (Isochronous, No Sync, Data EP)
	0x00, 0x02,  // wMaxPacketSize 512
	0x01,        // bInterval 1 (unit depends on device speed)

	0x09,        // bLength
	0x04,        // bDescriptorType (Interface)
	0x00,        // bInterfaceNumber 0
	0x03,        // bAlternateSetting
	0x01,        // bNumEndpoints 1
	0xFF,        // bInterfaceClass
	0x00,        // bInterfaceSubClass
	0x00,        // bInterfaceProtocol
	0x00,        // iInterface (String Index)

	0x07,        // bLength
	0x05,        // bDescriptorType (Endpoint)
	0x81,        // bEndpointAddress (IN/D2H)
	0x01,        // bmAttributes (Isochronous, No Sync, Data EP)
	0x00, 0x03,  // wMaxPacketSize 768
	0x01,        // bInterval 1 (unit depends on device speed)

	0x09,        // bLength
	0x04,        // bDescriptorType (Interface)
	0x00,        // bInterfaceNumber 0
	0x04,        // bAlternateSetting
	0x01,        // bNumEndpoints 1
	0xFF,        // bInterfaceClass
	0x00,        // bInterfaceSubClass
	0x00,        // bInterfaceProtocol
	0x00,        // iInterface (String Index)

	0x07,        // bLength
	0x05,        // bDescriptorType (Endpoint)
	0x81,        // bEndpointAddress (IN/D2H)
	0x01,        // bmAttributes (Isochronous, No Sync, Data EP)
	0x80, 0x03,  // wMaxPacketSize 896
	0x01,        // bInterval 1 (unit depends on device speed)

	0x09,        // bLength
	0x04,        // bDescriptorType (Interface)
	0x01,        // bInterfaceNumber 1
	0x00,        // bAlternateSetting
	0x00,        // bNumEndpoints 0
	0x01,        // bInterfaceClass (Audio)
	0x01,        // bInterfaceSubClass (Audio Control)
	0x00,        // bInterfaceProtocol
	0x00,        // iInterface (String Index)

	0x09,        // bLength
	0x24,        // bDescriptorType (See Next Line)
	0x01,        // bDescriptorSubtype (CS_INTERFACE -> HEADER)
	0x00, 0x01,  // bcdADC 1.00
	0x1E, 0x00,  // wTotalLength 30
	0x01,        // binCollection 0x01
	0x02,        // baInterfaceNr 2

	0x0C,        // bLength
	0x24,        // bDescriptorType (See Next Line)
	0x02,        // bDescriptorSubtype (CS_INTERFACE -> INPUT_TERMINAL)
	0x01,        // bTerminalID
	0x01, 0x02,  // wTerminalType (Microphone)
	0x00,        // bAssocTerminal
	0x01,        // bNrChannels 1
	0x00, 0x00,  // wChannelConfig
	0x00,        // iChannelNames
	0x00,        // iTerminal

	0x09,        // bLength
	0x24,        // bDescriptorType (See Next Line)
	0x03,        // bDescriptorSubtype (CS_INTERFACE -> OUTPUT_TERMINAL)
	0x02,        // bTerminalID
	0x01, 0x01,  // wTerminalType (USB Streaming)
	0x00,        // bAssocTerminal
	0x01,        // bSourceID
	0x00,        // iTerminal

	0x09,        // bLength
	0x04,        // bDescriptorType (Interface)
	0x02,        // bInterfaceNumber 2
	0x00,        // bAlternateSetting
	0x00,        // bNumEndpoints 0
	0x01,        // bInterfaceClass (Audio)
	0x02,        // bInterfaceSubClass (Audio Streaming)
	0x00,        // bInterfaceProtocol
	0x00,        // iInterface (String Index)

	0x09,        // bLength
	0x04,        // bDescriptorType (Interface)
	0x02,        // bInterfaceNumber 2
	0x01,        // bAlternateSetting
	0x01,        // bNumEndpoints 1
	0x01,        // bInterfaceClass (Audio)
	0x02,        // bInterfaceSubClass (Audio Streaming)
	0x00,        // bInterfaceProtocol
	0x00,        // iInterface (String Index)

	0x07,        // bLength
	0x24,        // bDescriptorType (See Next Line)
	0x01,        // bDescriptorSubtype (CS_INTERFACE -> AS_GENERAL)
	0x02,        // bTerminalLink
	0x01,        // bDelay 1
	0x01, 0x00,  // wFormatTag (PCM)

	0x0B,        // bLength
	0x24,        // bDescriptorType (See Next Line)
	0x02,        // bDescriptorSubtype (CS_INTERFACE -> FORMAT_TYPE)
	0x01,        // bFormatType 1
	0x01,        // bNrChannels (Mono)
	0x02,        // bSubFrameSize 2
	0x10,        // bBitResolution 16
	0x01,        // bSamFreqType 1
	0x80, 0x3E, 0x00,  // tSamFreq[1] 16000 Hz

	0x09,        // bLength
	0x05,        // bDescriptorType (See Next Line)
	0x82,        // bEndpointAddress (IN/D2H)
	0x05,        // bmAttributes (Isochronous, Async, Data EP)
	0x28, 0x00,  // wMaxPacketSize 40
	0x01,        // bInterval 1 (unit depends on device speed)
	0x00,        // bRefresh
	0x00,        // bSyncAddress

	0x07,        // bLength
	0x25,        // bDescriptorType (See Next Line)
	0x01,        // bDescriptorSubtype (CS_ENDPOINT -> EP_GENERAL)
	0x00,        // bmAttributes (None)
	0x00,        // bLockDelayUnits
	0x00, 0x00,  // wLockDelay 0
};

static void reset_i2c(EYETOYState *s)
{
	/* OV7648 defaults */
	s->i2c_regs[0x00] = 0x00; //gain
	s->i2c_regs[0x01] = 0x80; //blue
	s->i2c_regs[0x02] = 0x80; //red
	s->i2c_regs[0x03] = 0x84; //saturation
	s->i2c_regs[0x04] = 0x34; //hue
	s->i2c_regs[0x05] = 0x3E; //AWB
	s->i2c_regs[0x06] = 0x80; //ABC brightness
	s->i2c_regs[0x0A] = 0x76; //Product ID r/o
	s->i2c_regs[0x0B] = 0x48; //Product version r/o
	s->i2c_regs[0x10] = 0x41; //exposure
	s->i2c_regs[0x11] = 0x00; //clk
	s->i2c_regs[0x12] = 0x14; //Common A
	s->i2c_regs[0x13] = 0xA3; //Common B
	s->i2c_regs[0x14] = 0x04; //Common C
	s->i2c_regs[0x15] = 0x00; //Common D
	s->i2c_regs[0x17] = 0x1A; //hstart
	s->i2c_regs[0x18] = 0xBA; //hstop
	s->i2c_regs[0x19] = 0x03; //vstart
	s->i2c_regs[0x1A] = 0xF3; //vstop
	s->i2c_regs[0x1B] = 0x00; //pshift
	s->i2c_regs[0x1C] = 0x7F; //Manufacture ID High read-only
	s->i2c_regs[0x1D] = 0xA2; //Manufacture ID Low read-only
	s->i2c_regs[0x1F] = 0x01; //output format
	s->i2c_regs[0x20] = 0xC0; //Common E
	s->i2c_regs[0x24] = 0x10; //AEW
	s->i2c_regs[0x25] = 0x8A; //AEB
	s->i2c_regs[0x26] = 0xA2; //Common F
	s->i2c_regs[0x27] = 0xE2; //Common G
	s->i2c_regs[0x28] = 0x20; //Common H
	s->i2c_regs[0x29] = 0x00; //Common I
	s->i2c_regs[0x2A] = 0x00; //Frame rate adj HI
	s->i2c_regs[0x2B] = 0x00; //Frame rate adj LO
	s->i2c_regs[0x2D] = 0x81; //Common J
	s->i2c_regs[0x60] = 0x06; //Signal process B
	s->i2c_regs[0x6C] = 0x11; //Color matrix R
	s->i2c_regs[0x6D] = 0x01; //Color matrix G
	s->i2c_regs[0x6E] = 0x06; //Color matrix B
	s->i2c_regs[0x71] = 0x00; //Common L
	s->i2c_regs[0x72] = 0x10; //HSYNC rising
	s->i2c_regs[0x73] = 0x50; //HSYNC falling
	s->i2c_regs[0x74] = 0x20; //Common M
	s->i2c_regs[0x75] = 0x02; //Common N
	s->i2c_regs[0x76] = 0x00; //Common O
	s->i2c_regs[0x7E] = 0x00; //AVGY
	s->i2c_regs[0x7F] = 0x00; //AVGR
	s->i2c_regs[0x80] = 0x00; //AVGB
}

static void eyetoy_handle_reset(USBDevice *dev)
{
	reset_i2c((EYETOYState *)dev);
}

static void eyetoy_handle_control(USBDevice *dev, USBPacket *p, int request, int value,
								  int index, int length, uint8_t *data)
{
	EYETOYState *s = (EYETOYState *)dev;
	int ret = 0;

    ret = usb_desc_handle_control(dev, p, request, value, index, length, data);
    if (ret >= 0) {
        return;
    }

	switch(request) {
	case VendorDeviceRequest | 0x1: //Read register
		data[0] = s->regs[index & 0xFF];
		OSDebugOut(TEXT("=== READ  reg 0x%02x = 0x%02x (%d)\n"), index, data[0], data[0]);
		p->actual_length = 1;
		break;

	case VendorDeviceOutRequest | 0x1: //Write register
		if (!(index >= R51x_I2C_SADDR_3 && index <= R518_I2C_CTL))
		OSDebugOut(TEXT("*** WRITE reg 0x%02x = 0x%02x (%d)\n"), index, data[0], data[0]);

		switch (index)
		{
			case OV519_R51_RESET1:
				if (data[0] & 0x8)
				{
					// reset video FIFO
					//s->videodev->SetSize(s->regs[OV519_R10_H_SIZE] << 4, s->regs[OV519_R11_V_SIZE] << 3);
				}
				break;
			case OV519_R10_H_SIZE:
				OSDebugOut(TEXT("Set width %d\n"), data[0]<<4);
				break;
			case OV519_R11_V_SIZE:
				OSDebugOut(TEXT("Set height %d\n"), data[0]<<3);
				break;
			case R518_I2C_CTL:
				//OSDebugOut(TEXT("Set R518_I2C_CTL %d %d\n"), s->regs[R518_I2C_CTL], data[0]);
				if (data[0] == 1) // Commit I2C write
				{
					//uint8_t reg = s->regs[s->regs[R51x_I2C_W_SID]];
					uint8_t reg = s->regs[R51x_I2C_SADDR_3];
					uint8_t val = s->regs[R51x_I2C_DATA];
					if ((reg == 0x12) && (val & 0x80))
					{
						s->i2c_regs[0x12] = val & ~0x80; //or skip?
						reset_i2c(s);
					}
					else if (reg < sizeof(s->i2c_regs))
					{
						s->i2c_regs[reg] = val;
						OSDebugOut(TEXT("I2C write to 0x%02x = 0x%02x\n"), reg, val);
					}
					else
						OSDebugOut(TEXT("I2C write out-of-bounds\n"));
				}
				else if (s->regs[R518_I2C_CTL] == 0x03 && data[0] == 0x05)
				{
					//s->regs[s->regs[R51x_I2C_R_SID]] but seems to default to 0x43 (R51x_I2C_SADDR_2)
					uint8_t i2c_reg = s->regs[R51x_I2C_SADDR_2];
					s->regs[R51x_I2C_DATA] = 0;

					if (i2c_reg < sizeof(s->i2c_regs))
					{
						s->regs[R51x_I2C_DATA] = s->i2c_regs[i2c_reg];
						OSDebugOut(TEXT("I2C read from 0x%02x = 0x%02x\n"), i2c_reg, s->regs[R51x_I2C_DATA]);
					}
					else
						OSDebugOut(TEXT("Unhandled read from I2C: 0x%02x\n"), s->regs[R51x_I2C_SADDR_2]);
				}
				break;
			default:
			break;
		}

		//Max 0xFFFF regs?
		s->regs[index & 0xFF] = data[0];
		p->actual_length = 1;

		break;
	default:
	fail:
	OSDebugOut(TEXT("default ******************* %04x\n"), request);
		p->status = USB_RET_STALL;
		break;
	}
}

static void eyetoy_handle_data(USBDevice *dev, USBPacket *p)
{
	EYETOYState *s = (EYETOYState *)dev;
	uint8_t data[896];
	int ret = 0;
	uint8_t devep = p->ep->nr;
	size_t len = p->iov.size;

	switch(p->pid) {
	case USB_TOKEN_IN:
		if (devep == 1) {
			// get image
			/* Header of ov519 is 16 bytes:
			*  Byte  Value   Description
			*  0     0xff    magic
			*  1     0xff    magic
			*  2     0xff    magic
			*  3     0xXX    0x50 = SOF, 0x51 = EOF
			*  9     0xXX    0x01 initial frame without data,
			*                0x00 standard frame with image
			*  14    Lo      in EOF: length of image data / 8
			*  15    Hi
			* 
			* start:
			*  ff ff ff 50
			*  00
			*  7e 1b 63  67
			*  00
			*  03 00 00 00 10 4b
			* last:
			*  ff ff ff 51
			*  00 6b e1 87  67
			*  00
			*  03 00 4b 0e 90 0b
			* next
			*  ff ff ff 50
			*  00
			*  7b 08 88  67
			*  00
			*  03 00 00 00 0e 4b
			*/
			int sz = std::min((int)len - 1, mjpg_frame_size - s->frame_offset);
			static int counter = 0;
			memset(data, 0, 16);
			data[0] = 0xFF;
			data[1] = 0xFF;
			data[2] = 0xFF;
			
			data[10] = 0x03;
			
			counter = 1 + counter % 255;
			if (s->frame_offset == 0) {
				data[3] = 0x50;
				*(uint16_t*)&data[14] = mjpg_frame_size / 8;
				memcpy(data + 16, &mjpg_frame[s->frame_offset], sz - 16);
				s->frame_offset += sz - 16;
				s->regs[0xea] ++;
				counter = 0;
			}
			else if (s->frame_offset >= mjpg_frame_size) {
				s->frame_offset = 0;
				data[3] = 0x51;
				data[9] = 0x1; // discard, no data frame
				*(uint16_t*)&data[14] = mjpg_frame_size / 8;
				sz = 16;
				fprintf(stderr, "last packet, nodata\n");
			}
			else if (s->frame_offset + sz >= mjpg_frame_size && sz + 16 <= len) {
				fprintf(stderr, "last %d %d\n", s->frame_offset, sz);
				sz = (mjpg_frame_size - s->frame_offset) + 16;
				data[3] = 0x51;
				*(uint16_t*)&data[14] = mjpg_frame_size / 8;
				memcpy(data + 16, &mjpg_frame[s->frame_offset], sz - 16);
				s->frame_offset += sz - 16;
			}
			else
			{
				//memcpy(data, &mjpg_frame[s->frame_offset], sz);
				memset(data, (s->frame_offset>>2) & 0xFF, sz);
				s->frame_offset += sz;
			}
			fprintf(stderr, "memcpy %02x len %d sz %d %d/%d\n", data[3], len, sz, s->frame_offset, mjpg_frame_size);

			data[sz] = counter;
			usb_packet_copy (p, data, sz + 1);
		}
		else if (devep == 2) {
			// get audio
			//fprintf(stderr, "get audio %d\n", len);
			memset(data, 0, p->iov.size);
			usb_packet_copy (p, data, p->iov.size);
		}
		break;
	case USB_TOKEN_OUT:
	default:
	fail:
		p->status = USB_RET_STALL;
		break;
	}
}

static void eyetoy_handle_destroy(USBDevice *dev)
{
	EYETOYState *s = (EYETOYState *)dev;

	delete s;
}

int eyetoy_open(USBDevice *dev)
{
	EYETOYState *s = (EYETOYState *) dev;
	return 1;
}

void eyetoy_close(USBDevice *dev)
{
	EYETOYState *s = (EYETOYState *) dev;
}

USBDevice *EyeToyWebCamDevice::CreateDevice(int port)
{
	VideoDevice *videodev = nullptr;
	/*CONFIGVARIANT varApi(N_DEVICE_API, CONFIG_TYPE_CHAR);
	LoadSetting(port, DEVICENAME, varApi);
	VideoDeviceProxyBase *proxy = RegisterVideoDevice::instance().Proxy(varApi.strValue);
	if (!proxy)
	{
		SysMessage(TEXT("Invalid video device API: " SFMTs "\n"), varApi.strValue.c_str());
		return NULL;
	}

	videodev = proxy->CreateObject(port);

	if (!videodev)
		return NULL;*/

	EYETOYState *s;

	s = new EYETOYState();
	if (!s)
		return NULL;

	s->desc.full = &s->desc_dev;
	s->desc.str = desc_strings;

	if (usb_desc_parse_dev(eyetoy_dev_descriptor, sizeof(eyetoy_dev_descriptor), s->desc, s->desc_dev) < 0)
		goto fail;
	if (usb_desc_parse_config(eyetoy_config_descriptor, sizeof(eyetoy_config_descriptor), s->desc_dev) < 0)
		goto fail;

	s->videodev = videodev;
	s->dev.speed = USB_SPEED_FULL;
	s->dev.klass.handle_attach = usb_desc_attach;
	s->dev.klass.handle_reset = eyetoy_handle_reset;
	s->dev.klass.handle_control = eyetoy_handle_control;
	s->dev.klass.handle_data = eyetoy_handle_data;
	s->dev.klass.unrealize = eyetoy_handle_destroy;
	s->dev.klass.open = eyetoy_open;
	s->dev.klass.close = eyetoy_close;
	s->dev.klass.usb_desc = &s->desc;
	s->dev.klass.product_desc = s->desc.str[2];

	usb_desc_init(&s->dev);
	usb_ep_init(&s->dev);
	eyetoy_handle_reset((USBDevice *)s);

	reset_i2c(s);
	s->frame_offset = 0;
	s->regs[OV519_R10_H_SIZE] = 320>>4;
	s->regs[OV519_R11_V_SIZE] = 240>>3;

	return (USBDevice *)s;
fail:
	eyetoy_handle_destroy((USBDevice *)s);
	return nullptr;
}

int EyeToyWebCamDevice::Configure(int port, const std::string& api, void *data)
{
	auto proxy = RegisterVideoDevice::instance().Proxy(api);
	if (proxy)
		return proxy->Configure(port, TypeName(), data);
	return RESULT_CANCELED;
}

int EyeToyWebCamDevice::Freeze(int mode, USBDevice *dev, void *data)
{
	EYETOYState *s = (EYETOYState *)dev;

	/*switch (mode)
	{
		case FREEZE_LOAD:
			if (!s) return -1;
			s->f = *(PADState::freeze *)data;
			s->pad->Type((PS2WheelTypes)s->f.wheel_type);
			return sizeof(PADState::freeze);
		case FREEZE_SAVE:
			if (!s) return -1;
			*(PADState::freeze *)data = s->f;
			return sizeof(PADState::freeze);
		case FREEZE_SIZE:
			return sizeof(PADState::freeze);
		default:
		break;
	}*/
	return -1;
}

REGISTER_DEVICE(DEVTYPE_EYETOY_WEBCAM, EyeToyWebCamDevice);
} //namespace
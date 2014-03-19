#ifndef USBPAD_H
#define USBPAD_H

#include "../qemu-usb/vl.h"

#include "../usb-mic/type.h"
#include "../usb-mic/usb.h"
#include "../usb-mic/usbcfg.h"
#include "../usb-mic/usbdesc.h"

#include "config.h"

void 
bitarray_copy(const uint8_t*src_org, int src_offset, int src_len,
                    uint8_t*dst_org, int dst_offset);

#ifdef _WIN32

	#if BUILD_RAW
	PADState* get_new_raw_padstate();
	#endif

	#if BUILD_DX
	PADState* get_new_dx_padstate();
	#endif

#else

PADState* get_new_padstate();

#endif

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

// Any game actually queries for hid reports?
//#define pad_hid_report_descriptor pad_driving_force_pro_hid_report_descriptor
//#define pad_hid_report_descriptor pad_momo_hid_report_descriptor
//#define pad_hid_report_descriptor pad_generic_hid_report_descriptor

/* descriptor Logitech Driving Force Pro */
static /*const*/ uint8_t dfp_dev_descriptor[] = {
	/* bLength             */ 0x12, //(18)
	/* bDescriptorType     */ 0x01, //(1)
	/* bcdUSB              */ WBVAL(0x0110), //(272) //USB 1.1
	/* bDeviceClass        */ 0x00, //(0)
	/* bDeviceSubClass     */ 0x00, //(0)
	/* bDeviceProtocol     */ 0x00, //(0)
	/* bMaxPacketSize0     */ 0x08, //(8)
	/* idVendor            */ WBVAL(0x046d),
	/* idProduct           */ WBVAL(DFP_PID),
	/* bcdDevice           */ WBVAL(0x0001), //(1)
	/* iManufacturer       */ 0x03, //(1)
	/* iProduct            */ 0x01, //(2)
	/* iSerialNumber       */ 0x00, //(0)
	/* bNumConfigurations  */ 0x01, //(1)
};

static /*const*/ uint8_t pad_dev_descriptor[] = {
	/* bLength             */ 0x12, //(18)
	/* bDescriptorType     */ 0x01, //(1)
	/* bcdUSB              */ WBVAL(0x0110), //(272) //USB 1.1
	/* bDeviceClass        */ 0x00, //(0)
	/* bDeviceSubClass     */ 0x00, //(0)
	/* bDeviceProtocol     */ 0x00, //(0)
	/* bMaxPacketSize0     */ 0x08, //(8)
	/* idVendor            */ WBVAL(0x046d),
	/* idProduct           */ WBVAL(GENERIC_PID), //WBVAL(0xc294), 0xc298 dfp
	/* bcdDevice           */ WBVAL(0x0001), //(1)
	/* iManufacturer       */ 0x01, //(1)
	/* iProduct            */ 0x02, //(2)
	/* iSerialNumber       */ 0x00, //(0)
	/* bNumConfigurations  */ 0x01, //(1)

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

static const uint8_t pad_generic_hid_report_descriptor[] = {
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
	0x95, 0x0a, /* Report Count (10), */
	0x75, 0x01, /* Report Size (1), */
	0x25, 0x01, /* Logical Maximum (1), */
	0x45, 0x01, /* Physical Maximum (1), */
	0x05, 0x09, /* Usage Page (Button), */
	0x19, 0x01, /* Usage Minimum (01h), */
	0x29, 0x0a, /* Usage Maximum (0ah), */
	0x81, 0x02, /* Input (Variable), */
	0x06, 0x00, 0xFF, /* Usage Page (FF00h), */
	0x75, 0x0C, /* Report Size (12), */
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
	0x09, 0x35, /* Usage (RZ), */
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

#define USB_PSIZE 8
#define DESC_CONFIG_WORD(a) (a&0xFF),((a>>8)&0xFF)

static const uint8_t df_config_descriptor[] = {
	0x09,   /* bLength */
	USB_CONFIGURATION_DESCRIPTOR_TYPE,    /* bDescriptorType */
	WBVAL(41),                        /* wTotalLength */
	0x01,                                 /* bNumInterfaces */
	0x01,                                 /* bConfigurationValue */
	0x00,                                 /* iConfiguration */
	0xc0,               /* bmAttributes */
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
	DESC_CONFIG_WORD(0x0100),                 // HID Spec Release Number in BCD format (1.11)
	0x21,                   // Country Code (0x00 for Not supported, 0x21 for US)
	1,                      // Number of class descriptors, see usbcfg.h
	0x22,//DSC_RPT,                // Report descriptor type
	DESC_CONFIG_WORD(sizeof(pad_driving_force_hid_report_descriptor)), // Size of the report descriptor

	/* Endpoint Descriptor */
	0x07,/*sizeof(USB_EP_DSC)*/
	0x05, //USB_DESCRIPTOR_ENDPOINT,    //Endpoint Descriptor
	0x1|0x80, //HID_EP | _EP_IN,        //EndpointAddress
	0x03, //_INTERRUPT,                 //Attributes
	DESC_CONFIG_WORD(USB_PSIZE),        //size
	0x02,                       //Interval

	/* Endpoint Descriptor */
	0x07,/*sizeof(USB_EP_DSC)*/
	0x05, //USB_DESCRIPTOR_ENDPOINT,    //Endpoint Descriptor
	0x1|0x0, //HID_EP | _EP_OUT,        //EndpointAddress
	0x03, //_INTERRUPT,                 //Attributes
	DESC_CONFIG_WORD(USB_PSIZE),        //size
	0x02,                        //Interval 0x2 - 2ms (G27) , 0x0A default?
};

static const uint8_t dfp_config_descriptor[] = {
	0x09,   /* bLength */
	USB_CONFIGURATION_DESCRIPTOR_TYPE,    /* bDescriptorType */
	WBVAL(41),                        /* wTotalLength */
	0x01,                                 /* bNumInterfaces */
	0x01,                                 /* bConfigurationValue */
	0x00,                                 /* iConfiguration */
	0xc0,               /* bmAttributes */
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
	DESC_CONFIG_WORD(0x0100),                 // HID Spec Release Number in BCD format (1.11)
	0x21,                   // Country Code (0x00 for Not supported, 0x21 for US)
	1,                      // Number of class descriptors, see usbcfg.h
	0x22,//DSC_RPT,                // Report descriptor type
	DESC_CONFIG_WORD(sizeof(pad_driving_force_pro_hid_report_descriptor)),

	/* Endpoint Descriptor */
	0x07,/*sizeof(USB_EP_DSC)*/
	0x05, //USB_DESCRIPTOR_ENDPOINT,    //Endpoint Descriptor
	0x1|0x80, //HID_EP | _EP_IN,        //EndpointAddress
	0x03, //_INTERRUPT,                 //Attributes
	DESC_CONFIG_WORD(USB_PSIZE),        //size, might be 16 bytes
	0x02,                       //Interval

	/* Endpoint Descriptor */
	0x07,/*sizeof(USB_EP_DSC)*/
	0x05, //USB_DESCRIPTOR_ENDPOINT,    //Endpoint Descriptor
	0x1|0x0, //HID_EP | _EP_OUT,        //EndpointAddress
	0x03, //_INTERRUPT,                 //Attributes
	DESC_CONFIG_WORD(USB_PSIZE),        //size
	0x02,                        //Interval 0x2 - 2ms (G27) , 0x0A default?
};

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
};

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
};

struct dfp_data_t
{
	uint32_t axis_x : 14;
	uint32_t buttons : 14;
	uint32_t hatswitch : 4;

	uint32_t magic1 : 8;
	
	uint32_t axis_z : 8; //or y
	uint32_t axis_rz : 8;
	uint32_t magic2 : 8;
	//uint32_t pad3 : 2;
};

struct momo2_data_t
{
	uint32_t pad0 : 8;//report id probably
	uint32_t axis_x : 10;
	uint32_t buttons : 10;
	uint32_t padding0 : 4;//32

	uint8_t padding1;
	uint8_t axis_z;
	uint8_t axis_rz;
	uint8_t padding2;//32
};

// DF or any LG wheel in non-native mode
struct generic_data_t
{
	uint32_t axis_x : 10;
	uint32_t buttons : 12;
	uint32_t pad0 : 2;//vendor
	uint32_t axis_y : 8;//constant (0x7f on PC, 0xFF on console?)

	uint32_t hatswitch : 4;
	uint32_t pad1 : 4;//vendor
	uint32_t axis_z : 8;
	uint32_t axis_rz : 8;
	uint32_t pad2 : 8;
};

struct random_data_t 
{
	uint32_t axis_x : 10;
	uint32_t buttons : 10;
	uint32_t pad1 : 12;

	uint32_t axis_y : 8;//constant
	uint32_t axis_z : 8;
	uint32_t axis_rz : 8;
	uint32_t pad2 : 8;
};

//packet is 8 bytes
struct ff_data
{
	uint32_t reportid : 8; //17 or 19?
	uint32_t index : 8; //0-255
	uint32_t data1 : 8; //0-255
	uint32_t data2 : 8; //always 128 ??
	//32
	uint32_t pad1 : 8; //packet is 8 bytes
	uint32_t pad2 : 8; //
	uint32_t pad3 : 8; //
	uint32_t pad4 : 8; //
	//32
};

#if _WIN32
//extern struct generic_data_t	generic_data;
#else
extern struct generic_data_t	generic_data[2];
#endif

void ResetData(generic_data_t *d);
void ResetData(dfp_data_t *d);
void pad_copy_data(uint32_t idx, uint8_t *buf, wheel_data_t &data);
#endif

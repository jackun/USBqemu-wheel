//#include "../qemu-usb/vl.h"
#include "../qemu-usb/USBinternal.h"
#include "usb-pad.h"

//struct df_data_t	df_data;
//struct dfp_data_t	dfp_data;
//struct dfp_buttons_t	dfp_buttons;
//struct dfgt_buttons_t	dfgt_buttons;
struct momo_data_t	momo_data;
#if _WIN32
struct generic_data_t	generic_data;
#else
struct generic_data_t	generic_data[2];
#endif

std::string player_joys[2]; //two players
bool has_rumble[2];

//extern OHCIState *qemu_ohci;

/* HID interface requests */
#define GET_REPORT   0xa101
#define GET_IDLE     0xa102
#define GET_PROTOCOL 0xa103
#define SET_IDLE     0x210a
#define SET_PROTOCOL 0x210b

//FIXME seems like it should work eventually
static void pad_handle_data(USBDevice *dev, USBPacket *p)
{
	PADState *s = (PADState *)dev;
	uint8_t buf[16];
	//uint8_t buf[p->iov.size];
	int len = 0;

	switch(p->pid) {
	case USB_TOKEN_IN:
		if (p->ep->nr == 1) {
			len = usb_pad_poll(s, buf, 16);
			usb_packet_copy(p, buf, len);
		}
		else{
			goto fail;
		}
		break;
	case USB_TOKEN_OUT:
		//fprintf(stderr,"usb-pad: data token out len=0x%X\n",len);
		usb_packet_copy(p, buf, 16);
		token_out(s, buf, p->actual_length);
		break;
	default:
	fail:
		p->status = USB_RET_STALL;
		break;
	}
}

static void pad_handle_reset(USBDevice *dev)
{
	/* XXX: do it */
	return;
}

static void pad_handle_control(USBDevice *dev, USBPacket *p, int request, int value,
								int index, int length, uint8_t *data)
{
	PADState *s = (PADState *)dev;
	int ret = 0;

	//ret = usb_desc_handle_control(dev, p, request, value, index, length, data);
	//if (ret >= 0) {
	//	return;
	//}

	switch(request) {
	case DeviceRequest | USB_REQ_GET_STATUS:
		data[0] = (dev->remote_wakeup << USB_DEVICE_REMOTE_WAKEUP);
		data[1] = 0x00;
		p->actual_length = 2;
		break;
	case DeviceOutRequest | USB_REQ_CLEAR_FEATURE:
		if (value == USB_DEVICE_REMOTE_WAKEUP) {
			dev->remote_wakeup = 0;
		} else {
			goto fail;
		}
		p->actual_length = 0;
		break;
	case DeviceOutRequest | USB_REQ_SET_FEATURE:
		if (value == USB_DEVICE_REMOTE_WAKEUP) {
			dev->remote_wakeup = 1;
		} else {
			goto fail;
		}
		p->actual_length = 0;
		break;
	case DeviceOutRequest | USB_REQ_SET_ADDRESS:
		dev->addr = value;
		p->actual_length = 0;
		break;
	case DeviceRequest | USB_REQ_GET_DESCRIPTOR:
		switch(value >> 8) {
		case USB_DT_DEVICE:

#if _WIN32
			if(s->doPassthrough)
			{
				pad_dev_descriptor[10] = 0xC2;
				pad_dev_descriptor[11] = 0x98;
			}
#endif

			memcpy(data, pad_dev_descriptor, 
					sizeof(pad_dev_descriptor));
			p->actual_length = sizeof(pad_dev_descriptor);
			break;
		case USB_DT_CONFIG:
			memcpy(data, momo_config_descriptor, 
					sizeof(momo_config_descriptor));
			p->actual_length = sizeof(momo_config_descriptor);
			break;
		case USB_DT_STRING:
			switch(value & 0xff) {
			case 0:
				/* language ids */
				data[0] = 4;
				data[1] = 3;
				data[2] = 0x09;
				data[3] = 0x04;
				p->actual_length = 4;
				break;
			case 1:
				/* serial number */
				p->actual_length = set_usb_string(data, "1");
				break;
			case 2:
				/* product description */
				p->actual_length = set_usb_string(data, "Driving Force Pro");
				break;
			case 3:
				/* vendor description */
				p->actual_length = set_usb_string(data, "Logitech");
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
		p->actual_length = 1;
		break;
	case DeviceOutRequest | USB_REQ_SET_CONFIGURATION:
		ret = 0;
		break;
	case DeviceRequest | USB_REQ_GET_INTERFACE:
		data[0] = 0;
		p->actual_length = 1;
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
#if _WIN32
			//TODO For now, only supports DFP
			if(s->doPassthrough)
			{
				ret = sizeof(pad_driving_force_pro_hid_report_descriptor);
				memcpy(data, pad_driving_force_pro_hid_report_descriptor, ret);
			}
			else
			{
#endif
				ret = sizeof(pad_hid_report_descriptor);
				memcpy(data, pad_hid_report_descriptor, ret);
#if _WIN32
			}
#endif
			p->actual_length = ret;
			break;
		default:
			goto fail;
		}
		break;
	case GET_REPORT:
		p->actual_length = 0;
		break;
	case SET_IDLE:
		ret = 0;
		break;
	default:
	fail:
		p->status = USB_RET_STALL;
		break;
	}
}

static void pad_handle_destroy(USBDevice *dev)
{
	PADState *s = (PADState *)dev;
	if(s){
		destroy_pad(s);
		free(s->dev.klass);
		free(s);
	}
}

//Needed with 1.7.0 ?
void pad_handle_packet(USBDevice *s, USBPacket *p)
{
	//fprintf(stderr,"usb-pad: packet received with pid=%x, devaddr=%x, devep=%x and len=%x\n",pid,devaddr,devep,len);
	fprintf(stderr,"usb-pad: packet received with pid=%x\n",p->pid);
	usb_handle_packet(s, p);
}


static USBDesc desc = { 0 };
static USBDescDevice desc_device_full = { 0 };
static USBDescIface desc_iface_full = { 0 };

enum {
	STR_MANUFACTURER = 1,
	STR_PRODUCT,
	STR_SERIALNUMBER,
	STR_CONFIG_FULL,
};

static const USBDescStrings desc_strings = {
	NULL,
	"Logitech",
	"Driving Force Pro",
	"1",
	"HID Joystick"
};

USBDescEndpoint eps[2];

USBDevice *pad_init(int port)
{
	PADState *s;

	s = (PADState *)get_new_padstate();//qemu_mallocz(sizeof(PADState));
	if (!s)
		return NULL;
	
	s->port = port;

	if(!find_pad(s))
	{
		free(s);
		return NULL;
	}

	//I don't know yet if these are needed. Just here for now for usb_desc_* functions.
	desc_iface_full.bInterfaceNumber              = 0;
	desc_iface_full.bNumEndpoints                 = 2;
	desc_iface_full.bInterfaceClass               = USB_CLASS_HID;
	desc_iface_full.bInterfaceSubClass            = 0x0;
	desc_iface_full.bInterfaceProtocol            = 0x0;
	
	eps[0].bEndpointAddress      = USB_DIR_IN | 0x01;
	eps[0].bmAttributes          = USB_ENDPOINT_XFER_INT;
	eps[0].wMaxPacketSize        = 16;

	eps[1].bEndpointAddress      = USB_DIR_OUT | 0x02;
	eps[1].bmAttributes          = USB_ENDPOINT_XFER_INT;
	eps[1].wMaxPacketSize        = 16;

	desc_iface_full.eps = eps;

	desc_device_full.bcdUSB                        = 0x0110;
	desc_device_full.bMaxPacketSize0               = 8;
	desc_device_full.bNumConfigurations            = 1;
	USBDescConfig conf = {0};
	conf.bNumInterfaces        = 1;
	conf.bConfigurationValue   = 1;
	conf.iConfiguration        = STR_CONFIG_FULL;
	conf.bmAttributes          = 0xa0;
	conf.bMaxPower             = 80;
	conf.nif = 1;
	conf.ifs = &desc_iface_full;
	desc_device_full.confs = &conf;

	desc.id.idVendor          = PAD_VID, /* CRC16() of "QEMU" */
	desc.id.idProduct         = PAD_PID,
	desc.id.bcdDevice         = 0,
	desc.id.iManufacturer     = STR_MANUFACTURER;
	desc.id.iProduct          = STR_PRODUCT;
	desc.id.iSerialNumber     = STR_SERIALNUMBER;
	desc.full  = &desc_device_full,
	desc.high  = NULL;//&desc_device_high,
	desc.super = NULL;//&desc_device_super,
	desc.str   = desc_strings;

	s->dev.speed = USB_SPEED_FULL;
	s->dev.klass = (USBDeviceClass*)qemu_mallocz(sizeof(USBDeviceClass));
	s->dev.klass->handle_packet  = pad_handle_packet; //maybe unneeded
	s->dev.klass->handle_reset   = pad_handle_reset;
	s->dev.klass->handle_control = pad_handle_control;
	s->dev.klass->handle_data    = pad_handle_data;
	s->dev.klass->handle_destroy = pad_handle_destroy;
	s->dev.klass->handle_attach  = usb_desc_attach;
	s->dev.klass->usb_desc = &desc;
	s->dev.usb_desc = &desc;

	// GT4 doesn't seem to care for a proper name?
	strncpy(s->dev.product_desc, "Driving Force Pro", sizeof(s->dev.product_desc));
	

	//usb_desc_create_serial(&s->dev);
	usb_desc_init(&s->dev);
	//Hackish
	//usb_desc_set_config(&s->dev, 1);
	usb_desc_ep_init(&s->dev);

	//Dunno if correct but we don't care
	s->dev.klass->product_desc = s->dev.product_desc;

	return (USBDevice *)s;

}

//#include "../qemu-usb/vl.h"
#include "../USB.h"
#include "usb-pad.h"

//struct df_data_t	df_data;
//struct dfp_data_t	dfp_data;
//struct dfp_buttons_t	dfp_buttons;
//struct dfgt_buttons_t	dfgt_buttons;
struct momo_data_t	momo_data = {0};
#if _WIN32
//struct generic_data_t	generic_data;
#else
struct generic_data_t generic_data[2] = {0};
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

struct lg4ff_native_cmd {
	const uint8_t cmd_num;	/* Number of commands to send */
	const uint8_t cmd[];
};

static const struct lg4ff_native_cmd native_dfp = {
	1,
	{0xf8, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00}
};

static const struct lg4ff_native_cmd native_dfgt = {
	2,
	{0xf8, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00,	/* 1st command */
	 0xf8, 0x09, 0x03, 0x01, 0x00, 0x00, 0x00}	/* 2nd command */
};

static int pad_handle_data(USBDevice *dev, int pid, 
							uint8_t devep, uint8_t *data, int len)
{
	PADState *s = (PADState *)dev;

	int ret = 0;

	switch(pid) {
	case USB_TOKEN_IN:
		if (devep == 1) {
			ret = usb_pad_poll(s, data, len);
		} else {
			goto fail;
		}
		break;
	case USB_TOKEN_OUT:
		//fprintf(stderr,"usb-pad: data token out len=0x%X\n",len);
		if(len > 6 && !memcmp(native_dfp.cmd, data, 7)) //len is prolly 16
			fprintf(stderr,"usb-pad: tried to set DFP to native mode\n");
		uint8_t tmp[8];
		memcpy(tmp, data, 7);
		ret = token_out(s, data, len);
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
	if(s == NULL) return USB_RET_STALL;

	int t;
	if(s->port == 1)
		t = conf.WheelType1;
	else
		t = conf.WheelType2;

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
			if(/*s->doPassthrough ||*/ t == WT_DRIVING_FORCE_PRO)
			{
				pad_dev_descriptor[10] = 0xC2;
				pad_dev_descriptor[11] = DFP_PID & 0xFF;
			}
			else if(t == WT_DRIVING_FORCE)
			{
				pad_dev_descriptor[10] = 0xC2;
				pad_dev_descriptor[11] = DF_PID & 0xFF;
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
			//TODO For now, only supporting DFP
			if(s->doPassthrough || t == WT_DRIVING_FORCE_PRO)
			{
				ret = sizeof(pad_driving_force_pro_hid_report_descriptor);
				memcpy(data, pad_driving_force_pro_hid_report_descriptor, ret);
			}
			else
			{
				ret = sizeof(pad_hid_report_descriptor);
				memcpy(data, pad_hid_report_descriptor, ret);
			}
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
	if(s){
		destroy_pad(s);
		free(s);
	}
}

int pad_handle_packet(USBDevice *s, int pid, 
							uint8_t devaddr, uint8_t devep,
							uint8_t *data, int len)
{
	//fprintf(stderr,"usb-pad: packet received with pid=%x, devaddr=%x, devep=%x and len=%x\n",pid,devaddr,devep,len);
	return usb_generic_handle_packet(s,pid,devaddr,devep,data,len);
}

USBDevice *pad_init(int port)
{
	PADState *s;

	s = (PADState *)get_new_padstate();//qemu_mallocz(sizeof(PADState));
	if (!s)
		return NULL;
	s->dev.speed = USB_SPEED_FULL;
	s->dev.handle_packet  = pad_handle_packet;
	s->dev.handle_reset   = pad_handle_reset;
	s->dev.handle_control = pad_handle_control;
	s->dev.handle_data    = pad_handle_data;
	s->dev.handle_destroy = pad_handle_destroy;
	s->port = port;

	// GT4 doesn't seem to care for a proper name?
	strncpy(s->dev.devname, "Driving Force Pro", sizeof(s->dev.devname));

	if(!find_pad(s))
	{
		free(s);
		return NULL;
	}

	return (USBDevice *)s;

}

void ResetData(generic_data_t *d)
{
	ZeroMemory(d, sizeof(generic_data_t));
	d->axis_x = 0x3FF >> 1;
	d->axis_y = 0xFF;
	d->axis_z = 0xFF;
	d->axis_rz = 0xFF;
}

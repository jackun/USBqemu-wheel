#include "../USB.h"
#include "usb-pad.h"

#if _WIN32
static generic_data_t generic_data;
static dfp_data_t dfp_data;
#else
static generic_data_t	generic_data[2] = {0};
static dfp_data_t		dfp_data[2] = {0};
#endif

//TODO move to Config?
std::string player_joys[2]; //two players
bool has_rumble[2];

/* HID interface requests */
#define GET_REPORT   0xa101
#define GET_IDLE     0xa102
#define GET_PROTOCOL 0xa103
#define SET_IDLE     0x210a
#define SET_PROTOCOL 0x210b

//All here is speculation
static const uint8_t GT4Inits[4] = {
	0xf3,// 0, 0, 0, 0, 0, 0}, //de-activate all forces?
	0xf4,// 0, 0, 0, 0, 0, 0}, //activate autocenter?
	0x09,// 6, 0, 0, 0, 0, 0}, //???
	0xf5                       //de-activate autocenter?
};

static uint8_t last_cmd = 0;
static uint8_t dfp_range[] =  {
	//0x81, 
	//0x0B, 
	0x19, 
	0xE6, 
	0xFF, 
	0x4A, 
	0xFF
};

static int pad_handle_data(USBDevice *dev, int pid, 
							uint8_t devep, uint8_t *data, int len)
{
	PADState *s = (PADState *)dev;

	int ret = 0;

	switch(pid) {
	case USB_TOKEN_IN:
		if (devep == 1 && s->usb_pad_poll) {
			/*fprintf(stderr, "usb-pad: data poll len=%d last cmd:0x%X\n", len, last_cmd);
			if(last_cmd == 0xB) {
				memcpy(data, dfp_range, sizeof(dfp_range));
				last_cmd = 0;
			}
			else*/
			ret = s->usb_pad_poll(s, data, len);
		} else {
			goto fail;
		}
		break;
	case USB_TOKEN_OUT:
		last_cmd = data[0];
		/*fprintf(stderr,"usb-pad: data token out len=0x%X %X,%X,%X,%X,%X,%X,%X,%X\n",len, 
			data[0],data[1],data[2],data[3],data[4],data[5],data[6],data[7]);*/
		//fprintf(stderr,"usb-pad: data token out len=0x%X\n",len);
		//if(s->initStage < 3 &&  GT4Inits[s->initStage] == data[0])
		//	s->initStage ++;

		if(s->token_out)
			ret = s->token_out(s, data, len);
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

	int t = s->port == 1 ? conf.WheelType[0] : conf.WheelType[1];

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
			//if(s->initStage > 2)
			if(t == WT_DRIVING_FORCE_PRO)
			{
				pad_dev_descriptor[11] = 0xC2;
				pad_dev_descriptor[10] = DFP_PID & 0xFF;
			}

			ret = sizeof(pad_dev_descriptor);
			memcpy(data, pad_dev_descriptor, ret);
			break;
		case USB_DT_CONFIG:
			if(t == WT_DRIVING_FORCE_PRO)
			{
				ret = sizeof(dfp_config_descriptor);
				memcpy(data, dfp_config_descriptor, ret);
			}
			else
			{
				ret = sizeof(df_config_descriptor);
				memcpy(data, df_config_descriptor, ret);
			}
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
	case InterfaceRequest | USB_REQ_GET_DESCRIPTOR: //Never called?
		fprintf(stderr, "InterfaceRequest | USB_REQ_GET_DESCRIPTOR %d\n", value>>8);
		switch(value >> 8) {
		case 0x22:
			fprintf(stderr, "Sending hid report desc.\n");
			//TODO For now, only supporting DFP
			if(/*s->doPassthrough ||*/ /*s->initStage > 2 &&*/ t == WT_DRIVING_FORCE_PRO)
			{
				ret = sizeof(pad_driving_force_pro_hid_report_descriptor);
				memcpy(data, pad_driving_force_pro_hid_report_descriptor, ret);
			}
			else
			{
				ret = sizeof(pad_driving_force_hid_report_descriptor);
				memcpy(data, pad_driving_force_hid_report_descriptor, ret);
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
	if(s && s->destroy_pad){
		s->destroy_pad(s);
	}
}

int pad_handle_packet(USBDevice *s, int pid, 
							uint8_t devaddr, uint8_t devep,
							uint8_t *data, int len)
{
	//fprintf(stderr,"usb-pad: packet received with pid=%x, devaddr=%x, devep=%x and len=%x\n",pid,devaddr,devep,len);
	return usb_generic_handle_packet(s,pid,devaddr,devep,data,len);
}

USBDevice *pad_init(int port, int type)
{
	PADState *s = NULL;

#ifdef _WIN32

	#if BUILD_RAW
		if(type == 0)
			s = (PADState *)get_new_raw_padstate();
		else
	#endif
	#if BUILD_DX
		if(type == 1)
			s = (PADState *)get_new_dx_padstate();
	#else
			return NULL; //BUILD_RAW else
	#endif

#else //linux

	s = (PADState *)get_new_padstate();

#endif

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

	if(s->find_pad && !s->find_pad(s))
	{
		free(s);
		return NULL;
	}

	return (USBDevice *)s;

}

void ResetData(generic_data_t *d)
{
	memset(d, 0, sizeof(generic_data_t));
	d->axis_x = 0x3FF >> 1;
	d->axis_y = 0xFF;
	d->axis_z = 0xFF;
	d->axis_rz = 0xFF;
}

void ResetData(dfp_data_t *d)
{
	memset(d, 0, sizeof(dfp_data_t));
	d->axis_x = 0x3FFF >> 1;
	//d->axis_y = 0xFF;
	d->axis_z = 0xFF;
	d->axis_rz = 0xFF;
}

void pad_copy_data(uint32_t idx, uint8_t *buf, wheel_data_t &data)
{
	int type = conf.WheelType[idx];

	//fprintf(stderr,"usb-pad: axis x %d\n", data.axis_x);

	switch(type){
	case WT_GENERIC:
		memset(&generic_data, 0xff, sizeof(generic_data_t));
		//ResetData(&generic_data);

		generic_data.buttons = data.buttons;
		generic_data.hatswitch = data.hatswitch;
		generic_data.axis_x = data.axis_x;
		generic_data.axis_y = 0xFF; //data.axis_y;
		generic_data.axis_z = data.axis_z;
		generic_data.axis_rz = data.axis_rz;

		memcpy(buf, &generic_data, sizeof(generic_data_t));
		break;

	case WT_DRIVING_FORCE_PRO:
		//memset(&dfp_data, 0, sizeof(dfp_data_t));
		//ResetData(&dfp_data);

		dfp_data.buttons = data.buttons;
		dfp_data.hatswitch = data.hatswitch;
		dfp_data.axis_x = data.axis_x;
		//dfp_data.axis_y = data.axis_y;
		dfp_data.axis_z = data.axis_z;
		dfp_data.axis_rz = data.axis_rz;
		dfp_data.magic1 = 1 << 0;
		dfp_data.magic2 = (1<<0) /* enable axes? */
			;

		memcpy(buf, &dfp_data, sizeof(dfp_data_t));

		break;
	default:
		break;
	}
}

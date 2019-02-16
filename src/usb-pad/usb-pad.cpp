#include "../deviceproxy.h"
#include "../qemu-usb/desc.h"
#include "padproxy.h"

namespace usb_pad {

#define DEVICENAME "pad"

static const USBDescStrings pad_desc_strings = {
	"",
	"Logitech",
	"Driving Force"
};

static const USBDescStrings pad_dfp_desc_strings = {
	"",
	"Logitech",
	"Driving Force Pro"
};

class PadDevice : public Device
{
public:
	virtual ~PadDevice() {}
	static USBDevice* CreateDevice(int port);
	static const TCHAR* Name()
	{
		return TEXT("Pad/Wheel device");
	}
	static const char* TypeName()
	{
		return DEVICENAME;
	}
	static std::list<std::string> ListAPIs()
	{
		return RegisterPad::instance().Names();
	}
	static const TCHAR* LongAPIName(const std::string& name)
	{
		auto proxy = RegisterPad::instance().Proxy(name);
		if (proxy)
			return proxy->Name();
		return nullptr;
	}
	static int Configure(int port, const std::string& api, void *data);
	static int Freeze(int mode, USBDevice *dev, void *data);
};

#ifdef _DEBUG
void PrintBits(void * data, int size)
{
	std::vector<unsigned char> buf(size * 8 + 1 + size);
	unsigned char *bits = buf.data();
	unsigned char *ptrD = (unsigned char*)data;
	unsigned char *ptrB = bits;
	for (int i = 0; i < size * 8; i++)
	{
		*(ptrB++) = '0' + (*(ptrD + i / 8) & (1 << (i % 8)) ? 1 : 0);
		if (i % 8 == 7)
			*(ptrB++) = ' ';
	}
	*ptrB = '\0';

	OSDebugOut(TEXT("%" SFMTs "\n"), bits);
}

#else
#define PrintBits(...)
#define DbgPrint(...)
#endif //_DEBUG

typedef struct PADState {
	USBDevice		dev;
	USBDesc desc;
	USBDescDevice desc_dev;
	Pad*			pad;
	uint8_t			port;
	int				initStage;
	//Config instead?
	bool			doPassthrough;// = false; //Mainly for Win32 Driving Force Pro passthrough
	struct freeze {
		int wheel_type;
	} f;
} PADState;

typedef struct u_wheel_data_t {
	union {
		generic_data_t generic_data;
		dfp_data_t dfp_data;
		gtforce_data_t gtf_data;
	} u;
} u_wheel_data_t;

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

//Convert DF Pro buttons to selected wheel type
uint32_t convert_wt_btn(PS2WheelTypes type, uint32_t inBtn)
{
	if(type == WT_GT_FORCE)
	{
		/***
		R1 > SQUARE == menu down	L1 > CROSS == menu up
		SQUARE > CIRCLE == X		TRIANG > TRIANG == Y
		CROSS > R1 == A				CIRCLE > L1 == B
		***/
		switch(inBtn)
		{
		case PAD_L1: return PAD_CROSS;
		case PAD_R1: return PAD_SQUARE;
		case PAD_SQUARE: return PAD_CIRCLE;
		case PAD_TRIANGLE: return PAD_TRIANGLE;
		case PAD_CIRCLE: return PAD_L1;
		case PAD_CROSS: return PAD_R1;
		default:
			return PAD_BUTTON_COUNT; //Aka invalid
		}
	}
	else if(type == WT_GENERIC)
	{
		switch(inBtn)
		{
		case PAD_R1: return PAD_R2;
		case PAD_R2: return PAD_R1;
		case PAD_L1: return PAD_L2;
		case PAD_L2: return PAD_L1;
		default:
			return inBtn;
		}
	}

	return inBtn;
}

static void pad_handle_data(USBDevice *dev, USBPacket *p)
{
	PADState *s = (PADState *)dev;
	uint8_t data[64];

	int ret = 0;
	uint8_t devep = p->ep->nr;

	switch(p->pid) {
	case USB_TOKEN_IN:
		if (devep == 1 && s->pad) {
			ret = s->pad->TokenIn(data, p->iov.size);
			usb_packet_copy (p, data, MIN(ret, sizeof(data)));
		} else {
			goto fail;
		}
		break;
	case USB_TOKEN_OUT:
		usb_packet_copy (p, data, MIN(p->iov.size, sizeof(data)));
		last_cmd = data[0];
		/*fprintf(stderr,"usb-pad: data token out len=0x%X %X,%X,%X,%X,%X,%X,%X,%X\n",len, 
			data[0],data[1],data[2],data[3],data[4],data[5],data[6],data[7]);*/
		//fprintf(stderr,"usb-pad: data token out len=0x%X\n",len);
		//if(s->initStage < 3 &&  GT4Inits[s->initStage] == data[0])
		//	s->initStage ++;
		ret = s->pad->TokenOut(data, p->iov.size);
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
	PADState *s = (PADState*)dev;
	s->pad->Reset();
	return;
}

static void pad_handle_control(USBDevice *dev, USBPacket *p, int request, int value,
								  int index, int length, uint8_t *data)
{
	PADState *s = (PADState *)dev;
	int ret = 0;

	int t = (s->port == PLAYER_ONE_PORT) ? conf.WheelType[0] : conf.WheelType[1];

	switch(request) {
	case DeviceRequest | USB_REQ_GET_DESCRIPTOR:
		ret = usb_desc_handle_control(dev, p, request, value, index, length, data);
		if (ret < 0)
			goto fail;

		// Change PID according to selected wheel
		if ((value >> 8) == USB_DT_DEVICE) {
			if (t == WT_DRIVING_FORCE_PRO)
				*(uint16_t*)&data[10] = PID_DFP;
			else if (t == WT_GT_FORCE)
				*(uint16_t*)&data[10] = PID_FFGP;
		}

		break;
		/* hid specific requests */
	case InterfaceRequest | USB_REQ_GET_DESCRIPTOR: //GT3
		OSDebugOut(TEXT("InterfaceRequest | USB_REQ_GET_DESCRIPTOR 0x%04X\n"), value);
		switch(value >> 8) {
		case 0x22:
			OSDebugOut(TEXT("Sending hid report desc.\n"));
			if(/*s->initStage > 2 &&*/ t == WT_DRIVING_FORCE_PRO)
			{
				ret = sizeof(pad_driving_force_pro_hid_report_descriptor);
				memcpy(data, pad_driving_force_pro_hid_report_descriptor, ret);
			}
			else if (t == WT_GT_FORCE)
			{
				ret = sizeof(pad_gtforce_hid_report_descriptor);
				memcpy(data, pad_gtforce_hid_report_descriptor, ret);
			}
			else
			{
				ret = sizeof(pad_driving_force_hid_report_descriptor);
				memcpy(data, pad_driving_force_hid_report_descriptor, ret);
			}
			p->actual_length = ret;
			break;
		default:
			goto fail;
		}
		break;
	default:
		ret = usb_desc_handle_control(dev, p, request, value, index, length, data);
		if (ret >= 0) {
			return;
		}
	fail:
		p->status = USB_RET_STALL;
		break;
	}
}

static void pad_handle_destroy(USBDevice *dev)
{
	PADState *s = (PADState *)dev;
	delete s;
}

int pad_open(USBDevice *dev)
{
	PADState *s = (PADState *) dev;
	if (s)
		return s->pad->Open();
	return 1;
}

void pad_close(USBDevice *dev)
{
	PADState *s = (PADState *) dev;
	if (s)
		s->pad->Close();
}

USBDevice *PadDevice::CreateDevice(int port)
{
	std::string varApi;
	LoadSetting(port, DEVICENAME, N_DEVICE_API, varApi);
	PadProxyBase *proxy = RegisterPad::instance().Proxy(varApi);
	if (!proxy)
	{
		SysMessage(TEXT("Invalid pad API.\n"));
		return NULL;
	}

	Pad *pad = proxy->CreateObject(port);

	if (!pad)
		return NULL;

	pad->Type((PS2WheelTypes)conf.WheelType[1 - port]);
	PADState *s = new PADState();

	s->desc.full = &s->desc_dev;
	s->desc.str = pad_desc_strings;

	int dev_desc_len = sizeof(pad_dev_descriptor);
	const uint8_t *dev_desc = pad_dev_descriptor;
	const uint8_t *config_desc = df_config_descriptor;
	int config_desc_len = sizeof(df_config_descriptor);

	if (pad->Type() == WT_DRIVING_FORCE_PRO)
	{
		s->desc.str = pad_dfp_desc_strings;
		dev_desc = dfp_dev_descriptor;
		config_desc = dfp_config_descriptor;
		config_desc_len = sizeof(dfp_config_descriptor);
	}
	else if (pad->Type() == WT_GT_FORCE)
	{
		dev_desc = ffgp_dev_descriptor;
		//config_desc = pad_driving_force_hid_report_descriptor; //TODO
		//config_desc_len = sizeof(pad_driving_force_hid_report_descriptor);
	}

	if (usb_desc_parse_dev (dev_desc, dev_desc_len, s->desc, s->desc_dev) < 0)
		goto fail;
	if (usb_desc_parse_config (config_desc, config_desc_len, s->desc_dev) < 0)
		goto fail;

	s->f.wheel_type = conf.WheelType[1 - port];
	s->pad = pad;
	s->dev.speed = USB_SPEED_FULL;
	s->dev.klass.handle_attach  = usb_desc_attach;
	s->dev.klass.handle_reset   = pad_handle_reset;
	s->dev.klass.handle_control = pad_handle_control;
	s->dev.klass.handle_data    = pad_handle_data;
	s->dev.klass.unrealize      = pad_handle_destroy;
	s->dev.klass.open           = pad_open;
	s->dev.klass.close          = pad_close;
	s->dev.klass.usb_desc       = &s->desc;
	s->dev.klass.product_desc   = s->desc.str[2];
	s->port = port;

	usb_desc_init(&s->dev);
	usb_ep_init(&s->dev);
	pad_handle_reset ((USBDevice *)s);

	return (USBDevice *)s;

fail:
	pad_handle_destroy ((USBDevice *)s);
	return nullptr;
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
	d->axis_z = 0x3F;
	d->axis_rz = 0x3F;
}

void pad_copy_data(PS2WheelTypes type, uint8_t *buf, wheel_data_t &data)
{
#if 1
	struct wheel_data_t {
		uint32_t lo;
		uint32_t hi;
	};

	wheel_data_t *w = (wheel_data_t *)buf;
	memset(w, 0, 8);
	
	switch (type) {
	case WT_GENERIC:
		w->lo = data.steering & 0x3FF;
		w->lo |= (data.buttons & 0xFFF) << 10;
		w->lo |= 0xFF << 24;

		w->hi = (data.hatswitch & 0xF);
		w->hi |= (data.throttle & 0xFF) << 8;
		w->hi |= (data.brake & 0xFF) << 16;

		break;

	case WT_GT_FORCE:

		w->lo = data.steering & 0x3FF;
		w->lo |= (data.buttons & 0x3F) << 10;
		w->lo |= 0xFF << 24;

		w->hi = (data.throttle & 0xFF);
		w->hi |= (data.brake & 0xFF) << 8;

		break;
	case WT_DRIVING_FORCE_PRO:

		// what's up with the bitmap?
		// xxxxxxxx xxxxxxbb bbbbbbbb bbbbhhhh ???????? ?01zzzzz 1rrrrrr1 10001000
		w->lo = data.steering & 0x3FFF;
		w->lo |= (data.buttons & 0x3FFF) << 14;
		w->lo |= (data.hatswitch & 0xF ) << 28;

		w->hi = 0x00;
		//w->hi |= 0 << 9; //bit 9 must be 0
		w->hi |= (1 | (data.throttle * 0x3F) / 0xFF) << 10; //axis_z
		w->hi |= 1 << 16; //bit 16 must be 1
		w->hi |= ((0x3F - (data.brake * 0x3F) / 0xFF) & 0x3F) << 17; //axis_rz
		w->hi |= 1 << 23; //bit 23 must be 1
		w->hi |= 0x11 << 24; //enables wheel and pedals?

		//PrintBits(w, sizeof(*w));

		break;
	default:
		break;
	}
#endif

#if 0
	u_wheel_data_t *w = (u_wheel_data_t *)buf;

	//fprintf(stderr,"usb-pad: axis x %d\n", data.axis_x);
	switch(type){
	case WT_GENERIC:
		memset(&w->u.generic_data, 0xff, sizeof(generic_data_t));
		//ResetData(&w->u.generic_data);

		w->u.generic_data.buttons = data.buttons;
		w->u.generic_data.hatswitch = data.hatswitch;
		w->u.generic_data.axis_x = data.steering;
		w->u.generic_data.axis_y = 0xFF; //data.clutch;
		w->u.generic_data.axis_z = data.throttle;
		w->u.generic_data.axis_rz = data.brake;

		break;

	case WT_DRIVING_FORCE_PRO:
		//memset(&w->u.dfp_data, 0, sizeof(dfp_data_t));
		//ResetData(&w->u.dfp_data);

		w->u.dfp_data.buttons = data.buttons;
		w->u.dfp_data.hatswitch = data.hatswitch;
		w->u.dfp_data.axis_x = data.steering;
		w->u.dfp_data.axis_z = 1 | (data.throttle * 0x3F) / 0xFF; //TODO Always > 0 or everything stops working, wut.
		w->u.dfp_data.axis_rz = 0x3F - (data.brake * 0x3F) / 0xFF;
		//OSDebugOut(TEXT("dfp: axis_z=0x%02x, axis_rz=0x%02x\n"), w->u.dfp_data.axis_z, w->u.dfp_data.axis_rz);

		w->u.dfp_data.magic1 = 1;
		w->u.dfp_data.magic2 = 1;
		w->u.dfp_data.magic3 = 1;
		w->u.dfp_data.magic4 =
			1 << 0 | //enable pedals?
			0 << 1 |
			0 << 2 |
			0 << 3 |
			1 << 4 | //enable wheel?
			0 << 5 |
			0 << 6 |
			0 << 7 ;

		PrintBits(&w->u.dfp_data, sizeof(dfp_data_t));

		break;

	case WT_GT_FORCE:
		memset(&w->u.gtf_data, 0xff, sizeof(gtforce_data_t));

		w->u.gtf_data.buttons = data.buttons;
		w->u.gtf_data.axis_x = data.steering;
		w->u.gtf_data.axis_y = 0xFF; //data.clutch;
		w->u.gtf_data.axis_z = data.throttle;
		w->u.gtf_data.axis_rz = data.brake;

		break;

	default:
		break;
	}

#endif
}

int PadDevice::Configure(int port, const std::string& api, void *data)
{
	auto proxy = RegisterPad::instance().Proxy(api);
	if (proxy)
		return proxy->Configure(port, data);
	return RESULT_CANCELED;
}

int PadDevice::Freeze(int mode, USBDevice *dev, void *data)
{
	PADState *s = (PADState *)dev;

	switch (mode)
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
	}
	return -1;
}

REGISTER_DEVICE(DEVTYPE_PAD, DEVICENAME, PadDevice);
#undef DEVICENAME
} //namespace
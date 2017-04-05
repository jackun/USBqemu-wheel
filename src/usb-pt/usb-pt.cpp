#include "usb-pt.h"

// #Get permission to use raw usb device. 
// #paste to /etc/udev/rules.d/99-usb-perms.rules for example, probably
// SUBSYSTEM=="usb", ATTRS{idVendor}=="01ab", ATTRS{idProduct}=="23cd", MODE="0664", GROUP="users"

// udevadm info -q all -p $(udevadm info -q path -n /dev/input/js0)

static int libusb_err_to_qemu(int err)
{
	switch(err)
	{
		case LIBUSB_ERROR_IO:
			return USB_RET_IOERROR;

		case LIBUSB_ERROR_INVALID_PARAM:
		case LIBUSB_ERROR_ACCESS:
		case LIBUSB_ERROR_BUSY:
		case LIBUSB_ERROR_TIMEOUT:
		case LIBUSB_ERROR_INTERRUPTED:
		case LIBUSB_ERROR_NO_MEM:
		case LIBUSB_ERROR_NOT_SUPPORTED:
		case LIBUSB_ERROR_PIPE:
			return USB_RET_STALL;

		case LIBUSB_ERROR_OVERFLOW:
			return USB_RET_BABBLE;

		case LIBUSB_ERROR_NO_DEVICE:
		case LIBUSB_ERROR_NOT_FOUND:
			return USB_RET_NODEV;

		default:
		break;
	}
	return 0;
}

#define SETUP_STATE_IDLE 0
#define SETUP_STATE_DATA 1
#define SETUP_STATE_ACK  2

static void pt_release_interfaces (PTState *s, int config)
{
	int r;
	libusb_config_descriptor *desc = nullptr;
	libusb_device *dev = libusb_get_device (s->usb_handle);
	r = libusb_get_config_descriptor (dev, config, &desc);
	if (r == 0) {
		for (int i=0; i< desc->bNumInterfaces; i++) //TODO
		{
			if (libusb_kernel_driver_active (s->usb_handle, i) &&
				libusb_detach_kernel_driver (s->usb_handle, i) < 0)
			{
				OSDebugOut(TEXT("usb-pt: Failed to detach kernel driver for interface %d\n"), i);
			}

			if (libusb_release_interface (s->usb_handle, i) < 0)
			{
				OSDebugOut(TEXT("usb-pt: Failed to release interface %d\n"), i);
			}
		}
	} else {
		//SysMessage (TEXT("usb-pt: Failed to get config descriptor"));
		OSDebugOut (TEXT("usb-pt: Failed to get config descriptor\n"));
		return;
	}
	libusb_free_config_descriptor (desc);
}

static bool pt_set_configuration(PTState *s, int config_num)
{
	int r;
	libusb_device *dev = libusb_get_device (s->usb_handle);

	libusb_config_descriptor *config;
	r = libusb_get_config_descriptor (dev, config_num, &config);
	if (r < 0) {
		SysMessage (TEXT("usb-pt: Failed to get config descriptor"));
		return false;
	}

	pt_release_interfaces (s, s->config);

	s->config = config_num;

	//r = libusb_reset_device (s->usb_handle);

	r = libusb_set_configuration (s->usb_handle, config_num);

	for (int i=0; i<config->bNumInterfaces; i++)
	{
		if(libusb_kernel_driver_active (s->usb_handle, i))
			libusb_detach_kernel_driver (s->usb_handle, i);

		if (!libusb_claim_interface (s->usb_handle, i))
		{
			SysMessage (TEXT("usb-pt: Failed to claim interface %d\n"), i);
			goto fail;
		}
	}

	libusb_free_config_descriptor (config);
	return true;

fail:
	libusb_free_config_descriptor (config);
	return false;
}

//TODO how fast is it?
static bool pt_get_ep_type(PTState *s, int& type)
{
	int r, c = s->config;
	libusb_config_descriptor *config;

	libusb_device *dev = libusb_get_device (s->usb_handle);

	//r = libusb_get_configuration (s->usb_handle, &c);

	r = libusb_get_config_descriptor (dev, c, &config);
	if (r < 0) {
		SysMessage (TEXT("usb-pt: Failed to get config descriptor"));
		return false;
	}

	const libusb_interface *intf = &config->interface[s->intf];
	const libusb_interface_descriptor *intf_desc = &intf->altsetting[s->altset];
	const libusb_endpoint_descriptor *ep_desc = &intf_desc->endpoint[s->ep];

	type = ep_desc->bmAttributes & USB_ENDPOINT_TYPE_MASK;

	libusb_free_config_descriptor (config);
	return true;

fail:
	libusb_free_config_descriptor (config);
	return false;
}

static int pt_handle_data (USBDevice *dev, int pid, uint8_t devep, uint8_t *data, int len)
{
	PTState *s = (PTState *)dev;
	int ret = LIBUSB_ERROR_IO;
	int xfer = 0;

	if (s->transfer_type_ep != devep)
	{
		if (pt_get_ep_type(s, s->transfer_type))
			s->transfer_type_ep = devep;
		else
			return USB_RET_STALL;
	}

	if (s->transfer_type == USB_ENDPOINT_TYPE_BULK)
		ret = libusb_bulk_transfer (s->usb_handle,
						devep, data, len, &xfer, 1000);
	else //if (s->transfer_type == USB_ENDPOINT_TYPE_INTERRUPT)
		ret = libusb_interrupt_transfer (s->usb_handle,
						devep, data, len, &xfer, 1000);
	if (ret < 0)
	{
		OSDebugOut(TEXT("error: %d\n"), ret);
		ret = libusb_err_to_qemu(ret);
	}
	else
		ret = xfer;

	return ret;
}

static int pt_handle_control (USBDevice *dev, int request, int value,
							int index, int length, uint8_t *data)
{
	PTState *s = (PTState *)dev;
	int ret = 0;
	int bmRequestType = ( request >> 8) & 0xFF;
	int bRequest = request & 0xFF;

	switch(request) {
	case DeviceRequest | USB_REQ_GET_CONFIGURATION:
		OSDebugOut(TEXT("Get configuration %d\n"), s->config);
		data[0] = s->config;
		ret = 1;
		break;
	case DeviceOutRequest | USB_REQ_SET_CONFIGURATION:
		s->config = index; //TODO
		//ret = libusb_reset_device (s->usb_handle);
		pt_release_interfaces (s, s->config);
		ret = libusb_set_configuration (s->usb_handle, s->config);
		OSDebugOut(TEXT("Set configuration, index %d val %d ret=%d\n"), index, value, ret);

		ret = libusb_err_to_qemu (ret);
		break;
	case InterfaceRequest | USB_REQ_GET_INTERFACE:
		OSDebugOut(TEXT("Get interface, intf %d altset %d\n"), index, s->altset);
		data[0] = s->altset;
		ret = 1;
		break;
	case InterfaceOutRequest | USB_REQ_SET_INTERFACE: //TODO
		if (s->intf > -1 && s->intf != index)
			ret = libusb_release_interface (s->usb_handle, s->intf);

		s->intf = index;
		s->altset = value;

		ret = libusb_claim_interface (s->usb_handle, s->intf);
		OSDebugOut(TEXT("libusb_claim_interface %d, ret=%d\n"), s->intf, ret);

		if (ret == 0)
		{
			ret = libusb_set_interface_alt_setting (s->usb_handle, s->intf, s->altset);
			OSDebugOut(TEXT("libusb_set_interface_alt_setting: %d %d ret=%d\n"), s->intf, s->altset, ret);
		}

		ret = libusb_err_to_qemu (ret);
		break;
	case EndpointOutRequest | USB_REQ_CLEAR_FEATURE:
		ret = libusb_clear_halt (s->usb_handle, index); //TODO
		OSDebugOut(TEXT("clear halt: 0x%02x %d ret=%d\n"), index, value, ret);
		break;
	case DeviceOutRequest | USB_REQ_SET_ADDRESS:
		OSDebugOut(TEXT("Set address: %d\n"), value);
		s->dev.addr = value;
		ret = 0;
		break;

	default:
		ret = libusb_control_transfer (s->usb_handle, bmRequestType,
										bRequest, value, index,
										data, length, 1000);
		OSDebugOut(TEXT("request=0x%04x, index=0x%04x, value=%d, ret=%d\n"), 
					request, value, index, ret);
		if (ret < 0)
			ret = libusb_err_to_qemu(ret);
		break;
	}
	return ret;
}

static void pt_handle_reset(USBDevice *dev)
{
	/* XXX: do it */
	PTState *s = (PTState*)dev;
	libusb_reset_device (s->usb_handle);
	return;
}

static void pt_handle_destroy(USBDevice *dev)
{
	PTState *s = (PTState *)dev;
	if (s)
	{
		if (s->usb_handle)
		{
			//pt_release_intf (s);
			libusb_release_interface (s->usb_handle, s->intf);
			libusb_device *dev = libusb_get_device (s->usb_handle);
			libusb_unref_device (dev);
			libusb_close (s->usb_handle);
		}
		libusb_exit (s->usb_ctx);
	}
	free(s);
}

static int pt_open(USBDevice *dev)
{
	PTState *s = (PTState *) dev;
	return 0;
}

static void pt_close(USBDevice *dev)
{
	PTState *s = (PTState *) dev;
}

static libusb_device* find_device(const ConfigUSBDevice& dev)
{
	// discover devices
	libusb_device **list;
	libusb_device *found = nullptr;
	libusb_device_descriptor desc;
	ssize_t cnt = libusb_get_device_list(nullptr, &list);
	int err = 0;

	if (cnt < 0)
		return nullptr;

	for (ssize_t i = 0; i < cnt; i++) {
		libusb_device *device = list[i];

		int r = libusb_get_device_descriptor(device, &desc);
		if (r < 0) {
			OSDebugOut(TEXT("failed to get device descriptor\n"));
			continue;
		}

		bool bp = dev.ignore_busport ||
			(dev.bus == libusb_get_bus_number (device)) &&
			(dev.port == libusb_get_port_number (device));

		if (bp && desc.idProduct == dev.pid && desc.idVendor == dev.vid) {
			found = libusb_ref_device(device);
			break;
		}
	}

	libusb_free_device_list(list, 1);
	return found;
}

USBDevice *PTDevice::CreateDevice(int port)
{
	int r;
	libusb_device *dev;
	libusb_device_descriptor desc;

	CONFIGVARIANT var(N_DEVICE, CONFIG_TYPE_CHAR);
	if (!LoadSetting(port, APINAME, var))
		return nullptr;

	CONFIGVARIANT varIgnore(N_IGNORE_BUSPORT, CONFIG_TYPE_INT);
	LoadSetting(port, APINAME, varIgnore);

	ConfigUSBDevice current(varIgnore.intValue);
	if (sscanf(var.strValue.c_str(), "%d:%d:%x:%x:",
		&current.bus, &current.port, &current.vid, &current.pid) == EOF)
		return nullptr;

	PTState *s = (PTState *)qemu_mallocz(sizeof(PTState));

	if (!s)
		return nullptr;

	r = libusb_init (&s->usb_ctx);

	if (r < 0) {
		SysMessage (TEXT("usb-pt: Error initing libusb\n"));
		goto fail;
	}

	dev = find_device (current);
	if (!dev)
		goto fail;

	r = libusb_open (dev, &s->usb_handle);
	if (r < 0)
	{
		SysMessage (TEXT("usb-pt: Cannot open usb device %04X:%04X\n"),
			r, current.vid, current.pid);
	}

	r = libusb_reset_device (s->usb_handle);
	OSDebugOut (TEXT("libusb_reset_device: %d\n"), r);

	r = libusb_get_device_descriptor (dev, &desc);
	if (r < 0) {
		SysMessage (TEXT("usb-pt: Failed to get device descriptor"));
		goto fail;
	}

	r = libusb_set_auto_detach_kernel_driver (s->usb_handle, 0);
	OSDebugOut (TEXT("libusb_set_auto_detach_kernel_driver %d \n"), r);
	libusb_set_debug(s->usb_ctx, LIBUSB_LOG_LEVEL_DEBUG);
	
	libusb_device_descriptor dev_desc;
	if (!libusb_get_device_descriptor (dev, &dev_desc))
	{
		for (int i=0; i<dev_desc.bNumConfigurations; i++)
			pt_release_interfaces (s, i);
	} else {
		OSDebugOut (TEXT("failed to release all interfaces\n"));
	}

	s->intf = -1;
	//r = libusb_claim_interface (s->usb_handle, s->intf);
	//OSDebugOut (TEXT("claim intf 0: %d \n"), r);

	s->dev.speed          = USB_SPEED_FULL;
	s->dev.handle_packet  = usb_generic_handle_packet;
	s->dev.handle_reset   = pt_handle_reset;
	s->dev.handle_control = pt_handle_control;
	s->dev.handle_data    = pt_handle_data;
	s->dev.handle_destroy = pt_handle_destroy;
	s->dev.open           = pt_open;
	s->dev.close          = pt_close;
	s->port               = port;

	return (USBDevice *)s;
fail:
	pt_handle_destroy ((USBDevice*)s);
	return nullptr;
}

/*int PTDevice::Configure(int port, const std::string& api, void *data)
{
	return RESULT_CANCELED;
}*/

int PTDevice::Freeze(int mode, USBDevice *dev, void *data)
{
	PTState *s = (PTState *)dev;

	switch (mode)
	{
		case FREEZE_SIZE:
			return 0;
		default:
		break;
	}
	return -1;
}

REGISTER_DEVICE(DEVTYPE_PT, DEVICENAME, PTDevice);
#undef DEVICENAME

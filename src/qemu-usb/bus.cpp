#include "USBinternal.h"
/**
	These are modified from vanilla functions and use extra klass field from USBDevice instead of a macro to get USBDeviceClass.
*/

void usb_device_set_interface(USBDevice *dev, int intf,
								int alt_old, int alt_new)
{
	USBDeviceClass *klass = dev->klass;
	if (klass->set_interface) {
		klass->set_interface(dev, intf, alt_old, alt_new);
	}
}


static int usb_device_init(USBDevice *dev)
{
	USBDeviceClass *klass = dev->klass;
	if (klass->init) {
		return klass->init(dev);
	}
	return 0;
}

USBDevice *usb_device_find_device(USBDevice *dev, uint8_t addr)
{
	USBDeviceClass *klass = dev->klass;
	if (klass->find_device) {
		return klass->find_device(dev, addr);
	}
	return NULL;
}

static void usb_device_handle_destroy(USBDevice *dev)
{
	USBDeviceClass *klass = dev->klass;
	if (klass->handle_destroy) {
		klass->handle_destroy(dev);
	}
}

void usb_device_cancel_packet(USBDevice *dev, USBPacket *p)
{
	USBDeviceClass *klass = dev->klass;
	if (klass->cancel_packet) {
		klass->cancel_packet(dev, p);
	}
}

void usb_device_handle_attach(USBDevice *dev)
{
	USBDeviceClass *klass = dev->klass;
	if (klass->handle_attach) {
		klass->handle_attach(dev);
	}
}

void usb_device_handle_reset(USBDevice *dev)
{
	USBDeviceClass *klass = dev->klass;
	if (klass->handle_reset) {
		klass->handle_reset(dev);
	}
}

void usb_device_handle_control(USBDevice *dev, USBPacket *p, int request,
							int value, int index, int length, uint8_t *data)
{
	USBDeviceClass *klass = dev->klass;
	if (klass->handle_control) {
		klass->handle_control(dev, p, request, value, index, length, data);
	}
}

void usb_device_handle_data(USBDevice *dev, USBPacket *p)
{
	USBDeviceClass *klass = dev->klass;
	if (klass->handle_data) {
		klass->handle_data(dev, p);
	}
}

const char *usb_device_get_product_desc(USBDevice *dev)
{
	USBDeviceClass *klass = dev->klass;
	return klass->product_desc;
}

const USBDesc *usb_device_get_usb_desc(USBDevice *dev)
{
	USBDeviceClass *klass = dev->klass;
	if (dev->usb_desc) {
		return dev->usb_desc;
	}
	return klass->usb_desc;
}

void usb_device_flush_ep_queue(USBDevice *dev, USBEndpoint *ep)
{
	USBDeviceClass *klass = dev->klass;
	if (klass->flush_ep_queue) {
		klass->flush_ep_queue(dev, ep);
	}
}

void usb_device_ep_stopped(USBDevice *dev, USBEndpoint *ep)
{
	USBDeviceClass *klass = dev->klass;
	if (klass->ep_stopped) {
		klass->ep_stopped(dev, ep);
	}
}
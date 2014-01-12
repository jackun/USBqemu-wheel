/* 
 * USB Mass Storage Device emulation
 *
 * Copyright (c) 2006 CodeSourcery.
 * Written by Paul Brook
 *
 * This code is licenced under the LGPL.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vl.h"

#define le32_to_cpu(x) (x)
#define cpu_to_le32(x) (x)

struct usb_msd_cbw {
    uint32_t sig;
    uint32_t tag;
    uint32_t data_len;
    uint8_t flags;
    uint8_t lun;
    uint8_t cmd_len;
    uint8_t cmd[16];
};

struct usb_msd_csw {
    uint32_t sig;
    uint32_t tag;
    uint32_t residue;
    uint8_t status;
};

#define LBA_BLOCK_SIZE 512
#define DEBUG_MSD

#ifdef DEBUG_MSD
#define DPRINTF(fmt, ...) { fprintf(stderr, "usb-msd: " fmt , __VA_ARGS__); }
#else
#define DPRINTF(fmt, ...) do {} while(0)
#endif

/* USB requests.  */
#define MassStorageReset  0x21ff
#define GetMaxLun         0xa1fe

enum USBMSDMode {
    USB_MSDM_CBW, /* Command Block.  */
    USB_MSDM_DATAOUT, /* Tranfer data to device.  */
    USB_MSDM_DATAIN, /* Transfer data from device.  */
    USB_MSDM_CSW /* Command Status.  */
};

typedef struct {
    USBDevice dev;
    enum USBMSDMode mode;
    uint32_t data_len;
    uint32_t tag;
    //SCSIDevice *scsi_dev;
    FILE *hfile;
    int result;

    uint32_t off; //buffer offset
    uint8_t buf[4096];//random length right now
    uint8_t last_cmd;
} MSDState;

static const uint8_t qemu_msd_dev_descriptor[] = {
	0x12,       /*  u8 bLength; */
	0x01,       /*  u8 bDescriptorType; Device */
	0x10, 0x00, /*  u16 bcdUSB; v1.0 */

	0x00,	    /*  u8  bDeviceClass; */
	0x00,	    /*  u8  bDeviceSubClass; */
	0x00,       /*  u8  bDeviceProtocol; [ low/full speeds only ] */
	0x08,       /*  u8  bMaxPacketSize0; 8 Bytes */

        /* Vendor and product id are arbitrary.  */
	0x00, 0x00, /*  u16 idVendor; */
 	0x00, 0x00, /*  u16 idProduct; */
	0x00, 0x00, /*  u16 bcdDevice */

	0x01,       /*  u8  iManufacturer; */
	0x02,       /*  u8  iProduct; */
	0x03,       /*  u8  iSerialNumber; */
	0x01        /*  u8  bNumConfigurations; */
};

static const uint8_t qemu_msd_config_descriptor[] = {

	/* one configuration */
	0x09,       /*  u8  bLength; */
	0x02,       /*  u8  bDescriptorType; Configuration */
	0x20, 0x00, /*  u16 wTotalLength; */
	0x01,       /*  u8  bNumInterfaces; (1) */
	0x01,       /*  u8  bConfigurationValue; */
	0x00,       /*  u8  iConfiguration; */
	0xc0,       /*  u8  bmAttributes; 
				 Bit 7: must be set,
				     6: Self-powered,
				     5: Remote wakeup,
				     4..0: resvd */
	0x00,       /*  u8  MaxPower; */
      
	/* one interface */
	0x09,       /*  u8  if_bLength; */
	0x04,       /*  u8  if_bDescriptorType; Interface */
	0x00,       /*  u8  if_bInterfaceNumber; */
	0x00,       /*  u8  if_bAlternateSetting; */
	0x02,       /*  u8  if_bNumEndpoints; */
	0x08,       /*  u8  if_bInterfaceClass; MASS STORAGE */
	0x06,       /*  u8  if_bInterfaceSubClass; SCSI */
	0x50,       /*  u8  if_bInterfaceProtocol; Bulk Only */
	0x00,       /*  u8  if_iInterface; */
     
	/* Bulk-In endpoint */
	0x07,       /*  u8  ep_bLength; */
	0x05,       /*  u8  ep_bDescriptorType; Endpoint */
	0x81,       /*  u8  ep_bEndpointAddress; IN Endpoint 1 */
 	0x02,       /*  u8  ep_bmAttributes; Bulk */
 	0x40, 0x00, /*  u16 ep_wMaxPacketSize; */
	0x00,       /*  u8  ep_bInterval; */

	/* Bulk-Out endpoint */
	0x07,       /*  u8  ep_bLength; */
	0x05,       /*  u8  ep_bDescriptorType; Endpoint */
	0x02,       /*  u8  ep_bEndpointAddress; OUT Endpoint 2 */
 	0x02,       /*  u8  ep_bmAttributes; Bulk */
 	0x40, 0x00, /*  u16 ep_wMaxPacketSize; */
	0x00        /*  u8  ep_bInterval; */
};


/*
 *      SCSI opcodes
 */

#define TEST_UNIT_READY       0x00
#define REZERO_UNIT           0x01
#define REQUEST_SENSE         0x03
#define FORMAT_UNIT           0x04
#define READ_BLOCK_LIMITS     0x05
#define REASSIGN_BLOCKS       0x07
#define READ_6                0x08
#define WRITE_6               0x0a
#define SEEK_6                0x0b
#define READ_REVERSE          0x0f
#define WRITE_FILEMARKS       0x10
#define SPACE                 0x11
#define INQUIRY               0x12
#define RECOVER_BUFFERED_DATA 0x14
#define MODE_SELECT           0x15
#define RESERVE               0x16
#define RELEASE               0x17
#define COPY                  0x18
#define ERASE                 0x19
#define MODE_SENSE            0x1a
#define START_STOP            0x1b
#define RECEIVE_DIAGNOSTIC    0x1c
#define SEND_DIAGNOSTIC       0x1d
#define ALLOW_MEDIUM_REMOVAL  0x1e

#define SET_WINDOW            0x24
#define READ_CAPACITY         0x25
#define READ_10               0x28
#define WRITE_10              0x2a
#define SEEK_10               0x2b
#define WRITE_VERIFY          0x2e
#define VERIFY                0x2f
#define SEARCH_HIGH           0x30
#define SEARCH_EQUAL          0x31
#define SEARCH_LOW            0x32
#define SET_LIMITS            0x33
#define PRE_FETCH             0x34
#define READ_POSITION         0x34
#define SYNCHRONIZE_CACHE     0x35
#define LOCK_UNLOCK_CACHE     0x36
#define READ_DEFECT_DATA      0x37
#define MEDIUM_SCAN           0x38
#define COMPARE               0x39
#define COPY_VERIFY           0x3a
#define WRITE_BUFFER          0x3b
#define READ_BUFFER           0x3c
#define UPDATE_BLOCK          0x3d
#define READ_LONG             0x3e
#define WRITE_LONG            0x3f
#define CHANGE_DEFINITION     0x40
#define WRITE_SAME            0x41
#define READ_TOC              0x43
#define LOG_SELECT            0x4c
#define LOG_SENSE             0x4d
#define MODE_SELECT_10        0x55
#define RESERVE_10            0x56
#define RELEASE_10            0x57
#define MODE_SENSE_10         0x5a
#define PERSISTENT_RESERVE_IN 0x5e
#define PERSISTENT_RESERVE_OUT 0x5f
#define MAINTENANCE_IN        0xa3
#define MAINTENANCE_OUT       0xa4
#define MOVE_MEDIUM           0xa5
#define READ_12               0xa8
#define WRITE_12              0xaa
#define WRITE_VERIFY_12       0xae
#define SEARCH_HIGH_12        0xb0
#define SEARCH_EQUAL_12       0xb1
#define SEARCH_LOW_12         0xb2
#define READ_ELEMENT_STATUS   0xb8
#define SEND_VOLUME_TAG       0xb6
#define WRITE_LONG_2          0xea

/* from hw/scsi-generic.c */
#define REWIND 0x01
#define REPORT_DENSITY_SUPPORT 0x44
#define GET_CONFIGURATION 0x46
#define READ_16 0x88
#define WRITE_16 0x8a
#define WRITE_VERIFY_16 0x8e
#define SERVICE_ACTION_IN 0x9e
#define REPORT_LUNS 0xa0
#define LOAD_UNLOAD 0xa6
#define SET_CD_SPEED 0xbb
#define BLANK 0xa1

/*
 *  Status codes
 */

#define GOOD                 0x00
#define CHECK_CONDITION      0x01
#define CONDITION_GOOD       0x02
#define BUSY                 0x04
#define INTERMEDIATE_GOOD    0x08
#define INTERMEDIATE_C_GOOD  0x0a
#define RESERVATION_CONFLICT 0x0c
#define COMMAND_TERMINATED   0x11
#define QUEUE_FULL           0x14

#define STATUS_MASK          0x3e

/*
 *  SENSE KEYS
 */

#define NO_SENSE            0x00
#define RECOVERED_ERROR     0x01
#define NOT_READY           0x02
#define MEDIUM_ERROR        0x03
#define HARDWARE_ERROR      0x04
#define ILLEGAL_REQUEST     0x05
#define UNIT_ATTENTION      0x06
#define DATA_PROTECT        0x07
#define BLANK_CHECK         0x08
#define COPY_ABORTED        0x0a
#define ABORTED_COMMAND     0x0b
#define VOLUME_OVERFLOW     0x0d
#define MISCOMPARE          0x0e

static void usb_msd_command_complete(void *opaque, uint32_t tag, int fail)
{
    MSDState *s = (MSDState *)opaque;

    DPRINTF("Command complete\n");
    s->result = fail;
    s->mode = USB_MSDM_CSW;
}

static void usb_msd_handle_reset(USBDevice *dev)
{
    MSDState *s = (MSDState *)dev;

    DPRINTF("Reset\n");
    s->mode = USB_MSDM_CBW;
}

#define bswap32(x) (\
        (((x)>>24)&0xff)\
        |\
        (((x)>>8)&0xff00)\
        |\
        (((x)<<8)&0xff0000)\
        |\
        (((x)<<24)&0xff000000)\
)

#define bswap16(x) ( (((x)>>8)&0xff) | (((x)<<8)&0xff00) )

static void send_command(void *opaque, struct usb_msd_cbw *cbw, uint8_t *data, uint32_t len)
{
	MSDState *s = (MSDState *)opaque;
	DPRINTF("Command: lun=%d tag=0x%x len %zd data=0x%02x", cbw->lun, cbw->tag, cbw->cmd[0]);

	uint32_t lba;
	uint16_t xfer_len;

	switch(cbw->cmd[0])
	{
	case INQUIRY:
		s->last_cmd = INQUIRY;
		memset(s->buf, 0, sizeof(s->buf));
		s->off = 0;
		s->buf[0] = 0 & 0x4; //0x1f - no fdd
		s->buf[1] = 1 << 7; //removable
		s->buf[3] = 1; //UFI response data format
		//inq data len can be zero
		memcpy(&s->buf[8], "QEMU", 4); //8 bytes vendor
		memcpy(&s->buf[16], "USB Drive", 9); //16 bytes product
		memcpy(&s->buf[32], "1", 1); //4 bytes product revision
		s->result = 0;
		break;

	case READ_CAPACITY:
		s->last_cmd = READ_CAPACITY;
		long cur_tell, end_tell;
		uint32_t *last_lba, *blk_len;

		memset(s->buf, 0, sizeof(s->buf));
		s->off = 0;
		cur_tell = ftell(s->hfile);
		fseek(s->hfile, 0, SEEK_END);
		end_tell = ftell(s->hfile);
		fseek(s->hfile, cur_tell, SEEK_SET);

		last_lba = (uint32_t*)&s->buf[0];
		blk_len = (uint32_t*)&s->buf[4]; //in bytes
		//right?
		*blk_len = LBA_BLOCK_SIZE;//descriptor is currently max 64 bytes for bulk though
		*last_lba = end_tell / *blk_len;
		*last_lba = bswap32(*last_lba);
		*blk_len = bswap32(*blk_len);

		//DPRINTF("usb-msd: read capacity lba=0x%x, block=0x%x\n", *last_lba, *blk_len);
		s->result = GOOD;
		break;

	case READ_10:
		s->result = NO_SENSE;//everything is fine
		s->off = 0;
		s->last_cmd = READ_10;

		lba = bswap32(*(uint32_t *)&cbw->cmd[2]);
		xfer_len = bswap16(*(uint16_t *)&cbw->cmd[7]);
		DPRINTF("usb-msd: read (10) lba=0x%x, len=0x%x\n", lba, xfer_len * LBA_BLOCK_SIZE);

		//if(xfer_len == 0) //nothing to do
		//	break;
		if(fseek(s->hfile, lba * LBA_BLOCK_SIZE, SEEK_SET)) {
			s->result = 0x2;//?
			return;
		}

		memset(s->buf, 0, sizeof(s->buf));
		
		if(!(s->data_len = fread(s->buf, 1, /*s->data_len*/ xfer_len * LBA_BLOCK_SIZE, s->hfile)))
			s->result = 0x2;//?
		break;

	case WRITE_10:
		s->result = NO_SENSE;//everything is fine
		s->off = 0;
		s->last_cmd = WRITE_10;

		lba = bswap32(*(uint32_t *)&cbw->cmd[2]);
		xfer_len = bswap16(*(uint16_t *)&cbw->cmd[7]);
		DPRINTF("usb-msd: write (10) lba=0x%x, len=0x%x\n", lba, xfer_len * LBA_BLOCK_SIZE);

		//if(xfer_len == 0) //nothing to do
		//	break;
		if(fseek(s->hfile, lba * LBA_BLOCK_SIZE, SEEK_SET)) {
			s->result = 0x2;//?
			return;
		}
		s->data_len = xfer_len * LBA_BLOCK_SIZE;

		//if(!(s->data_len = fwrite(s->buf, 1, /*s->data_len*/ xfer_len * LBA_BLOCK_SIZE, s->hfile)))
		//	s->result = 0x2;//?
		break;
	default:
		//s->result = COMMAND_TERMINATED; //??
		s->last_cmd = -1;
		break;
	}
}

static int usb_msd_handle_control(USBDevice *dev, int request, int value,
                                  int index, int length, uint8_t *data)
{
    MSDState *s = (MSDState *)dev;
    int ret = 0;

    switch (request) {
    case DeviceRequest | USB_REQ_GET_STATUS:
        data[0] = (1 << USB_DEVICE_SELF_POWERED) |
            (dev->remote_wakeup << USB_DEVICE_REMOTE_WAKEUP);
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
            memcpy(data, qemu_msd_dev_descriptor, 
                   sizeof(qemu_msd_dev_descriptor));
            ret = sizeof(qemu_msd_dev_descriptor);
            break;
        case USB_DT_CONFIG:
            memcpy(data, qemu_msd_config_descriptor, 
                   sizeof(qemu_msd_config_descriptor));
            ret = sizeof(qemu_msd_config_descriptor);
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
                /* vendor description */
                ret = set_usb_string(data, "QEMU ");
                break;
            case 2:
                /* product description */
                ret = set_usb_string(data, "QEMU USB HARDDRIVE");
                break;
            case 3:
                /* serial number */
                ret = set_usb_string(data, "1");
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
	case InterfaceOutRequest | USB_REQ_SET_INTERFACE:
    case DeviceRequest | USB_REQ_GET_INTERFACE:
        data[0] = 0;
        ret = 1;
        break;
    case DeviceOutRequest | USB_REQ_SET_INTERFACE:
        ret = 0;
        break;
    case EndpointOutRequest | USB_REQ_CLEAR_FEATURE:
        if (value == 0 && index != 0x81) { /* clear ep halt */
            goto fail;
        }
        ret = 0;
        break;
        /* Class specific requests.  */
    case MassStorageReset:
        /* Reset state ready for the next CBW.  */
        s->mode = USB_MSDM_CBW;
        ret = 0;
        break;
    case GetMaxLun:
        data[0] = 0;
        ret = 1;
        break;
    default:
    fail:
        ret = USB_RET_STALL;
        break;
    }
    return ret;
}


static int usb_msd_handle_data(USBDevice *dev, int pid, uint8_t devep,
                               uint8_t *data, int len)
{
    MSDState *s = (MSDState *)dev;
    int ret = 0;
    struct usb_msd_cbw cbw;
    struct usb_msd_csw csw;

    switch (pid) {
    case USB_TOKEN_OUT:
        if (devep != 2)
            goto fail;

        switch (s->mode) {
        case USB_MSDM_CBW:
            if (len != 31) {
                fprintf(stderr, "usb-msd: Bad CBW size");
                goto fail;
            }
            memcpy(&cbw, data, 31);
            if (le32_to_cpu(cbw.sig) != 0x43425355) {
                fprintf(stderr, "usb-msd: Bad signature %08x\n",
                        le32_to_cpu(cbw.sig));
                goto fail;
            }
            DPRINTF("Command on LUN %d\n", cbw.lun);
            if (cbw.lun != 0) {
                fprintf(stderr, "usb-msd: Bad LUN %d\n", cbw.lun);
                goto fail;
            }
            s->tag = le32_to_cpu(cbw.tag);
            s->data_len = le32_to_cpu(cbw.data_len);
            if (s->data_len == 0) {
                s->mode = USB_MSDM_CSW;
            } else if (cbw.flags & 0x80) {
                s->mode = USB_MSDM_DATAIN;
            } else {
                s->mode = USB_MSDM_DATAOUT;
            }
            DPRINTF("Command tag 0x%x flags %08x len %d data %d\n",
                    s->tag, cbw.flags, cbw.cmd_len, s->data_len);
            //scsi_send_command(s->scsi_dev, s->tag, cbw.cmd, 0);
			send_command(s, &cbw, data, len);
            ret = len;
            break;

        case USB_MSDM_DATAOUT:
            DPRINTF("Data out %d/%d\n", len, s->data_len);
			//len == 0x1f falls into here on write error :S. USB_RET_STALL is correct?
            if (len > s->data_len)
                goto fail;

            if (!fwrite(data, 1, len, s->hfile))
                goto fail;

            s->data_len -= len;
            if (s->data_len == 0)
                s->mode = USB_MSDM_CSW;
            ret = len;
            break;

        default:
            DPRINTF("Unexpected write (len %d)\n", len);
            goto fail;
        }
        break;

    case USB_TOKEN_IN:
        if (devep != 1)
            goto fail;

        switch (s->mode) {
        case USB_MSDM_CSW:
            DPRINTF("Command status %d tag 0x%x, len %d\n",
                    s->result, s->tag, len);
            if (len < 13)
                goto fail;

            csw.sig = cpu_to_le32(0x53425355);
            csw.tag = cpu_to_le32(s->tag);
            csw.residue = 0;
            csw.status = s->result;
            memcpy(data, &csw, 13);
            ret = 13;
            s->mode = USB_MSDM_CBW;
            break;

        case USB_MSDM_DATAIN:
            DPRINTF("Data in %d/%d\n", len, s->data_len);
            if (len > s->data_len)
                len = s->data_len;

			if(s->off >= sizeof(s->buf)) //TODO off by one?
				goto fail;

			memcpy(data, &s->buf[s->off], len);
			s->off += len;

            s->data_len -= len;
            if (s->data_len == 0)
                s->mode = USB_MSDM_CSW;
            ret = len;
            break;

        default:
            DPRINTF("Unexpected read (len %d)\n", len);
            goto fail;
        }
        break;

    default:
        DPRINTF("Bad token\n");
    fail:
        ret = USB_RET_STALL;
		s->mode = USB_MSDM_CBW;
        break;
    }

    return ret;
}

static void usb_msd_handle_destroy(USBDevice *dev)
{
    MSDState *s = (MSDState *)dev;

    //scsi_disk_destroy(s->scsi_dev);
    //qemu_free(s);
	if(s->hfile)
		fclose(s->hfile);
}

USBDevice *usb_msd_init(const char *filename)
{
    MSDState *s;

    s = (MSDState*)qemu_mallocz(sizeof(MSDState));
    if (!s)
        return NULL;

	s->hfile = NULL;
    s->hfile = fopen(filename, "r+b");
	if (!s->hfile)
        return NULL;

    s->last_cmd = -1;
    s->dev.speed = USB_SPEED_FULL;
    s->dev.handle_packet = usb_generic_handle_packet;

    s->dev.handle_reset = usb_msd_handle_reset;
    s->dev.handle_control = usb_msd_handle_control;
    s->dev.handle_data = usb_msd_handle_data;
    s->dev.handle_destroy = usb_msd_handle_destroy;

	sprintf_s(s->dev.devname, "QEMU USB MSD(%.16s)",
             filename);

    usb_msd_handle_reset((USBDevice *)s);
    return (USBDevice *)s;
}

/*
 * f_serial.c - generic USB serial function driver
 *
 * Copyright (C) 2003 Al Borchers (alborchers@steinerpoint.com)
 * Copyright (C) 2008 by David Brownell
 * Copyright (C) 2008 by Nokia Corporation
 *
 * This software is distributed under the terms of the GNU General
 * Public License ("GPL") as published by the Free Software Foundation,
 * either version 2 of that License or (at your option) any later version.
 */

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/device.h>

#include <linux/usb/composite.h>
#include <linux/usb/cdc.h>

#include "u_serial.h"
#include "gadget_chips.h"


/*
 * This function packages a simple "generic serial" port with no real
 * control mechanisms, just raw data transfer over two bulk endpoints.
 *
 * Because it's not standardized, this isn't as interoperable as the
 * CDC ACM driver.  However, for many purposes it's just as functional
 * if you can arrange appropriate host side drivers.
 */


struct f_com {
	struct gserial			port;
	u8				data_id;
	u8				port_num;
};

static inline struct f_com *func_to_com(struct usb_function *f)
{
	return container_of(f, struct f_com, port.func);
}

/*-------------------------------------------------------------------------*/

/* interface descriptor: */

static struct usb_interface_descriptor com_interface_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	/* .bInterfaceNumber = DYNAMIC */
	.bNumEndpoints =	2,
	.bInterfaceClass =	USB_CLASS_VENDOR_SPEC,
	.bInterfaceSubClass =	0,
	.bInterfaceProtocol =	0,
	/* .iInterface = DYNAMIC */
};

/* full speed support: */

static struct usb_endpoint_descriptor com_fs_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor com_fs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *com_fs_function[] = {
	(struct usb_descriptor_header *) &com_interface_desc,
	(struct usb_descriptor_header *) &com_fs_in_desc,
	(struct usb_descriptor_header *) &com_fs_out_desc,
	NULL,
};

/* high speed support: */

static struct usb_endpoint_descriptor com_hs_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_endpoint_descriptor com_hs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_descriptor_header *com_hs_function[] = {
	(struct usb_descriptor_header *) &com_interface_desc,
	(struct usb_descriptor_header *) &com_hs_in_desc,
	(struct usb_descriptor_header *) &com_hs_out_desc,
	NULL,
};

static struct usb_endpoint_descriptor com_ss_in_desc __initdata = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_endpoint_descriptor com_ss_out_desc __initdata = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor com_ss_bulk_comp_desc __initdata = {
	.bLength =              sizeof com_ss_bulk_comp_desc,
	.bDescriptorType =      USB_DT_SS_ENDPOINT_COMP,
};

static struct usb_descriptor_header *com_ss_function[] __initdata = {
	(struct usb_descriptor_header *) &com_interface_desc,
	(struct usb_descriptor_header *) &com_ss_in_desc,
	(struct usb_descriptor_header *) &com_ss_bulk_comp_desc,
	(struct usb_descriptor_header *) &com_ss_out_desc,
	(struct usb_descriptor_header *) &com_ss_bulk_comp_desc,
	NULL,
};

/* string descriptors: */

static struct usb_string com_string_defs[] = {
	[0].s = "Generic Serial",
	{  } /* end of list */
};

static struct usb_gadget_strings com_string_table = {
	.language =		0x0409,	/* en-us */
	.strings =		com_string_defs,
};

static struct usb_gadget_strings *com_strings[] = {
	&com_string_table,
	NULL,
};

/*-------------------------------------------------------------------------*/

static int com_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
	struct f_com		*com = func_to_com(f);
	struct usb_composite_dev *cdev = f->config->cdev;

	/* we know alt == 0, so this is an activation or a reset */

	if (com->port.in->driver_data) {
		DBG(cdev, "reset generic ttyGS%d\n", com->port_num);
		gserial_disconnect(&com->port);
	}
	/* if (!com->port.in->desc || !com->port.out->desc) { */
	DBG(cdev, "activate generic ttyGS%d\n", com->port_num);
	if (config_ep_by_speed(cdev->gadget, f, com->port.in) ||
	    config_ep_by_speed(cdev->gadget, f, com->port.out)) {
		com->port.in->desc = NULL;
		com->port.out->desc = NULL;
		return -EINVAL;
	}
	/* } */
	gserial_connect(&com->port, com->port_num);
	return 0;
}

static void com_disable(struct usb_function *f)
{
	struct f_com	*com = func_to_com(f);
	struct usb_composite_dev *cdev = f->config->cdev;

	DBG(cdev, "generic ttyGS%d deactivated\n", com->port_num);
	gserial_disconnect(&com->port);
}

/*-------------------------------------------------------------------------*/

/* serial function driver setup/binding */

static int 
com_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct f_com		*com = func_to_com(f);
	int			status;
	struct usb_ep		*ep;

	/* allocate instance-specific interface IDs */
	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;
	com->data_id = status;
	com_interface_desc.bInterfaceNumber = status;

	status = -ENODEV;

	/* allocate instance-specific endpoints */
	ep = usb_ep_autoconfig(cdev->gadget, &com_fs_in_desc);
	if (!ep)
		goto fail;
	com->port.in = ep;
	ep->driver_data = cdev;	/* claim */

	ep = usb_ep_autoconfig(cdev->gadget, &com_fs_out_desc);
	if (!ep)
		goto fail;
	com->port.out = ep;
	ep->driver_data = cdev;	/* claim */

	/* copy descriptors, and track endpoint copies */
	f->descriptors = usb_copy_descriptors(com_fs_function);


	/* support all relevant hardware speeds... we expect that when
	 * hardware is dual speed, all bulk-capable endpoints work at
	 * both speeds
	 */
	if (gadget_is_dualspeed(c->cdev->gadget)) {
		com_hs_in_desc.bEndpointAddress =
				com_fs_in_desc.bEndpointAddress;
		com_hs_out_desc.bEndpointAddress =
				com_fs_out_desc.bEndpointAddress;

		/* copy descriptors, and track endpoint copies */
		f->hs_descriptors = usb_copy_descriptors(com_hs_function);
	}
	if (gadget_is_superspeed(c->cdev->gadget)) {
		com_ss_in_desc.bEndpointAddress =
			com_fs_in_desc.bEndpointAddress;
		com_ss_out_desc.bEndpointAddress =
			com_fs_out_desc.bEndpointAddress;

		/* copy descriptors, and track endpoint copies */
		f->ss_descriptors = usb_copy_descriptors(com_ss_function);
		if (!f->ss_descriptors)
			goto fail;
	}

	DBG(cdev, "generic ttyGS%d: %s speed IN/%s OUT/%s\n",
			com->port_num,
			gadget_is_superspeed(c->cdev->gadget) ? "super" :
			gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full",
			com->port.in->name, com->port.out->name);
	return 0;

fail:
	/* we might as well release our claims on endpoints */
	if (com->port.out)
		com->port.out->driver_data = NULL;
	if (com->port.in)
		com->port.in->driver_data = NULL;

	ERROR(cdev, "%s: can't bind, err %d\n", f->name, status);

	return status;
}

static void
com_unbind(struct usb_configuration *c, struct usb_function *f)
{
	if (gadget_is_dualspeed(c->cdev->gadget))
		usb_free_descriptors(f->hs_descriptors);
	if (gadget_is_superspeed(c->cdev->gadget))
		usb_free_descriptors(f->ss_descriptors);
	usb_free_descriptors(f->descriptors);
	kfree(func_to_com(f));
}

/**
 * com_bind_config - add a generic serial function to a configuration
 * @c: the configuration to support the serial instance
 * @port_num: /dev/ttyGS* port this interface will use
 * Context: single threaded during gadget setup
 *
 * Returns zero on success, else negative errno.
 *
 * Caller must have called @gserial_setup() with enough ports to
 * handle all the ones it binds.  Caller is also responsible
 * for calling @gserial_cleanup() before module unload.
 */
int com_bind_config(struct usb_configuration *c, u8 port_num)
{
	struct f_com	*com;
	int		status;

	/* REVISIT might want instance-specific strings to help
	 * distinguish instances ...
	 */

	/* maybe allocate device-global string ID */
	if (com_string_defs[0].id == 0) {
		status = usb_string_id(c->cdev);
		if (status < 0)
			return status;
		com_string_defs[0].id = status;
	}

	/* allocate and initialize one new instance */
	com = kzalloc(sizeof *com, GFP_KERNEL);
	if (!com)
		return -ENOMEM;

	com->port_num = port_num;

	com->port.func.name = kasprintf(GFP_KERNEL, "com%u", port_num);
	com->port.func.strings = com_strings;
	com->port.func.bind = com_bind;
	com->port.func.unbind = com_unbind;
	com->port.func.set_alt = com_set_alt;
	com->port.func.disable = com_disable;

	status = usb_add_function(c, &com->port.func);
	if (status)
		kfree(com);
	return status;
}

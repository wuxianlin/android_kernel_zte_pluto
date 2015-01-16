/*
 * ZTE USB debug Driver for Android
 *
 * Copyright (C) 2012 ZTE, Inc.
 * Author: Li Xingyuan <li.xingyuan@zte.com.cn>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/sysrq.h>

static void dbg_sysrq_complete(struct usb_ep *ep, struct usb_request *req)
{
	int length = req->actual;
	char key;
	
	if (req->status != 0) {
		pr_err("%s: err %d\n", __func__, req->status);
		return;
	}

	if (length >= 1) {
		key = ((char *)req->buf)[0];
		pr_info("%s: %02x\n", __func__, key);
		handle_sysrq(key);
	}
}

#define CMD_BUFF_LEN	1024
#define CMD_ARGV_NUM	10
static char buf[CMD_BUFF_LEN + 1];
static int pending;

static void dbg_run_work(struct work_struct *bullshit)
{
	char *p = buf;
	char *argv[CMD_ARGV_NUM + 1];
	char *envp[] = {
		"HOME=/data",
		"LD_LIBRARY_PATH=/system/lib",
		"PATH=/sbin:/system/bin:/system/xbin",
		NULL};
	int i;
	int ret;

	pr_info("%s: +\n", __func__);
	
	i = 0;
	while (p && i < CMD_ARGV_NUM) {
		argv[i] = strsep(&p, " ");
		if (strlen(argv[i])) {
			pr_info("%s: argv=%s, i=%d\n", __func__, argv[i], i);
			i++;
		}
	}
	argv[i] = NULL;
	ret = call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC);
	pr_info("%s: ret=%d\n", __func__, ret);
	pending = 0;
}

static DECLARE_WORK(work, dbg_run_work);

static void dbg_run_complete(struct usb_ep *ep, struct usb_request *req)
{
	int length = req->actual;
	char *p;
	
	if (req->status != 0) {
		pr_err("%s: err %d\n", __func__, req->status);
		return;
	}

	if (!length || length > CMD_BUFF_LEN)
		return;

	p = strim(req->buf);
	pr_info("%s: p=%s\n", __func__, p);
	memcpy(buf, p, length);
	p[length] = '\0';

	pending = 1;
	schedule_work(&work);
}

static int dbg_ctrlrequest(struct usb_composite_dev *cdev,
				const struct usb_ctrlrequest *ctrl)
{
	int	value = -EOPNOTSUPP;
	u8 b_requestType = ctrl->bRequestType;
	u8 b_request = ctrl->bRequest;
	u16	w_index = le16_to_cpu(ctrl->wIndex);
	u16	w_value = le16_to_cpu(ctrl->wValue);
	u16	w_length = le16_to_cpu(ctrl->wLength);

	pr_debug("%s: %02x %02x %04x %04x %04x\n", __func__, 
		b_requestType, b_request, w_index, w_value, w_length);

	if (b_requestType == (USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE)
			&& w_value == 0xffff && w_index == 0xffff) {
		if (b_request == 0x00) {
			pr_info("%s: sysrq request!\n", __func__);
			cdev->req->complete = dbg_sysrq_complete;
			value = w_length;
		} else if (b_request == 0x01) {
			if (!pending) {
				pr_info("%s: call user request!\n", __func__);
				cdev->req->complete = dbg_run_complete;
				value = w_length;
			} else {
				pr_info("%s: request pending!\n", __func__);
			}
		}
	}
	
	if (value >= 0) {
		cdev->req->zero = 0;
		cdev->req->length = value;
		value = usb_ep_queue(cdev->gadget->ep0, cdev->req, GFP_ATOMIC);
		if (value < 0)
			pr_err("%s: setup response queue error %d\n",
				__func__, value);
	}

	return value;
}

/*
 * nvc_ov12830.h - ov12830 sensor driver
 *
 *  * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * Contributors:
 *	Frank Shi <fshi@nvidia.com>
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#ifndef __OV12830_H__
#define __OV12830_H__

#include <media/nvc.h>
#include <media/nvc_image.h>
#include <linux/edp.h>

/* See notes in the nvc.h file on the GPIO usage */
enum ov12830_gpio_type {
	OV12830_GPIO_TYPE_SHTDN = 0,
	OV12830_GPIO_TYPE_PWRDN,
	OV12830_GPIO_GP1,
	OV12830_GPIO_GP2,
};

struct ov12830_power_rail {
	struct regulator *dvdd;
	struct regulator *avdd;
	struct regulator *dovdd;
};

struct ov12830_platform_data {
	unsigned cfg;
	unsigned num;
	unsigned sync;
	const char *dev_name;
	unsigned gpio_count;
	struct nvc_gpio_pdata *gpio; /* see nvc.h GPIO notes */
	struct nvc_imager_cap *cap;
	unsigned lens_focal_length; /* / _INT2FLOAT_DIVISOR */
	unsigned lens_max_aperture; /* / _INT2FLOAT_DIVISOR */
	unsigned lens_fnumber; /* / _INT2FLOAT_DIVISOR */
	unsigned lens_view_angle_h; /* / _INT2FLOAT_DIVISOR */
	unsigned lens_view_angle_v; /* / _INT2FLOAT_DIVISOR */
	int (*probe_clock)(unsigned long);
	int (*power_on)(struct ov12830_power_rail *);
	int (*power_off)(struct ov12830_power_rail *);
	struct edp_client edpc_config;
};
#endif  /* __OV12830_H__ */

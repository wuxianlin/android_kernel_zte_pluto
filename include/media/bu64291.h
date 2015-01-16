/*
 * Copyright (C) 2011-2012 NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#ifndef __BU64291_H__
#define __BU64291_H__

#include <media/nvc_focus.h>
#include <media/nvc.h>

/* See notes in the nvc.h file on the GPIO usage */
enum bu64291_gpio_type {
	BU64291_GPIO_TYPE_PWRDN = 0,
};

struct bu64291_power_rail {
	struct regulator *vdd;
	struct regulator *vdd_i2c;
};

struct bu64291_platform_data {
	int cfg;
	int num;
	int sync;
	const char *dev_name;
	struct nvc_focus_nvc (*nvc);
	struct nvc_focus_cap (*cap);
	struct bu64291_pdata_info (*info);
	int gpio_count;
	struct nvc_gpio_pdata *gpio;
	int (*power_on)(struct bu64291_power_rail *pw);
	int (*power_off)(struct bu64291_power_rail *pw);
};

struct bu64291_pdata_info {
	float focal_length;
	float fnumber;
	__u32 settle_time;
	__s16 pos_low;
	__s16 pos_high;
	__s16 limit_low;
	__s16 limit_high;
	int move_timeoutms;
	__u32 focus_hyper_ratio;
	__u32 focus_hyper_div;
};

#endif
/* __BU64291_H__ */

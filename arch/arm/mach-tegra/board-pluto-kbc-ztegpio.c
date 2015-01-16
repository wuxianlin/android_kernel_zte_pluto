/*
 * arch/arm/mach-tegra/board-pluto-kbc-ztegpio.c
 * Keys configuration for Nvidia tegra3 ztepluto platform.
 *
 * Copyright (C) 2011 NVIDIA, Inc.
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

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/gpio_keys.h>
#include <asm/io.h>
#include <mach/io.h>
#include <mach/iomap.h>

#include "gpio-names.h"
#include "wakeups-t11x.h"


/* PMC Wake status registers */
#define PMC_WAKE_STATUS	0x14
#define PMC_WAKE2_STATUS	0x168

#define GPIO_KEY(_id, _gpio, _iswake)		\
	{					\
		.code = _id,			\
		.gpio = TEGRA_GPIO_##_gpio,	\
		.active_low = 1,		\
		.desc = #_id,			\
		.type = EV_KEY,			\
		.wakeup = _iswake,		\
		.debounce_interval = 10,	\
	}

#define GPIO_IKEY(_id, _irq, _iswake, _deb)	\
	{					\
		.code = _id,			\
		.gpio = -1,			\
		.irq = _irq,			\
		.desc = #_id,			\
		.type = EV_KEY,			\
		.wakeup = _iswake,		\
		.debounce_interval = _deb,	\
	}

static struct gpio_keys_button ztepluto_keys[] = {
    [0] = GPIO_KEY(KEY_POWER, PS2, 1),
    [1] = GPIO_KEY(KEY_VOLUMEUP, PQ1, 0),
    [2] = GPIO_KEY(KEY_VOLUMEDOWN, PQ0, 0),
    [3] = GPIO_KEY(KEY_CAMERA, PR4, 1), /*ZTE: added by tong.weili for camera key 20130403*/
};
static int ztepluto_wakeup_key(void)
{
	int wakeup_key;
	u64 status = readl(IO_ADDRESS(TEGRA_PMC_BASE) + PMC_WAKE_STATUS)
		| (u64)readl(IO_ADDRESS(TEGRA_PMC_BASE)
		+ PMC_WAKE2_STATUS) << 32;

	if (status & ((u64)1 << TEGRA_WAKE_GPIO_PS2))
		wakeup_key = KEY_POWER;
	else if (status & ((u64)1 << TEGRA_WAKE_GPIO_PR4))
		wakeup_key = KEY_CAMERA;
	else
		wakeup_key = KEY_RESERVED;

	return wakeup_key;
}

static struct gpio_keys_platform_data ztepluto_keys_pdata = {
	.buttons	= ztepluto_keys,
	.nbuttons	= ARRAY_SIZE(ztepluto_keys),
	.wakeup_key     = ztepluto_wakeup_key,   

};

static struct platform_device ztepluto_keys_device = {
	.name   = "gpio-keys",
	.id     = 0,
	.dev    = {
		.platform_data  = &ztepluto_keys_pdata,
	},
};

void __init ztepluto_keys_init(void)
{
	platform_device_register(&ztepluto_keys_device);
}

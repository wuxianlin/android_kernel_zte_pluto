/*
 * max98096.c -- MAX98095/6 ALSA SoC Audio driver
 *
 * Copyright 2012 Maxim Integrated Products
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define DEVICE_MAX98096

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/stddef.h>
#include <linux/printk.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/spi/spi.h>
#include <linux/spinlock.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <sound/max98096.h>
#include "max9809X.h"
#include "max9809X-dsp.h"

#include "max9809X.c"
#include "max9809X-dsp.c"

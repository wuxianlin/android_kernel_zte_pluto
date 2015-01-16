/*
 * inclue/linux/tmd2771x.h
 *
 * Copyright (C) 2010-2012 ZTE Corporation
 *
 * Author:
 *	Poyuan Lu <lu.poyuan@zte.com.cn>
 *	John Koshi - Surya Software
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
#ifndef TMD2771X_H
#define TMD2771X_H

struct als_config{
	unsigned char enabled;
#define ALS_INT_PERS_MASK 0x0F
	unsigned char int_pers; //shared register  INTERRUPT PERSISTENCE
	unsigned char adc_time; // 2.72ms/percount
#define ALS_GAIN_MASK 0x03
#define ALS_GAIN_1X 0x00
#define ALS_GAIN_8X 0x01
#define ALS_GAIN_16X 0x02
#define ALS_GAIN_120X 0x03
	unsigned char gain; //shared register CONFIG
	unsigned short threshold_hi;
	unsigned short threshold_lo;
	unsigned short saturation;
	unsigned short scale_factor;
	unsigned short scale_up;
	unsigned short scale_down;
};

struct prox_config{
	unsigned char enabled;
#define PROX_INT_PERS_MASK 0xF0
	unsigned char int_pers; //shared register INTERRUPT PERSISTENCE
	unsigned char adc_time; // 2.72ms/percount
	unsigned char pulse_count;
#define PROX_LED_DRIVE_MASK 0xC0
#define PROX_LED_DRIVE_100MA 0x00
#define PROX_LED_DRIVE_50MA 0x40
#define PROX_LED_DRIVE_25MA 0x80
#define PROX_LED_DRIVE_12P5MA 0xC0
	unsigned char led_drive; //shared register CONFIG
#define PROX_CHANNEL_MASK 0x30
#define PROX_CHANNEL_0 0x10
#define PROX_CHANNEL_1 0x20
#define PROX_CHANNEL_BOTH 0x30
	unsigned char channel; //shared register CONFIG
	unsigned short threshold_hi;
	unsigned short threshold_lo;
	unsigned short saturation;
	unsigned short thresh_offset;
	unsigned short thresh_gain_hi;
	unsigned short thresh_gain_lo;
};

struct wait_config{
	unsigned char wait_time; // 2.72ms/percount
#define WAIT_LONG_MASK 0x02
	unsigned char wlong; //enlarge 12 times
};

struct tmd2771x_platform_data{
	struct als_config *als_config;
	struct prox_config *prox_config;

	int (*power_control)(int enable);
	int (*irq_gpio_setup)(void);
};

#endif


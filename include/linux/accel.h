/*
 * drivers/misc/nct1008.c
 *
 * Driver for NCT1008, temperature monitoring device from ON Semiconductors
 *
 * Copyright (c) 2010, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#ifndef ACCEL_H
#define ACCEL_H

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/input-polldev.h>
#include <linux/platform_device.h>
#include <linux/sensors.h>
 
#define ACCEL_FS_2G                 (0)
#define ACCEL_FS_4G                 (1)
#define ACCEL_FS_8G                 (2)
#define ACCEL_FS_16G               (3)

 struct accel_platform_data {
	int adapt_nr;
	unsigned short *accel_list;
    
	unsigned int poll_inerval;	
	unsigned int g_range;
};

struct accel_driver_descr {
	char *name;
    
#define ID_CHECK 0
#define NORMAL_CHECK 1
	unsigned char check_type;

	unsigned char dev_id;
	unsigned char id_reg;
	unsigned long *axis_map;
	
	int (*enable) (void);
	int (*disable) (void);
	int (*get_axis_data) (int *axis_data);
	int (*update_odr) (unsigned int interval);	
	int (*update_fs) (unsigned int g_range);
	void (*update_countsperg) (int countspg);
};

extern int accel_i2c_write(unsigned int len, unsigned char *data);
extern int accel_i2c_read(unsigned char reg, unsigned int len, unsigned char *data);
extern int accel_i2c_write_register(unsigned char reg, unsigned char value);
extern int accel_i2c_read_register(unsigned char reg, unsigned char *value);

#ifdef CONFIG_ACCEL_SENSORS_KXTF9
extern struct accel_driver_descr *kxtf9_get_driver_descr(void);
#endif
#ifdef CONFIG_ACCEL_SENSORS_LIS3DH
extern struct accel_driver_descr *lis3dh_get_driver_descr(void);
#endif
#ifdef CONFIG_ACCEL_SENSORS_MMA845X
extern struct accel_driver_descr *mma845x_get_driver_descr(void);
#endif
#ifdef CONFIG_ACCEL_SENSORS_LSM330D_A
extern struct accel_driver_descr *lsm330da_get_driver_descr(void);
#endif
#endif

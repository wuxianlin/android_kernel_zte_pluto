/*
 * drivers/misc/accel/chip/lsm330d_acc.c
 *
 * Driver for ACCELLEMETER.
 *
 * Copyright (c) 2011, ZTE Corporation.
 *
 * history:
 *            created by lupoyuan10105246 (lu.poyuan@zte.com.cn) in 20120710
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
#include <linux/accel.h>

//register address macros
//control and status register
#define CTRL_REG1_A 0x20
#define CTRL_REG2_A 0x21
#define CTRL_REG3_A 0x22
#define CTRL_REG4_A 0x23
#define CTRL_REG5_A 0x24
#define CTRL_REG6_A 0x25
#define REFERENCE_A 0x26
#define STATUS_REG_A 0X27
//out register
#define OUT_X_L_A 0x28
#define OUT_X_H_A 0x29
#define OUT_Y_L_A 0x2A
#define OUT_Y_H_A 0x2B
#define OUT_Z_L_A 0x2C
#define OUT_Z_H_A 0x2D

//fifo and interrupt config register
#define FIFO_CTRL_REG 0x2E
#define FIFO_SRC_REG 0x2F

#define INT1_CFG_A 0x30
#define INT1_SOURCE_A 0x31
#define INT1_THS_A 0x32
#define INT1_DURATION_A 0x33
#define INT2_CFG_A 0x34
#define INT2_SOURCE_A 0x35
#define INT2_THS_A 0x36
#define INT2_DURATION_A 0x37

#define CLICK_CFG_A 0x38
#define CLICK_SRC_A 0x39
#define CLICK_THS_A 0x3A
#define TIME_LIMIT_A 0x3B
#define TIME_LATENCY_A 0x3C
#define TIME_WINDOW_A 0x3D
#define ACT_THS 0x3E
#define ACT_DUR 0x3F

/* controls the output data rate of the part */

#define ODR_MASK			0xF0

#define ODR_1		0x10  /* 1Hz output data rate */
#define ODR_10		0x20  /* 10Hz output data rate */
#define ODR_25		0x30  /* 25Hz output data rate */
#define ODR_50		0x40  /* 50Hz output data rate */
#define ODR_100		0x50  /* 100Hz output data rate */
#define ODR_200		0x60  /* 200Hz output data rate */
#define ODR_400		0x70  /* 400Hz output data rate */
#define ODR_1620	0x80  /* 1620Hz output data rate */
#define ODR_1344	0x90  /* 1344Hz/5376Hz output data rate */

/*
 * The following table lists the maximum appropriate poll interval for each
 * available output data rate.
 */
static const struct {
	unsigned int cutoff;
	unsigned char odr_mask;
} lsm330da_odr_table[] = {
	{ 3, ODR_1344 },
	{ 5, ODR_400 },
	{ 10, ODR_200 },
	{ 20, ODR_100 },
	{ 40, ODR_50  },
	{ 100, ODR_25  },
	{ 1000, ODR_10},
	{ 0, ODR_1},
};

/* Output g-range: +/-2g, 4g, or 8g */
#define FS_2G                 0x00
#define FS_4G                 0x10
#define FS_8G                 0x20
#define FS_16G               0x30
#define FS_MASK             0x30

#define LSM330D_A_AXIS_ENABLE 0x07
#define HIGH_RESOLUTION 0x80
#define I2C_AUTO_INCREMENT 0x80

static const struct {
	unsigned char accel_fs;
	unsigned char dev_fs;
} lsm330da_fs_table[] = {
	{ ACCEL_FS_2G, FS_2G },
	{ ACCEL_FS_4G, FS_4G },
	{ ACCEL_FS_8G, FS_8G },
};

static unsigned long axis_map = CONFIG_SENSORS_LSM330D_A_AXISMAP;

static int sensitivy_table[] = {
1024, // +/-2G
512, // +/-4G
256 // +/-8G
};

static int s_lsm330da_countsperg =1024;
static unsigned int s_lsm330da_fs = ACCEL_FS_2G;
static unsigned int s_lsm330da_poll_interval = 100;
    
static int  lsm330da_sensitivity_convert(int axis_data){
    return (axis_data * s_lsm330da_countsperg / sensitivy_table[s_lsm330da_fs]);
}

static unsigned char lsm330da_get_fs (void){
    int i;
    
    /* lookup the match full scale */
    for (i = 0; i < ARRAY_SIZE(lsm330da_fs_table); i++) {
    	if (s_lsm330da_fs == lsm330da_fs_table[i].accel_fs)
    		break;
    }
    
    /*defualt full scale*/
    if(i == ARRAY_SIZE(lsm330da_fs_table)){
        s_lsm330da_fs = ACCEL_FS_2G;
        return FS_2G;
    }    

    return lsm330da_fs_table[i].dev_fs;
}

static unsigned char lsm330da_get_odr(void)
{
    int i;

    /* Use the lowest ODR that can support the requested poll interval */
    for (i = 0; i < ARRAY_SIZE(lsm330da_odr_table); i++) {
    	if (s_lsm330da_poll_interval < lsm330da_odr_table[i].cutoff)
    		break;
    }

    return lsm330da_odr_table[i].odr_mask;
}

static int lsm330da_enable (void){
    int ret;
    unsigned char reg_ctrl1 = 0;
    
    /* ensure that PC1 is cleared before updating control registers */
    ret = accel_i2c_write_register(CTRL_REG1_A, 0x00);
    if(ret<0){
        printk("%s:write CTRL_REG1 failed\n", __func__);
        return -1;
    }
    
    /*set full scale and high resolution*/
    ret = accel_i2c_write_register(CTRL_REG4_A, HIGH_RESOLUTION | lsm330da_get_fs());
    if(ret<0){
        printk("%s:write CTRL_REG4 failed\n", __func__);
        return -1;
    }   
    
    /*set odr*/
    reg_ctrl1 |= lsm330da_get_odr();
    
    /*enable active mode*/
    reg_ctrl1 |= LSM330D_A_AXIS_ENABLE;

    ret = accel_i2c_write_register(CTRL_REG1_A, reg_ctrl1);
    if(ret<0){
        printk("%s:write CTRL_REG1 failed\n", __func__);
        return -1;
    }

    return 0;    
}

static int lsm330da_disable (void){
    int ret ;
    ret = accel_i2c_write_register(CTRL_REG1_A, 0x00);
    if(ret<0){
        printk("%s:write CTRL_REG1 failed\n", __func__);
        return -1;
    }    
    return 0;
}

static int lsm330da_get_axis_data (int *axis_data){
    int ret;
    s16 acc_data[3];

    ret = accel_i2c_read((I2C_AUTO_INCREMENT | OUT_X_L_A), 6, (unsigned char *)acc_data);
    if(ret < 0){
        printk("LIS3DH:read the data failed\n");
        return -1;
    }
  
    // big-little endian convert and get upper 12-bit data
    acc_data[0] = ((s16)le16_to_cpu(acc_data[0])) >> 4;
    acc_data[1] = ((s16)le16_to_cpu(acc_data[1])) >> 4;
    acc_data[2] = ((s16)le16_to_cpu(acc_data[2])) >> 4;
    
    //axis transform
    axis_convert(axis_map, acc_data);
   
    //sensitvy convert
    axis_data[0] = lsm330da_sensitivity_convert(acc_data[0]);
    axis_data[1] = lsm330da_sensitivity_convert(acc_data[1]);
    axis_data[2] = lsm330da_sensitivity_convert(acc_data[2]);

    return 0;

}

static int lsm330da_update_odr (unsigned int interval){
    unsigned char ctrl1_val;
    int ret;
    
    s_lsm330da_poll_interval = interval;

    //save the ctrl1 register
    ret = accel_i2c_read_register(CTRL_REG1_A, &ctrl1_val);
    if(ret<0){
        printk("%s:read CTRL_REG1 failed\n", __func__);
        return -1;
    }

    //power down the device    
    ret = accel_i2c_write_register(CTRL_REG1_A, 0x00);
    if(ret<0){
        printk("%s:write CTRL_REG1 failed 1\n", __func__);
        return -1;
    }
    
    //update the odr
    ctrl1_val = (ctrl1_val & ~(ODR_MASK)) | lsm330da_get_odr();
    
    //restore the ctrl1 register
    ret = accel_i2c_write_register(CTRL_REG1_A, ctrl1_val);
    if(ret<0){
        printk("%s:write CTRL_REG1 failed\n", __func__);
        return -1;
    }

    return 0;
}

static int lsm330da_update_fs (unsigned int g_range){
    unsigned char ctrl1_val;
    int ret;
    
    s_lsm330da_fs = g_range;

    //save the ctrl1 register
    ret = accel_i2c_read_register(CTRL_REG1_A, &ctrl1_val);
    if(ret<0){
        printk("%s:read CTRL_REG1 failed\n", __func__);
        return -1;
    }

    //power down the device
    ret = accel_i2c_write_register(CTRL_REG1_A, 0x00);
    if(ret<0){
        printk("%s:write CTRL_REG1 failed\n", __func__);
        return -1;
    }

    //update the full scale
    ret = accel_i2c_write_register(CTRL_REG4_A, HIGH_RESOLUTION | lsm330da_get_fs());
    if(ret<0){
        printk("%s:write CTRL_REG4 failed\n", __func__);
        return -1;
    }

    //restore the ctrl1 register
    ret = accel_i2c_write_register(CTRL_REG1_A, ctrl1_val);
    if(ret<0){
        printk("%s:write CTRL_REG1 failed\n", __func__);
        return -1;
    }

    return 0;
}
    
static void lsm330da_update_countsperg (int countspg){
    s_lsm330da_countsperg = countspg;
}

static struct accel_driver_descr lsm330da_descr = {
	/*.name              = */ "lsm330d_A",
	/*.check_type     = */ ID_CHECK,
	/*.dev_id            = */ 0x33,
	/*.id_reg            = */ 0x0F,
	/*.axis_map       = */ &axis_map,
	
	/*.enable           = */  lsm330da_enable,
	/*.disable           = */ lsm330da_disable,
	/*.get_axis_data = */ lsm330da_get_axis_data,
	/*.update_odr     = */ lsm330da_update_odr,
	/*.update_fs       = */ lsm330da_update_fs,
	/*.update_countsperg  = */  lsm330da_update_countsperg,
};

struct accel_driver_descr *lsm330da_get_driver_descr(void)
{
	return &lsm330da_descr;
}
//EXPORT_SYMBOL(lsm330da_get_driver_descr);



/*
 * drivers/misc/accel/chip/lis3dh.c
 *
 * Driver for ACCELLEMETER.
 *
 * Copyright (c) 2011, ZTE Corporation.
 *
 * history:
 *            created by lupoyuan10105246 (lu.poyuan@zte.com.cn) in 20111202
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
#define STATUS_REG_AUX     0x07
#define OUT_ADC1_L             0x08
#define OUT_ADC1_H             0x09
#define OUT_ADC2_L             0x0A
#define OUT_ADC2_H             0x0B
#define OUT_ADC3_L             0x0C
#define OUT_ADC3_H             0x0D
#define INT_COUNTER_REG   0x0E
#define WHO_AM_I                0x0F
#define TEMP_CFG_REG         0x1F
#define CTRL_REG1                0x20
#define CTRL_REG2                0x21
#define CTRL_REG3                0x22
#define CTRL_REG4                0x23
#define CTRL_REG5                0x24
#define CTRL_REG6                0x25
#define REFERENCE                0x26
#define STATUS_REG2           0x27
#define OUT_X_L                    0x28
#define OUT_X_H                   0x29
#define OUT_Y_L                    0x2A
#define OUT_Y_H                    0x2B
#define OUT_Z_L                    0x2C
#define OUT_Z_H                    0x2D
#define FIFO_CTRL_REG         0x2E
#define FIFO_SRC_REG           0x2F
#define INT1_CFG                   0x30
#define INT1_SOURCE             0x31
#define INT2_THS                   0x32
#define INT2_DURATION         0x33
#define CLICK_CFG                  0x38
#define CLICK_SRC                  0x39
#define CLICK_THS                  0x3A
#define TIME_LIMIT                0x3B
#define TIME_LATENCY           0x3C
#define TIME_WINDOW           0x3D

#define I2C_AUTO_INCREMENT 0x80
#define BLOCK_DATA_UPDATE 0x80

#define LIS3DH_AXIS_ENABLE 0x07
#define HIGH_RESOLUTION      0x08

/* controls the output data rate of the part */

#define ODR_MASK			0xF0

#define ODR_1		0x10  /* 1Hz output data rate */
#define ODR_10		0x20  /* 10Hz output data rate */
#define ODR_25		0x30  /* 25Hz output data rate */
#define ODR_50		0x40  /* 50Hz output data rate */
#define ODR_100		0x50  /* 100Hz output data rate */
#define ODR_200		0x60  /* 200Hz output data rate */
#define ODR_400		0x70  /* 400Hz output data rate */
#define ODR_1250	0x90  /* 1250Hz output data rate */

/*
 * The following table lists the maximum appropriate poll interval for each
 * available output data rate.
 */
static const struct {
	unsigned int cutoff;
	unsigned char odr_mask;
} lis3dh_odr_table[] = {
	{ 3, ODR_1250 },
	{ 5, ODR_400 },
	{ 10, ODR_200 },
	{ 20, ODR_100 },
	{ 40, ODR_50  },
	{ 100, ODR_25  },
	{ 1000, ODR_10},
	{ 0, ODR_1},
};

/* Output g-range: +/-2g, 4g, or 8g */
#define LIS3DH_FS_2G                 0x00
#define LIS3DH_FS_4G                 0x10
#define LIS3DH_FS_8G                 0x20
#define LIS3DH_FS_16G               0x30

#define LIS3DH_FS_MASK             0x30

static const struct {
	unsigned char accel_fs;
	unsigned char dev_fs;
} lis3dh_fs_table[] = {
	{ ACCEL_FS_2G, LIS3DH_FS_2G },
	{ ACCEL_FS_4G, LIS3DH_FS_4G },
	{ ACCEL_FS_8G, LIS3DH_FS_8G },
};

static unsigned long axis_map = CONFIG_SENSORS_LIS3DH_AXISMAP;

static int sensitivy_table[] = {
1024, // +/-2G
512, // +/-4G
256 // +/-8G
};

static int s_lis3dh_countsperg =1024;
static unsigned int s_lis3dh_fs = ACCEL_FS_2G;
static unsigned int s_lis3dh_poll_interval = 100;
    
static int  lis3dh_sensitivity_convert(int axis_data){
    return (axis_data * s_lis3dh_countsperg / sensitivy_table[s_lis3dh_fs]);
}

static unsigned char lis3dh_get_fs (void){
    int i;
    
    /* lookup the match full scale */
    for (i = 0; i < ARRAY_SIZE(lis3dh_fs_table); i++) {
    	if (s_lis3dh_fs == lis3dh_fs_table[i].accel_fs)
    		break;
    }
    
    /*defualt full scale*/
    if(i == ARRAY_SIZE(lis3dh_fs_table)){
        s_lis3dh_fs = ACCEL_FS_2G;
        return LIS3DH_FS_2G;
    }    

    return lis3dh_fs_table[i].dev_fs;
}

static unsigned char lis3dh_get_odr(void)
{
    int i;

    /* Use the lowest ODR that can support the requested poll interval */
    for (i = 0; i < ARRAY_SIZE(lis3dh_odr_table); i++) {
    	if (s_lis3dh_poll_interval < lis3dh_odr_table[i].cutoff)
    		break;
    }

    return lis3dh_odr_table[i].odr_mask;
}

static int lis3dh_enable (void){
    int ret;
    unsigned char reg_ctrl1 = 0;
    
    /* ensure that PC1 is cleared before updating control registers */
    ret = accel_i2c_write_register(CTRL_REG1, 0x00);
    if(ret<0){
        printk("%s:write CTRL_REG1 failed\n", __func__);
        return -1;
    }
    
    /*set full scale and high resolution*/
    ret = accel_i2c_write_register(CTRL_REG4, HIGH_RESOLUTION |BLOCK_DATA_UPDATE | lis3dh_get_fs());
    if(ret<0){
        printk("%s:write CTRL_REG4 failed\n", __func__);
        return -1;
    }   
    
    /*set odr*/
    reg_ctrl1 |= lis3dh_get_odr();
    
    /*enable active mode*/
    reg_ctrl1 |= LIS3DH_AXIS_ENABLE;

    ret = accel_i2c_write_register(CTRL_REG1, reg_ctrl1);
    if(ret<0){
        printk("%s:write CTRL_REG1 failed\n", __func__);
        return -1;
    }

    return 0;    
}

static int lis3dh_disable (void){
    int ret ;
    ret = accel_i2c_write_register(CTRL_REG1, 0x00);
    if(ret<0){
        printk("%s:write CTRL_REG1 failed\n", __func__);
        return -1;
    }    
    return 0;
}

static int lis3dh_get_axis_data (int *axis_data){
    int ret;
//    unsigned char reg_val;

    s16 acc_data[3];
/*    
    ret = accel_i2c_read_register(STATUS_REG2, &reg_val);
    if(ret < 0){
        printk("LIS3DH:read the data status failed\n");
        return -1;
    }

    if (!(reg_val & 0x08)){
        printk("LIS3DH:data not ready");
        return -1;
    }
*/    
    ret = accel_i2c_read((I2C_AUTO_INCREMENT | OUT_X_L), 6, (unsigned char *)acc_data);
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
    axis_data[0] = lis3dh_sensitivity_convert(acc_data[0]);
    axis_data[1] = lis3dh_sensitivity_convert(acc_data[1]);
    axis_data[2] = lis3dh_sensitivity_convert(acc_data[2]);

    return 0;

}

static int lis3dh_update_odr (unsigned int interval){
    unsigned char ctrl1_val;
    int ret;
    
    s_lis3dh_poll_interval = interval;

    //save the ctrl1 register
    ret = accel_i2c_read_register(CTRL_REG1, &ctrl1_val);
    if(ret<0){
        printk("%s:read CTRL_REG1 failed\n", __func__);
        return -1;
    }

    //power down the device    
    ret = accel_i2c_write_register(CTRL_REG1, 0x00);
    if(ret<0){
        printk("%s:write CTRL_REG1 failed\n", __func__);
        return -1;
    }
    
    //update the odr
    ctrl1_val = (ctrl1_val & ~(ODR_MASK)) | lis3dh_get_odr();
    
    //restore the ctrl1 register
    ret = accel_i2c_write_register(CTRL_REG1, ctrl1_val);
    if(ret<0){
        printk("%s:write CTRL_REG1 failed\n", __func__);
        return -1;
    }

    return 0;
}

static int lis3dh_update_fs (unsigned int g_range){
    unsigned char ctrl1_val;
    int ret;
    
    s_lis3dh_fs = g_range;

    //save the ctrl1 register
    ret = accel_i2c_read_register(CTRL_REG1, &ctrl1_val);
    if(ret<0){
        printk("%s:read CTRL_REG1 failed\n", __func__);
        return -1;
    }

    //power down the device
    ret = accel_i2c_write_register(CTRL_REG1, 0x00);
    if(ret<0){
        printk("%s:write CTRL_REG1 failed\n", __func__);
        return -1;
    }

    //update the full scale
    ret = accel_i2c_write_register(CTRL_REG4, HIGH_RESOLUTION |BLOCK_DATA_UPDATE  | lis3dh_get_fs());
    if(ret<0){
        printk("%s:write CTRL_REG4 failed\n", __func__);
        return -1;
    }

    //restore the ctrl1 register
    ret = accel_i2c_write_register(CTRL_REG1, ctrl1_val);
    if(ret<0){
        printk("%s:write CTRL_REG1 failed\n", __func__);
        return -1;
    }

    return 0;
}
    
static void lis3dh_update_countsperg (int countspg){
    s_lis3dh_countsperg = countspg;
}

static struct accel_driver_descr lis3dh_descr = {
	/*.name              = */ "lis3dh",
	/*.check_type     = */ ID_CHECK,
	/*.dev_id            = */ 0x33,
	/*.id_reg            = */ 0x0F,
	/*.axis_map       = */ &axis_map,
	
	/*.enable           = */  lis3dh_enable,
	/*.disable           = */ lis3dh_disable,
	/*.get_axis_data = */ lis3dh_get_axis_data,
	/*.update_odr     = */ lis3dh_update_odr,
	/*.update_fs       = */ lis3dh_update_fs,
	/*.update_countsperg  = */  lis3dh_update_countsperg,
};

struct accel_driver_descr *lis3dh_get_driver_descr(void)
{
	return &lis3dh_descr;
}
EXPORT_SYMBOL(lis3dh_get_driver_descr);



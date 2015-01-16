/*
 * drivers/misc/accel/chip/kxtf9.c
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

//register map table
#define KXTF9_XOUT_HPF_L                (0x00) /* 0000 0000 */
#define KXTF9_XOUT_HPF_H                (0x01) /* 0000 0001 */
#define KXTF9_YOUT_HPF_L                (0x02) /* 0000 0010 */
#define KXTF9_YOUT_HPF_H                (0x03) /* 0000 0011 */
#define KXTF9_ZOUT_HPF_L                (0x04) /* 0001 0100 */
#define KXTF9_ZOUT_HPF_H                (0x05) /* 0001 0101 */
#define KXTF9_XOUT_L                         (0x06) /* 0000 0110 */
#define KXTF9_XOUT_H                         (0x07) /* 0000 0111 */
#define KXTF9_YOUT_L                         (0x08) /* 0000 1000 */
#define KXTF9_YOUT_H                         (0x09) /* 0000 1001 */
#define KXTF9_ZOUT_L                         (0x0A) /* 0001 1010 */
#define KXTF9_ZOUT_H                         (0x0B) /* 0001 1011 */
#define KXTF9_ST_RESP                       (0x0C) /* 0000 1100 */
#define KXTF9_WHO_AM_I                    (0x0F) /* 0000 1111 */
#define KXTF9_TILT_POS_CUR              (0x10) /* 0001 0000 */
#define KXTF9_TILT_POS_PRE              (0x11) /* 0001 0001 */
#define KXTF9_INT_SRC_REG1              (0x15) /* 0001 0101 */
#define KXTF9_INT_SRC_REG2              (0x16) /* 0001 0110 */
#define KXTF9_STATUS_REG                 (0x18) /* 0001 1000 */
#define KXTF9_INT_REL                         (0x1A) /* 0001 1010 */
#define KXTF9_CTRL_REG1                    (0x1B) /* 0001 1011 */
#define KXTF9_CTRL_REG2                    (0x1C) /* 0001 1100 */
#define KXTF9_CTRL_REG3                    (0x1D) /* 0001 1101 */
#define KXTF9_INT_CTRL_REG1            (0x1E) /* 0001 1110 */
#define KXTF9_INT_CTRL_REG2            (0x1F) /* 0001 1111 */
#define KXTF9_INT_CTRL_REG3            (0x20) /* 0010 0000 */
#define KXTF9_DATA_CTRL_REG           (0x21) /* 0010 0001 */
#define KXTF9_TILT_TIMER                  (0x28) /* 0010 1000 */
#define KXTF9_WUF_TIMER                  (0x29) /* 0010 1001 */
#define KXTF9_TDT_TIMER                   (0x2B) /* 0010 1011 */
#define KXTF9_TDT_H_THRESH            (0x2C) /* 0010 1100 */
#define KXTF9_TDT_L_THRESH             (0x2D) /* 0010 1101 */
#define KXTF9_TDT_TAP_TIMER           (0x2E) /* 0010 1110 */
#define KXTF9_TDT_TOTAL_TIMER           (0x2F) /* 0010 1111 */
#define KXTF9_TDT_LATENCY_TIMER       (0x30) /* 0011 0000 */
#define KXTF9_TDT_WINDOW_TIMER       (0x31) /* 0011 0001 */
#define KXTF9_WUF_THRESH                    (0x5A) /* 0101 1010 */
#define KXTF9_TILT_ANGLE                      (0x5C) /* 0101 1100 */
#define KXTF9_HYST_SET                          (0x5F) /* 0101 1111 */

/* CTRL_REG1: set resolution, g-range*/
#define PC1_ON			(1 << 7)

/* Output resolution: 8-bit valid or 12-bit valid */
#define RES_8BIT		0
#define RES_12BIT		(1 << 6)

/* Output g-range: +/-2g, 4g, or 8g */
#define KXTF_G_2G		0
#define KXTF_G_4G		(1 << 3)
#define KXTF_G_8G		(1 << 4)

static const struct {
	unsigned char accel_fs;
	unsigned char kxtf9_fs;
} kxtf9_fs_table[] = {
	{ ACCEL_FS_2G, KXTF_G_2G },
	{ ACCEL_FS_4G, KXTF_G_4G },
	{ ACCEL_FS_8G, KXTF_G_8G },
};

/* DATA_CTRL_REG: controls the output data rate of the part */
#define ODR12_5F		0
#define ODR25F			1
#define ODR50F			2
#define ODR100F			3
#define ODR200F			4
#define ODR400F			5
#define ODR800F			6

/*
 * The following table lists the maximum appropriate poll interval for each
 * available output data rate.
 */
static const struct {
	unsigned int cutoff;
	unsigned char odr_mask;
} kxtf9_odr_table[] = {
	{ 3, ODR800F },
	{ 5, ODR400F },
	{ 10, ODR200F },
	{ 20, ODR100F },
	{ 40, ODR50F  },
	{ 80, ODR25F  },
	{ 0, ODR12_5F},
};

static unsigned long axis_map = CONFIG_SENSORS_KXTF9_AXISMAP;

static int sensitivy_table[] = {
1024, // +/-2G
512, // +/-4G
256 // +/-8G
};

static int s_countsperg =1024;
static unsigned int s_kxtf9_fs = ACCEL_FS_2G;
static unsigned int s_kxtf9_poll_interval = 100;
    
static int  kxtf9_sensitivity_convert(int axis_data){
    return (axis_data * s_countsperg / sensitivy_table[s_kxtf9_fs]);
}

static unsigned char kxtf9_get_fs (void){
    int i;
    
    /* lookup the match full scale */
    for (i = 0; i < ARRAY_SIZE(kxtf9_fs_table); i++) {
    	if (s_kxtf9_fs == kxtf9_fs_table[i].accel_fs)
    		break;
    }

    /*defualt full scale*/
    if(i == ARRAY_SIZE(kxtf9_fs_table)){
        s_kxtf9_fs = ACCEL_FS_2G;
        return KXTF_G_2G;
    }

    return kxtf9_fs_table[i].kxtf9_fs;
}

static unsigned char kxtf9_get_odr(void)
{
    int i;

    /* Use the lowest ODR that can support the requested poll interval */
    for (i = 0; i < ARRAY_SIZE(kxtf9_odr_table); i++) {
    	if (s_kxtf9_poll_interval < kxtf9_odr_table[i].cutoff)
    		break;
    }
   
    return kxtf9_odr_table[i].odr_mask;
}

static int kxtf9_enable (void){
    int ret;
    unsigned char reg_ctrl1 = 0;
    
    /* ensure that PC1 is cleared before updating control registers */
    ret = accel_i2c_write_register(KXTF9_CTRL_REG1, 0x00);
    if(ret<0){
        printk("%s:write CTRL_REG1 failed\n", __func__);
        return -1;
    }
    
    /*enable 12-bit resolution*/
    reg_ctrl1 |= RES_12BIT;

    /*set full scale*/
    reg_ctrl1 |= kxtf9_get_fs();
    
    /*set odr*/
    ret = accel_i2c_write_register(KXTF9_DATA_CTRL_REG, kxtf9_get_odr());
    if(ret<0){
        printk("%s:write DATA_CTRL_REG failed\n", __func__);
        return -1;
    }
    
    /*interrupt pin configure*/
    ret = accel_i2c_write_register(KXTF9_INT_CTRL_REG1, 0x28);
    if(ret<0){
        printk("%s:write INT_CTRL_REG1 failed\n", __func__);
        return -1;
    }

    ret = accel_i2c_write_register(KXTF9_INT_CTRL_REG2, 0x00);
    if(ret<0){
        printk("%s:write INT_CTRL_REG1 failed\n", __func__);
        return -1;
    }

    ret = accel_i2c_write_register(KXTF9_INT_CTRL_REG3, 0x00);
    if(ret<0){
        printk("%s:write INT_CTRL_REG1 failed\n", __func__);
        return -1;
    }

    /*enable operating mode*/
    reg_ctrl1 |= PC1_ON;

    ret = accel_i2c_write_register(KXTF9_CTRL_REG1, reg_ctrl1);
    if(ret<0){
        printk("%s:write CTRL_REG1 failed\n", __func__);
        return -1;
    }

    return 0;    
}

static int kxtf9_disable (void){
    int ret ;
    
    ret = accel_i2c_write_register(KXTF9_CTRL_REG1, 0x00);
    if(ret<0){
        printk("%s:write CTRL_REG1 failed\n", __func__);
        return -1;
    }
    
    return 0;
}

static int kxtf9_get_axis_data (int *axis_data){
    int ret;
//    unsigned char reg_val;

    s16 accel_data[3];
/*    
    ret = accel_i2c_read_register(KXTF9_INT_SRC_REG2, &reg_val);
    if(ret < 0){
        printk("KXTF9:read the data status failed\n");
        return -1;
    }

    if (!(reg_val & 0x10)){
        printk("KXTF9:data not ready");
        return -1;
    }
*/    
    ret = accel_i2c_read(KXTF9_XOUT_L, 6, (unsigned char *)accel_data);
    if(ret < 0){
        printk("KXTF9:read the data failed\n");
        return -1;
    }
    
    //get valid bits for out register  
    accel_data[0] = ((s16)le16_to_cpu(accel_data[0])) >> 4;
    accel_data[1] = ((s16)le16_to_cpu(accel_data[1])) >> 4;
    accel_data[2] = ((s16)le16_to_cpu(accel_data[2])) >> 4;
    
    //axis convert
    axis_convert(axis_map, accel_data);
    
    //sensitvy convert
    axis_data[0] = kxtf9_sensitivity_convert(accel_data[0]);
    axis_data[1] = kxtf9_sensitivity_convert(accel_data[1]);
    axis_data[2] = kxtf9_sensitivity_convert(accel_data[2]);
    
    return 0;

}

static int kxtf9_update_odr (unsigned int interval){
    int ret;
    unsigned char ctr1_val;
    
    s_kxtf9_poll_interval = interval;

    //save the ctrl1 register
    ret = accel_i2c_read_register(KXTF9_CTRL_REG1, &ctr1_val);
    if(ret<0){
        printk("%s:write CTRL_REG1 failed\n", __func__);
        return -1;
    }
    
    /* ensure that PC1 is cleared before updating control registers */
    ret = accel_i2c_write_register(KXTF9_CTRL_REG1, 0x00);
    if(ret<0){
        printk("%s:write CTRL_REG1 failed\n", __func__);
        return -1;
    }

    //update the ODR
    ret = accel_i2c_write_register(KXTF9_DATA_CTRL_REG, kxtf9_get_odr());
    if(ret<0){
        printk("%s:write DATA_CTRL_REG failed\n", __func__);
        return -1;
    }    

    //restore the ctrl1 register
    ret = accel_i2c_write_register(KXTF9_CTRL_REG1, ctr1_val);
    if(ret<0){
        printk("%s:write CTRL_REG1 failed\n", __func__);
        return -1;
    }

    return 0;     
}

static int kxtf9_update_fs (unsigned int g_range){
    int ret;
    unsigned char ctr1_val;
    
    s_kxtf9_fs = g_range;

    //save the ctrl1 register
    ret = accel_i2c_read_register(KXTF9_CTRL_REG1, &ctr1_val);
    if(ret<0){
        printk("%s:write CTRL_REG1 failed\n", __func__);
        return -1;
    }
    
    /* ensure that PC1 is cleared before updating control registers */
    ret = accel_i2c_write_register(KXTF9_CTRL_REG1, 0x00);
    if(ret<0){
        printk("%s:write CTRL_REG1 failed\n", __func__);
        return -1;
    }

    //update the fullscale    
    ctr1_val = (ctr1_val & ~0x07) | kxtf9_get_fs();

    //restore the ctrl1 register
    ret = accel_i2c_write_register(KXTF9_CTRL_REG1, ctr1_val);
    if(ret<0){
        printk("%s:write CTRL_REG1 failed\n", __func__);
        return -1;
    }

    return 0;   
}
    
static void kxtf9_update_countsperg (int countspg){
    s_countsperg = countspg;
}

static struct accel_driver_descr kxtf9_descr = {
	/*.name              = */ "kxtf9",
	/*.check_type     = */ ID_CHECK,
	/*.dev_id            = */ 0x05,
	/*.id_reg            = */ 0x0F,
	/*.axis_map       = */ &axis_map,
	
	/*.enable           = */  kxtf9_enable,
	/*.disable           = */ kxtf9_disable,
	/*.get_axis_data = */ kxtf9_get_axis_data,
	/*.update_odr     = */ kxtf9_update_odr,
	/*.update_fs       = */ kxtf9_update_fs,
	/*.update_countsperg  = */  kxtf9_update_countsperg,
};

struct accel_driver_descr *kxtf9_get_driver_descr(void)
{
	return &kxtf9_descr;
}
EXPORT_SYMBOL(kxtf9_get_driver_descr);

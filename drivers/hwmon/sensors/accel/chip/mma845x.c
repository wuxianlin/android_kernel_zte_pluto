/*
 * drivers/misc/accel/chip/mma845x.c
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
enum {
	MMA8452_STATUS = 0x00,
	MMA8452_OUT_X_MSB,
	MMA8452_OUT_X_LSB,
	MMA8452_OUT_Y_MSB,
	MMA8452_OUT_Y_LSB,
	MMA8452_OUT_Z_MSB,
	MMA8452_OUT_Z_LSB,
	
	MMA8452_SYSMOD = 0x0B,
	MMA8452_INT_SOURCE,
	MMA8452_WHO_AM_I,
	MMA8452_XYZ_DATA_CFG,
	MMA8452_HP_FILTER_CUTOFF,
	
	MMA8452_PL_STATUS,
	MMA8452_PL_CFG,
	MMA8452_PL_COUNT,
	MMA8452_PL_BF_ZCOMP,
	MMA8452_PL_P_L_THS_REG,
	
	MMA8452_FF_MT_CFG,
	MMA8452_FF_MT_SRC,
	MMA8452_FF_MT_THS,
	MMA8452_FF_MT_COUNT,

	MMA8452_TRANSIENT_CFG = 0x1D,
	MMA8452_TRANSIENT_SRC,
	MMA8452_TRANSIENT_THS,
	MMA8452_TRANSIENT_COUNT,
	
	MMA8452_PULSE_CFG,
	MMA8452_PULSE_SRC,
	MMA8452_PULSE_THSX,
	MMA8452_PULSE_THSY,
	MMA8452_PULSE_THSZ,
	MMA8452_PULSE_TMLT,
	MMA8452_PULSE_LTCY,
	MMA8452_PULSE_WIND,
	
	MMA8452_ASLP_COUNT,
	MMA8452_CTRL_REG1,
	MMA8452_CTRL_REG2,
	MMA8452_CTRL_REG3,
	MMA8452_CTRL_REG4,
	MMA8452_CTRL_REG5,
	
	MMA8452_OFF_X,
	MMA8452_OFF_Y,
	MMA8452_OFF_Z,
	
	MMA8452_REG_END,
};

/* ctrl1 register bit map for normal configuration*/
#define MMA8452_LNOISE_NORMAL            0x00
#define MMA8452_LNOISE_REDUCED          0x04

#define MMA8452_READ_NORMAL               0x00
#define MMA8452_READ_FAST                    0x02

#define MMA8452_MODE_STANDBY            0x00
#define MMA8452_MODE_ACTIVE                0x01

/* controls the output data rate of the part */
/*ctrl1 regitster bit map for ODR*/
#define ODR_800        0x00
#define ODR_400        0x08
#define ODR_200        0x10
#define ODR_100        0x18
#define ODR_50          0x20
#define ODR_12P5      0x28
#define ODR_6P25      0x30
#define ODR_1P56      0x38

/*
 * The following table lists the maximum appropriate poll interval for each
 * available output data rate.
 */
static const struct {
	unsigned int cutoff;
	unsigned char odr_mask;
} mma845x_odr_table[] = {
	{ 3, ODR_800 },
	{ 5, ODR_400 },
	{ 10, ODR_200 },
	{ 20, ODR_100 },
	{ 80, ODR_50  },
	{ 160, ODR_12P5  },
	{ 640, ODR_6P25},
	{ 0, ODR_1P56},
};

/* Output g-range: +/-2g, 4g, or 8g */
/*XYZ_DATA_CFG register bit map for full scale*/
#define MMA8452_FS_2G                 0x00
#define MMA8452_FS_4G                 0x01
#define MMA8452_FS_8G                 0x02

static const struct {
	unsigned char accel_fs;
	unsigned char dev_fs;
} mma845x_fs_table[] = {
	{ ACCEL_FS_2G, MMA8452_FS_2G },
	{ ACCEL_FS_4G, MMA8452_FS_4G },
	{ ACCEL_FS_8G, MMA8452_FS_8G },
};

static unsigned long axis_map = CONFIG_SENSORS_MMA845X_AXISMAP;

static int sensitivy_table[] = {
1024, // +/-2G
512, // +/-4G
256 // +/-8G
};

static int s_mma845x_countsperg =1024;
static unsigned int s_mma845x_fs = ACCEL_FS_2G;
static unsigned int s_mma845x_poll_interval = 100;
    
static int  mma845x_sensitivity_convert(int axis_data){
    return (axis_data * s_mma845x_countsperg / sensitivy_table[s_mma845x_fs]);
}

static unsigned char mma845x_get_fs (void){
    int i;
    
    /* lookup the match full scale */
    for (i = 0; i < ARRAY_SIZE(mma845x_fs_table); i++) {
    	if (s_mma845x_fs == mma845x_fs_table[i].accel_fs)
    		break;
    }

    /*defualt full scale*/
    if(i == ARRAY_SIZE(mma845x_fs_table)){
        s_mma845x_fs = ACCEL_FS_2G;
        return MMA8452_FS_2G;
    }

    return mma845x_fs_table[i].dev_fs;
}

static unsigned char mma845x_get_odr(void)
{
    int i;

    /* Use the lowest ODR that can support the requested poll interval */
    for (i = 0; i < ARRAY_SIZE(mma845x_odr_table); i++) {
    	if (s_mma845x_poll_interval < mma845x_odr_table[i].cutoff)
    		break;
    }
    
    return mma845x_odr_table[i].odr_mask;
}

static int mma845x_enable (void){
    int ret;
    unsigned char reg_ctrl1 = 0;
    
    /* ensure that PC1 is cleared before updating control registers */
    ret = accel_i2c_write_register(MMA8452_CTRL_REG1, 0x00);
    if(ret<0){
        printk("%s:write CTRL_REG1 failed\n", __func__);
        return -1;
    }
    
    /*set full scale*/
    ret = accel_i2c_write_register(MMA8452_XYZ_DATA_CFG, mma845x_get_fs());
    if(ret<0){
        printk("%s:write XYZ_DATA_CFG failed\n", __func__);
        return -1;
    }
    
    reg_ctrl1 = MMA8452_LNOISE_NORMAL | MMA8452_READ_NORMAL;
    
    /*set odr*/
    reg_ctrl1 |= mma845x_get_odr();
    
    /*enable active mode*/
    reg_ctrl1 |= MMA8452_MODE_ACTIVE;

    ret = accel_i2c_write_register(MMA8452_CTRL_REG1, reg_ctrl1);
    if(ret<0){
        printk("%s:write CTRL_REG1 failed\n", __func__);
        return -1;
    }

    return 0;    
}

static int mma845x_disable (void){
    int ret ;
    ret = accel_i2c_write_register(MMA8452_CTRL_REG1, MMA8452_MODE_STANDBY);
    if(ret<0){
        printk("%s:write CTRL_REG1 failed\n", __func__);
        return -1;
    }
    
    return 0;
}

static int mma845x_get_axis_data (int *axis_data){
    int ret;
//    unsigned char reg_val;

    s16 acc_data[3];
/*    
    ret = accel_i2c_read_register(MMA8452_STATUS, &reg_val);
    if(ret < 0){
        printk("MMA845X:read the data status failed\n");
        return -1;
    }

    if (!(reg_val & 0x08)){
        printk("MMA845X:data not ready");
        return -1;
    }
*/    
    ret = accel_i2c_read(MMA8452_OUT_X_MSB, 6, (unsigned char *)acc_data);
    if(ret < 0){
        printk("MMA845X:read the data failed\n");
        return -1;
    }
    
    // big-little endian convert
    acc_data[0] = ((s16)be16_to_cpu(acc_data[0])) >> 4;
    acc_data[1] = ((s16)be16_to_cpu(acc_data[1])) >> 4;
    acc_data[2] = ((s16)be16_to_cpu(acc_data[2])) >> 4;
    
    //axis transform
    axis_convert(axis_map, acc_data);

    //sensitvy convert
    axis_data[0] = mma845x_sensitivity_convert(acc_data[0]);
    axis_data[1] = mma845x_sensitivity_convert(acc_data[1]);
    axis_data[2] = mma845x_sensitivity_convert(acc_data[2]);
    
    printk("data x=%d, y=%d, z=%d\n", axis_data[0], axis_data[1], axis_data[2]);
    return 0;

}

static int mma845x_update_odr (unsigned int interval){
    unsigned char ctrl1_val;
    int ret;
    
    s_mma845x_poll_interval = interval;

    //save the ctrl1 register
    ret = accel_i2c_read_register(MMA8452_CTRL_REG1, &ctrl1_val);
    if(ret<0){
        printk("%s:read CTRL_REG1 failed\n", __func__);
        return -1;
    }

    //power down the device    
    ret = accel_i2c_write_register(MMA8452_CTRL_REG1, 0x00);
    if(ret<0){
        printk("%s:write CTRL_REG1 failed\n", __func__);
        return -1;
    }
    
    //update the odr
    ctrl1_val = (ctrl1_val & ~0x38) | mma845x_get_odr();
    
    //restore the ctrl1 register
    ret = accel_i2c_write_register(MMA8452_CTRL_REG1, ctrl1_val);
    if(ret<0){
        printk("%s:write CTRL_REG1 failed\n", __func__);
        return -1;
    }

    return 0;
}

static int mma845x_update_fs (unsigned int g_range){
    unsigned char ctrl1_val;
    int ret;
    
    s_mma845x_fs = g_range;

    //save the ctrl1 register
    ret = accel_i2c_read_register(MMA8452_CTRL_REG1, &ctrl1_val);
    if(ret<0){
        printk("%s:read CTRL_REG1 failed\n", __func__);
        return -1;
    }

    //power down the device
    ret = accel_i2c_write_register(MMA8452_CTRL_REG1, 0x00);
    if(ret<0){
        printk("%s:write CTRL_REG1 failed\n", __func__);
        return -1;
    }

    //update the full scale
    ret = accel_i2c_write_register(MMA8452_XYZ_DATA_CFG, mma845x_get_fs());
    if(ret<0){
        printk("%s:write XYZ_DATA_CFG failed\n", __func__);
        return -1;
    }

    //restore the ctrl1 register
    ret = accel_i2c_write_register(MMA8452_CTRL_REG1, ctrl1_val);
    if(ret<0){
        printk("%s:write CTRL_REG1 failed\n", __func__);
        return -1;
    }

    return 0;
}
    
static void mma845x_update_countsperg (int countspg){
    s_mma845x_countsperg = countspg;
}

static struct accel_driver_descr mma845x_descr = {
	/*.name              = */ "mma845x",
	/*.check_type     = */ ID_CHECK,
	/*.dev_id            = */ 0x2A,
	/*.id_reg            = */ 0x0D,
	/*.axis_map       = */ &axis_map,
	
	/*.enable           = */  mma845x_enable,
	/*.disable           = */ mma845x_disable,
	/*.get_axis_data = */ mma845x_get_axis_data,
	/*.update_odr     = */ mma845x_update_odr,
	/*.update_fs       = */ mma845x_update_fs,
	/*.update_countsperg  = */  mma845x_update_countsperg,
};

struct accel_driver_descr *mma845x_get_driver_descr(void)
{
	return &mma845x_descr;
}
EXPORT_SYMBOL(mma845x_get_driver_descr);


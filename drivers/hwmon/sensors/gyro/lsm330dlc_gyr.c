/******************** (C) COPYRIGHT 2011 STMicroelectronics ********************
*
* File Name		: lsm330dlc_gyr_sysfs.c
* Authors		: MH - C&I BU - Application Team
*			: Carmine Iascone (carmine.iascone@st.com)
*			: Matteo Dameno (matteo.dameno@st.com)
*			: Both authors are willing to be considered the contact
*			: and update points for the driver.
* Version		: V 1.1.5.3 sysfs
* Date			: 2011/Dec/20
* Description		: LSM330DLC digital output gyroscope sensor API
*
********************************************************************************
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* THE PRESENT SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES
* OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, FOR THE SOLE
* PURPOSE TO SUPPORT YOUR APPLICATION DEVELOPMENT.
* AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
* INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
* CONTENT OF SUCH SOFTWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
* INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
*
********************************************************************************
* REVISON HISTORY
*
* VERSION	| DATE		| AUTHORS		| DESCRIPTION
* 1.0		| 2010/May/02	| Carmine Iascone	| First Release
* 1.1.3		| 2011/Jun/24	| Matteo Dameno		| Corrects ODR Bug
* 1.1.4		| 2011/Sep/02	| Matteo Dameno		| SMB Bus Mng,
*		|		|			| forces BDU setting
* 1.1.5		| 2011/Sep/24	| Matteo Dameno		| Introduces FIFO Feat.
* 1.1.5.2	| 2011/Nov/11	| Matteo Dameno		| enable gpio_int to be
*		|		|			| passed as parameter at
*		|		|			| module loading time;
*		|		|			| corrects polling
*		|		|			| bug at end of probing;
* 1.1.5.3	| 2011/Dec/20	| Matteo Dameno		| corrects error in
*		|		|			| I2C SADROOT; Modifies
*		|		|			| resume suspend func.
*******************************************************************************/

#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/input-polldev.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/moduleparam.h>
#include <linux/module.h>

/** Maximum polled-device-reported rot speed value value in dps*/
#define FS_MAX			32768

/* lsm330dlc gyroscope registers */
#define WHO_AM_I        0x0F

#define CTRL_REG1       0x20    /* CTRL REG1 */
#define CTRL_REG2       0x21    /* CTRL REG2 */
#define CTRL_REG3       0x22    /* CTRL_REG3 */
#define CTRL_REG4       0x23    /* CTRL_REG4 */
#define CTRL_REG5       0x24    /* CTRL_REG5 */
#define	REFERENCE	0x25    /* REFERENCE REG */
#define	FIFO_CTRL_REG	0x2E    /* FIFO CONTROL REGISTER */
#define FIFO_SRC_REG	0x2F    /* FIFO SOURCE REGISTER */
#define	OUT_X_L		0x28    /* 1st AXIS OUT REG of 6 */

#define AXISDATA_REG	OUT_X_L

/* CTRL_REG1 */
#define ALL_ZEROES	0x00
#define PM_OFF		0x00
#define PM_NORMAL	0x08
#define ENABLE_ALL_AXES	0x07
#define ENABLE_NO_AXES	0x00
#define BW00		0x00
#define BW01		0x10
#define BW10		0x20
#define BW11		0x30
#define ODR095		0x00  /* ODR =  95Hz */
#define ODR190		0x40  /* ODR = 190Hz */
#define ODR380		0x80  /* ODR = 380Hz */
#define ODR760		0xC0  /* ODR = 760Hz */

/* CTRL_REG3 bits */
#define	I2_DRDY		0x08
#define	I2_WTM		0x04
#define	I2_OVRUN	0x02
#define	I2_EMPTY	0x01
#define	I2_NONE		0x00
#define	I2_MASK		0x0F

/* CTRL_REG4 bits */
#define	FS_MASK				0x30
#define	BDU_ENABLE			0x80

/* CTRL_REG5 bits */
#define	FIFO_ENABLE	0x40
#define HPF_ENALBE	0x11

/* FIFO_CTRL_REG bits */
#define	FIFO_MODE_MASK		0xE0
#define	FIFO_MODE_BYPASS	0x00
#define	FIFO_MODE_FIFO		0x20
#define	FIFO_MODE_STREAM	0x40
#define	FIFO_MODE_STR2FIFO	0x60
#define	FIFO_MODE_BYPASS2STR	0x80
#define	FIFO_WATERMARK_MASK	0x1F

#define FIFO_STORED_DATA_MASK	0x1F


#define FUZZ			0
#define FLAT			0
#define I2C_AUTO_INCREMENT	0x80

/* RESUME STATE INDICES */
#define	RES_CTRL_REG1		0
#define	RES_CTRL_REG2		1
#define	RES_CTRL_REG3		2
#define	RES_CTRL_REG4		3
#define	RES_CTRL_REG5		4
#define	RES_FIFO_CTRL_REG	5
#define	RESUME_ENTRIES		6


#define DEBUG 0

#define LSM330DLC_GYR_FS_250DPS		0x00
#define LSM330DLC_GYR_FS_500DPS		0x10
#define LSM330DLC_GYR_FS_2000DPS		0x30

/** Registers Contents */
#define WHOAMI_LSM330DLC		0xD4	/* Expected content for WAI register*/
#define LSM330DLC_GYR_DEV_NAME		"lsm330dlc_gyr"
#define LSM330DLC_GYR_SEC_DEV_NAME    "lsm330dlc_sec_gyr"
#define LSM330DLC_MIN_POLL_PERIOD_MS	2
/*
 * LSM330DLC gyroscope data
 * brief structure containing gyroscope values for yaw, pitch and roll in
 * signed short
 */

struct lsm330dlc_gyr_platform_data {
	unsigned int poll_interval;
	unsigned int min_interval;

	u8 fs_range;

	/* axis mapping */
	u8 axis_map_x;
	u8 axis_map_y;
	u8 axis_map_z;

	u8 negate_x;
	u8 negate_y;
	u8 negate_z;
};

struct lsm330dlc_triple {
	short	x,	/* x-axis angular rate data. */
		y,	/* y-axis angluar rate data. */
		z;	/* z-axis angular rate data. */
};

struct output_rate {
	int poll_rate_ms;
	u8 mask;
};

static const struct output_rate odr_table[] = {
	{	2,	ODR760|BW10},
	{	3,	ODR380|BW01},
	{	6,	ODR190|BW00},
	{	11,	ODR095|BW00},
};

static int use_smbus;

static struct lsm330dlc_gyr_platform_data default_lsm330dlc_gyr_pdata = {
	.fs_range = LSM330DLC_GYR_FS_2000DPS,
	.axis_map_x = 0,
	.axis_map_y = 1,
	.axis_map_z = 2,
	.negate_x = 1,
	.negate_y = 1,
	.negate_z = 0,

	.poll_interval = 10,
	.min_interval = LSM330DLC_MIN_POLL_PERIOD_MS, /* 2ms */
};

struct lsm330dlc_data {
	struct i2c_client *client;
	struct lsm330dlc_gyr_platform_data *pdata;

	struct mutex lock;

	struct input_polled_dev *input_poll_dev;
	int hw_initialized;

	atomic_t enabled;

	u8 reg_addr;
	u8 resume_state[RESUME_ENTRIES];

	bool polling_enabled;
	int shutdown_complete;
};


static int lsm330dlc_i2c_read(struct lsm330dlc_data *gyr, u8 *buf, int len)
{
	int ret;
	u8 reg = buf[0];
	u8 cmd = reg;
      if(gyr && gyr->shutdown_complete){
         return -ENODEV;
     }
	if (use_smbus) {
		if (len == 1) {
			ret = i2c_smbus_read_byte_data(gyr->client, cmd);
			buf[0] = ret & 0xff;
#if DEBUG
			dev_warn(&gyr->client->dev,
				"i2c_smbus_read_byte_data: ret=0x%02x, len:%d ,"
				"command=0x%02x, buf[0]=0x%02x\n",
				ret, len, cmd , buf[0]);
#endif
		} else if (len > 1) {
			/* cmd =  = I2C_AUTO_INCREMENT | reg; */
			ret = i2c_smbus_read_i2c_block_data(gyr->client,
								cmd, len, buf);
#if DEBUG
			dev_warn(&gyr->client->dev,
				"i2c_smbus_read_i2c_block_data: ret:%d len:%d, "
				"command=0x%02x, ",
				ret, len, cmd);
			unsigned int ii;
			for (ii = 0; ii < len; ii++)
				printk(KERN_DEBUG "buf[%d]=0x%02x,",
								ii, buf[ii]);

			printk("\n");
#endif
		} else
			ret = -1;

		if (ret < 0) {
			dev_err(&gyr->client->dev,
				"read transfer error: len:%d, command=0x%02x\n",
				len, cmd);
			return 0; /* failure */
		}
		return len; /* success */
	}

	/* cmd =  = I2C_AUTO_INCREMENT | reg; */
	ret = i2c_master_send(gyr->client, &cmd, sizeof(cmd));
	if (ret != sizeof(cmd))
		return ret;

	return i2c_master_recv(gyr->client, buf, len);
}

static int lsm330dlc_i2c_write(struct lsm330dlc_data *gyr, u8 *buf, int len)
{
	int ret;
	u8 reg, value;
    
      if(gyr && gyr->shutdown_complete){
         return -ENODEV;
     }
	reg = buf[0];
	value = buf[1];

	if (use_smbus) {
		if (len == 1) {
			ret = i2c_smbus_write_byte_data(gyr->client,
								reg, value);
#if DEBUG
			dev_warn(&gyr->client->dev,
				"i2c_smbus_write_byte_data: ret=%d, len:%d, "
				"command=0x%02x, value=0x%02x\n",
				ret, len, reg , value);
#endif
			return ret;
		} else if (len > 1) {
			ret = i2c_smbus_write_i2c_block_data(gyr->client,
							reg, len, buf + 1);
#if DEBUG
			dev_warn(&gyr->client->dev,
				"i2c_smbus_write_i2c_block_data: ret=%d, "
				"len:%d, command=0x%02x, ",
				ret, len, reg);
			unsigned int ii;
			for (ii = 0; ii < (len + 1); ii++)
				printk(KERN_DEBUG "value[%d]=0x%02x,",
								ii, buf[ii]);

			printk("\n");
#endif
			return ret;
		}
	}

	ret = i2c_master_send(gyr->client, buf, len+1);
	return (ret == len+1) ? 0 : ret;
}


static int lsm330dlc_register_write(struct lsm330dlc_data *gyro, u8 *buf,
		u8 reg_address, u8 new_value)
{
	int err;

	/* Sets configuration register at reg_address
	 *  NOTE: this is a straight overwrite  */
	buf[0] = reg_address;
	buf[1] = new_value;
	err = lsm330dlc_i2c_write(gyro, buf, 1);
	if (err < 0)
		return err;

	return err;
}

static int lsm330dlc_register_read(struct lsm330dlc_data *gyro, u8 *buf,
		u8 reg_address)
{
	int err = -1;
	buf[0] = (reg_address);
	err = lsm330dlc_i2c_read(gyro, buf, 1);
	return err;
}

static int lsm330dlc_register_update(struct lsm330dlc_data *gyro, u8 *buf,
		u8 reg_address, u8 mask, u8 new_bit_values)
{
	int err = -1;
	u8 init_val;
	u8 updated_val;
	err = lsm330dlc_register_read(gyro, buf, reg_address);
	if (!(err < 0)) {
		init_val = buf[0];
		updated_val = ((mask & new_bit_values) | ((~mask) & init_val));
		err = lsm330dlc_register_write(gyro, buf, reg_address,
				updated_val);
	}
	return err;
}

static int lsm330dlc_update_fs_range(struct lsm330dlc_data *gyro,
							u8 new_fs)
{
	int res ;
	u8 buf[2];

	buf[0] = CTRL_REG4;

	res = lsm330dlc_register_update(gyro, buf, CTRL_REG4,
							FS_MASK, new_fs);

	if (res < 0) {
		pr_err("%s : failed to update fs:0x%02x\n",
							__func__, new_fs);
		return res;
	}
	gyro->resume_state[RES_CTRL_REG4] =
		((FS_MASK & new_fs) |
		(~FS_MASK & gyro->resume_state[RES_CTRL_REG4]));

	return res;
}


static int lsm330dlc_update_odr(struct lsm330dlc_data *gyro,
			unsigned int poll_interval_ms)
{
	int err = -1;
	int i;
	u8 config[2];

	for (i = ARRAY_SIZE(odr_table) - 1; i >= 0; i--) {
		if ((odr_table[i].poll_rate_ms <= poll_interval_ms) || (i == 0))
			break;
	}

	config[1] = odr_table[i].mask;
	config[1] |= (ENABLE_ALL_AXES + PM_NORMAL);

	/* If device is currently enabled, we need to write new
	 *  configuration out to it */
	if (atomic_read(&gyro->enabled)) {
		config[0] = CTRL_REG1;
		err = lsm330dlc_i2c_write(gyro, config, 1);
		if (err < 0)
			return err;
		gyro->resume_state[RES_CTRL_REG1] = config[1];
	}


	return err;
}

/* gyroscope data readout */
static int lsm330dlc_get_data(struct lsm330dlc_data *gyro,
			     struct lsm330dlc_triple *data)
{
	int err;
	unsigned char gyro_out[6];
	/* y,p,r hardware data */
	s16 hw_d[3] = { 0 };

	gyro_out[0] = (I2C_AUTO_INCREMENT | AXISDATA_REG);

	err = lsm330dlc_i2c_read(gyro, gyro_out, 6);

	if (err < 0)
		return err;

	hw_d[0] = (s16) (((gyro_out[1]) << 8) | gyro_out[0]);
	hw_d[1] = (s16) (((gyro_out[3]) << 8) | gyro_out[2]);
	hw_d[2] = (s16) (((gyro_out[5]) << 8) | gyro_out[4]);

	data->x = ((gyro->pdata->negate_x) ? (-hw_d[gyro->pdata->axis_map_x])
		   : (hw_d[gyro->pdata->axis_map_x]));
	data->y = ((gyro->pdata->negate_y) ? (-hw_d[gyro->pdata->axis_map_y])
		   : (hw_d[gyro->pdata->axis_map_y]));
	data->z = ((gyro->pdata->negate_z) ? (-hw_d[gyro->pdata->axis_map_z])
		   : (hw_d[gyro->pdata->axis_map_z]));

#if DEBUG
	printk(KERN_INFO "gyro_out: x = %d, y = %d, z = %d\n",
		data->x, data->y, data->z);
#endif

	return err;
}

static void lsm330dlc_report_values(struct lsm330dlc_data *gyr,
						struct lsm330dlc_triple *data)
{
	struct input_dev *input = gyr->input_poll_dev->input;
	input_report_abs(input, ABS_HAT1X, data->x);
	input_report_abs(input, ABS_HAT1Y, data->y);
	input_report_abs(input, ABS_HAT2X, data->z);
	input_sync(input);
}

static int lsm330dlc_hw_init(struct lsm330dlc_data *gyro)
{
	int err;
	u8 buf[6];

	pr_info("%s hw init\n", LSM330DLC_GYR_DEV_NAME);

	buf[0] = (I2C_AUTO_INCREMENT | CTRL_REG1);
	buf[1] = gyro->resume_state[RES_CTRL_REG1];
	buf[2] = gyro->resume_state[RES_CTRL_REG2];
	buf[3] = gyro->resume_state[RES_CTRL_REG3];
	buf[4] = gyro->resume_state[RES_CTRL_REG4];
	buf[5] = gyro->resume_state[RES_CTRL_REG5];

	err = lsm330dlc_i2c_write(gyro, buf, 5);
	if (err < 0)
		return err;

	buf[0] = FIFO_CTRL_REG;
	buf[1] = gyro->resume_state[RES_FIFO_CTRL_REG];
	err = lsm330dlc_i2c_write(gyro, buf, 1);
	if (err < 0)
			return err;

	gyro->hw_initialized = 1;

	return err;
}

static void lsm330dlc_device_power_off(struct lsm330dlc_data *dev_data)
{
	int err;
	u8 buf[2];

	pr_info("%s power off\n", LSM330DLC_GYR_DEV_NAME);

	buf[0] = CTRL_REG1;
	buf[1] = PM_OFF;
	err = lsm330dlc_i2c_write(dev_data, buf, 1);
	if (err < 0)
		dev_err(&dev_data->client->dev, "soft power off failed\n");

	dev_data->hw_initialized = 0;
}

static int lsm330dlc_device_power_on(struct lsm330dlc_data *dev_data)
{
	int err;

	if (!dev_data->hw_initialized) {
		err = lsm330dlc_hw_init(dev_data);
		if (err < 0) {
			lsm330dlc_device_power_off(dev_data);
			return err;
		}
	}
	return 0;
}

static int lsm330dlc_enable(struct lsm330dlc_data *dev_data)
{
	int err;

	if (!atomic_cmpxchg(&dev_data->enabled, 0, 1)) {

		err = lsm330dlc_device_power_on(dev_data);
		if (err < 0) {
			atomic_set(&dev_data->enabled, 0);
			return err;
		}

		if (dev_data->polling_enabled) {
			dev_data->input_poll_dev->poll_interval =
						dev_data->pdata->poll_interval;
 			input_polldev_resume(dev_data->input_poll_dev);
		}

	}

	return 0;
}

static int lsm330dlc_disable(struct lsm330dlc_data *dev_data)
{
	printk(KERN_DEBUG "%s: dev_data->enabled = %d\n", __func__,
		atomic_read(&dev_data->enabled));

	if (atomic_cmpxchg(&dev_data->enabled, 1, 0)) {
		if (dev_data->polling_enabled) {
			dev_data->input_poll_dev->poll_interval = 0;
			//stop the poll work
			input_polldev_suspend(dev_data->input_poll_dev);
		}
		lsm330dlc_device_power_off(dev_data);
	}
	return 0;
}

static ssize_t attr_polling_rate_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	int val;
	struct lsm330dlc_data *gyro = dev_get_drvdata(dev);
	mutex_lock(&gyro->lock);
	val = gyro->pdata->poll_interval;
	mutex_unlock(&gyro->lock);
	return sprintf(buf, "%d\n", val);
}

static ssize_t attr_polling_rate_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	struct lsm330dlc_data *gyro = dev_get_drvdata(dev);
	unsigned long interval_ms;

	if (strict_strtoul(buf, 10, &interval_ms))
		return -EINVAL;
	if (!interval_ms)
		return -EINVAL;
	interval_ms = max((unsigned int)interval_ms, gyro->pdata->min_interval);
	mutex_lock(&gyro->lock);
	if (atomic_read(&gyro->enabled) && &gyro->polling_enabled)
		gyro->input_poll_dev->poll_interval = interval_ms;
	gyro->pdata->poll_interval = interval_ms;
	lsm330dlc_update_odr(gyro, interval_ms);
	mutex_unlock(&gyro->lock);
	return size;
}

static ssize_t attr_enable_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct lsm330dlc_data *gyro = dev_get_drvdata(dev);
	int val = atomic_read(&gyro->enabled);
	return sprintf(buf, "%d\n", val);
}

static ssize_t attr_enable_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	struct lsm330dlc_data *gyro = dev_get_drvdata(dev);
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	if (val)
		lsm330dlc_enable(gyro);
	else
		lsm330dlc_disable(gyro);

	return size;
}

#ifdef DEBUG
static ssize_t attr_reg_set(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	int rc;
	struct lsm330dlc_data *gyro = dev_get_drvdata(dev);
	u8 x[2];
	unsigned long val;

	if (strict_strtoul(buf, 16, &val))
		return -EINVAL;
	mutex_lock(&gyro->lock);
	x[0] = gyro->reg_addr;
	mutex_unlock(&gyro->lock);
	x[1] = val;
	rc = lsm330dlc_i2c_write(gyro, x, 1);
	return size;
}

static ssize_t attr_reg_get(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	ssize_t ret;
	struct lsm330dlc_data *gyro = dev_get_drvdata(dev);
	int rc;
	u8 data;

	mutex_lock(&gyro->lock);
	data = gyro->reg_addr;
	mutex_unlock(&gyro->lock);
	rc = lsm330dlc_i2c_read(gyro, &data, 1);
	ret = sprintf(buf, "0x%02x\n", data);
	return ret;
}

static ssize_t attr_addr_set(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct lsm330dlc_data *gyro = dev_get_drvdata(dev);
	unsigned long val;

	if (strict_strtoul(buf, 16, &val))
		return -EINVAL;

	mutex_lock(&gyro->lock);

	gyro->reg_addr = val;

	mutex_unlock(&gyro->lock);

	return size;
}
#endif /* DEBUG */

static struct device_attribute attributes[] = {
	__ATTR(pollrate_ms, 0644, attr_polling_rate_show,
						attr_polling_rate_store),
	__ATTR(enable, 0644, attr_enable_show, attr_enable_store),
#ifdef DEBUG
	__ATTR(reg_value, 0600, attr_reg_get, attr_reg_set),
	__ATTR(reg_addr, 0200, NULL, attr_addr_set),
#endif
};

static int create_sysfs_interfaces(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		if (device_create_file(dev, attributes + i))
			goto error;
	return 0;

error:
	for (; i >= 0; i--)
		device_remove_file(dev, attributes + i);
	dev_err(dev, "%s:Unable to create interface\n", __func__);
	return -1;
}

static int remove_sysfs_interfaces(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		device_remove_file(dev, attributes + i);
	return 0;
}

#if 0
static void report_triple(struct lsm330dlc_data *gyro)
{
	int err;
	struct lsm330dlc_triple data_out;

	err = lsm330dlc_get_data(gyro, &data_out);
	if (err < 0)
		dev_err(&gyro->client->dev, "get_gyroscope_data failed\n");
	else
		lsm330dlc_report_values(gyro, &data_out);
}
#endif

static void lsm330dlc_input_poll_func(struct input_polled_dev *dev)
{
	struct lsm330dlc_data *gyro = dev->private;

	struct lsm330dlc_triple data_out;

	int err;

	dev_dbg(&gyro->client->dev, "%s++\n", __func__);

	if(atomic_read(&gyro->enabled)){
		mutex_lock(&gyro->lock);
		err = lsm330dlc_get_data(gyro, &data_out);
		if (err < 0)
			dev_err(&gyro->client->dev, "get_gyroscope_data failed\n");
		else
			lsm330dlc_report_values(gyro, &data_out);

		mutex_unlock(&gyro->lock);
	}
}

int lsm330dlc_input_open(struct input_dev *input)
{
	pr_debug("%s\n", __func__);
	return 0;
}

void lsm330dlc_input_close(struct input_dev *dev)
{
	pr_debug("%s\n", __func__);
}

static int lsm330dlc_validate_pdata(struct lsm330dlc_data *gyro)
{
	/* checks for correctness of minimal polling period */
	gyro->pdata->min_interval =
		max((unsigned int) LSM330DLC_MIN_POLL_PERIOD_MS,
						gyro->pdata->min_interval);

	gyro->pdata->poll_interval = max(gyro->pdata->poll_interval,
			gyro->pdata->min_interval);

	if (gyro->pdata->axis_map_x > 2 ||
	    gyro->pdata->axis_map_y > 2 ||
	    gyro->pdata->axis_map_z > 2) {
		dev_err(&gyro->client->dev,
			"invalid axis_map value x:%u y:%u z%u\n",
			gyro->pdata->axis_map_x,
			gyro->pdata->axis_map_y,
			gyro->pdata->axis_map_z);
		return -EINVAL;
	}

	/* Only allow 0 and 1 for negation boolean flag */
	if (gyro->pdata->negate_x > 1 ||
	    gyro->pdata->negate_y > 1 ||
	    gyro->pdata->negate_z > 1) {
		dev_err(&gyro->client->dev,
			"invalid negate value x:%u y:%u z:%u\n",
			gyro->pdata->negate_x,
			gyro->pdata->negate_y,
			gyro->pdata->negate_z);
		return -EINVAL;
	}

	/* Enforce minimum polling interval */
	if (gyro->pdata->poll_interval < gyro->pdata->min_interval) {
		dev_err(&gyro->client->dev,
			"minimum poll interval violated\n");
		return -EINVAL;
	}
	return 0;
}

static int lsm330dlc_input_init(struct lsm330dlc_data *gyro)
{
	int err = -1;
	struct input_dev *input;

	pr_debug("%s++\n", __func__);

	gyro->input_poll_dev = input_allocate_polled_device();
	if (!gyro->input_poll_dev) {
		err = -ENOMEM;
		dev_err(&gyro->client->dev,
			"input device allocation failed\n");
		goto err0;
	}

	gyro->input_poll_dev->private = gyro;
	gyro->input_poll_dev->poll = lsm330dlc_input_poll_func;
	gyro->input_poll_dev->poll_interval = gyro->pdata->poll_interval;

	input = gyro->input_poll_dev->input;

	input->open = lsm330dlc_input_open;
	input->close = lsm330dlc_input_close;

	input->id.bustype = BUS_I2C;
	input->dev.parent = &gyro->client->dev;

	input_set_drvdata(gyro->input_poll_dev->input, gyro);

	set_bit(EV_ABS, input->evbit);

	input_set_abs_params(input, ABS_HAT1X, -FS_MAX, FS_MAX, FUZZ, FLAT);
	input_set_abs_params(input, ABS_HAT1Y, -FS_MAX, FS_MAX, FUZZ, FLAT);
	input_set_abs_params(input, ABS_HAT2X, -FS_MAX, FS_MAX, FUZZ, FLAT);

	input->name = "gyro";

	err = input_register_polled_device(gyro->input_poll_dev);
	if (err) {
		dev_err(&gyro->client->dev,
			"unable to register input polled device %s\n",
			gyro->input_poll_dev->input->name);
		goto err1;
	}

	return 0;

err1:
	input_free_polled_device(gyro->input_poll_dev);
err0:
	return err;
}

static void lsm330dlc_input_cleanup(struct lsm330dlc_data *gyro)
{
	input_unregister_polled_device(gyro->input_poll_dev);
	input_free_polled_device(gyro->input_poll_dev);
}

static int lsm330dlc_probe(struct i2c_client *client,
					const struct i2c_device_id *devid)
{
	struct lsm330dlc_data *gyro;

	u32 smbus_func = I2C_FUNC_SMBUS_BYTE_DATA |
			I2C_FUNC_SMBUS_WORD_DATA | I2C_FUNC_SMBUS_I2C_BLOCK ;

	int err = -1;

	pr_info("%s: probe start.\n", LSM330DLC_GYR_DEV_NAME);

	/* Support for both I2C and SMBUS adapter interfaces. */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_warn(&client->dev, "client not i2c capable\n");
		if (i2c_check_functionality(client->adapter, smbus_func)) {
			use_smbus = 1;
			dev_warn(&client->dev, "client using SMBUS\n");
		} else {
			err = -ENODEV;
			dev_err(&client->dev, "client nor SMBUS capable\n");
			goto err0;
		}
	}

	gyro = kzalloc(sizeof(*gyro), GFP_KERNEL);
	if (gyro == NULL) {
		dev_err(&client->dev,
			"failed to allocate memory for module data\n");
		err = -ENOMEM;
		goto err0;
	}
      gyro->shutdown_complete = 0;
	mutex_init(&gyro->lock);
	mutex_lock(&gyro->lock);
	gyro->client = client;

	gyro->pdata = kmalloc(sizeof(*gyro->pdata), GFP_KERNEL);
	if (gyro->pdata == NULL) {
		dev_err(&client->dev,
			"failed to allocate memory for pdata: %d\n", err);
		goto err1;
	}

	if (client->dev.platform_data == NULL) {
		memcpy(gyro->pdata, &default_lsm330dlc_gyr_pdata,
							sizeof(*gyro->pdata));
	} else {
		memcpy(gyro->pdata, client->dev.platform_data,
						sizeof(*gyro->pdata));
	}

	err = lsm330dlc_validate_pdata(gyro);
	if (err < 0) {
		dev_err(&client->dev, "failed to validate platform data\n");
		goto err1_1;
	}

	i2c_set_clientdata(client, gyro);

	memset(gyro->resume_state, 0, ARRAY_SIZE(gyro->resume_state));

	gyro->resume_state[RES_CTRL_REG1] = ALL_ZEROES | ENABLE_ALL_AXES;
	gyro->resume_state[RES_CTRL_REG2] = ALL_ZEROES;
	gyro->resume_state[RES_CTRL_REG3] = ALL_ZEROES;
	gyro->resume_state[RES_CTRL_REG4] = ALL_ZEROES | BDU_ENABLE;
	gyro->resume_state[RES_CTRL_REG5] = ALL_ZEROES;
	gyro->resume_state[RES_FIFO_CTRL_REG] = ALL_ZEROES;

	gyro->polling_enabled = true;
	dev_info(&client->dev, "polling enabled\n");

	err = lsm330dlc_device_power_on(gyro);
	if (err < 0) {
		dev_err(&client->dev, "power on failed: %d\n", err);
		goto err2;
	}

	atomic_set(&gyro->enabled, 1);

	err = lsm330dlc_update_fs_range(gyro, gyro->pdata->fs_range);
	if (err < 0) {
		dev_err(&client->dev, "update_fs_range failed\n");
		goto err2;
	}

	err = lsm330dlc_update_odr(gyro, gyro->pdata->poll_interval);
	if (err < 0) {
		dev_err(&client->dev, "update_odr failed\n");
		goto err2;
	}

	err = lsm330dlc_input_init(gyro);
	if (err < 0)
		goto err3;

	err = create_sysfs_interfaces(&client->dev);
	if (err < 0) {
		dev_err(&client->dev,
			"%s device register failed\n", LSM330DLC_GYR_DEV_NAME);
		goto err4;
	}

	lsm330dlc_device_power_off(gyro);

	/* As default, do not report information */
	atomic_set(&gyro->enabled, 0);

	mutex_unlock(&gyro->lock);

#if DEBUG
	dev_info(&client->dev, "%s probed: device created successfully\n",
							LSM330DLC_GYR_DEV_NAME);
#endif

	return 0;

err4:
	lsm330dlc_input_cleanup(gyro);
err3:
	lsm330dlc_device_power_off(gyro);
err2:
err1_1:
	mutex_unlock(&gyro->lock);
	kfree(gyro->pdata);
err1:
      mutex_destroy(&gyro->lock);
	kfree(gyro);
err0:
		pr_err("%s: Driver Initialization failed\n",
							LSM330DLC_GYR_DEV_NAME);
		return err;
}

static int lsm330dlc_remove(struct i2c_client *client)
{
	struct lsm330dlc_data *gyro = i2c_get_clientdata(client);
#if DEBUG
	pr_info("%s: driver removing\n", LSM330DLC_GYR_DEV_NAME);
#endif

	lsm330dlc_disable(gyro);
	lsm330dlc_input_cleanup(gyro);

	remove_sysfs_interfaces(&client->dev);

	kfree(gyro->pdata);
      mutex_destroy(&gyro->lock);
	kfree(gyro);
	return 0;
}

static int lsm330dlc_suspend(struct device *dev)
{
	int err = 0;
#define SLEEP
#ifdef CONFIG_PM
	struct i2c_client *client = to_i2c_client(dev);
	struct lsm330dlc_data *data = i2c_get_clientdata(client);
	u8 buf[2];
//#if DEBUG
	dev_info(&client->dev, "suspend\n");
//#endif /* DEBUG */
	printk(KERN_INFO "%s\n", __func__);
	if (atomic_read(&data->enabled)) {
		mutex_lock(&data->lock);
		if (data->polling_enabled) {
			pr_info("polling disabled\n");
			input_polldev_suspend(data->input_poll_dev);
		}
#ifdef SLEEP
		err = lsm330dlc_register_update(data, buf, CTRL_REG1,
				0x0F, (ENABLE_NO_AXES | PM_NORMAL));
#else
		err = lsm330dlc_register_update(data, buf, CTRL_REG1,
				0x08, PM_OFF);
#endif /*SLEEP*/
		mutex_unlock(&data->lock);
	}
#endif /*CONFIG_PM*/
	return err;
}

static int lsm330dlc_resume(struct device *dev)
{
	int err = 0;
#ifdef CONFIG_PM
	struct i2c_client *client = to_i2c_client(dev);
	struct lsm330dlc_data *data = i2c_get_clientdata(client);
	u8 buf[2];

//#if DEBUG
	dev_info(&client->dev, "resume\n");
//#endif /*DEBUG */
	pr_debug(KERN_INFO "%s\n", __func__);
	if (atomic_read(&data->enabled)) {
		mutex_lock(&data->lock);
		if (data->polling_enabled) {
			pr_info("polling enabled\n");
			input_polldev_resume(data->input_poll_dev);
		}
#ifdef SLEEP
		err = lsm330dlc_register_update(data, buf, CTRL_REG1,
				0x0F, (ENABLE_ALL_AXES | PM_NORMAL));
#else
		err = lsm330dlc_register_update(data, buf, CTRL_REG1,
				0x08, PM_NORMAL);
#endif
		mutex_unlock(&data->lock);

	}
#endif /*CONFIG_PM*/
	return err;
}

static void lsm330dlc_shutdown(struct i2c_client *client)
{
	struct lsm330dlc_data *data = i2c_get_clientdata(client);
      mutex_lock(&data->lock);
      data->shutdown_complete =1;
      mutex_unlock(&data->lock);
}

static const struct i2c_device_id lsm330dlc_id[] = {
	{ LSM330DLC_GYR_DEV_NAME , 0 },
	{ LSM330DLC_GYR_SEC_DEV_NAME , 0 },
	{},
};

MODULE_DEVICE_TABLE(i2c, lsm330dlc_id);

static struct dev_pm_ops lsm330dlc_pm = {
	.suspend = lsm330dlc_suspend,
	.resume = lsm330dlc_resume,
};

static struct i2c_driver lsm330dlc_driver = {
	.driver = {
			.owner = THIS_MODULE,
			.name = LSM330DLC_GYR_DEV_NAME,
			.pm = &lsm330dlc_pm,
	},
	.probe = lsm330dlc_probe,
	.remove = __devexit_p(lsm330dlc_remove),
	.shutdown = lsm330dlc_shutdown,
	.id_table = lsm330dlc_id,

};

static int __init lsm330dlc_init(void)
{
#if DEBUG
	pr_info("%s: gyroscope sysfs driver init\n", LSM330DLC_GYR_DEV_NAME);
#endif
	return i2c_add_driver(&lsm330dlc_driver);
}

static void __exit lsm330dlc_exit(void)
{
#if DEBUG
	pr_info("%s exit\n", LSM330DLC_GYR_DEV_NAME);
#endif
	i2c_del_driver(&lsm330dlc_driver);
	return;
}

module_init(lsm330dlc_init);
module_exit(lsm330dlc_exit);

MODULE_DESCRIPTION("lsm330dlc digital gyroscope section sysfs driver");
MODULE_AUTHOR("Matteo Dameno, Carmine Iascone, STMicroelectronics");
MODULE_LICENSE("GPL");


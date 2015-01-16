/* Copyright (C) 2011-2012 NVIDIA Corporation.
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
/* This is a NVC kernel driver for a focuser device called
 * bu64291.
 */
/* Implementation
 * --------------
 * The board level details about the device need to be provided in the board
 * file with the <device>_platform_data structure.
 * Standard among NVC kernel drivers in this structure is:
 * .cfg = Use the NVC_CFG_ defines that are in nvc.h.
 *  Descriptions of the configuration options are with the defines.
 *      This value is typically 0.
 * .num = The number of the instance of the device.  This should start at 1 and
 *      and increment for each device on the board.  This number will be
 *      appended to the MISC driver name, Example: /dev/focuser.1
 *      If not used or 0, then nothing is appended to the name.
 * .sync = If there is a need to synchronize two devices, then this value is
 *       the number of the device instance (.num above) this device is to
 *       sync to.  For example:
 *       Device 1 platform entries =
 *       .num = 1,
 *       .sync = 2,
 *       Device 2 platfrom entries =
 *       .num = 2,
 *       .sync = 1,
 *       The above example sync's device 1 and 2.
 *       To disable sync, set .sync = 0.  Note that the .num = 0 device is not
 *       allowed to be synced to.
 *       This is typically used for stereo applications.
 * .dev_name = The MISC driver name the device registers as.  If not used,
 *       then the part number of the device is used for the driver name.
 *       If using the NVC user driver then use the name found in this
 *       driver under _default_pdata.
 * .gpio_count = The ARRAY_SIZE of the nvc_gpio_pdata table.
 * .gpio = A pointer to the nvc_gpio_pdata structure's platform GPIO data.
 *       The GPIO mechanism works by cross referencing the .gpio_type key
 *       among the nvc_gpio_pdata GPIO data and the driver's nvc_gpio_init
 *       GPIO data to build a GPIO table the driver can use.  The GPIO's
 *       defined in the device header file's _gpio_type enum are the
 *       gpio_type keys for the nvc_gpio_pdata and nvc_gpio_init structures.
 *       These need to be present in the board file's nvc_gpio_pdata
 *       structure for the GPIO's that are used.
 *       The driver's GPIO logic uses assert/deassert throughout until the
 *       low level _gpio_wr/rd calls where the .assert_high is used to
 *       convert the value to the correct signal level.
 *       See the GPIO notes in nvc.h for additional information.
 *
 * The following is specific to NVC kernel focus drivers:
 * .nvc = Pointer to the nvc_focus_nvc structure.  This structure needs to
 *      be defined and populated if overriding the driver defaults.
 * .cap = Pointer to the nvc_focus_cap structure.  This structure needs to
 *      be defined and populated if overriding the driver defaults.
 *
 * The following is specific to this NVC kernel focus driver:
 * .info = Pointer to the bu64291_pdata_info structure.  This structure does
 *       not need to be defined and populated unless overriding ROM data.
 *
 * Power Requirements:
 * The device's header file defines the voltage regulators needed with the
 * enumeration <device>_vreg.  The order these are enumerated is the order
 * the regulators will be enabled when powering on the device.  When the
 * device is powered off the regulators are disabled in descending order.
 * The <device>_vregs table in this driver uses the nvc_regulator_init
 * structure to define the regulator ID strings that go with the regulators
 * defined with <device>_vreg.  These regulator ID strings (or supply names)
 * will be used in the regulator_get function in the _vreg_init function.
 * The board power file and <device>_vregs regulator ID strings must match.
 */

/*#define DEBUG 1*/

#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <media/bu64291.h>

#define BU64291_FOCAL_LENGTH_FLOAT		(3.81f)
#define BU64291_FNUMBER_FLOAT			(2.2f)
#define BU64291_FOCAL_LENGTH		(0x4073D70A)
#define BU64291_FNUMBER			(0x400CCCCD)
#define BU64291_SLEW_RATE		1
#define BU64291_ACTUATOR_RANGE		(BU64291_POS_HIGH_DEFAULT - BU64291_POS_LOW_DEFAULT)
#define BU64291_SETTLETIME		30
#define BU64291_FOCUS_MACRO		576
#define BU64291_FOCUS_INFINITY		140
#define BU64291_POS_LOW_DEFAULT		0//16
#define BU64291_POS_HIGH_DEFAULT		1023//1023
#define BU64291_MAX_RETRIES		3

/* yaoling for bu64291 */
#define BU64291_SETTING_POINT_A		0x94
#define BU64291_POINTA_VALUE			0xCD

#define BU64291_SETTING_POINT_B		0x9d
#define BU64291_POINTB_VALUE			0x33

#define BU64291_SETTING_POINT_ATOB	0xa4
#define BU64291_POINTATOB_VALUE			0xec


#define BU64291_VCM_SLEW_MASK		0x7fa0
/*
 * FIXME: SLEW_MAX was defined base on valid register value, but a smaller
 * sane value must be defined.
 */
#define BU64291_VCM_SLEW_MAX		0x7fa0
#define BU64291_VCM_SLEW_MIN		0x280
#define BU64291_VCM_SLEW_DEFAULT	BU64291_VCM_SLEW_MIN
#define BU64291_VCM_ABS_MASK		0xffa0

#define BU64291_VCM_SLEW_TIME_MAX	0xff
#define BU64291_VCM_SLEW_TIME_DEFAULT	0x2
#define BU64291_VCM_SLEW_RETRY_MAX	50
#define BU64291_VCM_STAB_RETRY_MAX	15
#define BU64291_VCM_HOME_POS		0
#define BU64291_VCM_INVALID_MIN_POS	0x8001
#define BU64291_VCM_INVALID_MAX_POS	0x7fff

#define BU64291_WARMUP_STEP		10	/* 10% */
#define BU64291_WARMUP_POS_START	1
#define BU64291_WARMUP_POS_END		6

#define BU64291_REG16_MS1Z12		0x16	/* VCM slew */
#define BU64291_REG_STMVINT		0xa0	/* VCM step timing */
#define BU64291_REG16_STMVEND		0xa1	/* target position */
#define BU64291_REG16_RZ		0x04	/* current position */

#define BU64291_REG_STMVEN		0x8a
#define BU64291_REG_STMVEN_MOVE_BUSY	(1 << 0)	/* moving? */

#define BU64291_REG_MSSET		0x8f
#define BU64291_REG_MSSET_BUSY		(1 << 0)	/* stabilized? */
extern int Bu64291_infinity;
extern int Bu64291_macro;
#define  INF_OFFSET (-136)
#define  MACRO_OFFSET (-196)

static struct nvc_gpio_init bu64291_gpio[] = {
	{ BU64291_GPIO_TYPE_PWRDN, GPIOF_OUT_INIT_LOW, "pwrdn", false, true, }
};

struct bu64291_info {
	atomic_t in_use;
	struct i2c_client *i2c_client;
	struct bu64291_platform_data *pdata;
	struct miscdevice miscdev;
	struct list_head list;
	int pwr_dev;
	struct nvc_gpio gpio[ARRAY_SIZE(bu64291_gpio)];
	struct bu64291_power_rail power;
	int id_minor;
	u32 pos;
	u8 s_mode;
	bool reset_flag;
	struct bu64291_info *s_info;
	struct nvc_focus_nvc nvc;
	struct nvc_focus_cap cap;
	struct nv_focuser_config nv_config;
	struct bu64291_pdata_info config;
};

/**
 * The following are default values
 */

static struct bu64291_pdata_info bu64291_default_info = {
	.pos_low = BU64291_POS_LOW_DEFAULT,
	.pos_high = BU64291_POS_HIGH_DEFAULT,
};

static struct nvc_focus_cap bu64291_default_cap = {
	.version = NVC_FOCUS_CAP_VER2,
	.slew_rate = BU64291_SLEW_RATE,
	.actuator_range = BU64291_ACTUATOR_RANGE,
	.settle_time = BU64291_SETTLETIME,
	.focus_macro = BU64291_FOCUS_MACRO,
	.focus_infinity = BU64291_FOCUS_INFINITY,
	.focus_hyper = BU64291_FOCUS_INFINITY,
	.inf_offset = INF_OFFSET,
	.macro_offset = MACRO_OFFSET,
};

static struct nvc_focus_nvc bu64291_default_nvc = {
	.focal_length = BU64291_FOCAL_LENGTH,
	.fnumber = BU64291_FNUMBER,
};

static struct bu64291_platform_data bu64291_default_pdata = {
	.cfg = 0,
	.num = 0,
	.sync = 0,
	.dev_name = "focuser",
};
static LIST_HEAD(bu64291_info_list);
static DEFINE_SPINLOCK(bu64291_spinlock);

static int bu64291_i2c_rd16(struct bu64291_info *info, u16 *val)
{
	struct i2c_msg msg;
	u8 buf[2];
	msg.addr = info->i2c_client->addr;
	msg.flags = I2C_M_RD;
	msg.len = 2;
	msg.buf = &buf[0];
	if (i2c_transfer(info->i2c_client->adapter, &msg, 1) != 1)
		return -EIO;
	*val = (((u16)buf[0] << 8) | (u16)buf[1]);
	return 0;
}
#if 0
static int bu64291_i2c_wr16(struct bu64291_info *info, u16 val)
{
	struct i2c_msg msg;
	int retry = 0;
	u8 buf[2];
	buf[0] = (u8)(val >> 8);
	buf[1] = (u8)(val & 0xff);
	msg.addr = info->i2c_client->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = &buf[0];

	do {
		if (i2c_transfer(info->i2c_client->adapter, &msg, 1) == 1)
			return 0;
		retry++;
		dev_err(&info->i2c_client->dev,
			"[CAM] bu64291: i2c transfer failed, " \
			"retrying value: %x\n", val);
		msleep(3);
	} while (retry <= BU64291_MAX_RETRIES);
	return -EIO;
}
static int bu64291_i2c_rd8(struct bu64291_info *info, u8 reg, u8 *val)
{
	struct i2c_msg msg[2];
	u8 buf[3];
	
	msg[0].addr = info->i2c_client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = buf;
	
	msg[1].addr = info->i2c_client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = buf+1;
	
	msg[0].addr = info->i2c_client->addr;
	msg[0].flags = I2C_M_RD;
	msg[0].len = 1;
	msg[0].buf = buf+2;

	
	if (i2c_transfer(info->i2c_client->adapter, &msg, 3) != 3)
		return -EIO;
	*val = (((u16)buf[1] << 8) | (u16)buf[2]);
	return 0;
}
#endif
static int bu64291_i2c_wr8(struct bu64291_info *info, u8 reg, u8 val)
{
	struct i2c_msg msg;
	int retry = 0;
	u8 buf[2];
	buf[0] = reg;
	buf[1] = val;
	msg.addr = info->i2c_client->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = &buf[0];

	do {
		if (i2c_transfer(info->i2c_client->adapter, &msg, 1) == 1)
			return 0;
		retry++;
		dev_err(&info->i2c_client->dev,
			"[CAM] bu64291: i2c transfer failed, " \
			"retrying value: %x\n", val);
		msleep(3);
	} while (retry <= BU64291_MAX_RETRIES);
	return -EIO;
}

static void bu64291_gpio_exit(struct bu64291_info *info)
{
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(bu64291_gpio); i++) {
		if (info->gpio[i].gpio && info->gpio[i].own)
			gpio_free(info->gpio[i].gpio);
	}
}

static void bu64291_gpio_init(struct bu64291_info *info)
{
	char label[32];
	unsigned long flags;
	unsigned type;
	unsigned i;
	unsigned j;
	int err;

	if (!info->pdata->gpio_count || !info->pdata->gpio)
		return;

	for (i = 0; i < ARRAY_SIZE(bu64291_gpio); i++) {
		type = bu64291_gpio[i].gpio_type;
		for (j = 0; j < info->pdata->gpio_count; j++) {
			if (type == info->pdata->gpio[j].gpio_type)
				break;
		}
		if (j == info->pdata->gpio_count)
			continue;

		info->gpio[type].gpio = info->pdata->gpio[j].gpio;
		if (bu64291_gpio[i].use_flags) {
			flags = bu64291_gpio[i].flags;
			info->gpio[type].active_high =
						bu64291_gpio[i].active_high;
		} else {
			info->gpio[type].active_high =
					info->pdata->gpio[j].active_high;
			if (info->gpio[type].active_high)
				flags = GPIOF_OUT_INIT_LOW;
			else
				flags = GPIOF_OUT_INIT_HIGH;
		}
		if (!info->pdata->gpio[j].init_en)
			continue;

		snprintf(label, sizeof(label), "bu64291_%u_%s",
			 info->pdata->num, bu64291_gpio[i].label);
		err = gpio_request_one(info->gpio[type].gpio, flags, label);
		if (err) {
			dev_err(&info->i2c_client->dev, "%s ERR %s %u\n",
				__func__, label, info->gpio[type].gpio);
		} else {
			info->gpio[type].own = true;
			dev_dbg(&info->i2c_client->dev, "%s %s %u\n",
				__func__, label, info->gpio[type].gpio);
		}
	}
}

static int bu64291_pm_wr(struct bu64291_info *info, int pwr)
{
	int err = 0;
	if ((info->pdata->cfg & (NVC_CFG_OFF2STDBY | NVC_CFG_BOOT_INIT)) &&
		(pwr == NVC_PWR_OFF ||
		pwr == NVC_PWR_STDBY_OFF))
			pwr = NVC_PWR_STDBY;

	if (pwr == info->pwr_dev)
           return 0;

   
		

	switch (pwr) {
	case NVC_PWR_OFF_FORCE:
	case NVC_PWR_OFF:
		if (info->pdata && info->pdata->power_off)
			info->pdata->power_off(&info->power);
		break;
	case NVC_PWR_STDBY_OFF:
	case NVC_PWR_STDBY:
		if (info->pdata && info->pdata->power_off)
			info->pdata->power_off(&info->power);
		break;
	case NVC_PWR_COMM:
	case NVC_PWR_ON:
		if (info->pdata && info->pdata->power_on)
			info->pdata->power_on(&info->power);
		break;
	default:
		err = -EINVAL;
		break;
	}

	if (err < 0) {
		dev_err(&info->i2c_client->dev, "%s err %d\n", __func__, err);
		pwr = NVC_PWR_ERR;
	}

	info->pwr_dev = pwr;
	dev_dbg(&info->i2c_client->dev, "%s pwr_dev=%d\n", __func__,
		info->pwr_dev);

	if (err > 0)
		return 0;

	return err;
}

static int bu64291_power_put(struct bu64291_power_rail *pw)
{
	if (unlikely(!pw))
		return -EFAULT;

	if (likely(pw->vdd))
		regulator_put(pw->vdd);

	if (likely(pw->vdd_i2c))
		regulator_put(pw->vdd_i2c);

	pw->vdd = NULL;
	pw->vdd_i2c = NULL;

	return 0;
}

static int bu64291_regulator_get(struct bu64291_info *info,
	struct regulator **vreg, char vreg_name[])
{
	struct regulator *reg = NULL;
	int err = 0;

	reg = regulator_get(&info->i2c_client->dev, vreg_name);
	if (unlikely(IS_ERR(reg))) {
		dev_err(&info->i2c_client->dev, "%s %s ERR: %d\n",
			__func__, vreg_name, (int)reg);
		err = PTR_ERR(reg);
		reg = NULL;
	} else
		dev_dbg(&info->i2c_client->dev, "%s: %s\n",
			__func__, vreg_name);

	*vreg = reg;
	return err;
}

static int bu64291_power_get(struct bu64291_info *info)
{
	struct bu64291_power_rail *pw = &info->power;

	bu64291_regulator_get(info, &pw->vdd, "vdd");
	bu64291_regulator_get(info, &pw->vdd_i2c, "vdd_i2c");

	return 0;
}

static int bu64291_pm_dev_wr(struct bu64291_info *info, int pwr)
{
	if (pwr < info->pwr_dev)
		pwr = info->pwr_dev;  // to do
     
	return bu64291_pm_wr(info, pwr);
}

static void bu64291_pm_exit(struct bu64291_info *info)
{
	bu64291_pm_wr(info, NVC_PWR_OFF_FORCE);
	bu64291_power_put(&info->power);
	bu64291_gpio_exit(info);
}

static void bu64291_pm_init(struct bu64291_info *info)
{
	bu64291_gpio_init(info);
	bu64291_power_get(info);
}

static int bu64291_reset(struct bu64291_info *info, u32 level)
{
	int err = 0;

	if (level == NVC_RESET_SOFT) {
		dev_err(&info->i2c_client->dev, "%s SW reset not supported\n",
			__func__);
		err = -EINVAL;
	}
	else
		err = bu64291_pm_wr(info, NVC_PWR_OFF_FORCE);

	return err;
}

static int bu64291_dev_id(struct bu64291_info *info)
{
	u16 val = 0;
	int err;

	bu64291_pm_dev_wr(info, NVC_PWR_COMM);
	err = bu64291_i2c_rd16(info, &val);
	if (!err) {
		dev_info(&info->i2c_client->dev, "%s found device\n",
			__func__);
		info->id_minor = 0;
	}

	bu64291_pm_dev_wr(info, NVC_PWR_OFF);
	return err;
}

/**
 * Below are device specific functions.
 */

static int bu64291_position_rd(struct bu64291_info *info, unsigned *position)
{

	u16 pos = 0;
	int err = 0;
	err = bu64291_i2c_rd16(info, &pos);
	pos = pos  & 0x3FF;

	if (pos < info->config.pos_low)
		pos = info->config.pos_low;
	else if (pos > info->config.pos_high)
		pos = info->config.pos_high;

	*position = pos;

	return err;
}

static int bu64291_position_wr(struct bu64291_info *info, s32 position)
{
	int err;
	//s16 data;
	u8 reg;
	if (position < info->config.pos_low || position > info->config.pos_high)
		return -EINVAL;

       #if 0
	err = bu64291_i2c_wr16(info, 0xECA3);
	if (err)
		goto bu64291_set_position_fail;

	err = bu64291_i2c_wr16(info, 0xF200|(0x0F<<3));
	if (err)
		goto bu64291_set_position_fail;

	err = bu64291_i2c_wr16(info, 0xDC51);
	if (err)
		goto bu64291_set_position_fail;

	data = ((position & 0x3FF) << 4) |
		(0x3 << 2) |
		(0x0 << 0);
	err = bu64291_i2c_wr16(info, data);
	if (err)
		goto bu64291_set_position_fail;
       #endif
      
       reg = ((position >> 8) & 0x03) |0xF4;  // only for send focus target data
       position = position &0xff;
       err = bu64291_i2c_wr8(info, reg, position);
	 if (err ) {
		goto bu64291_set_position_fail;
	} 

	return 0;

bu64291_set_position_fail:
	dev_err(&info->i2c_client->dev, "[CAM] BU64291: %s: set position failed\n", __func__);
	return err;
}
extern int g_tmd2771x_lux;
extern int g_ov12830_nightmode;

static void bu64291_get_focuser_capabilities(struct bu64291_info *info)
{
	memset(&info->nv_config, 0, sizeof(info->nv_config));
	info->nv_config.focal_length = info->nvc.focal_length;
	info->nv_config.fnumber = info->nvc.fnumber;
	info->nv_config.max_aperture = info->nvc.fnumber;
	info->nv_config.range_ends_reversed = 0;

	info->nv_config.pos_working_low = info->config.pos_low;
	info->nv_config.pos_working_high = info->config.pos_high;

	info->nv_config.pos_actual_low = info->config.pos_low;
	info->nv_config.pos_actual_high = info->config.pos_high;

	info->nv_config.slew_rate = info->cap.slew_rate;
	info->nv_config.circle_of_confusion = -1;
	info->nv_config.num_focuser_sets = 1;
	info->nv_config.focuser_set[0].macro = info->cap.focus_macro;
	info->nv_config.focuser_set[0].hyper = info->cap.focus_hyper;
	info->nv_config.focuser_set[0].inf = info->cap.focus_infinity;
	info->nv_config.focuser_set[0].settle_time = info->cap.settle_time;
	if(g_ov12830_nightmode == 1)
	{
		info->nv_config.focuser_set[0].inf_offset = (-136);
		info->nv_config.focuser_set[0].macro_offset = (-396);	
	}
	else
	{
		info->nv_config.focuser_set[0].inf_offset = info->cap.inf_offset;
		info->nv_config.focuser_set[0].macro_offset = info->cap.macro_offset;	
	}

	printk(KERN_ERR" af_debug: %s,bu64291_macro is %d,bu64291_infinity is %d  tmd2771x_lux %d g_ov12830_nightmode %d\n",__func__,info->nv_config.focuser_set[0].macro_offset, info->cap.inf_offset, g_tmd2771x_lux, g_ov12830_nightmode);
	
}

static int bu64291_set_focuser_capabilities(struct bu64291_info *info,
					struct nvc_param *params)
{
	if (copy_from_user(&info->nv_config,
		(const void __user *)params->p_value,
		sizeof(struct nv_focuser_config))) {
			dev_err(&info->i2c_client->dev,
			"%s Error: copy_from_user bytes %d\n",
			__func__, sizeof(struct nv_focuser_config));
			return -EFAULT;
	}

	/* set pre-set value, as currently ODM sets incorrect value */
	info->cap.settle_time = BU64291_SETTLETIME;

	dev_dbg(&info->i2c_client->dev,
		"%s: copy_from_user bytes %d info->cap.settle_time %d\n",
		__func__, sizeof(struct nv_focuser_config),
		info->cap.settle_time);

	return 0;
}

static int bu64291_param_rd(struct bu64291_info *info, unsigned long arg)
{
	struct nvc_param params;
	const void *data_ptr = NULL;
	u32 data_size = 0;
	u32 position;
	int err;
	if (copy_from_user(&params,
		(const void __user *)arg,
		sizeof(struct nvc_param))) {
		dev_err(&info->i2c_client->dev, "%s %d copy_from_user err\n",
			__func__, __LINE__);
		return -EFAULT;
	}
	if (info->s_mode == NVC_SYNC_SLAVE)
		info = info->s_info;
	switch (params.param) {
	case NVC_PARAM_LOCUS:
		err = bu64291_position_rd(info, &position);
		if (err && !(info->pdata->cfg & NVC_CFG_NOERR))
			return err;
		data_ptr = &position;
		data_size = sizeof(position);
		dev_dbg(&info->i2c_client->dev, "%s LOCUS: %d\n",
			__func__, position);
		break;
	case NVC_PARAM_FOCAL_LEN:
		info->nvc.focal_length = BU64291_FOCAL_LENGTH;
		data_ptr = &info->nvc.focal_length;
		data_size = sizeof(info->nvc.focal_length);
		break;
	case NVC_PARAM_MAX_APERTURE:
		data_ptr = &info->nvc.max_aperature;
		data_size = sizeof(info->nvc.max_aperature);
		dev_dbg(&info->i2c_client->dev, "%s MAX_APERTURE: %x\n",
				__func__, info->nvc.max_aperature);
		break;
	case NVC_PARAM_FNUMBER:
		data_ptr = &info->nvc.fnumber;
		data_size = sizeof(info->nvc.fnumber);
		dev_dbg(&info->i2c_client->dev, "%s FNUMBER: %u\n",
				__func__, info->nvc.fnumber);
		break;
	case NVC_PARAM_CAPS:
		/* send back just what's requested or our max size */
		bu64291_get_focuser_capabilities(info);
		data_ptr = &info->nv_config;
		data_size = sizeof(info->nv_config);
		dev_err(&info->i2c_client->dev, "%s CAPS\n", __func__);
		break;
	case NVC_PARAM_STS:
		/*data_ptr = &info->sts;
		data_size = sizeof(info->sts);*/
		dev_dbg(&info->i2c_client->dev, "%s\n", __func__);
		break;
	case NVC_PARAM_STEREO:
		data_ptr = &info->s_mode;
		data_size = sizeof(info->s_mode);
		dev_err(&info->i2c_client->dev, "%s STEREO: %d\n", __func__,
			info->s_mode);
		break;
	default:
		dev_err(&info->i2c_client->dev,
			"%s unsupported parameter: %d\n",
			__func__, params.param);
		return -EINVAL;
	}
	if (params.sizeofvalue < data_size) {
		dev_err(&info->i2c_client->dev,
			"%s data size mismatch %d != %d Param: %d\n",
			__func__, params.sizeofvalue, data_size, params.param);
		return -EINVAL;
	}
	if (copy_to_user((void __user *)params.p_value, data_ptr, data_size)) {
		dev_err(&info->i2c_client->dev, "%s copy_to_user err line %d\n",
			__func__, __LINE__);
		return -EFAULT;
	}
	return 0;
}

static int bu64291_param_wr_s(struct bu64291_info *info,
		struct nvc_param *params, s32 s32val)
{
	int err = 0;

	switch (params->param) {
	case NVC_PARAM_LOCUS:
		dev_dbg(&info->i2c_client->dev, "%s LOCUS: %d\n",
			__func__, s32val);
		err = bu64291_position_wr(info, s32val);
		return err;
	case NVC_PARAM_RESET:
		err = bu64291_reset(info, s32val);
		dev_dbg(&info->i2c_client->dev, "%s RESET: %d\n",
			__func__, err);
		return err;
	case NVC_PARAM_SELF_TEST:
		err = 0;
		dev_dbg(&info->i2c_client->dev, "%s SELF_TEST: %d\n",
			__func__, err);
		return err;
	default:
		dev_dbg(&info->i2c_client->dev,
			"%s unsupported parameter: %d\n",
			__func__, params->param);
		return -EINVAL;
	}
}

static int bu64291_param_wr(struct bu64291_info *info, unsigned long arg)
{
	struct nvc_param params;
	u8 u8val;
	s32 s32val;
	int err = 0;
	if (copy_from_user(&params, (const void __user *)arg,
		sizeof(struct nvc_param))) {
		dev_err(&info->i2c_client->dev,
			"%s copy_from_user err line %d\n",
			__func__, __LINE__);
		return -EFAULT;
	}
	if (copy_from_user(&s32val,
		(const void __user *)params.p_value, sizeof(s32val))) {
		dev_err(&info->i2c_client->dev, "%s %d copy_from_user err\n",
			__func__, __LINE__);
		return -EFAULT;
	}
	u8val = (u8)s32val;
	/* parameters independent of sync mode */
	switch (params.param) {
	case NVC_PARAM_STEREO:
		dev_dbg(&info->i2c_client->dev, "%s STEREO: %d\n",
			__func__, u8val);
		if (u8val == info->s_mode)
			return 0;
		switch (u8val) {
		case NVC_SYNC_OFF:
			info->s_mode = u8val;
			break;
		case NVC_SYNC_MASTER:
			info->s_mode = u8val;
			break;
		case NVC_SYNC_SLAVE:
			if (info->s_info != NULL) {
				/* default slave lens position */
				err = bu64291_position_wr(info->s_info,
					info->s_info->cap.focus_infinity);
				if (!err) {
					info->s_mode = u8val;
					info->s_info->s_mode = u8val;
				}
				else {
					if (info->s_mode != NVC_SYNC_STEREO)
						bu64291_pm_wr(info->s_info,
						NVC_PWR_OFF);
						err = -EIO;
				}
			} else {
				err = -EINVAL;
			}
			break;
		case NVC_SYNC_STEREO:
			if (info->s_info != NULL) {
				/* sync power */
				info->s_info->pwr_dev = info->pwr_dev;
				/* move slave lens to master position */
				err = bu64291_position_wr(info->s_info,
					(s32)info->pos);
				if (!err) {
					info->s_mode = u8val;
					info->s_info->s_mode = u8val;
				}
				else {
					if (info->s_mode != NVC_SYNC_SLAVE)
						bu64291_pm_wr(info->s_info,
							NVC_PWR_OFF);
					err = -EIO;
				}
			} else {
				err = -EINVAL;
			}
			break;
		default:
			err = -EINVAL;
		}
		if (info->pdata->cfg & NVC_CFG_NOERR)
			return 0;
		return err;

	case NVC_PARAM_CAPS:
		if (bu64291_set_focuser_capabilities(info, &params)) {
			dev_err(&info->i2c_client->dev,
				"%s: Error: copy_from_user bytes %d\n",
				__func__, params.sizeofvalue);
			return -EFAULT;
		}
		return 0;

	default:
		/* parameters dependent on sync mode */
		switch (info->s_mode) {
		case NVC_SYNC_OFF:
		case NVC_SYNC_MASTER:
			return bu64291_param_wr_s(info, &params, s32val);
		case NVC_SYNC_SLAVE:
			return bu64291_param_wr_s(info->s_info, &params, s32val);
		case NVC_SYNC_STEREO:
			err = bu64291_param_wr_s(info, &params, s32val);
			if (!(info->pdata->cfg & NVC_CFG_SYNC_I2C_MUX))
				err |= bu64291_param_wr_s(info->s_info,
						&params,
						s32val);
			return err;
		default:
			dev_err(&info->i2c_client->dev, "%s %d internal err\n",
					__func__, __LINE__);
			return -EINVAL;
		}
	}
}

static long bu64291_ioctl(struct file *file,
					unsigned int cmd,
					unsigned long arg)
{
	struct bu64291_info *info = file->private_data;
	int pwr;
	int err = 0;
	switch (cmd) {
	case NVC_IOCTL_PARAM_WR:
		bu64291_pm_dev_wr(info, NVC_PWR_ON);
		err = bu64291_param_wr(info, arg);
		bu64291_pm_dev_wr(info, NVC_PWR_OFF);
		return err;
	case NVC_IOCTL_PARAM_RD:
		bu64291_pm_dev_wr(info, NVC_PWR_ON);
		err = bu64291_param_rd(info, arg);
		bu64291_pm_dev_wr(info, NVC_PWR_OFF);
		return err;
	case NVC_IOCTL_PWR_WR:
		/* This is a Guaranteed Level of Service (GLOS) call */
		pwr = (int)arg * 2;
		dev_dbg(&info->i2c_client->dev, "%s PWR_WR: %d\n",
				__func__, pwr);
		err = bu64291_pm_dev_wr(info, pwr);
		return err;
	case NVC_IOCTL_PWR_RD:
		if (info->s_mode == NVC_SYNC_SLAVE)
			pwr = info->s_info->pwr_dev;
		else
			pwr = info->pwr_dev;
		dev_dbg(&info->i2c_client->dev, "%s PWR_RD: %d\n",
				__func__, pwr);
		if (copy_to_user((void __user *)arg,
			(const void *)&pwr, sizeof(pwr))) {
			dev_err(&info->i2c_client->dev,
				"%s copy_to_user err line %d\n",
				__func__, __LINE__);
			return -EFAULT;
		}
		return 0;
	default:
		dev_dbg(&info->i2c_client->dev, "%s unsupported ioctl: %x\n",
			__func__, cmd);
	}
	return -EINVAL;
}


static void bu64291_sdata_init(struct bu64291_info *info)
{
	printk(KERN_ERR" af_debug: %s,bu64291_macro is %d,bu64291_infinity is %d \n",__func__,Bu64291_macro, Bu64291_infinity);
	bu64291_default_cap.focus_macro = Bu64291_macro;
	bu64291_default_cap.focus_infinity = Bu64291_infinity;
       bu64291_default_cap.focus_hyper = Bu64291_infinity;
	printk(KERN_ERR" af_debug: %s,after doing offest bu64291_macro is %d,bu64291_infinity is %d \n",__func__,bu64291_default_cap.focus_macro, 
	bu64291_default_cap.focus_infinity);
	/* set defaults */
	memcpy(&info->config, &bu64291_default_info, sizeof(info->config));
	memcpy(&info->nvc, &bu64291_default_nvc, sizeof(info->nvc));
	memcpy(&info->cap, &bu64291_default_cap, sizeof(info->cap));
     printk("%s,info->cap.focus_macro is %d,info->cap.focus_infinity is %d \n",__func__,info->cap.focus_macro,info->cap.focus_infinity);

	info->config.settle_time = BU64291_SETTLETIME;
	info->config.focal_length = BU64291_FOCAL_LENGTH_FLOAT;
	info->config.fnumber = BU64291_FNUMBER_FLOAT;
	info->config.pos_low = BU64291_POS_LOW_DEFAULT;
	info->config.pos_high = BU64291_POS_HIGH_DEFAULT;

	/* set to proper value */
	info->cap.actuator_range = info->config.pos_high - info->config.pos_low;

	/* set overrides if any */
	if (info->pdata->nvc) {
		if (info->pdata->nvc->fnumber)
			info->nvc.fnumber = info->pdata->nvc->fnumber;
		if (info->pdata->nvc->focal_length)
			info->nvc.focal_length = info->pdata->nvc->focal_length;
		if (info->pdata->nvc->max_aperature)
			info->nvc.max_aperature =
				info->pdata->nvc->max_aperature;
	}

	if (info->pdata->cap) {
		if (info->pdata->cap->actuator_range)
			info->cap.actuator_range =
				info->pdata->cap->actuator_range;
		if (info->pdata->cap->settle_time)
			info->cap.settle_time = info->pdata->cap->settle_time;
		if (info->pdata->cap->focus_macro)
			info->cap.focus_macro = info->pdata->cap->focus_macro;
		if (info->pdata->cap->focus_hyper)
			info->cap.focus_hyper = info->pdata->cap->focus_hyper;
		if (info->pdata->cap->focus_infinity)
			info->cap.focus_infinity =
				info->pdata->cap->focus_infinity;
	}
}

static int bu64291_sync_en(unsigned num, unsigned sync)
{
	struct bu64291_info *master = NULL;
	struct bu64291_info *slave = NULL;
	struct bu64291_info *pos = NULL;
	rcu_read_lock();
	list_for_each_entry_rcu(pos, &bu64291_info_list, list) {
		if (pos->pdata->num == num) {
			master = pos;
			break;
		}
	}
	pos = NULL;
	list_for_each_entry_rcu(pos, &bu64291_info_list, list) {
		if (pos->pdata->num == sync) {
			slave = pos;
			break;
		}
	}
	rcu_read_unlock();
	if (master != NULL)
		master->s_info = NULL;
	if (slave != NULL)
		slave->s_info = NULL;
	if (!sync)
		return 0; /* no err if sync disabled */
	if (num == sync)
		return -EINVAL; /* err if sync instance is itself */
	if ((master != NULL) && (slave != NULL)) {
		master->s_info = slave;
		slave->s_info = master;
	}
	return 0;
}

static int bu64291_sync_dis(struct bu64291_info *info)
{
	if (info->s_info != NULL) {
		info->s_info->s_mode = 0;
		info->s_info->s_info = NULL;
		info->s_mode = 0;
		info->s_info = NULL;
		return 0;
	}
	return -EINVAL;
}
/* yangchunni test start */
#if 0
//Romh setting
static int bu64291_chip_init(struct bu64291_info *info)
{
	int err = 0;

	err = bu64291_i2c_wr8(info, 0xcc, 0x2b);
	if (err)
		goto bu64291_init_fail;

	err = bu64291_i2c_wr8(info, 0xd4,0x96);
	if (err)
		goto bu64291_init_fail;
	
	err = bu64291_i2c_wr8(info, 0xdd, 0x36);
	if (err)
		goto bu64291_init_fail;

	err = bu64291_i2c_wr8(info, 0xe4, 0xe3);
	if (err)
		goto bu64291_init_fail;
       
	return 0;

bu64291_init_fail:
	dev_err(&info->i2c_client->dev, "[CAM] BU64291: %s: bu64291_chip_initfailed \n", __func__);
	return err;

}
#else
// nv setting
static int bu64291_chip_init(struct bu64291_info *info)
{
int err = 0;
int point_b_value = Bu64291_infinity + INF_OFFSET - 1;
int point_a_value = point_b_value - 40;
dev_err(&info->i2c_client->dev, " %s point_b_value = %d\n", __func__, point_b_value);

	err = bu64291_i2c_wr8(info, 0x8c, 0x2b);
if (err)
goto bu64291_init_fail;

	err = bu64291_i2c_wr8(info, 0x94 |( 0x3 & (point_a_value >>8)), point_a_value & 0xff);
if (err)
goto bu64291_init_fail;
	err = bu64291_i2c_wr8(info, 0x9c |( 0x3 & (point_b_value >>8)), point_b_value & 0xff);
if (err)
goto bu64291_init_fail;
	err = bu64291_i2c_wr8(info, 0xa4, 0x64);
if (err)
goto bu64291_init_fail;

return 0;

bu64291_init_fail:
dev_err(&info->i2c_client->dev, "[CAM] BU64291: %s: set position failed\n", __func__);
return err;

} 
#endif
/* yangchunni test end */
static int bu64291_open(struct inode *inode, struct file *file)
{
	struct bu64291_info *info = NULL;
	struct bu64291_info *pos = NULL;
	int err;
	rcu_read_lock();
	list_for_each_entry_rcu(pos, &bu64291_info_list, list) {
		if (pos->miscdev.minor == iminor(inode)) {
			info = pos;
			break;
		}
	}
	rcu_read_unlock();
	if (!info)
		return -ENODEV;
	err = bu64291_sync_en(info->pdata->num, info->pdata->sync);
	if (err == -EINVAL)
		dev_err(&info->i2c_client->dev,
			"%s err: invalid num (%u) and sync (%u) instance\n",
			__func__, info->pdata->num, info->pdata->sync);
	if (atomic_xchg(&info->in_use, 1))
		return -EBUSY;
	if (info->s_info != NULL) {
		if (atomic_xchg(&info->s_info->in_use, 1))
			return -EBUSY;
	}
	file->private_data = info;
  

	bu64291_pm_dev_wr(info, NVC_PWR_ON);
	bu64291_chip_init(info);
   
	bu64291_position_wr(info, info->cap.focus_infinity);
     
	bu64291_pm_dev_wr(info, NVC_PWR_OFF);
   
	dev_dbg(&info->i2c_client->dev, "%s\n", __func__);

	return 0;
}

static int bu64291_release(struct inode *inode, struct file *file)
{
	struct bu64291_info *info = file->private_data;
	dev_dbg(&info->i2c_client->dev, "%s\n", __func__);
	bu64291_pm_dev_wr(info, NVC_PWR_OFF);
	file->private_data = NULL;
	WARN_ON(!atomic_xchg(&info->in_use, 0));
	if (info->s_info != NULL)
		WARN_ON(!atomic_xchg(&info->s_info->in_use, 0));
	bu64291_sync_dis(info);
	return 0;
}

static const struct file_operations bu64291_fileops = {
	.owner = THIS_MODULE,
	.open = bu64291_open,
	.unlocked_ioctl = bu64291_ioctl,
	.release = bu64291_release,
};

static void bu64291_del(struct bu64291_info *info)
{
	bu64291_pm_exit(info);
	if ((info->s_mode == NVC_SYNC_SLAVE) ||
		(info->s_mode == NVC_SYNC_STEREO))
		bu64291_pm_exit(info->s_info);

	bu64291_sync_dis(info);
	spin_lock(&bu64291_spinlock);
	list_del_rcu(&info->list);
	spin_unlock(&bu64291_spinlock);
	synchronize_rcu();
}

static int bu64291_remove(struct i2c_client *client)
{
	struct bu64291_info *info = i2c_get_clientdata(client);
	dev_dbg(&info->i2c_client->dev, "%s\n", __func__);
	misc_deregister(&info->miscdev);
	bu64291_del(info);
	return 0;
}

static int bu64291_probe(
		struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct bu64291_info *info;
	char dname[16];
	int err;

	dev_dbg(&client->dev, "%s\n", __func__);
	pr_info("bu64291: probing focuser.\n");

	info = devm_kzalloc(&client->dev, sizeof(*info), GFP_KERNEL);
	if (info == NULL) {
		dev_err(&client->dev, "%s: kzalloc error\n", __func__);
		return -ENOMEM;
	}
	info->i2c_client = client;
	if (client->dev.platform_data) {
		info->pdata = client->dev.platform_data;
	} else {
		info->pdata = &bu64291_default_pdata;
		dev_dbg(&client->dev, "%s No platform data.  Using defaults.\n",
			__func__);
	}

	i2c_set_clientdata(client, info);
	INIT_LIST_HEAD(&info->list);
	spin_lock(&bu64291_spinlock);
	list_add_rcu(&info->list, &bu64291_info_list);
	spin_unlock(&bu64291_spinlock);
	bu64291_pm_init(info);
	bu64291_sdata_init(info);

	if (info->pdata->cfg & (NVC_CFG_NODEV | NVC_CFG_BOOT_INIT)) {
		err = bu64291_dev_id(info);
		if (err < 0) {
			dev_err(&client->dev, "%s device not found\n",
				__func__);
			bu64291_pm_wr(info, NVC_PWR_OFF);
			if (info->pdata->cfg & NVC_CFG_NODEV) {
				bu64291_del(info);
				return -ENODEV;
			}
		} else {
			dev_dbg(&client->dev, "%s device found\n", __func__);
			if (info->pdata->cfg & NVC_CFG_BOOT_INIT) {
				/* initial move causes full initialization */
				bu64291_pm_dev_wr(info, NVC_PWR_ON);
				bu64291_position_wr(info,
					info->cap.focus_infinity);
				bu64291_pm_dev_wr(info, NVC_PWR_OFF);
			}
		}
	}

	if (info->pdata->dev_name != 0)
		strcpy(dname, info->pdata->dev_name);
	else
		strcpy(dname, "bu64291");

	if (info->pdata->num)
		snprintf(dname, sizeof(dname),
			"%s.%u", dname, info->pdata->num);

	info->miscdev.name = dname;
	info->miscdev.fops = &bu64291_fileops;
	info->miscdev.minor = MISC_DYNAMIC_MINOR;
	if (misc_register(&info->miscdev)) {
		dev_err(&client->dev, "%s unable to register misc device %s\n",
			__func__, dname);
		bu64291_del(info);
		return -ENODEV;
	}

	return 0;
}


static const struct i2c_device_id bu64291_id[] = {
	{ "bu64291", 0 },
	{ },
};


MODULE_DEVICE_TABLE(i2c, bu64291_id);

static struct i2c_driver bu64291_i2c_driver = {
	.driver = {
		.name = "bu64291",
		.owner = THIS_MODULE,
	},
	.id_table = bu64291_id,
	.probe = bu64291_probe,
	.remove = bu64291_remove,
};

static int __init bu64291_init(void)
{
	return i2c_add_driver(&bu64291_i2c_driver);
}

static void __exit bu64291_exit(void)
{
	i2c_del_driver(&bu64291_i2c_driver);
}

module_init(bu64291_init);
module_exit(bu64291_exit);
MODULE_LICENSE("GPL");

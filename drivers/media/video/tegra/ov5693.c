/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <media/ov5693.h>

#define OV5693_ID			0x5693
#define OV5693_SENSOR_TYPE		NVC_IMAGER_TYPE_RAW
#define OV5693_RES_CHG_WAIT_TIME_MS	100
#define OV5693_SIZEOF_I2C_BUF		16
#define OV5693_TABLE_WAIT_MS		0
#define OV5693_TABLE_END		1
#define OV5693_TABLE_RESET		2
#define OV5693_TABLE_RESET_TIMEOUT	50
#define OV5693_LENS_MAX_APERTURE	0	/* _INT2FLOAT_DIVISOR */
#define OV5693_LENS_FNUMBER		0	/* _INT2FLOAT_DIVISOR */
#define OV5693_LENS_FOCAL_LENGTH	6120	/* _INT2FLOAT_DIVISOR */
#define OV5693_LENS_VIEW_ANGLE_H	60000	/* _INT2FLOAT_DIVISOR */
#define OV5693_LENS_VIEW_ANGLE_V	60000	/* _INT2FLOAT_DIVISOR */

static struct nvc_gpio_init ov5693_gpio[] = {
	{ OV5693_GPIO_TYPE_PWRDN, GPIOF_OUT_INIT_LOW, "pwrdn", false, true, },
};

struct ov5693_info {
	atomic_t in_use;
	struct i2c_client *i2c_client;
	struct ov5693_platform_data *pdata;
	struct miscdevice miscdev;
	int pwr_api;
	int pwr_dev;
	struct nvc_gpio gpio[ARRAY_SIZE(ov5693_gpio)];
	struct ov5693_power_rail regulators;
	bool power_on;
	u32 mode_index;
	bool mode_valid;
	bool mode_enable;
	unsigned test_pattern;
	struct nvc_imager_static_nvc sdata;
	u8 bin_en;
	struct ov5693_fuseid fuseid;
	struct regmap *regmap;
	struct regulator *ext_vcm_vdd;
};

struct ov5693_reg {
	u16 addr;
	u16 val;
};

struct ov5693_mode_data {
	struct nvc_imager_mode sensor_mode;
	struct nvc_imager_dynamic_nvc sensor_dnvc;
	struct ov5693_reg *p_mode_i2c;
};

static struct ov5693_platform_data ov5693_dflt_pdata = {
	.cfg		= 0,
	.num		= 0,
	.dev_name	= "camera",
};

/*
 * NOTE: static vs dynamic
 * If a member in the nvc_imager_static_nvc structure is not actually
 * static data, then leave blank and add the parameter to the parameter
 * read function that dynamically reads the data.  The NVC user driver
 * will call the parameter read for the data if the member data is 0.
 * If the dynamic data becomes static during probe (a one time read
 * such as device ID) then add the dynamic read to the _sdata_init
 * function.
 */
static struct nvc_imager_static_nvc ov5693_dflt_sdata = {
	.api_version		= NVC_IMAGER_API_STATIC_VER,
	.sensor_type		= OV5693_SENSOR_TYPE,
	.bits_per_pixel		= 10,
	.sensor_id		= OV5693_ID,
	.sensor_id_minor	= 0,
	.focal_len		= OV5693_LENS_FOCAL_LENGTH,
	.max_aperture		= OV5693_LENS_MAX_APERTURE,
	.fnumber		= OV5693_LENS_FNUMBER,
	.view_angle_h		= OV5693_LENS_VIEW_ANGLE_H,
	.view_angle_v		= OV5693_LENS_VIEW_ANGLE_V,
	.res_chg_wait_time	= OV5693_RES_CHG_WAIT_TIME_MS,
};

static const struct ov5693_reg ov5693_2592x1944_i2c[] = {
	{OV5693_TABLE_RESET, 0},/* Including sw reset */
	{0x3001, 0x0a},
	{0x3002, 0x80},
	{0x3006, 0x00},
	{0x3011, 0x21},
	{0x3012, 0x09},
	{0x3013, 0x10},
	{0x3014, 0x00},
	{0x3015, 0x08},
	{0x3016, 0xf0},
	{0x3017, 0xf0},
	{0x3018, 0xf0},
	{0x301b, 0xb4},
	{0x301d, 0x02},
	{0x3021, 0x00},
	{0x3022, 0x01},
	{0x3028, 0x44},
	{0x3090, 0x02},
	{0x3091, 0x0e},
	{0x3092, 0x00},
	{0x3093, 0x00},
	{0x3098, 0x03},
	{0x3099, 0x1e},
	{0x309a, 0x02},
	{0x309b, 0x01},
	{0x309c, 0x00},
	{0x30a0, 0xd2},
	{0x30a2, 0x01},
	{0x30b2, 0x00},
	{0x30b3, 0x68},
	{0x30b4, 0x03},
	{0x30b5, 0x04},
	{0x30b6, 0x01},
	{0x3104, 0x21},
	{0x3106, 0x00},
	{0x3400, 0x04},
	{0x3401, 0x00},
	{0x3402, 0x04},
	{0x3403, 0x00},
	{0x3404, 0x04},
	{0x3405, 0x00},
	{0x3406, 0x01},
	{0x3500, 0x00},
	{0x3501, 0x7b},
	{0x3502, 0x00},
	{0x3503, 0x07},
	{0x3504, 0x00},
	{0x3505, 0x00},
	{0x3506, 0x00},
	{0x3507, 0x02},
	{0x3508, 0x00},
	{0x3509, 0x08},
	{0x350a, 0x00},
	{0x350b, 0x40},
	{0x3601, 0x0a},
	{0x3602, 0x18},
	{0x3612, 0x80},
	{0x3620, 0x54},
	{0x3621, 0xc7},
	{0x3622, 0x0f},
	{0x3625, 0x10},
	{0x3630, 0x55},
	{0x3631, 0xf4},
	{0x3632, 0x00},
	{0x3633, 0x34},
	{0x3634, 0x02},
	{0x364d, 0x0d},
	{0x364f, 0xdd},
	{0x3660, 0x04},
	{0x3662, 0x10},
	{0x3663, 0xf1},
	{0x3665, 0x00},
	{0x3666, 0x20},
	{0x3667, 0x00},
	{0x366a, 0x80},
	{0x3680, 0xe0},
	{0x3681, 0x00},
	{0x3700, 0x42},
	{0x3701, 0x14},
	{0x3702, 0xa0},
	{0x3703, 0xd8},
	{0x3704, 0x78},
	{0x3705, 0x02},
	{0x3708, 0xe2},
	{0x3709, 0xc3},
	{0x370a, 0x00},
	{0x370b, 0x20},
	{0x370c, 0x0c},
	{0x370d, 0x11},
	{0x370e, 0x00},
	{0x370f, 0x40},
	{0x3710, 0x00},
	{0x371a, 0x1c},
	{0x371b, 0x05},
	{0x371c, 0x01},
	{0x371e, 0xa1},
	{0x371f, 0x0c},
	{0x3721, 0x00},
	{0x3726, 0x00},
	{0x372a, 0x01},
	{0x3730, 0x10},
	{0x3738, 0x22},
	{0x3739, 0xe5},
	{0x373a, 0x50},
	{0x373b, 0x02},
	{0x373c, 0x41},
	{0x373f, 0x02},
	{0x3740, 0x42},
	{0x3741, 0x02},
	{0x3742, 0x18},
	{0x3743, 0x01},
	{0x3744, 0x02},
	{0x3747, 0x10},
	{0x374c, 0x04},
	{0x3751, 0xf0},
	{0x3752, 0x00},
	{0x3753, 0x00},
	{0x3754, 0xc0},
	{0x3755, 0x00},
	{0x3756, 0x1a},
	{0x3758, 0x00},
	{0x3759, 0x0f},
	{0x376b, 0x44},
	{0x375c, 0x04},
	{0x3776, 0x00},
	{0x377f, 0x08},
	{0x3780, 0x22},
	{0x3781, 0x0c},
	{0x3784, 0x2c},
	{0x3785, 0x1e},
	{0x378f, 0xf5},
	{0x3791, 0xb0},
	{0x3795, 0x00},
	{0x3796, 0x64},
	{0x3797, 0x11},
	{0x3798, 0x30},
	{0x3799, 0x41},
	{0x379a, 0x07},
	{0x379b, 0xb0},
	{0x379c, 0x0c},
	{0x37c5, 0x00},
	{0x37c6, 0x00},
	{0x37c7, 0x00},
	{0x37c9, 0x00},
	{0x37ca, 0x00},
	{0x37cb, 0x00},
	{0x37de, 0x00},
	{0x37df, 0x00},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x00},
	{0x3804, 0x0a},
	{0x3805, 0x3f},
	{0x3806, 0x07},
	{0x3807, 0xa3},
	{0x3808, 0x0a},
	{0x3809, 0x20},
	{0x380a, 0x07},
	{0x380b, 0x98},
	{0x380c, 0x0a},
	{0x380d, 0x80},
	{0x380e, 0x07},
	{0x380f, 0xc0},
	{0x3810, 0x00},
	{0x3811, 0x02},
	{0x3812, 0x00},
	{0x3813, 0x02},
	{0x3814, 0x11},
	{0x3815, 0x11},
	{0x3820, 0x00},
	{0x3821, 0x1e},
	{0x3823, 0x00},
	{0x3824, 0x00},
	{0x3825, 0x00},
	{0x3826, 0x00},
	{0x3827, 0x00},
	{0x382a, 0x04},
	{0x3a04, 0x06},
	{0x3a05, 0x14},
	{0x3a06, 0x00},
	{0x3a07, 0xfe},
	{0x3b00, 0x00},
	{0x3b02, 0x00},
	{0x3b03, 0x00},
	{0x3b04, 0x00},
	{0x3b05, 0x00},
	{0x3d00, 0x00},
	{0x3d01, 0x00},
	{0x3d02, 0x00},
	{0x3d03, 0x00},
	{0x3d04, 0x00},
	{0x3d05, 0x00},
	{0x3d06, 0x00},
	{0x3d07, 0x00},
	{0x3d08, 0x00},
	{0x3d09, 0x00},
	{0x3d0a, 0x00},
	{0x3d0b, 0x00},
	{0x3d0c, 0x00},
	{0x3d0d, 0x00},
	{0x3d0e, 0x00},
	{0x3d0f, 0x00},
	{0x3d80, 0x00},
	{0x3d81, 0x00},
	{0x3d84, 0x00},
	{0x3e07, 0x20},
	{0x4000, 0x08},
	{0x4001, 0x04},
	{0x4002, 0x45},
	{0x4004, 0x08},
	{0x4005, 0x18},
	{0x4006, 0x20},
	{0x4008, 0x24},
	{0x4009, 0x10},
	{0x400c, 0x00},
	{0x400d, 0x00},
	{0x4058, 0x00},
	{0x4101, 0xb2},
	{0x4303, 0x00},
	{0x4304, 0x08},
	{0x4307, 0x30},
	{0x4311, 0x04},
	{0x4315, 0x01},
	{0x4511, 0x05},
	{0x4512, 0x01},
	{0x4806, 0x00},
	{0x4816, 0x52},
	{0x481f, 0x30},
	{0x4826, 0x2c},
	{0x4831, 0x64},
	{0x4d00, 0x04},
	{0x4d01, 0x71},
	{0x4d02, 0xfd},
	{0x4d03, 0xf5},
	{0x4d04, 0x0c},
	{0x4d05, 0xcc},
	{0x4837, 0x09},
	{0x5000, 0x06},
	{0x5001, 0x01},
	{0x5002, 0x00},
	{0x5003, 0x20},
	{0x5046, 0x0a},
	{0x5013, 0x00},
	{0x5046, 0x0a},
	{0x5780, 0x1c},
	{0x5786, 0x20},
	{0x5787, 0x10},
	{0x5788, 0x18},
	{0x578a, 0x04},
	{0x578b, 0x02},
	{0x578c, 0x02},
	{0x578e, 0x06},
	{0x578f, 0x02},
	{0x5790, 0x02},
	{0x5791, 0xff},
	{0x5842, 0x01},
	{0x5843, 0x2b},
	{0x5844, 0x01},
	{0x5845, 0x92},
	{0x5846, 0x01},
	{0x5847, 0x8f},
	{0x5848, 0x01},
	{0x5849, 0x0c},
	{0x5e00, 0x00},
	{0x5e10, 0x0c},
	{0x0100, 0x01},

	{OV5693_TABLE_END, 0x0000}
};

static const struct ov5693_reg ov5693_1296x972_i2c[] = {
	{OV5693_TABLE_RESET, 0},/* Including sw reset */
	{0x0103, 0x01},
	{0x3001, 0x0a},
	{0x3002, 0x80},
	{0x3006, 0x00},
	{0x3011, 0x21},
	{0x3012, 0x09},
	{0x3013, 0x10},
	{0x3014, 0x00},
	{0x3015, 0x08},
	{0x3016, 0xf0},
	{0x3017, 0xf0},
	{0x3018, 0xf0},
	{0x301b, 0xb4},
	{0x301d, 0x02},
	{0x3021, 0x00},
	{0x3022, 0x01},
	{0x3028, 0x44},
	{0x3098, 0x03},
	{0x3099, 0x1e},
	{0x309a, 0x02},
	{0x309b, 0x01},
	{0x309c, 0x00},
	{0x30a0, 0xd2},
	{0x30a2, 0x01},
	{0x30b2, 0x00},
	{0x30b3, 0x64},
	{0x30b4, 0x03},
	{0x30b5, 0x04},
	{0x30b6, 0x01},
	{0x3104, 0x21},
	{0x3106, 0x00},
	{0x3400, 0x04},
	{0x3401, 0x00},
	{0x3402, 0x04},
	{0x3403, 0x00},
	{0x3404, 0x04},
	{0x3405, 0x00},
	{0x3406, 0x01},
	{0x3500, 0x00},
	{0x3501, 0x7b},
	{0x3502, 0x00},
	{0x3503, 0x07},
	{0x3504, 0x00},
	{0x3505, 0x00},
	{0x3506, 0x00},
	{0x3507, 0x02},
	{0x3508, 0x00},
	{0x3509, 0x08},
	{0x350a, 0x00},
	{0x350b, 0x40},
	{0x3601, 0x0a},
	{0x3602, 0x38},
	{0x3612, 0x80},
	{0x3620, 0x54},
	{0x3621, 0xc7},
	{0x3622, 0x0f},
	{0x3625, 0x10},
	{0x3630, 0x55},
	{0x3631, 0xf4},
	{0x3632, 0x00},
	{0x3633, 0x34},
	{0x3634, 0x02},
	{0x364d, 0x0d},
	{0x364f, 0xdd},
	{0x3660, 0x04},
	{0x3662, 0x10},
	{0x3663, 0xf1},
	{0x3665, 0x00},
	{0x3666, 0x20},
	{0x3667, 0x00},
	{0x366a, 0x80},
	{0x3680, 0xe0},
	{0x3681, 0x00},
	{0x3700, 0x42},
	{0x3701, 0x14},
	{0x3702, 0xa0},
	{0x3703, 0xd8},
	{0x3704, 0x78},
	{0x3705, 0x02},
	{0x3708, 0xe6},
	{0x3709, 0xc3},
	{0x370a, 0x00},
	{0x370b, 0x20},
	{0x370c, 0x0c},
	{0x370d, 0x11},
	{0x370e, 0x00},
	{0x370f, 0x40},
	{0x3710, 0x00},
	{0x371a, 0x1c},
	{0x371b, 0x05},
	{0x371c, 0x01},
	{0x371e, 0xa1},
	{0x371f, 0x0c},
	{0x3721, 0x00},
	{0x3724, 0x10},
	{0x3726, 0x00},
	{0x372a, 0x01},
	{0x3730, 0x10},
	{0x3738, 0x22},
	{0x3739, 0xe5},
	{0x373a, 0x50},
	{0x373b, 0x02},
	{0x373c, 0x41},
	{0x373f, 0x02},
	{0x3740, 0x42},
	{0x3741, 0x02},
	{0x3742, 0x18},
	{0x3743, 0x01},
	{0x3744, 0x02},
	{0x3747, 0x10},
	{0x374c, 0x04},
	{0x3751, 0xf0},
	{0x3752, 0x00},
	{0x3753, 0x00},
	{0x3754, 0xc0},
	{0x3755, 0x00},
	{0x3756, 0x1a},
	{0x3758, 0x00},
	{0x3759, 0x0f},
	{0x376b, 0x44},
	{0x375c, 0x04},
	{0x3774, 0x10},
	{0x3776, 0x00},
	{0x377f, 0x08},
	{0x3780, 0x22},
	{0x3781, 0x0c},
	{0x3784, 0x2c},
	{0x3785, 0x1e},
	{0x378f, 0xf5},
	{0x3791, 0xb0},
	{0x3795, 0x00},
	{0x3796, 0x64},
	{0x3797, 0x11},
	{0x3798, 0x30},
	{0x3799, 0x41},
	{0x379a, 0x07},
	{0x379b, 0xb0},
	{0x379c, 0x0c},
	{0x37c5, 0x00},
	{0x37c6, 0x00},
	{0x37c7, 0x00},
	{0x37c9, 0x00},
	{0x37ca, 0x00},
	{0x37cb, 0x00},
	{0x37de, 0x00},
	{0x37df, 0x00},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x00},
	{0x3804, 0x0a},
	{0x3805, 0x3f},
	{0x3806, 0x07},
	{0x3807, 0xa3},
	{0x3808, 0x05},
	{0x3809, 0x10},
	{0x380a, 0x03},
	{0x380b, 0xcc},
	{0x380c, 0x0a},
	{0x380d, 0x80},
	{0x380e, 0x07},
	{0x380f, 0xc0},
	{0x3810, 0x00},
	{0x3811, 0x02},
	{0x3812, 0x00},
	{0x3813, 0x02},
	{0x3814, 0x31},
	{0x3815, 0x31},
	{0x3820, 0x01},
	{0x3821, 0x1f},
	{0x3823, 0x00},
	{0x3824, 0x00},
	{0x3825, 0x00},
	{0x3826, 0x00},
	{0x3827, 0x00},
	{0x382a, 0x04},
	{0x3a04, 0x06},
	{0x3a05, 0x14},
	{0x3a06, 0x00},
	{0x3a07, 0xfe},
	{0x3b00, 0x00},
	{0x3b02, 0x00},
	{0x3b03, 0x00},
	{0x3b04, 0x00},
	{0x3b05, 0x00},
	{0x3e07, 0x20},
	{0x4000, 0x08},
	{0x4001, 0x04},
	{0x4002, 0x45},
	{0x4004, 0x08},
	{0x4005, 0x18},
	{0x4006, 0x20},
	{0x4008, 0x24},
	{0x4009, 0x40},
	{0x400c, 0x00},
	{0x400d, 0x00},
	{0x4058, 0x00},
	{0x404e, 0x37},
	{0x404f, 0x8f},
	{0x4058, 0x00},
	{0x4101, 0xb2},
	{0x4303, 0x00},
	{0x4304, 0x08},
	{0x4307, 0x30},
	{0x4311, 0x04},
	{0x4315, 0x01},
	{0x4511, 0x05},
	{0x4512, 0x01},
	{0x4806, 0x00},
	{0x4816, 0x52},
	{0x481f, 0x30},
	{0x4826, 0x2c},
	{0x4831, 0x64},
	{0x4d00, 0x04},
	{0x4d01, 0x71},
	{0x4d02, 0xfd},
	{0x4d03, 0xf5},
	{0x4d04, 0x0c},
	{0x4d05, 0xcc},
	{0x4837, 0x0a},
	{0x5000, 0x06},
	{0x5001, 0x01},
	{0x5002, 0x00},
	{0x5003, 0x20},
	{0x5046, 0x0a},
	{0x5013, 0x00},
	{0x5046, 0x0a},
	{0x5780, 0x1c},
	{0x5786, 0x20},
	{0x5787, 0x10},
	{0x5788, 0x18},
	{0x578a, 0x04},
	{0x578b, 0x02},
	{0x578c, 0x02},
	{0x578e, 0x06},
	{0x578f, 0x02},
	{0x5790, 0x02},
	{0x5791, 0xff},
	{0x5842, 0x01},
	{0x5843, 0x2b},
	{0x5844, 0x01},
	{0x5845, 0x92},
	{0x5846, 0x01},
	{0x5847, 0x8f},
	{0x5848, 0x01},
	{0x5849, 0x0c},
	{0x5e00, 0x00},
	{0x5e10, 0x0c},
	{0x0100, 0x01},
	{OV5693_TABLE_END, 0x0000}
};

static const struct ov5693_reg ov5693_1920x1080_i2c[] = {
	{OV5693_TABLE_RESET, 0x0},/*, 0xIncluding, 0xsw, 0xreset, 0x*/
	{0x3001, 0x0a},
	{0x3002, 0x80},
	{0x3006, 0x00},
	{0x3011, 0x21},
	{0x3012, 0x09},
	{0x3013, 0x10},
	{0x3014, 0x00},
	{0x3015, 0x08},
	{0x3016, 0xf0},
	{0x3017, 0xf0},
	{0x3018, 0xf0},
	{0x301b, 0xb4},
	{0x301d, 0x02},
	{0x3021, 0x00},
	{0x3022, 0x01},
	{0x3028, 0x44},
	{0x3098, 0x03},
	{0x3099, 0x1e},
	{0x309a, 0x02},
	{0x309b, 0x01},
	{0x309c, 0x00},
	{0x30a0, 0xd2},
	{0x30a2, 0x01},
	{0x30b2, 0x00},
	{0x30b3, 0x64},
	{0x30b4, 0x03},
	{0x30b5, 0x04},
	{0x30b6, 0x01},
	{0x3104, 0x21},
	{0x3106, 0x00},
	{0x3400, 0x04},
	{0x3401, 0x00},
	{0x3402, 0x04},
	{0x3403, 0x00},
	{0x3404, 0x04},
	{0x3405, 0x00},
	{0x3406, 0x01},
	{0x3500, 0x00},
	{0x3501, 0x7b},
	{0x3502, 0x00},
	{0x3503, 0x07},
	{0x3504, 0x00},
	{0x3505, 0x00},
	{0x3506, 0x00},
	{0x3507, 0x02},
	{0x3508, 0x00},
	{0x3509, 0x08},
	{0x350a, 0x00},
	{0x350b, 0x40},
	{0x3601, 0x0a},
	{0x3602, 0x38},
	{0x3612, 0x80},
	{0x3620, 0x54},
	{0x3621, 0xc7},
	{0x3622, 0x0f},
	{0x3625, 0x10},
	{0x3630, 0x55},
	{0x3631, 0xf4},
	{0x3632, 0x00},
	{0x3633, 0x34},
	{0x3634, 0x02},
	{0x364d, 0x0d},
	{0x364f, 0xdd},
	{0x3660, 0x04},
	{0x3662, 0x10},
	{0x3663, 0xf1},
	{0x3665, 0x00},
	{0x3666, 0x20},
	{0x3667, 0x00},
	{0x366a, 0x80},
	{0x3680, 0xe0},
	{0x3681, 0x00},
	{0x3700, 0x42},
	{0x3701, 0x14},
	{0x3702, 0xa0},
	{0x3703, 0xd8},
	{0x3704, 0x78},
	{0x3705, 0x02},
	{0x3708, 0xe2},
	{0x3709, 0xc3},
	{0x370a, 0x00},
	{0x370b, 0x20},
	{0x370c, 0x0c},
	{0x370d, 0x11},
	{0x370e, 0x00},
	{0x370f, 0x40},
	{0x3710, 0x00},
	{0x371a, 0x1c},
	{0x371b, 0x05},
	{0x371c, 0x01},
	{0x371e, 0xa1},
	{0x371f, 0x0c},
	{0x3721, 0x00},
	{0x3724, 0x10},
	{0x3726, 0x00},
	{0x372a, 0x01},
	{0x3730, 0x10},
	{0x3738, 0x22},
	{0x3739, 0xe5},
	{0x373a, 0x50},
	{0x373b, 0x02},
	{0x373c, 0x41},
	{0x373f, 0x02},
	{0x3740, 0x42},
	{0x3741, 0x02},
	{0x3742, 0x18},
	{0x3743, 0x01},
	{0x3744, 0x02},
	{0x3747, 0x10},
	{0x374c, 0x04},
	{0x3751, 0xf0},
	{0x3752, 0x00},
	{0x3753, 0x00},
	{0x3754, 0xc0},
	{0x3755, 0x00},
	{0x3756, 0x1a},
	{0x3758, 0x00},
	{0x3759, 0x0f},
	{0x376b, 0x44},
	{0x375c, 0x04},
	{0x3774, 0x10},
	{0x3776, 0x00},
	{0x377f, 0x08},
	{0x3780, 0x22},
	{0x3781, 0x0c},
	{0x3784, 0x2c},
	{0x3785, 0x1e},
	{0x378f, 0xf5},
	{0x3791, 0xb0},
	{0x3795, 0x00},
	{0x3796, 0x64},
	{0x3797, 0x11},
	{0x3798, 0x30},
	{0x3799, 0x41},
	{0x379a, 0x07},
	{0x379b, 0xb0},
	{0x379c, 0x0c},
	{0x37c5, 0x00},
	{0x37c6, 0x00},
	{0x37c7, 0x00},
	{0x37c9, 0x00},
	{0x37ca, 0x00},
	{0x37cb, 0x00},
	{0x37de, 0x00},
	{0x37df, 0x00},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0xf8},
	{0x3804, 0x0a},
	{0x3805, 0x3f},
	{0x3806, 0x06},
	{0x3807, 0xab},
	{0x3808, 0x07},
	{0x3809, 0x80},
	{0x380a, 0x04},
	{0x380b, 0x38},
	{0x380c, 0x0a},
	{0x380d, 0x80},
	{0x380e, 0x07},
	{0x380f, 0xc0},
	{0x3810, 0x00},
	{0x3811, 0x02},
	{0x3812, 0x00},
	{0x3813, 0x02},
	{0x3814, 0x11},
	{0x3815, 0x11},
	{0x3820, 0x00},
	{0x3821, 0x1e},
	{0x3823, 0x00},
	{0x3824, 0x00},
	{0x3825, 0x00},
	{0x3826, 0x00},
	{0x3827, 0x00},
	{0x382a, 0x04},
	{0x3a04, 0x06},
	{0x3a05, 0x14},
	{0x3a06, 0x00},
	{0x3a07, 0xfe},
	{0x3b00, 0x00},
	{0x3b02, 0x00},
	{0x3b03, 0x00},
	{0x3b04, 0x00},
	{0x3b05, 0x00},
	{0x3e07, 0x20},
	{0x4000, 0x08},
	{0x4001, 0x04},
	{0x4002, 0x45},
	{0x4004, 0x08},
	{0x4005, 0x18},
	{0x4006, 0x20},
	{0x4008, 0x24},
	{0x4009, 0x40},
	{0x400c, 0x00},
	{0x400d, 0x00},
	{0x4058, 0x00},
	{0x404e, 0x37},
	{0x404f, 0x8f},
	{0x4058, 0x00},
	{0x4101, 0xb2},
	{0x4303, 0x00},
	{0x4304, 0x08},
	{0x4307, 0x30},
	{0x4311, 0x04},
	{0x4315, 0x01},
	{0x4511, 0x05},
	{0x4512, 0x01},
	{0x4806, 0x00},
	{0x4816, 0x52},
	{0x481f, 0x30},
	{0x4826, 0x2c},
	{0x4831, 0x64},
	{0x4d00, 0x04},
	{0x4d01, 0x71},
	{0x4d02, 0xfd},
	{0x4d03, 0xf5},
	{0x4d04, 0x0c},
	{0x4d05, 0xcc},
	{0x4837, 0x0a},
	{0x5000, 0x06},
	{0x5001, 0x01},
	{0x5002, 0x80},
	{0x5003, 0x20},
	{0x5046, 0x0a},
	{0x5013, 0x00},
	{0x5046, 0x0a},
	{0x5780, 0x1c},
	{0x5786, 0x20},
	{0x5787, 0x10},
	{0x5788, 0x18},
	{0x578a, 0x04},
	{0x578b, 0x02},
	{0x578c, 0x02},
	{0x578e, 0x06},
	{0x578f, 0x02},
	{0x5790, 0x02},
	{0x5791, 0xff},
	{0x5842, 0x01},
	{0x5843, 0x2b},
	{0x5844, 0x01},
	{0x5845, 0x92},
	{0x5846, 0x01},
	{0x5847, 0x8f},
	{0x5848, 0x01},
	{0x5849, 0x0c},
	{0x5e00, 0x00},
	{0x5e10, 0x0c},
	{0x0100, 0x01},
	{OV5693_TABLE_END, 0x0000}
};


static const struct ov5693_reg ov5693_1280x720_120fps_i2c[] = {
	{OV5693_TABLE_RESET, 0},/* Including sw reset */
	{0x3001, 0x0a},
	{0x3002, 0x80},
	{0x3006, 0x00},
	{0x3011, 0x21},
	{0x3012, 0x09},
	{0x3013, 0x10},
	{0x3014, 0x00},
	{0x3015, 0x08},
	{0x3016, 0xf0},
	{0x3017, 0xf0},
	{0x3018, 0xf0},
	{0x301b, 0xb4},
	{0x301d, 0x02},
	{0x3021, 0x00},
	{0x3022, 0x01},
	{0x3028, 0x44},
	{0x3098, 0x03},
	{0x3099, 0x1e},
	{0x309a, 0x02},
	{0x309b, 0x01},
	{0x309c, 0x00},
	{0x30a0, 0xd2},
	{0x30a2, 0x01},
	{0x30b2, 0x00},
	{0x30b3, 0x64},
	{0x30b4, 0x03},
	{0x30b5, 0x04},
	{0x30b6, 0x01},
	{0x3104, 0x21},
	{0x3106, 0x00},
	{0x3400, 0x04},
	{0x3401, 0x00},
	{0x3402, 0x04},
	{0x3403, 0x00},
	{0x3404, 0x04},
	{0x3405, 0x00},
	{0x3406, 0x01},
	{0x3500, 0x00},
	{0x3501, 0x2e},
	{0x3502, 0x80},
	{0x3503, 0x07},
	{0x3504, 0x00},
	{0x3505, 0x00},
	{0x3506, 0x00},
	{0x3507, 0x02},
	{0x3508, 0x00},
	{0x3509, 0x08},
	{0x350a, 0x00},
	{0x350b, 0x40},
	{0x3601, 0x0a},
	{0x3602, 0x38},
	{0x3612, 0x80},
	{0x3620, 0x54},
	{0x3621, 0xc7},
	{0x3622, 0x0f},
	{0x3625, 0x10},
	{0x3630, 0x55},
	{0x3631, 0xf4},
	{0x3632, 0x00},
	{0x3633, 0x34},
	{0x3634, 0x02},
	{0x364d, 0x0d},
	{0x364f, 0xdd},
	{0x3660, 0x04},
	{0x3662, 0x10},
	{0x3663, 0xf1},
	{0x3665, 0x00},
	{0x3666, 0x20},
	{0x3667, 0x00},
	{0x366a, 0x80},
	{0x3680, 0xe0},
	{0x3681, 0x00},
	{0x3700, 0x42},
	{0x3701, 0x14},
	{0x3702, 0xa0},
	{0x3703, 0xd8},
	{0x3704, 0x78},
	{0x3705, 0x02},
	{0x3708, 0xe6},
	{0x3709, 0xc7},
	{0x370a, 0x00},
	{0x370b, 0x20},
	{0x370c, 0x0c},
	{0x370d, 0x11},
	{0x370e, 0x00},
	{0x370f, 0x40},
	{0x3710, 0x00},
	{0x371a, 0x1c},
	{0x371b, 0x05},
	{0x371c, 0x01},
	{0x371e, 0xa1},
	{0x371f, 0x0c},
	{0x3721, 0x00},
	{0x3724, 0x10},
	{0x3726, 0x00},
	{0x372a, 0x01},
	{0x3730, 0x10},
	{0x3738, 0x22},
	{0x3739, 0xe5},
	{0x373a, 0x50},
	{0x373b, 0x02},
	{0x373c, 0x41},
	{0x373f, 0x02},
	{0x3740, 0x42},
	{0x3741, 0x02},
	{0x3742, 0x18},
	{0x3743, 0x01},
	{0x3744, 0x02},
	{0x3747, 0x10},
	{0x374c, 0x04},
	{0x3751, 0xf0},
	{0x3752, 0x00},
	{0x3753, 0x00},
	{0x3754, 0xc0},
	{0x3755, 0x00},
	{0x3756, 0x1a},
	{0x3758, 0x00},
	{0x3759, 0x0f},
	{0x376b, 0x44},
	{0x375c, 0x04},
	{0x3774, 0x10},
	{0x3776, 0x00},
	{0x377f, 0x08},
	{0x3780, 0x22},
	{0x3781, 0x0c},
	{0x3784, 0x2c},
	{0x3785, 0x1e},
	{0x378f, 0xf5},
	{0x3791, 0xb0},
	{0x3795, 0x00},
	{0x3796, 0x64},
	{0x3797, 0x11},
	{0x3798, 0x30},
	{0x3799, 0x41},
	{0x379a, 0x07},
	{0x379b, 0xb0},
	{0x379c, 0x0c},
	{0x37c5, 0x00},
	{0x37c6, 0x00},
	{0x37c7, 0x00},
	{0x37c9, 0x00},
	{0x37ca, 0x00},
	{0x37cb, 0x00},
	{0x37de, 0x00},
	{0x37df, 0x00},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0xf4},
	{0x3804, 0x0a},
	{0x3805, 0x3f},
	{0x3806, 0x06},
	{0x3807, 0xab},
	{0x3808, 0x05},
	{0x3809, 0x00},
	{0x380a, 0x02},
	{0x380b, 0xd0},
	{0x380c, 0x06},
	{0x380d, 0xd8},
	{0x380e, 0x02},
	{0x380f, 0xf8},
	{0x3810, 0x00},
	{0x3811, 0x02},
	{0x3812, 0x00},
	{0x3813, 0x02},
	{0x3814, 0x31},
	{0x3815, 0x31},
	{0x3820, 0x04},
	{0x3821, 0x1f},
	{0x3823, 0x00},
	{0x3824, 0x00},
	{0x3825, 0x00},
	{0x3826, 0x00},
	{0x3827, 0x00},
	{0x382a, 0x04},
	{0x3a04, 0x06},
	{0x3a05, 0x14},
	{0x3a06, 0x00},
	{0x3a07, 0xfe},
	{0x3b00, 0x00},
	{0x3b02, 0x00},
	{0x3b03, 0x00},
	{0x3b04, 0x00},
	{0x3b05, 0x00},
	{0x3e07, 0x20},
	{0x4000, 0x08},
	{0x4001, 0x04},
	{0x4002, 0x45},
	{0x4004, 0x08},
	{0x4005, 0x18},
	{0x4006, 0x20},
	{0x4008, 0x24},
	{0x4009, 0x40},
	{0x400c, 0x00},
	{0x400d, 0x00},
	{0x4058, 0x00},
	{0x404e, 0x37},
	{0x404f, 0x8f},
	{0x4058, 0x00},
	{0x4101, 0xb2},
	{0x4303, 0x00},
	{0x4304, 0x08},
	{0x4307, 0x30},
	{0x4311, 0x04},
	{0x4315, 0x01},
	{0x4511, 0x05},
	{0x4512, 0x01},
	{0x4806, 0x00},
	{0x4816, 0x52},
	{0x481f, 0x30},
	{0x4826, 0x2c},
	{0x4831, 0x64},
	{0x4d00, 0x04},
	{0x4d01, 0x71},
	{0x4d02, 0xfd},
	{0x4d03, 0xf5},
	{0x4d04, 0x0c},
	{0x4d05, 0xcc},
	{0x4837, 0x0a},
	{0x5000, 0x06},
	{0x5001, 0x01},
	{0x5002, 0x00},
	{0x5003, 0x20},
	{0x5046, 0x0a},
	{0x5013, 0x00},
	{0x5046, 0x0a},
	{0x5780, 0x1c},
	{0x5786, 0x20},
	{0x5787, 0x10},
	{0x5788, 0x18},
	{0x578a, 0x04},
	{0x578b, 0x02},
	{0x578c, 0x02},
	{0x578e, 0x06},
	{0x578f, 0x02},
	{0x5790, 0x02},
	{0x5791, 0xff},
	{0x5842, 0x01},
	{0x5843, 0x2b},
	{0x5844, 0x01},
	{0x5845, 0x92},
	{0x5846, 0x01},
	{0x5847, 0x8f},
	{0x5848, 0x01},
	{0x5849, 0x0c},
	{0x5e00, 0x00},
	{0x5e10, 0x0c},
	{0x0100, 0x01},
	{0x350b, 0xF8},
	{OV5693_TABLE_END, 0x0000}
};

enum {
	OV5693_MODE_2592x1944 = 0,
	OV5693_MODE_1920x1080,
	OV5693_MODE_1296x972,
	OV5693_MODE_1280x720_120FPS,
};

static const struct ov5693_reg *mode_table[] = {
	[OV5693_MODE_2592x1944]		= ov5693_2592x1944_i2c,
	[OV5693_MODE_1920x1080]		= ov5693_1920x1080_i2c,
	[OV5693_MODE_1296x972]		= ov5693_1296x972_i2c,
	[OV5693_MODE_1280x720_120FPS]	= ov5693_1280x720_120fps_i2c,
};

static int ov5693_i2c_rd8(struct ov5693_info *info, u16 reg, u8 *val)
{
	unsigned int data;
	int ret = regmap_read(info->regmap, reg, &data);
	*val = data;

	return ret;
}

static int ov5693_i2c_wr_table(struct ov5693_info *info,
				const struct ov5693_reg table[])
{
	int err;
	int buf_count = 0;
	const struct ov5693_reg *next, *n_next;
	u16 i2c_reg = 0;
	u8 i2c_buf[OV5693_SIZEOF_I2C_BUF];

	u8 *b_ptr = i2c_buf;
	u8 reset_status = 1;
	u8 reset_tries_left = OV5693_TABLE_RESET_TIMEOUT;

	for (next = table; next->addr != OV5693_TABLE_END; next++) {
		if (next->addr == OV5693_TABLE_WAIT_MS) {
			msleep(next->val);
			continue;
		} else if (next->addr == OV5693_TABLE_RESET) {
			err = regmap_write(info->regmap, 0x0103, 0x01);
			if (err)
				return err;
			while (reset_status) {
				usleep_range(200, 300);
				if (reset_tries_left < 1)
					return -EIO;
				err = ov5693_i2c_rd8(info, 0x0103,
							&reset_status);
				if (err)
					return err;
				reset_status &= 0x01;
				reset_tries_left -= 1;
			}
			continue;
		}

		if (buf_count == 0) {
			b_ptr = i2c_buf;
			i2c_reg = next->addr;
		}

		*b_ptr++ = next->val;
		buf_count++;
		n_next = next + 1;
		if (n_next->addr == next->addr + 1 &&
			n_next->addr != OV5693_TABLE_WAIT_MS &&
			buf_count < OV5693_SIZEOF_I2C_BUF &&
			n_next->addr != OV5693_TABLE_RESET &&
			n_next->addr != OV5693_TABLE_END)
			continue;

		err = regmap_bulk_write(info->regmap, i2c_reg,
					i2c_buf, buf_count);
		if (err)
			return err;

		buf_count = 0;
	}

	return 0;
}


static inline int ov5693_frame_length_reg(struct ov5693_reg *regs,
					u32 frame_length)
{
	regs->addr = 0x380E;
	regs->val = (frame_length >> 8) & 0xff;
	(regs + 1)->addr = 0x380F;
	(regs + 1)->val = (frame_length) & 0xff;

	return 2;
}

static inline int ov5693_coarse_time_reg(struct ov5693_reg *regs,
					u32 coarse_time)
{
	regs->addr = 0x3500;
	regs->val = (coarse_time >> 12) & 0xff;
	(regs + 1)->addr = 0x3501;
	(regs + 1)->val = (coarse_time >> 4) & 0xff;
	(regs + 2)->addr = 0x3502;
	(regs + 2)->val = (coarse_time & 0xf) << 4;

	return 3;
}

#define OV5693_ENTER_GROUP_HOLD(group_hold) \
	do {	\
		if (group_hold) {   \
			reg_list[offset].addr = 0x3208; \
			reg_list[offset].val = 0x01;\
			offset++;  \
		}   \
	} while (0)

#define OV5693_LEAVE_GROUP_HOLD(group_hold) \
	do {	\
		if (group_hold) {   \
			reg_list[offset].addr = 0x3208; \
			reg_list[offset].val = 0x11;\
			offset++;  \
			reg_list[offset].addr = 0x3208; \
			reg_list[offset].val = 0xe1;\
			offset++;  \
		} \
	} while (0)

static int ov5693_set_frame_length(struct ov5693_info *info,
				   u32 frame_length, bool group_hold)
{
	struct ov5693_reg reg_list[9];
	int err = 0;
	int offset = 0;

	OV5693_ENTER_GROUP_HOLD(group_hold);
	offset += ov5693_frame_length_reg(reg_list + offset, frame_length);
	OV5693_LEAVE_GROUP_HOLD(group_hold);

	reg_list[offset].addr = OV5693_TABLE_END;
	offset++;

	err = ov5693_i2c_wr_table(info, reg_list);

	return err;
}

static int ov5693_set_coarse_time(struct ov5693_info *info,
				  u32 coarse_time, bool group_hold)
{
	struct ov5693_reg reg_list[16];
	int err = 0;
	int offset = 0;

	OV5693_ENTER_GROUP_HOLD(group_hold);
	offset += ov5693_coarse_time_reg(reg_list + offset, coarse_time);
	OV5693_LEAVE_GROUP_HOLD(group_hold);

	reg_list[offset].addr = OV5693_TABLE_END;
	offset++;

	err = ov5693_i2c_wr_table(info, reg_list);

	return err;
}

static inline int ov5693_gain_reg(struct ov5693_reg *regs, u32 gain)
{
	(regs)->addr = 0x350B;
	(regs)->val = gain;

	return 1;
}

static int ov5693_bin_wr(struct ov5693_info *info, u8 enable)
{
	int err = 0;

	if (enable == info->bin_en)
		return 0;

	if (!err)
		info->bin_en = enable;
	dev_dbg(&info->i2c_client->dev, "%s bin_en=%x err=%d\n",
		__func__, info->bin_en, err);
	return err;
}

static int ov5693_exposure_wr(struct ov5693_info *info,
				struct ov5693_mode *mode)
{
	struct ov5693_reg reg_list[16];
	int err = 0;
	int offset = 0;
	bool group_hold = true; /* To use GROUP_HOLD macros */

	OV5693_ENTER_GROUP_HOLD(group_hold);
	offset += ov5693_coarse_time_reg(reg_list + offset, mode->coarse_time);
	offset += ov5693_gain_reg(reg_list + offset, mode->gain);
	OV5693_LEAVE_GROUP_HOLD(group_hold);

	reg_list[offset].addr = OV5693_TABLE_END;
	err = ov5693_i2c_wr_table(info, reg_list);

	return err;
}


static int ov5693_set_gain(struct ov5693_info *info, u32 gain, bool group_hold)
{
	struct ov5693_reg reg_list[9];
	int err = 0;
	int offset = 0;

	OV5693_ENTER_GROUP_HOLD(group_hold);
	offset += ov5693_gain_reg(reg_list + offset, gain);
	OV5693_LEAVE_GROUP_HOLD(group_hold);

	reg_list[offset].addr = OV5693_TABLE_END;
	offset++;

	err = ov5693_i2c_wr_table(info, reg_list);

	return err;
}

static int ov5693_set_group_hold(struct ov5693_info *info,
				struct ov5693_ae *ae)
{
	int err = 0;
	struct ov5693_reg reg_list[16];
	int offset = 0;
	bool group_hold = true; /* To use GROUP_HOLD macros */

	OV5693_ENTER_GROUP_HOLD(group_hold);
	if (ae->gain_enable)
		offset += ov5693_gain_reg(reg_list + offset,
					  ae->gain);
	if (ae->frame_length_enable)
		offset += ov5693_frame_length_reg(reg_list + offset,
						  ae->frame_length);
	if (ae->coarse_time_enable)
		offset += ov5693_coarse_time_reg(reg_list + offset,
						 ae->coarse_time);
	OV5693_LEAVE_GROUP_HOLD(group_hold);

	reg_list[offset].addr = OV5693_TABLE_END;
	err |= ov5693_i2c_wr_table(info, reg_list);

	return err;
}

static int ov5693_gpio_rd(struct ov5693_info *info,
			enum ov5693_gpio_type type)
{
	int val = -EINVAL;

	if (info->gpio[type].gpio) {
		val = gpio_get_value_cansleep(info->gpio[type].gpio);
		dev_dbg(&info->i2c_client->dev, "%s %u %d\n", __func__,
		       info->gpio[type].gpio, val);
		if (!info->gpio[type].active_high)
			val = !val;
		val &= 1;
	}
	return val; /* return read value or error */
}

static int ov5693_gpio_wr(struct ov5693_info *info,
			enum ov5693_gpio_type type,
			int val) /* val: 0=deassert, 1=assert */
{
	int err = -EINVAL;

	if (info->gpio[type].gpio) {
		if (!info->gpio[type].active_high)
			val = !val;
		val &= 1;
		err = val;
		gpio_set_value_cansleep(info->gpio[type].gpio, val);
		dev_dbg(&info->i2c_client->dev, "%s %u %d\n", __func__,
		       info->gpio[type].gpio, val);
	}
	return err; /* return value written or error */
}

static void ov5693_gpio_pwrdn(struct ov5693_info *info, int val)
{
	int prev_val;

	prev_val = ov5693_gpio_rd(info, OV5693_GPIO_TYPE_PWRDN);
	if ((prev_val < 0) || (val == prev_val))
		return;

	ov5693_gpio_wr(info, OV5693_GPIO_TYPE_PWRDN, val);
	if (!val && prev_val)
		/* if transition from assert to deassert then delay for I2C */
		msleep(50);
}

static void ov5693_gpio_exit(struct ov5693_info *info)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(ov5693_gpio); i++) {
		if (info->gpio[i].gpio && info->gpio[i].own)
			gpio_free(info->gpio[i].gpio);
	}
}

static void ov5693_gpio_init(struct ov5693_info *info)
{
	char label[32];
	unsigned long flags;
	unsigned type;
	unsigned i;
	unsigned j;
	int err;

	if (!info->pdata->gpio_count || !info->pdata->gpio)
		return;

	for (i = 0; i < ARRAY_SIZE(ov5693_gpio); i++) {
		type = ov5693_gpio[i].gpio_type;
		for (j = 0; j < info->pdata->gpio_count; j++) {
			if (type == info->pdata->gpio[j].gpio_type)
				break;
		}
		if (j == info->pdata->gpio_count)
			continue;

		info->gpio[type].gpio = info->pdata->gpio[j].gpio;
		if (ov5693_gpio[i].use_flags) {
			flags = ov5693_gpio[i].flags;
			info->gpio[type].active_high =
						ov5693_gpio[i].active_high;
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

		snprintf(label, sizeof(label), "ov5693_%u_%s",
			 info->pdata->num, ov5693_gpio[i].label);
		err = gpio_request_one(info->gpio[type].gpio, flags, label);
		if (err) {
			dev_err(&info->i2c_client->dev,
				"%s ERR %s %u\n", __func__, label,
				info->gpio[type].gpio);
		} else {
			info->gpio[type].own = true;
			dev_dbg(&info->i2c_client->dev,
				"%s %s %u\n", __func__, label,
				info->gpio[type].gpio);
		}
	}
}

static int ov5693_power_off(struct ov5693_info *info)
{
	struct ov5693_power_rail *pw = &info->regulators;
	int err;

	if (false == info->power_on)
		return 0;

	if (info->pdata && info->pdata->power_off) {
		err = info->pdata->power_off(pw);
		if (0 > err)
			return err;
		info->power_on = false;
		ov5693_gpio_pwrdn(info, 1);
	} else {
		dev_err(&info->i2c_client->dev,
			"%s ERR: has no power_off function\n", __func__);
		err = -EINVAL;
	}
	return err;
}

static int ov5693_power_on(struct ov5693_info *info, bool standby)
{
	struct ov5693_power_rail *pw = &info->regulators;
	int err;

	if (true == info->power_on)
		return 0;

	if (info->pdata && info->pdata->power_on) {
		err = info->pdata->power_on(pw);
		if (0 > err)
			return err;
		info->power_on = true;
		ov5693_gpio_pwrdn(info, standby ? 1 : 0);
		msleep(100);
	} else {
		dev_err(&info->i2c_client->dev,
			"%s ERR: has no power_on function\n", __func__);
		err = -EINVAL;
	}
	return err;
}

static int ov5693_pm_wr(struct ov5693_info *info, int pwr)
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
	case NVC_PWR_STDBY_OFF:
		err = ov5693_power_off(info);
		info->mode_valid = false;
		info->bin_en = 0;
		break;

	case NVC_PWR_STDBY:
		err = ov5693_power_on(info, true);
		break;

	case NVC_PWR_COMM:
	case NVC_PWR_ON:
		err = ov5693_power_on(info, false);
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
	dev_dbg(&info->i2c_client->dev, "%s pwr_dev=%d\n",
		__func__, info->pwr_dev);
	if (err > 0)
		return 0;

	return err;
}

static int ov5693_pm_dev_wr(struct ov5693_info *info, int pwr)
{
	if (info->mode_enable)
		pwr = NVC_PWR_ON;
	if (pwr < info->pwr_api)
		pwr = info->pwr_api;
	return ov5693_pm_wr(info, pwr);
}

static void ov5693_pm_exit(struct ov5693_info *info)
{
	ov5693_pm_wr(info, NVC_PWR_OFF_FORCE);

	ov5693_gpio_exit(info);
}

static void ov5693_regulator_get(struct ov5693_info *info,
				 struct regulator **vreg,
				 const char vreg_name[])
{
	struct regulator *reg = NULL;
	int err = 0;

	reg = devm_regulator_get(&info->i2c_client->dev, vreg_name);
	if (IS_ERR(reg)) {
		dev_err(&info->i2c_client->dev, "%s %s ERR: %d\n",
			__func__, vreg_name, (int)reg);
		err = PTR_ERR(reg);
		reg = NULL;
	} else {
		dev_dbg(&info->i2c_client->dev, "%s: %s\n",
			__func__, vreg_name);
	}

	*vreg = reg;
}

static void ov5693_pm_init(struct ov5693_info *info)
{
	struct ov5693_power_rail *pw = &info->regulators;

	ov5693_gpio_init(info);

	ov5693_regulator_get(info, &pw->dvdd, "dvdd");

	ov5693_regulator_get(info, &pw->avdd, "avdd");

	ov5693_regulator_get(info, &pw->dovdd, "dovdd");
	info->power_on = false;
}

static int ov5693_mode_able(struct ov5693_info *info, bool mode_enable)
{
	u8 val;
	int err;

	if (mode_enable)
		val = 0x01;
	else
		val = 0x00;
	err = regmap_write(info->regmap, 0x0100, val);
	if (!err) {
		info->mode_enable = mode_enable;
		dev_dbg(&info->i2c_client->dev, "%s streaming=%x\n",
			__func__, info->mode_enable);
		if (!mode_enable)
			ov5693_pm_dev_wr(info, NVC_PWR_STDBY);
	}
	return err;
}

static int ov5693_mode_wr_full(struct ov5693_info *info, u32 mode_index)
{
	int err;

	ov5693_pm_dev_wr(info, NVC_PWR_ON);
	ov5693_bin_wr(info, 0);
	err = ov5693_i2c_wr_table(info, mode_table[mode_index]);
	if (!err) {
		dev_dbg(&info->i2c_client->dev,
			"init done(mode=%d)!!!\n", mode_index);
		info->mode_index = mode_index;
		info->mode_valid = true;
	} else {
		dev_dbg(&info->i2c_client->dev,
			"init error(mode=%d)!!!\n", mode_index);
		info->mode_valid = false;
	}
	return err;
}


static int ov5693_set_mode(struct ov5693_info *info,
		struct ov5693_mode *mode)
{
	u32 mode_index = 0;
	int err = 0;

	if (!mode->res_x && !mode->res_y) {
		if (mode->frame_length || mode->coarse_time || mode->gain) {
			/* write exposure only */
			err = ov5693_exposure_wr(info, mode);
			return err;
		} else {
			/* turn off streaming */
			err = ov5693_mode_able(info, false);
			return err;
		}
	}

	if (mode->res_x == 2592 && mode->res_y == 1944)
		mode_index = OV5693_MODE_2592x1944;
	else if (mode->res_x == 1296 && mode->res_y == 972)
		mode_index = OV5693_MODE_1296x972;
	else if (mode->res_x == 1920 && mode->res_y == 1080)
		mode_index = OV5693_MODE_1920x1080;
	else if (mode->res_x == 1280 && mode->res_y == 720)
		mode_index = OV5693_MODE_1280x720_120FPS;

	if (!info->mode_valid || (info->mode_index != mode_index))
		err = ov5693_mode_wr_full(info, mode_index);
	else
		dev_dbg(&info->i2c_client->dev, "%s short mode\n", __func__);
	dev_dbg(&info->i2c_client->dev, "%s: mode #: %d\n",
		__func__, mode_index);
	dev_dbg(&info->i2c_client->dev, "%s: AE: %d, %d, %d\n",
		__func__, mode->frame_length,
		mode->coarse_time, mode->gain);
	err |= ov5693_exposure_wr(info, mode);
	if (err < 0) {
		info->mode_valid = false;
		dev_err(&info->i2c_client->dev,
			"%s set_mode error\n", __func__);
		goto ov5693_mode_wr_err;
	}

	return 0;

ov5693_mode_wr_err:
	if (!info->mode_enable)
		ov5693_pm_dev_wr(info, NVC_PWR_OFF);
	return err;
}

static int ov5693_get_fuse_id(struct ov5693_info *info)
{
	ov5693_i2c_rd8(info, 0x300A, &info->fuseid.id[0]);
	ov5693_i2c_rd8(info, 0x300B, &info->fuseid.id[1]);
	info->fuseid.size = 2;
	dev_dbg(&info->i2c_client->dev, "ov5693 fuse_id: %x,%x\n",
		info->fuseid.id[0], info->fuseid.id[1]);
	return 0;
}

static long ov5693_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct ov5693_info *info = file->private_data;
	int err;

	switch (cmd) {
	case OV5693_IOCTL_SET_MODE:
	{
		struct ov5693_mode mode;
		if (copy_from_user(&mode,
			(const void __user *)arg,
			sizeof(struct ov5693_mode))) {
			dev_err(&info->i2c_client->dev,
				"%s:Failed to get mode from user.\n",
			__func__);
			return -EFAULT;
		}
		return ov5693_set_mode(info, &mode);
	}
	case OV5693_IOCTL_GET_STATUS: {
		u8 status = 0;
		if (copy_to_user((void __user *)arg, &status, sizeof(status))) {
			dev_err(&info->i2c_client->dev,
				"%s:Failed to copy status to user.\n",
			__func__);
			return -EFAULT;
		}
		return 0;
		}

	case OV5693_IOCTL_SET_GROUP_HOLD: {
		struct ov5693_ae ae;
		if (copy_from_user(&ae, (const void __user *)arg,
				sizeof(struct ov5693_ae))) {
			dev_dbg(&info->i2c_client->dev,
				"%s:fail group hold\n", __func__);
			return -EFAULT;
		}

		return ov5693_set_group_hold(info, &ae);
		}

	case OV5693_IOCTL_SET_FRAME_LENGTH:
		return ov5693_set_frame_length(info, (u32)arg, true);

	case OV5693_IOCTL_SET_COARSE_TIME:
		return ov5693_set_coarse_time(info, (u32)arg, true);

	case OV5693_IOCTL_SET_GAIN:
		return ov5693_set_gain(info, (u32)arg, true);

	case OV5693_IOCTL_GET_FUSEID:
	{
		err = ov5693_get_fuse_id(info);

		if (err) {
			dev_err(&info->i2c_client->dev, "%s:Failed to get fuse id info.\n",
			__func__);
			return err;
		}
		if (copy_to_user((void __user *)arg,
				&info->fuseid,
				sizeof(struct ov5693_fuseid))) {
			dev_dbg(&info->i2c_client->dev, "%s:Fail copy fuse id to user space\n",
				__func__);
			return -EFAULT;
		}
		return 0;
	}
	default:
		dev_err(&info->i2c_client->dev, "%s unsupported ioctl: %x\n",
			__func__, cmd);
	}
	return -EINVAL;
}

static void ov5693_sdata_init(struct ov5693_info *info)
{
	memcpy(&info->sdata, &ov5693_dflt_sdata, sizeof(info->sdata));
	if (info->pdata->lens_focal_length)
		info->sdata.focal_len = info->pdata->lens_focal_length;
	if (info->pdata->lens_max_aperture)
		info->sdata.max_aperture = info->pdata->lens_max_aperture;
	if (info->pdata->lens_fnumber)
		info->sdata.fnumber = info->pdata->lens_fnumber;
	if (info->pdata->lens_view_angle_h)
		info->sdata.view_angle_h = info->pdata->lens_view_angle_h;
	if (info->pdata->lens_view_angle_v)
		info->sdata.view_angle_v = info->pdata->lens_view_angle_v;
}

static int ov5693_open(struct inode *inode, struct file *file)
{
	int err;
	struct miscdevice *miscdev = file->private_data;
	struct ov5693_info *info = dev_get_drvdata(miscdev->parent);

	if (atomic_xchg(&info->in_use, 1))
		return -EBUSY;

	file->private_data = info;
	dev_dbg(&info->i2c_client->dev, "%s\n", __func__);

	err = ov5693_power_on(info, false);
	return err;
}

int ov5693_release(struct inode *inode, struct file *file)
{
	struct ov5693_info *info = file->private_data;

	dev_dbg(&info->i2c_client->dev, "%s\n", __func__);
	ov5693_pm_wr(info, NVC_PWR_OFF);
	file->private_data = NULL;
	WARN_ON(!atomic_xchg(&info->in_use, 0));
	return 0;
}

static const struct file_operations ov5693_fileops = {
	.owner = THIS_MODULE,
	.open = ov5693_open,
	.unlocked_ioctl = ov5693_ioctl,
	.release = ov5693_release,
};

static void ov5693_del(struct ov5693_info *info)
{
	ov5693_pm_exit(info);
	synchronize_rcu();
}

static int ov5693_remove(struct i2c_client *client)
{
	struct ov5693_info *info = i2c_get_clientdata(client);

	dev_dbg(&info->i2c_client->dev, "%s\n", __func__);
	misc_deregister(&info->miscdev);
	ov5693_del(info);
	return 0;
}

static struct of_device_id ov5693_of_match[] = {
	{ .compatible = "nvidia,ov5693", },
	{ },
};

MODULE_DEVICE_TABLE(of, ov5693_of_match);

static int ov5693_platform_power_on(struct ov5693_power_rail *pw)
{
	int err;
	struct ov5693_info *info = container_of(pw, struct ov5693_info,
						regulators);

	if (info->pdata->use_vcm_vdd) {
		err = regulator_enable(info->ext_vcm_vdd);
		if (unlikely(err))
			goto ov5693_vcm_fail;
	}

	ov5693_gpio_wr(info, OV5693_GPIO_TYPE_PWRDN, 0);
	usleep_range(10, 20);

	err = regulator_enable(pw->avdd);
	if (err)
		goto ov5693_avdd_fail;

	err = regulator_enable(pw->dovdd);
	if (err)
		goto ov5693_iovdd_fail;

	usleep_range(1, 2);
	ov5693_gpio_wr(info, OV5693_GPIO_TYPE_PWRDN, 1);

	usleep_range(300, 310);

	return 0;

ov5693_iovdd_fail:
	regulator_disable(pw->avdd);

ov5693_avdd_fail:
	if (info->pdata->use_vcm_vdd)
		regulator_disable(info->ext_vcm_vdd);

ov5693_vcm_fail:
	pr_err("%s FAILED\n", __func__);
	return err;
}

static int ov5693_platform_power_off(struct ov5693_power_rail *pw)
{
	struct ov5693_info *info = container_of(pw, struct ov5693_info,
						regulators);

	usleep_range(21, 25);
	ov5693_gpio_wr(info, OV5693_GPIO_TYPE_PWRDN, 0);
	usleep_range(1, 2);

	regulator_disable(pw->dovdd);
	regulator_disable(pw->avdd);
	if (info->pdata->use_vcm_vdd)
		regulator_disable(info->ext_vcm_vdd);

	return 0;
}

static int ov5693_parse_dt_gpio(struct device_node *np, const char *name,
				enum ov5693_gpio_type type,
				struct nvc_gpio_pdata *pdata)
{
	enum of_gpio_flags gpio_flags;

	if (of_find_property(np, name, NULL)) {
		pdata->gpio = of_get_named_gpio_flags(np, name, 0, &gpio_flags);
		pdata->gpio_type = type;
		pdata->init_en = true;
		pdata->active_high = !(gpio_flags & OF_GPIO_ACTIVE_LOW);
		return 1;
	}
	return 0;
}

static struct ov5693_platform_data *ov5693_parse_dt(struct i2c_client *client)
{
	struct device_node *np = client->dev.of_node;
	struct ov5693_platform_data *pdata;
	struct nvc_gpio_pdata *gpio_pdata = NULL;

	pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&client->dev, "Failed to allocate pdata\n");
		return ERR_PTR(-ENOMEM);
	}

	gpio_pdata = devm_kzalloc(&client->dev,
		sizeof(*gpio_pdata) * ARRAY_SIZE(ov5693_gpio), GFP_KERNEL);
	if (!gpio_pdata) {
		dev_err(&client->dev, "cannot allocate gpio data memory\n");
		return ERR_PTR(-ENOMEM);
	}

	/* init with default platform data values */
	memcpy(pdata, &ov5693_dflt_pdata, sizeof(*pdata));

	/* extra regulators info */
	pdata->use_vcm_vdd = of_property_read_bool(np, "nvidia,use-vcm-vdd");

	/* generic info */
	of_property_read_u32(np, "nvidia,num", &pdata->num);
	of_property_read_string(np, "nvidia,dev-name", &pdata->dev_name);

	/* ov5693 gpios */
	pdata->gpio_count = 0;
	pdata->gpio_count += ov5693_parse_dt_gpio(np,
				"reset-gpios", OV5693_GPIO_TYPE_PWRDN,
				&gpio_pdata[pdata->gpio_count]);
	pdata->gpio = gpio_pdata;

	/* ov5693 power functions */
	pdata->power_on = ov5693_platform_power_on;
	pdata->power_off = ov5693_platform_power_off;

	return pdata;
}

static int ov5693_probe(
	struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct ov5693_info *info;
	char dname[16];
	unsigned long clock_probe_rate;
	int err;
	static struct regmap_config ad5823_regmap_config = {
		.reg_bits = 16,
		.val_bits = 8,
	};


	dev_dbg(&client->dev, "%s\n", __func__);
	info = devm_kzalloc(&client->dev, sizeof(*info), GFP_KERNEL);
	if (info == NULL) {
		dev_err(&client->dev, "%s: kzalloc error\n", __func__);
		return -ENOMEM;
	}

	info->i2c_client = client;
	if (client->dev.of_node) {
		info->pdata = ov5693_parse_dt(client);
		if (IS_ERR(info->pdata)) {
			err = PTR_ERR(info->pdata);
			dev_err(&client->dev,
				"Failed to parse OF node: %d\n", err);
			return err;
		}
	} else if (client->dev.platform_data)
		info->pdata = client->dev.platform_data;
	else {
		info->pdata = &ov5693_dflt_pdata;
		dev_dbg(&client->dev,
			"%s No platform data. Using defaults.\n",
			__func__);
	}
	if (info->pdata->use_vcm_vdd) {
		info->ext_vcm_vdd = devm_regulator_get(&info->i2c_client->dev,
							"ext_vcm_vdd");
		if (WARN_ON(IS_ERR(info->ext_vcm_vdd))) {
			err = PTR_ERR(info->ext_vcm_vdd);
			dev_err(&client->dev,
				"ext_vcm_vdd get failed %d\n", err);
			info->ext_vcm_vdd = NULL;
			return err;
		}
	}

	info->regmap = devm_regmap_init_i2c(client, &ad5823_regmap_config);
	if (IS_ERR(info->regmap)) {
		err = PTR_ERR(info->regmap);
		dev_err(&client->dev,
			"Failed to allocate register map: %d\n", err);
		return err;
	}


	i2c_set_clientdata(client, info);
	ov5693_pm_init(info);
	if (!info->regulators.avdd || !info->regulators.dovdd)
		return -EFAULT;

	ov5693_sdata_init(info);
	if (info->pdata->cfg & (NVC_CFG_NODEV | NVC_CFG_BOOT_INIT)) {
		if (info->pdata->probe_clock) {
			clock_probe_rate = 6000;  /* initial_clcok*/
			clock_probe_rate *= 1000;
			info->pdata->probe_clock(clock_probe_rate);
		}
		ov5693_pm_dev_wr(info, NVC_PWR_COMM);
		ov5693_pm_dev_wr(info, NVC_PWR_OFF);
		if (info->pdata->probe_clock)
			info->pdata->probe_clock(0);
	}
	if (info->pdata->dev_name != NULL)
		strcpy(dname, info->pdata->dev_name);
	else
		strcpy(dname, "ov5693");
	if (info->pdata->num)
		snprintf(dname, sizeof(dname), "%s.%u",
			 dname, info->pdata->num);
	info->miscdev.name = dname;
	info->miscdev.fops = &ov5693_fileops;
	info->miscdev.minor = MISC_DYNAMIC_MINOR;
	info->miscdev.parent = &client->dev;
	if (misc_register(&info->miscdev)) {
		dev_err(&client->dev, "%s unable to register misc device %s\n",
			__func__, dname);
		ov5693_del(info);
		return -ENODEV;
	}

	dev_dbg(&client->dev, "ov5693 sensor driver loading done\n");
	return 0;
}

static const struct i2c_device_id ov5693_id[] = {
	{ "ov5693", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, ov5693_id);

static struct i2c_driver ov5693_i2c_driver = {
	.driver = {
		.name = "ov5693",
		.owner = THIS_MODULE,
		.of_match_table = ov5693_of_match,
	},
	.id_table = ov5693_id,
	.probe = ov5693_probe,
	.remove = ov5693_remove,
};

module_i2c_driver(ov5693_i2c_driver);
MODULE_LICENSE("GPL v2");

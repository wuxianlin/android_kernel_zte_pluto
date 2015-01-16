/*
 * ov12830.c - ov12830 sensor driver
 *
 *  * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * Contributors:
 *	Frank Shi <fshi@nvidia.com>
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

/* Implementation
 * --------------
 * The board level details about the device are to be provided in the board
 * file with the <device>_platform_data structure.
 * Standard among NVC kernel drivers in this structure is:
 * .cfg = Use the NVC_CFG_ defines that are in nvc.h.
 *	Descriptions of the configuration options are with the defines.
 *	This value is typically 0.
 * .num = The number of the instance of the device.  This should start at 1 and
 *	and increment for each device on the board.  This number will be
 *	appended to the MISC driver name as "."%d, Example: /dev/camera.1
 *	If not used or 0, then nothing is appended to the name.
 * .sync = If there is a need to synchronize two devices, then this value is
 *	 the number of the device instance (.num above) this device is to
 *	 sync to.  For example:
 *	 Device 1 platform entries =
 *	 .num = 1,
 *	 .sync = 2,
 *	 Device 2 platfrom entries =
 *	 .num = 2,
 *	 .sync = 1,
 *	 The above example sync's device 1 and 2.
 *	 To disable sync, then .sync = 0.  Note that the .num = 0 device is
 *	 is not allowed to be synced to.
 *	 This is typically used for stereo applications.
 * .dev_name = The MISC driver name the device registers as.  If not used,
 *	     then the part number of the device is used for the driver name.
 *	     If using the NVC user driver then use the name found in this
 *	     driver under _default_pdata.
 * .gpio_count = The ARRAY_SIZE of the nvc_gpio_pdata table.
 * .gpio = A pointer to the nvc_gpio_pdata structure's platform GPIO data.
 *	 The GPIO mechanism works by cross referencing the .gpio_type key
 *	 among the nvc_gpio_pdata GPIO data and the driver's nvc_gpio_init
 *	 GPIO data to build a GPIO table the driver can use.  The GPIO's
 *	 defined in the device header file's _gpio_type enum are the
 *	 gpio_type keys for the nvc_gpio_pdata and nvc_gpio_init structures.
 *	 These need to be present in the board file's nvc_gpio_pdata
 *	 structure for the GPIO's that are used.
 *	 The driver's GPIO logic uses assert/deassert throughout until the
 *	 low level _gpio_wr/rd calls where the .assert_high is used to
 *	 convert the value to the correct signal level.
 *	 See the GPIO notes in nvc.h for additional information.
 *
 * The following is specific to NVC kernel sensor drivers:
 * .cap = Pointer to the nvc_imager_cap structure.  This structure needs to
 *	be defined and populated if overriding the driver defaults.  The
 *	driver defaults can be found at: default_<device>_pdata
 * .lens_focal_length = The lens focal length.  See note below.
 * .lens_view_angle_h = lens horizontal view angle.  See note below.
 * .lens_view_angle_v = lens vertical view angle.  See note below.
 * Note: The lens defines are suppose to be float values.  However, since the
 *       Linux kernel doesn't allow float data, these values are integers and
 *       will be divided by the NVC_IMAGER_INT2FLOAT_DIVISOR value when the
 *       data is in user space. For example, 12.3456 must be 123456
 * .clock_probe = The routine to call to turn on the sensor clock during the
 *		probe routine.  The routine should take one unsigned long
 *		parameter that is the clock frequency:
 *		(<probe_clock_routine>(unsigned long c)
 *		A value of 0 turns off the clock.
 *
 * Power Requirements
 * The board power file must contain the following labels for the power
 * regulator(s) of this device:
 * "avdd" = the power regulator for analog power.
 * "dvdd" = the power regulator for digital power.
 * "dovdd" = the power regulator for I/O power.
 *
 * NVC usage
 * ---------
 * The following is the expected usage method of the NVC architecture.
 * - OPEN: When opening the imager device, IOCTL's for capabilities
 * (NVC_IOCTL_CAPS_RD) and static data (NVC_IOCTL_STATIC_RD) are made to
 * populate the NVC user driver with this information.  The static data is data
 * specific to the imager device that doesn't change.  See the
 * static vs dynamic note below about static data that is really dynamic.
 * An IOCTL for dynamic data (NVC_IOCTL_DYNAMIC_RD) is also done to get the
 * data for the default mode.  This allows the NVC user driver to carry out
 * operations requiring dynamic data without a mode having been set yet.
 * This is accomplished by making the NVC_IOCTL_DYNAMIC_RD IOCTL with the mode
 * resolution set to 0 by 0 (x = 0, y = 0).  The default resolution will be
 * used which is determined by the preferred_mode_index member in the
 * capabilities structure.
 * To get a list of all the possible modes the device supports, the mode read
 * IOCTL is done (NVC_IOCTL_MODE_RD).
 * - OPERATION: The NVC_IOCTL_MODE_WR, NVC_IOCTL_DYNAMIC_RD, NVC_IOCTL_PWR_WR,
 *   and NVC_IOCTL_PARAM_(RD/WR) are used to operate the device.  See the
 *   summary of IOCTL usage for details.
 * - QUERY: Some user level functions request data about a specific mode.  The
 * NVC_IOCTL_DYNAMIC_RD serves this purpose without actually making the mode
 * switch.
 *
 * Summary of IOCTL usage:
 * - NVC_IOCTL_CAPS_RD: To read the capabilites of the device.  Board specific.
 * - NVC_IOCTL_MODE_WR: There are a number of functions with this:
 *   - If the entire nvc_imager_bayer structure is 0, the streaming is turned
 *     off and the device goes to standby.
 *   - If the x and y of the nvc_imager_bayer structure is set to 0, only the
 *     frame_length, coarse_time, and gain is written for the current set mode.
 *   - A fully populated nvc_imager_bayer structure sets the mode specified by
 *     the x and y.
 *   - Any invalid x and y other than 0,0 results in an error.
 * - NVC_IOCTL_MODE_RD: To read all the possible modes the device supports.
 * - NVC_IOCTL_STATIC_RD: To read the static data specific to the device.
 * - NVC_IOCTL_DYNAMIC_RD: To read the data specific to a mode specified by
 *   a valid x and y.  Setting the x and y to 0 allows reading this information
 *   for the default mode that is specified by the preferred_mode_index member
 *   in the capabilities structure.
 * - NVC_IOCTL_PWR_WR: This is a GLOS (Guaranteed Level Of Service) call.  The
 *   device will normally operate at the lowest possible power for its current
 *   use.  This GLOS call allows for minimum latencies during operation.
 * - NVC_IOCTL_PWR_RD: Reads the current GLOS setting.
 * - NVC_IOCTL_PARAM_WR: The IOCTL to set parameters.  Note that there is a
 *   separate GAIN parameter for when just the gain is changed.  If however,
 *   the gain should be changed along with the frame_length and coarse_time,
 *   the NVC_IOCTL_MODE_WR should be used instead.
 * - NVC_IOCTL_PARAM_RD: The IOCTL to read parameters.
 */

/*#define DEBUG 1*/

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/edp.h>
#include <media/ov12830.h>

#define OV12830_ID			0xc830
#define OV12830_SENSOR_TYPE		NVC_IMAGER_TYPE_RAW
#define OV12830_STARTUP_DELAY_MS	50
#define OV12830_RES_CHG_WAIT_TIME_MS	100
#define OV12830_SIZEOF_I2C_BUF		16
#define OV12830_TABLE_WAIT_MS		0
#define OV12830_TABLE_END		1
#define OV12830_TABLE_RESET		2
#define OV12830_TABLE_RESET_TIMEOUT	50
#define OV12830_NUM_MODES		ARRAY_SIZE(ov12830_mode_table)
#define OV12830_MODE_UNKNOWN		(OV12830_NUM_MODES + 1)
#define OV12830_LENS_MAX_APERTURE	10000 /* / _INT2FLOAT_DIVISOR */
#define OV12830_LENS_FNUMBER		2200 /* / _INT2FLOAT_DIVISOR */
#define OV12830_LENS_FOCAL_LENGTH	3850 /* / _INT2FLOAT_DIVISOR */
#define OV12830_LENS_VIEW_ANGLE_H	75000 /* / _INT2FLOAT_DIVISOR */
#define OV12830_LENS_VIEW_ANGLE_V	75000 /* / _INT2FLOAT_DIVISOR */
#define OV12830_I2C_TABLE_MAX_ENTRIES	400

/* comment out definition to disable mode */
#define OV12830_ENABLE_1920x1080
#define OV12830_ENABLE_2116x1504
#define OV12830_ENABLE_4224x3000
#define OV12830_ENABLE_4224x3000_NIGHT
//#define OV12830_ENABLE_2112x1500
/* for cam calibration */
#include <linux/proc_fs.h>
#define CAM_CAL_PROC_FILE "driver/cam_cal"
#define OV12830_OTP_WB_CAL_EN  1
#define OV12830_OTP_LENS_CAL_EN  2
#define OV12830_OTP_WB_LENS_CAL_UN  3
static struct proc_dir_entry *cam_cal_proc_file;
u8 cam_cal_flag= 4;
static int g_cam_status = 2; //0, capture; 1, video; 2, preview,3, night mode
int g_ov12830_nightmode = 0;
 int ov12830_cam_clibratiion_flag(void)
{
   printk("zhangduo otp  %s,cam_cal_flag is %d,",__func__,cam_cal_flag);
    return cam_cal_flag;
}

static ssize_t cam_cal_proc_write(struct file *filp,
				    const char *buff, size_t len,
				    loff_t * off)
{
       u8 val;

	//printk("cam_cal_proc_write\n");
	val=buff[0]-'0';
	cam_cal_flag = val;
	printk("cam_cal_proc_write cam_cal_flag=%d\n",cam_cal_flag);
	
	return len;
}

static struct file_operations cam_cal_proc_ops = 
{
	.write = cam_cal_proc_write,
	
};
void create_cam_cal_proc_file(void)
{
       printk("create_cam_cal_proc_file\n");
	cam_cal_proc_file =
	    create_proc_entry(CAM_CAL_PROC_FILE, 0660, NULL);
	if (cam_cal_proc_file) {
		cam_cal_proc_file->proc_fops = &cam_cal_proc_ops;
	} 
	else	{
		printk("proc file create failed!");
	}
}

/* end */

static u16 ov12830_ids[] = {
	0xc830,
};

static struct nvc_gpio_init ov12830_gpio[] = {
	{ OV12830_GPIO_TYPE_SHTDN, GPIOF_OUT_INIT_LOW, "shutdn", false, true, },
	{ OV12830_GPIO_TYPE_PWRDN, GPIOF_OUT_INIT_LOW, "pwrdn", false, true, },
	{ OV12830_GPIO_GP1, 0, "gp1", false, false},
	{ OV12830_GPIO_GP2, 0, "gp2", false, false},
};

struct ov12830_info {
	atomic_t in_use;
	struct i2c_client *i2c_client;
	struct ov12830_platform_data *pdata;
	struct nvc_imager_cap *cap;
	struct miscdevice miscdev;
	struct list_head list;
	int pwr_api;
	int pwr_dev;
	struct nvc_gpio gpio[ARRAY_SIZE(ov12830_gpio)];
	struct ov12830_power_rail regulators;
	bool power_on;
	u8 s_mode;
	struct ov12830_info *s_info;
	u32 mode_index;
	bool mode_valid;
	bool mode_enable;
	unsigned test_pattern;
	struct nvc_imager_static_nvc sdata;
	u8 i2c_buf[OV12830_SIZEOF_I2C_BUF];
	u8 bin_en;
	struct edp_client *edpc;
	unsigned edp_state;
};

struct ov12830_reg {
	u16 addr;
	u16 val;
};

struct ov12830_mode_data {
	struct nvc_imager_mode sensor_mode;
	struct nvc_imager_dynamic_nvc sensor_dnvc;
	struct ov12830_reg *p_mode_i2c;
};
#define NVC_FOCUS_GUID(n)         (0x665F4E5643414D30ULL | ((n) & 0xF))
static struct nvc_imager_cap ov12830_dflt_cap = {
	.identifier		= "OV12830",
	/* refer to NvOdmImagerSensorInterface enum in ODM nvodm_imager.h */
	.sensor_nvc_interface	= NVC_IMAGER_SENSOR_INTERFACE_SERIAL_A,
	/* refer to NvOdmImagerPixelType enum in ODM nvodm_imager.h */
	.pixel_types[0]		= 0x101,
	/* refer to NvOdmImagerOrientation enum in ODM nvodm_imager.h */
	.orientation		= 0,
	/* refer to NvOdmImagerDirection enum in ODM nvodm_imager.h */
	.direction		= 0,
	.initial_clock_rate_khz	= 6000,
	.clock_profiles[0] = {
		.external_clock_khz	= 24000,
		.clock_multiplier	= 10416667, /* value / 1,000,000 */
	},
	.clock_profiles[1] = {
		.external_clock_khz	= 0,
		.clock_multiplier	= 0,
	},
	.h_sync_edge		= 0,
	.v_sync_edge		= 0,
	.mclk_on_vgp0		= 0,
	.csi_port		= 0,
	.data_lanes		= 4,
	.virtual_channel_id	= 0,
	.discontinuous_clk_mode	= 0, /* use continuous clock */
	.cil_threshold_settle	= 0,
	.min_blank_time_width	= 16,
	.min_blank_time_height	= 16,
	.preferred_mode_index	= 0,
	.focuser_guid		= NVC_FOCUS_GUID(0),
	.torch_guid		= NVC_TORCH_GUID(0),
	.cap_version		= NVC_IMAGER_CAPABILITIES_VERSION2,
};

static struct ov12830_platform_data ov12830_dflt_pdata = {
	.cfg			= 0,
	.num			= 0,
	.sync			= 0,
	.dev_name		= "camera",
	.cap			= &ov12830_dflt_cap,
};

	/* NOTE: static vs dynamic
	 * If a member in the nvc_imager_static_nvc structure is not actually
	 * static data, then leave blank and add the parameter to the parameter
	 * read function that dynamically reads the data.  The NVC user driver
	 * will call the parameter read for the data if the member data is 0.
	 * If the dynamic data becomes static during probe (a one time read
	 * such as device ID) then add the dynamic read to the _sdata_init
	 * function.
	 */
static struct nvc_imager_static_nvc ov12830_dflt_sdata = {
	.api_version		= NVC_IMAGER_API_STATIC_VER,
	.sensor_type		= OV12830_SENSOR_TYPE,
	.bits_per_pixel		= 10,
	.sensor_id		= OV12830_ID,
	.sensor_id_minor	= 0,
	.focal_len		= OV12830_LENS_FOCAL_LENGTH,
	.max_aperture		= OV12830_LENS_MAX_APERTURE,
	.fnumber		= OV12830_LENS_FNUMBER,
	.view_angle_h		= OV12830_LENS_VIEW_ANGLE_H,
	.view_angle_v		= OV12830_LENS_VIEW_ANGLE_V,
	.res_chg_wait_time	= OV12830_RES_CHG_WAIT_TIME_MS,
};

static LIST_HEAD(ov12830_info_list);
static DEFINE_SPINLOCK(ov12830_spinlock);


static struct ov12830_reg tp_none_seq[] = {
	{0x5e00, 0x00},
	{OV12830_TABLE_END, 0x0000}
};

static struct ov12830_reg tp_cbars_seq[] = {
	{0x5e00, 0x80},
	{OV12830_TABLE_END, 0x0000}
};

static struct ov12830_reg *test_patterns[] = {
	tp_none_seq,
	tp_cbars_seq,
};

#ifdef OV12830_ENABLE_2112x1500
static struct ov12830_reg ov12830_2112x1500_i2c[] = {
	{OV12830_TABLE_RESET, 0},
{0x103, 0x01},
{0x3001, 0x06},
{0x3002, 0x80},
{0x3011, 0x41},
{0x3014, 0x16},
{0x3015, 0x0b},
{0x3022, 0x03},
{0x3090, 0x02},
{0x3091, 0x14},
{0x3092, 0x00},
{0x3093, 0x00},
{0x3098, 0x03},
{0x3099, 0x11},
{0x309c, 0x01},
{0x30b3, 0x4a}, //0x;, 0x6e
{0x30b4, 0x03},
{0x30b5, 0x04},
{0x3106, 0x01},
{0x3304, 0x28},
{0x3305, 0x41},
{0x3306, 0x30},
{0x3308, 0x00},
{0x3309, 0xc8},
{0x330a, 0x01},
{0x330b, 0x90},
{0x330c, 0x02},
{0x330d, 0x58},
{0x330e, 0x03},
{0x330f, 0x20},
{0x3300, 0x00},
{0x3500, 0x00},
{0x3501, 0x96},
{0x3502, 0x40},
{0x3503, 0x07},
{0x350a, 0x00},
{0x350b, 0x80},
{0x3602, 0x18},
{0x3612, 0x80},
{0x3620, 0x64},
{0x3621, 0xb5},
{0x3622, 0x09},
{0x3623, 0x28},
{0x3631, 0xb3},
{0x3634, 0x04},
{0x3660, 0x80},
{0x3662, 0x10},
{0x3663, 0xf0},
{0x3667, 0x00},
{0x366f, 0x20},
{0x3680, 0xb5},
{0x3682, 0x00},
{0x3701, 0x12},
{0x3702, 0x88},
{0x3708, 0xe6},
{0x3709, 0xc7},
{0x370b, 0xa0},
//{0x370c,0xcc},
{0x370d, 0x11},
{0x370e, 0x00},
{0x371c, 0x01},
{0x371f, 0x1b},
{0x3724, 0x10},
{0x3726, 0x00},
{0x372a, 0x09},
{0x3739, 0xb0},
{0x373c, 0x40},
{0x376b, 0x44},
{0x3774, 0x10},
{0x377b, 0x44},
{0x3780, 0x22},
{0x3781, 0xc8},
{0x3783, 0x31},
{0x3786, 0x16},
{0x3787, 0x02},
{0x3796, 0x84},
{0x379c, 0x0c},
{0x37c5, 0x00},
{0x37c6, 0x00},
{0x37c7, 0x00},
{0x37c9, 0x00},
{0x37ca, 0x00},
{0x37cb, 0x00},
{0x37cc, 0x00},
{0x37cd, 0x00},
{0x37ce, 0x10},
{0x37cf, 0x00},
{0x37d0, 0x00},
{0x37d1, 0x00},
{0x37d2, 0x00},
{0x37de, 0x00},
{0x37df, 0x00},
{0x3800, 0x00},
{0x3801, 0x00},
{0x3802, 0x00},
{0x3803, 0x00},
{0x3804, 0x10},
{0x3805, 0x9f},
{0x3806, 0x0b},
{0x3807, 0xc7},

{0x3808, 0x08}, //0x;, 0x10
{0x3809, 0x40}, //0x;, 0x80
{0x380a, 0x05}, //0x;, 0x0b
{0x380b, 0xdc}, //0x;, 0xb8

{0x380c, 0x11},
{0x380d, 0x50},
{0x380e, 0x07},
{0x380f, 0x00},
{0x3810, 0x00},
{0x3811, 0x08}, //0x;, 0x10
{0x3812, 0x00},
{0x3813, 0x04}, //0x;, 0x08
{0x3814, 0x31},
{0x3815, 0x31},
{0x3820, 0x14},
{0x3821, 0x0f},
{0x3823, 0x00},
{0x3824, 0x00},
{0x3825, 0x00},
{0x3826, 0x00},
{0x3827, 0x00},
{0x3829, 0x0b},
{0x382b, 0x6a},
{0x4000, 0x18},
{0x4001, 0x06},
{0x4002, 0x45},
{0x4004, 0x02},
{0x4005, 0x19},
{0x4006, 0x20},
{0x4007, 0x90},
{0x4008, 0x24},
{0x4009, 0x17},
{0x400c, 0x00},
{0x400d, 0x00},
{0x404e, 0x37},
{0x404f, 0x8f},
{0x4058, 0x40},
{0x4100, 0x2d},
{0x4101, 0x22},
{0x4102, 0x04},
{0x4104, 0x5c},
{0x4109, 0xa3},
{0x410a, 0x03},
{0x4300, 0xff},
{0x4303, 0x00},
{0x4304, 0x08},
{0x4307, 0x30},
{0x4311, 0x04},
{0x4511, 0x05},
{0x4816, 0x52},
{0x481f, 0x30},
{0x4826, 0x2c},
{0x4a00, 0xaa},
{0x4a03, 0x01},
{0x4a05, 0x08},
{0x4d00, 0x05},
{0x4d01, 0x19},
{0x4d02, 0xfd},
{0x4d03, 0xd1},
{0x4d04, 0xff},
{0x4d05, 0xff},
{0x4d07, 0x04},
{0x4837, 0x0e},
{0x484b, 0x05},
{0x5000, 0x06},
{0x5001, 0x01},
{0x5002, 0x88}, //0x;, 0x08
{0x5003, 0x21},
{0x5043, 0x48},
{0x5013, 0x80},
{0x501f, 0x00},
{0x583e,0x08},
{0x583f,0x04},
{0x5840,0x18},
{0x5841,0x0d},
{0x5e00, 0x00},
{0x5a01, 0x00},
{0x5a02, 0x00},
{0x5a03, 0x00},
{0x5a04, 0x10},
{0x5a05, 0xa0},
{0x5a06, 0x0c},
{0x5a07, 0x78},
{0x5a08, 0x00},
{0x5b05, 0x6c},
{0x5e00, 0x00},
{0x5e01, 0x41},
{0x5e11, 0x30},
{0x100, 0x01},

	{OV12830_TABLE_END, 0x0000}
};
#endif

#ifdef OV12830_ENABLE_1920x1080
static struct ov12830_reg ov12830_1920x1080_i2c[] = {
	{OV12830_TABLE_RESET, 0},
	{0x0103,0x01},
{0x3001,0x06},
{0x3002,0x80},
{0x3011,0x41},
{0x3014,0x16},
{0x3015,0x0b},
{0x3022,0x03},
{0x3090,0x03},
{0x3091,0x14},         //; 160MHz
{0x3092,0x00},
{0x3093,0x00},
{0x3098,0x03},
{0x3099,0x11},
{0x309c,0x01},
{0x30b3,0x4a},
{0x30b4,0x03},
{0x30b5,0x04},
{0x3106,0x01},
{0x3304,0x28},
{0x3305,0x41},
{0x3306,0x30},
{0x3308,0x00},
{0x3309,0xc8},
{0x330a,0x01},
{0x330b,0x90},
{0x330c,0x02},
{0x330d,0x58},
{0x330e,0x03},
{0x330f,0x20},
{0x3300,0x00},
{0x3500,0x00},
{0x3501,0x70},
{0x3502,0x20},
{0x3503,0x07},
{0x350a,0x00},
{0x350b,0x80},
{0x3602,0x18},
{0x3612,0x80},
{0x3620,0x64},
{0x3621,0xb5},
{0x3622,0x09},
{0x3623,0x28},
{0x3631,0xb3},
{0x3634,0x04},
{0x3660,0x80},
{0x3662,0x10},
{0x3663,0xf0},
{0x3667,0x00},
{0x366f,0x20},
{0x3680,0xb5},
{0x3682,0x00},
{0x3701,0x12},
{0x3702,0x88},
{0x3708,0xe6},
{0x3709,0xc7},
{0x370b,0xa0},
{0x370d,0x11},
{0x370e,0x00},
{0x371c,0x01},
{0x371f,0x1b},
{0x3724,0x10},
{0x3726,0x00},
{0x372a,0x09},
{0x3739,0xb0},
{0x373c,0x40},
{0x376b,0x44},
{0x377b,0x44},
{0x3780,0x22},
{0x3781,0xc8},
{0x3783,0x31},
{0x3786,0x16},
{0x3787,0x02},
{0x3796,0x84},
{0x379c,0x0c},
{0x37c5,0x00},
{0x37c6,0x00},
{0x37c7,0x00},
{0x37c9,0x00},
{0x37ca,0x00},
{0x37cb,0x00},
{0x37cc,0x00},
{0x37cd,0x00},
{0x37ce,0x10},
{0x37cf,0x00},
{0x37d0,0x00},
{0x37d1,0x00},
{0x37d2,0x00},
{0x37de,0x00},
{0x37df,0x00},
{0x3800,0x00},         //;
{0x3801,0x00},         //;
{0x3802,0x01},         //; 00 ;
{0x3803,0x38},         //; 00 ;
{0x3804,0x10},         //;
{0x3805,0x9f},         //;
{0x3806,0x0a},         //; 0b ;
{0x3807,0x8f},         //; c7 ;
{0x3808,0x07},         //; 08 ;
{0x3809,0x80},         //; 40 ;
{0x380a,0x04},         //; 04 ;
{0x380b,0x38},         //; a0 ;
{0x380c,0x0B},
{0x380d,0x8A},
{0x380e,0x07},
{0x380f,0x0C},
{0x3810,0x00},
{0x3811,0x66},
{0x3812,0x00},
{0x3813,0x38},
{0x3814,0x31},
{0x3815,0x31},
{0x3820,0x14},
{0x3821,0x0f},
{0x3823,0x00},
{0x3824,0x00},
{0x3825,0x00},
{0x3826,0x00},
{0x3827,0x00},
{0x3829,0x0b},
{0x382b,0x6a},
{0x4000,0x18},
{0x4001,0x06},
{0x4002,0x45},
{0x4004,0x02},
{0x4005,0x19},
{0x4006,0x20},
{0x4007,0x90},
{0x4008,0x24},
{0x4009,0x12},         //; 40
{0x400c,0x00},
{0x400d,0x00},
{0x404e,0x37},
{0x404f,0x8f},
{0x4058,0x40},
{0x4100,0x2d},
{0x4101,0x22},
{0x4102,0x04},
{0x4104,0x5c},
{0x4109,0xa3},
{0x410a,0x03},
{0x4300,0xff},
{0x4303,0x00},
{0x4304,0x08},
{0x4307,0x30},
{0x4311,0x04},
{0x4511,0x05},
{0x4816,0x52},
{0x481f,0x30},
{0x4826,0x2c},
{0x4a00,0xaa},
{0x4a03,0x01},
{0x4a05,0x08},
{0x4d00,0x05},
{0x4d01,0x19},
{0x4d02,0xfd},
{0x4d03,0xd1},
{0x4d04,0xff},
{0x4d05,0xff},
{0x4d07,0x04},
{0x4837,0x09},
{0x484b,0x05},
{0x5000,0x06},
{0x5001,0x01},
{0x5002,0x08},         //; 00
{0x5003,0x21},
{0x5043,0x48},
{0x5013,0x80},
{0x501f,0x00},
{0x5780, 0xfc},
{0x5781, 0x13},
{0x5782, 0x03},
{0x5786, 0x20},
{0x5787, 0x40},
{0x5788, 0x08},
{0x5789, 0x08},
{0x578a, 0x02},
{0x578b, 0x01},
{0x578c, 0x01},
{0x578d, 0x0c},
{0x578e, 0x02},
{0x578f, 0x01},
{0x5790, 0x01},
{0x583e,0x08},
{0x583f,0x04},
{0x5840,0x10},
{0x5841,0x0d},
{0x5e00,0x00},
{0x5a01,0x00},
{0x5a02,0x00},
{0x5a03,0x00},
{0x5a04,0x10},
{0x5a05,0xa0},
{0x5a06,0x0c},
{0x5a07,0x78},
{0x5a08,0x00},
{0x5e00,0x00},
{0x5e01,0x41},
{0x5e11,0x30},           
{0x3717,0x03},        //;;NEW TIMING
{0x373f,0x40},
{0x3755,0x03},
{0x3758,0x02},
{0x373a,0x70},
{0x3795,0x04},
{0x3796,0x16},
{0x0100,0x01},


	{OV12830_TABLE_END, 0x0000}
};
#endif
#ifdef OV12830_ENABLE_2116x1504
static struct ov12830_reg ov12830_2116x1504_i2c[] = {
	{OV12830_TABLE_RESET, 0},
	

{0x0103,0x01},

{0x3001,0x06},
{0x3002,0x80},
{0x3011,0x41},
{0x3014,0x16},
{0x3015,0x0b},
{0x3022,0x03},
{0x3090,0x03},  //02
{0x3091,0x14},
{0x3092,0x00},
{0x3093,0x00},
{0x3098,0x03},
{0x3099,0x11},
{0x309c,0x01},
{0x30b3,0x4a},
{0x30b4,0x03},
{0x30b5,0x04},
{0x3106,0x01},
{0x3304,0x28},
{0x3305,0x41},
{0x3306,0x30},
{0x3308,0x00},
{0x3309,0xc8},
{0x330a,0x01},
{0x330b,0x90},
{0x330c,0x02},
{0x330d,0x58},
{0x330e,0x03},
{0x330f,0x20},
{0x3300,0x00},
{0x3500,0x00},
{0x3501,0x70},  //0x96
{0x3502,0x20},
{0x3503,0x07},  //40
{0x350a,0x00},
{0x350b,0x80},
{0x3602,0x18},
{0x3612,0x80},
{0x3620,0x64},
{0x3621,0xb5},
{0x3622,0x09},  //09
{0x3623,0x28},
{0x3631,0xb3},
{0x3634,0x04},
{0x3660,0x80},
{0x3662,0x10},
{0x3663,0xf0},
{0x3667,0x00},
{0x366f,0x20},
{0x3680,0xb5},
{0x3682,0x00},
{0x3701,0x12},
{0x3702,0x88},
{0x3708,0xe6},
{0x3709,0xc7},
{0x370b,0xa0},
{0x370d,0x11},
//{0x370c,0xcc},
{0x370e,0x00},
{0x371c,0x01},
{0x371f,0x1b},
{0x3724,0x10},
{0x3726,0x00},
{0x372a,0x09},
{0x3739,0xb0},
{0x373c,0x40},
{0x376b,0x44},
//{0x3774,0x10},
{0x377b,0x44},
{0x3780,0x22},
{0x3781,0xc8},
{0x3783,0x31},
{0x3786,0x16},
{0x3787,0x02},
//{0x3796,0x84},
{0x379c,0x0c},
{0x37c5,0x00},
{0x37c6,0x00},
{0x37c7,0x00},
{0x37c9,0x00},
{0x37ca,0x00},
{0x37cb,0x00},
{0x37cc,0x00},
{0x37cd,0x00},
{0x37ce,0x10},
{0x37cf,0x00},
{0x37d0,0x00},
{0x37d1,0x00},
{0x37d2,0x00},
{0x37de,0x00},
{0x37df,0x00},
{0x3800,0x00},
{0x3801,0x00},
{0x3802,0x00},
{0x3803,0x00},
{0x3804,0x10},
{0x3805,0x9f},
{0x3806,0x0b},
{0x3807,0xc7},
{0x3808,0x08},
{0x3809,0x44},
{0x380a,0x05},
{0x380b,0xe0},
{0x380c,0x0b},
{0x380d,0x8a},
{0x380e,0x07},
{0x380f,0x0c},
{0x3810,0x00},
{0x3811,0x06},
{0x3812,0x00},
{0x3813,0x02},
{0x3814,0x31},
{0x3815,0x31},
{0x3820,0x14},
{0x3821,0x0f},
{0x3823,0x00},
{0x3824,0x00},
{0x3825,0x00},
{0x3826,0x00},
{0x3827,0x00},
{0x3829,0x0b},
{0x382b,0x6a},
{0x4000,0x18},
{0x4001,0x06},
{0x4002,0x45},
{0x4004,0x02},
{0x4005,0x19},
{0x4006,0x20},
{0x4007,0x90},
{0x4008,0x24},
{0x4009,0x12},
{0x400c,0x00},
{0x400d,0x00},
{0x404e,0x37},
{0x404f,0x8f},
{0x4058,0x40},
{0x4100,0x2d},
{0x4101,0x22},
{0x4102,0x04},
{0x4104,0x5c},
{0x4109,0xa3},
{0x410a,0x03},
{0x4300,0xff},
{0x4303,0x00},
{0x4304,0x08},
{0x4307,0x30},
{0x4311,0x04},
{0x4511,0x05},
{0x4816,0x52},
{0x481f,0x30},
{0x4826,0x2c},
{0x4a00,0xaa},
{0x4a03,0x01},
{0x4a05,0x08},
{0x4d00,0x05},
{0x4d01,0x19},
{0x4d02,0xfd},
{0x4d03,0xd1},
{0x4d04,0xff},
{0x4d05,0xff},
{0x4d07,0x04},
{0x4837,0x09},
{0x484b,0x05},
{0x5000,0x06},
{0x5001,0x01},
{0x5002,0x08},
{0x5003,0x21},
{0x5043,0x48},
{0x5013,0x80},
{0x501f,0x00},
{0x5780, 0xfc},
{0x5781, 0x13},
{0x5782, 0x03},
{0x5786, 0x20},
{0x5787, 0x40},
{0x5788, 0x08},
{0x5789, 0x08},
{0x578a, 0x02},
{0x578b, 0x01},
{0x578c, 0x01},
{0x578d, 0x0c},
{0x578e, 0x02},
{0x578f, 0x01},
{0x5790, 0x01},
{0x583e,0x08},
{0x583f,0x04},
{0x5840,0x10},
{0x5841,0x0d},
{0x5e00,0x00},
{0x5a01,0x00},
{0x5a02,0x00},
{0x5a03,0x00},
{0x5a04,0x10},
{0x5a05,0xa0},
{0x5a06,0x0c},
{0x5a07,0x78},
{0x5a08,0x00},
//{0x5b05,0x6c},
{0x5e00,0x00},
{0x5e01,0x41},
{0x5e11,0x30},
{0x3717,0x03},
{0x373f,0x40},
{0x3755,0x03},
{0x3758,0x02},

{0x373a,0x70},
{0x3795,0x04},
{0x3796,0x16},
{0x0100,0x01},

{OV12830_TABLE_END, 0x0000}
};
#endif
#ifdef OV12830_ENABLE_4224x3000
static struct ov12830_reg ov12830_4224x3000_i2c[] = {
	{OV12830_TABLE_RESET, 0},
	{0x3001, 0x06},
	{0x3002, 0x80},
	{0x3011, 0x41},
	{0x3014, 0x16},
	{0x3015, 0x0b},
	{0x3022, 0x03},
	{0x3090, 0x02},
	{0x3091, 0x14},
	{0x3092, 0x00},
	{0x3093, 0x00},
	{0x3098, 0x03},
	{0x3099, 0x11},
	{0x309c, 0x01},
	{0x30b3, 0x4a},
	{0x30b4, 0x03},
	{0x30b5, 0x04},
	{0x3106, 0x01},
	{0x3304, 0x28},
	{0x3305, 0x41},
	{0x3306, 0x30},
	{0x3308, 0x00},
	{0x3309, 0xc8},
	{0x330a, 0x01},
	{0x330b, 0x90},
	{0x330c, 0x02},
	{0x330d, 0x58},
	{0x330e, 0x03},
	{0x330f, 0x20},
	{0x3300, 0x00},
	{0x3500, 0x00},
	{0x3501, 0xbd},
	{0x3502, 0x40},
	{0x3503, 0x07},
	{0x350a, 0x00},
	{0x350b, 0x80},
  {0x3602, 0x18},
	{0x3612, 0x80},
  {0x3620, 0x64},
  {0x3621, 0xb5},
  {0x3622, 0x09},
  {0x3623, 0x28},
	{0x3631, 0xb3},
	{0x3634, 0x04},
	{0x3660, 0x80},
	{0x3662, 0x10},
	{0x3663, 0xf0},
	{0x3667, 0x00},
	{0x366f, 0x20},
	{0x3680, 0xb5},
	{0x3682, 0x00},
	{0x3701, 0x12},
  {0x3702, 0x88},
	{0x3708, 0xe3},
	{0x3709, 0xc3},
  {0x370b, 0xa0},
 // {0x370c,0x0c},
	{0x370d, 0x11},
	{0x370e, 0x00},
	{0x371c, 0x01},
	{0x371f, 0x1b},
  {0x3724, 0x10},
	{0x3726, 0x00},
	{0x372a, 0x09},
  {0x3739, 0xb0},
	{0x373c, 0x40},
	{0x376b, 0x44},
  {0x3774, 0x10},
	{0x377b, 0x44},
	{0x3780, 0x22},
	{0x3781, 0xc8},
	{0x3783, 0x31},
	{0x3786, 0x16},
	{0x3787, 0x02},
	{0x3796, 0x84},
	{0x379c, 0x0c},
	{0x37c5, 0x00},
	{0x37c6, 0x00},
	{0x37c7, 0x00},
	{0x37c9, 0x00},
	{0x37ca, 0x00},
	{0x37cb, 0x00},
	{0x37cc, 0x00},
	{0x37cd, 0x00},
	{0x37ce, 0x10},
	{0x37cf, 0x00},
	{0x37d0, 0x00},
	{0x37d1, 0x00},
	{0x37d2, 0x00},
	{0x37de, 0x00},
	{0x37df, 0x00},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x00},
	{0x3804, 0x10},
	{0x3805, 0x9f},
	{0x3806, 0x0b},
	{0x3807, 0xc7},
	{0x3808, 0x10},
	{0x3809, 0x80},
	{0x380a, 0x0b},
	{0x380b, 0xb8},
	{0x380c, 0x11},
	{0x380d, 0x50},
	{0x380e, 0x0e},
	{0x380f, 0x00},
	{0x3810, 0x00},
	{0x3811, 0x10},
	{0x3812, 0x00},
	{0x3813, 0x08},
	{0x3814, 0x11},
	{0x3815, 0x11},
	{0x3820, 0x10},
	{0x3821, 0x0e},
	{0x3823, 0x00},
	{0x3824, 0x00},
	{0x3825, 0x00},
	{0x3826, 0x00},
	{0x3827, 0x00},
	{0x3829, 0x0b},
	{0x382b, 0x6a},
	{0x4000, 0x18},
	{0x4001, 0x06},
	{0x4002, 0x45},
	{0x4004, 0x08},
	{0x4005, 0x19},
//  {0x4006, 0x20},
	{0x4007, 0x90},
  {0x4008, 0x24},
  {0x4009, 0x10},
  {0x400c, 0x00},
  {0x400d, 0x00},
  {0x404e, 0x37},
  {0x404f, 0x8f},
  {0x4058, 0x40},
	{0x4100, 0x2d},
	{0x4101, 0x22},
	{0x4102, 0x04},
	{0x4104, 0x5c},
	{0x4109, 0xa3},
	{0x410a, 0x03},
	{0x4300, 0xff},
	{0x4303, 0x00},
	{0x4304, 0x08},
	{0x4307, 0x30},
	{0x4311, 0x04},
	{0x4511, 0x05},
	{0x4816, 0x52},
	{0x481f, 0x30},
	{0x4826, 0x2c},
	{0x4a00, 0xaa},
	{0x4a03, 0x01},
	{0x4a05, 0x08},
	{0x4d00, 0x05},
	{0x4d01, 0x19},
	{0x4d02, 0xfd},
	{0x4d03, 0xd1},
	{0x4d04, 0xff},
	{0x4d05, 0xff},
	{0x4d07, 0x04},
	{0x4837, 0x0e},
  {0x484b, 0x05},
  {0x5000, 0x06},
	{0x5001, 0x01},
	{0x5002, 0x00},
	{0x5003, 0x21},
	{0x5043, 0x48},
	{0x5013, 0x80},
	{0x501f, 0x00},
	{0x5780, 0xfc},
{0x5781, 0x13},
{0x5782, 0x03},
{0x5786, 0x20},
{0x5787, 0x40},
{0x5788, 0x08},
{0x5789, 0x08},
{0x578a, 0x02},
{0x578b, 0x01},
{0x578c, 0x01},
{0x578d, 0x0c},
{0x578e, 0x02},
{0x578f, 0x01},
{0x5790, 0x01},
	{0x5e00, 0x00},
	{0x5a01, 0x00},
	{0x5a02, 0x00},
	{0x5a03, 0x00},
	{0x5a04, 0x10},
	{0x5a05, 0xa0},
	{0x5a06, 0x0c},
	{0x5a07, 0x78},
	{0x5a08, 0x00},
  {0x5b05, 0x6c},
	{0x5e00, 0x00},
	{0x5e01, 0x41},
	{0x5e11, 0x30},
{0x3717,01},
{0x373f,02},
{0x3755,00},
{0x3758,00},
{0x373a,50},
{0x3795,00},
{0x3796,84},

	{OV12830_TABLE_END, 0x0000}
};
#endif

#ifdef OV12830_ENABLE_4224x3000_NIGHT
static struct ov12830_reg ov12830_4224x3000_i2c_NIGHT[] = {
	{OV12830_TABLE_RESET, 0},
	{0x3001, 0x06},
	{0x3002, 0x80},
	{0x3011, 0x41},
	{0x3014, 0x16},
	{0x3015, 0x0b},
	{0x3022, 0x03},
	{0x3090, 0x04},
	{0x3091, 0x11},
	{0x3092, 0x00},
	{0x3093, 0x00},
	{0x3098, 0x03},
	{0x3099, 0x11},
	{0x309c, 0x01},
	{0x30b3, 0x4a},
	{0x30b4, 0x03},
	{0x30b5, 0x04},
	{0x3106, 0x01},
	{0x3304, 0x28},
	{0x3305, 0x41},
	{0x3306, 0x30},
	{0x3308, 0x00},
	{0x3309, 0xc8},
	{0x330a, 0x01},
	{0x330b, 0x90},
	{0x330c, 0x02},
	{0x330d, 0x58},
	{0x330e, 0x03},
	{0x330f, 0x20},
	{0x3300, 0x00},
	{0x3500, 0x00},
	{0x3501, 0xbd},
	{0x3502, 0x40},
	{0x3503, 0x07},
	{0x350a, 0x00},
	{0x350b, 0x80},
  {0x3602, 0x18},
	{0x3612, 0x80},
  {0x3620, 0x64},
  {0x3621, 0xb5},
  {0x3622, 0x09},
  {0x3623, 0x28},
	{0x3631, 0xb3},
	{0x3634, 0x04},
	{0x3660, 0x80},
	{0x3662, 0x10},
	{0x3663, 0xf0},
	{0x3667, 0x00},
	{0x366f, 0x20},
	{0x3680, 0xb5},
	{0x3682, 0x00},
	{0x3701, 0x12},
  {0x3702, 0x88},
	{0x3708, 0xe3},
	{0x3709, 0xc3},
  {0x370b, 0xa0},
 // {0x370c,0x0c},
	{0x370d, 0x11},
	{0x370e, 0x00},
	{0x371c, 0x01},
	{0x371f, 0x1b},
  {0x3724, 0x10},
	{0x3726, 0x00},
	{0x372a, 0x09},
  {0x3739, 0xb0},
	{0x373c, 0x40},
	{0x376b, 0x44},
  {0x3774, 0x10},
	{0x377b, 0x44},
	{0x3780, 0x22},
	{0x3781, 0xc8},
	{0x3783, 0x31},
	{0x3786, 0x16},
	{0x3787, 0x02},
	{0x3796, 0x84},
	{0x379c, 0x0c},
	{0x37c5, 0x00},
	{0x37c6, 0x00},
	{0x37c7, 0x00},
	{0x37c9, 0x00},
	{0x37ca, 0x00},
	{0x37cb, 0x00},
	{0x37cc, 0x00},
	{0x37cd, 0x00},
	{0x37ce, 0x10},
	{0x37cf, 0x00},
	{0x37d0, 0x00},
	{0x37d1, 0x00},
	{0x37d2, 0x00},
	{0x37de, 0x00},
	{0x37df, 0x00},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x00},
	{0x3804, 0x10},
	{0x3805, 0x9f},
	{0x3806, 0x0b},
	{0x3807, 0xc7},
	{0x3808, 0x10},
	{0x3809, 0x80},
	{0x380a, 0x0b},
	{0x380b, 0xb8},
	{0x380c, 0x11},
	{0x380d, 0x50},
	{0x380e, 0x0e},
	{0x380f, 0x00},
	{0x3810, 0x00},
	{0x3811, 0x10},
	{0x3812, 0x00},
	{0x3813, 0x08},
	{0x3814, 0x11},
	{0x3815, 0x11},
	{0x3820, 0x10},
	{0x3821, 0x0e},
	{0x3823, 0x00},
	{0x3824, 0x00},
	{0x3825, 0x00},
	{0x3826, 0x00},
	{0x3827, 0x00},
	{0x3829, 0x0b},
	{0x382b, 0x6a},
	{0x4000, 0x18},
	{0x4001, 0x06},
	{0x4002, 0x45},
	{0x4004, 0x08},
	{0x4005, 0x19},
//  {0x4006, 0x20},
	{0x4007, 0x90},
  {0x4008, 0x24},
  {0x4009, 0x10},
  {0x400c, 0x00},
  {0x400d, 0x00},
  {0x404e, 0x37},
  {0x404f, 0x8f},
  {0x4058, 0x40},
	{0x4100, 0x2d},
	{0x4101, 0x22},
	{0x4102, 0x04},
	{0x4104, 0x5c},
	{0x4109, 0xa3},
	{0x410a, 0x03},
	{0x4300, 0xff},
	{0x4303, 0x00},
	{0x4304, 0x08},
	{0x4307, 0x30},
	{0x4311, 0x04},
	{0x4511, 0x05},
	{0x4816, 0x52},
	{0x481f, 0x30},
	{0x4826, 0x2c},
	{0x4a00, 0xaa},
	{0x4a03, 0x01},
	{0x4a05, 0x08},
	{0x4d00, 0x05},
	{0x4d01, 0x19},
	{0x4d02, 0xfd},
	{0x4d03, 0xd1},
	{0x4d04, 0xff},
	{0x4d05, 0xff},
	{0x4d07, 0x04},
	{0x4837, 0x0e},
  {0x484b, 0x05},
  {0x5000, 0x06},
	{0x5001, 0x01},
	{0x5002, 0x00},
	{0x5003, 0x21},
	{0x5043, 0x48},
	{0x5013, 0x80},
	{0x501f, 0x00},
	{0x5780, 0xfc},
{0x5781, 0x13},
{0x5782, 0x03},
{0x5786, 0x20},
{0x5787, 0x40},
{0x5788, 0x08},
{0x5789, 0x08},
{0x578a, 0x02},
{0x578b, 0x01},
{0x578c, 0x01},
{0x578d, 0x0c},
{0x578e, 0x02},
{0x578f, 0x01},
{0x5790, 0x01},
	{0x5e00, 0x00},
	{0x5a01, 0x00},
	{0x5a02, 0x00},
	{0x5a03, 0x00},
	{0x5a04, 0x10},
	{0x5a05, 0xa0},
	{0x5a06, 0x0c},
	{0x5a07, 0x78},
	{0x5a08, 0x00},
  {0x5b05, 0x6c},
	{0x5e00, 0x00},
	{0x5e01, 0x41},
	{0x5e11, 0x30},
{0x3717,01},
{0x373f,02},
{0x3755,00},
{0x3758,00},
{0x373a,50},
{0x3795,00},
{0x3796,84},

	{OV12830_TABLE_END, 0x0000}
};
#endif
/* Each resolution requires the below data table setup and the corresponding
 * I2C data table.
 * If more NVC data is needed for the NVC driver, be sure and modify the
 * nvc_imager_nvc structure in nvc_imager.h
 * If more data sets are needed per resolution, they can be added to the
 * table format below with the ov12830_mode_data structure.  New data sets
 * should conform to an already defined NVC structure.  If it's data for the
 * NVC driver, then it should be added to the nvc_imager_nvc structure.
 * Steps to add a resolution:
 * 1. Add I2C data table
 * 2. Add ov12830_mode_data table
 * 3. Add entry to the ov12830_mode_table
 */
 #ifdef OV12830_ENABLE_2112x1500
static struct ov12830_mode_data ov12830_2112x1500 = {
	.sensor_mode = {
		.res_x			= 2112,
		.res_y			= 1500,
		.active_start_x		= 0,
		.active_stary_y		= 0,
		.peak_frame_rate	= 30000, /* / _INT2FLOAT_DIVISOR */
		.pixel_aspect_ratio	= 1000, /* / _INT2FLOAT_DIVISOR */
		.pll_multiplier		= 12333, /* / _INT2FLOAT_DIVISOR */
		.crop_mode		= NVC_IMAGER_CROPMODE_NONE,
	},
	.sensor_dnvc = {
		.api_version		= NVC_IMAGER_API_DYNAMIC_VER,
		.region_start_x		= 0,
		.region_start_y		= 0,
		.x_scale		= 1,
		.y_scale		= 1,
		.bracket_caps		= 0,
		.flush_count		= 0,
		.init_intra_frame_skip	= 0,
		.ss_intra_frame_skip	= 0,
		.ss_frame_number	= 0,
		.coarse_time		= 0xbd4, /* reg 0x3500,0x3501,0x3502 */
		.max_coarse_diff	= 16,
		.min_exposure_course	= 1,
		.max_exposure_course	= 0xFFFF,
		.diff_integration_time	= 0, /* / _INT2FLOAT_DIVISOR */
		.line_length		= 0xB8A, /* reg 0x380c,0x380d */
		.frame_length		= 0xbe4, /* reg 0x380e,0x380f */
		.min_frame_length	= 0xbe4,
		.max_frame_length	= 0xFFFF,
		.min_gain		= 1.0 * 1000 / 16,
						/* / _INT2FLOAT_DIVISOR */
		.max_gain		= 256.0 * 1000 / 16,
						/* / _INT2FLOAT_DIVISOR */
		.inherent_gain		= 1300, /* / _INT2FLOAT_DIVISOR */
		.inherent_gain_bin_en	= 1000, /* / _INT2FLOAT_DIVISOR */
		.support_bin_control	= 0,
		.support_fast_mode	= 0,
		.pll_mult		= 25,
		.pll_div		= 1,
		.mode_sw_wait_frames	= 1500, /* / _INT2FLOAT_DIVISOR */
	},
	.p_mode_i2c			= ov12830_2112x1500_i2c,
};
#endif
 #ifdef OV12830_ENABLE_1920x1080
 static struct ov12830_mode_data ov12830_1920x1080 = {
	.sensor_mode = {
		.res_x			= 1920,
		.res_y			= 1080,
		.active_start_x		= 0,
		.active_stary_y		= 0,
		.peak_frame_rate	= 30000, /* / _INT2FLOAT_DIVISOR */
		.pixel_aspect_ratio	= 1000, /* / _INT2FLOAT_DIVISOR */
		.pll_multiplier		= 8000, /* / _INT2FLOAT_DIVISOR */
		.crop_mode		= NVC_IMAGER_CROPMODE_NONE,
	},
	.sensor_dnvc = {
		.api_version		= NVC_IMAGER_API_DYNAMIC_VER,
		.region_start_x		= 0,
		.region_start_y		= 0,
		.x_scale		= 1,
		.y_scale		= 1,
		.bracket_caps		= 0,
		.flush_count		= 0,
		.init_intra_frame_skip	= 0,
		.ss_intra_frame_skip	= 0,
		.ss_frame_number	= 0,
		.coarse_time		= 0x702,//0x6f8, /* reg 0x3500,0x3501,0x3502 */
		.max_coarse_diff	= 16,
		.min_exposure_course	= 1,
		.max_exposure_course	= 0xFFFF,
		.diff_integration_time	= 0, /* / _INT2FLOAT_DIVISOR */
		.line_length		= 0xb8a, /* reg 0x380c,0x380d */
		.frame_length		= 0x70c, //0x974, /* reg 0x380e,0x380f */
		.min_frame_length	= 0x70c, //0x974,
		.max_frame_length	= 0xFFFF,
		.min_gain		= 1.0 * 1000 / 16,
						/* / _INT2FLOAT_DIVISOR */
		.max_gain		= 256.0 * 1000 / 16,
						/* / _INT2FLOAT_DIVISOR */
		.inherent_gain		= 1000, /* / _INT2FLOAT_DIVISOR */
		.inherent_gain_bin_en	= 1000, /* / _INT2FLOAT_DIVISOR */
		.support_bin_control	= 0,
		.support_fast_mode	= 0,
		.pll_mult		= 20,//25,
		.pll_div		= 3,//1,
		.mode_sw_wait_frames	= 1500, /* / _INT2FLOAT_DIVISOR */
	},
	.p_mode_i2c			= ov12830_1920x1080_i2c,
};
 #endif
#ifdef OV12830_ENABLE_2116x1504
static struct ov12830_mode_data ov12830_2116x1504 = {
	.sensor_mode = {
		.res_x			= 2116,
		.res_y			= 1504,
		.active_start_x		= 0,
		.active_stary_y		= 0,
		.peak_frame_rate	= 30000, /* / _INT2FLOAT_DIVISOR */
		.pixel_aspect_ratio	= 1000, /* / _INT2FLOAT_DIVISOR */
		.pll_multiplier		= 12333, /* / _INT2FLOAT_DIVISOR */
		.crop_mode		= NVC_IMAGER_CROPMODE_NONE,
	},
	.sensor_dnvc = {
		.api_version		= NVC_IMAGER_API_DYNAMIC_VER,
		.region_start_x		= 0,
		.region_start_y		= 0,
		.x_scale		= 1,
		.y_scale		= 1,
		.bracket_caps		= 0,
		.flush_count		= 0,
		.init_intra_frame_skip	= 0,
		.ss_intra_frame_skip	= 0,
		.ss_frame_number	= 0,
		.coarse_time		= 0x702, /* reg 0x3500,0x3501,0x3502 */
		.max_coarse_diff	= 16,
		.min_exposure_course	= 1,
		.max_exposure_course	= 0xFFFF,
		.diff_integration_time	= 0, /* / _INT2FLOAT_DIVISOR */
		.line_length		= 0xb8a, /* reg 0x380c,0x380d */
		.frame_length		= 0x70c, /* reg 0x380e,0x380f */
		.min_frame_length	= 0x70c,
		.max_frame_length	= 0xFFFF,
		.min_gain		= 1.0 * 1000 / 16,
						/* / _INT2FLOAT_DIVISOR */
		.max_gain		= 256.0 * 1000 / 16,
						/* / _INT2FLOAT_DIVISOR */
		.inherent_gain		= 1000, /* / _INT2FLOAT_DIVISOR */
		.inherent_gain_bin_en	= 1000, /* / _INT2FLOAT_DIVISOR */
		.support_bin_control	= 0,
		.support_fast_mode	= 0,
              .pll_mult		= 20,//25,
		.pll_div		= 3,//1,
		.mode_sw_wait_frames	= 1500, /* / _INT2FLOAT_DIVISOR */
	},
	.p_mode_i2c			= ov12830_2116x1504_i2c,
};
#endif
#ifdef OV12830_ENABLE_4224x3000
static struct ov12830_mode_data ov12830_4224x3000 = {
	.sensor_mode = {
		.res_x			= 4224,
		.res_y			= 3000,
		.active_start_x		= 0,
		.active_stary_y		= 0,
		.peak_frame_rate	= 15000, /* / _INT2FLOAT_DIVISOR */
		.pixel_aspect_ratio	= 1000, /* / _INT2FLOAT_DIVISOR */
		.pll_multiplier		= 12333, /* / _INT2FLOAT_DIVISOR */
		.crop_mode		= NVC_IMAGER_CROPMODE_NONE,
	},
	.sensor_dnvc = {
		.api_version		= NVC_IMAGER_API_DYNAMIC_VER,
		.region_start_x		= 0,
		.region_start_y		= 0,
		.x_scale		= 1,
		.y_scale		= 1,
		.bracket_caps		= 0,
		.flush_count		= 0,
		.init_intra_frame_skip	= 0,
		.ss_intra_frame_skip	= 0,
		.ss_frame_number	= 0,
		.coarse_time		= 0xbd4, /* reg 0x3500,0x3501,0x3502 */
		.max_coarse_diff	= 16,
		.min_exposure_course	= 1,
		.max_exposure_course	= 0xFFFF,
		.diff_integration_time	= 0, /* / _INT2FLOAT_DIVISOR */
		.line_length		= 0x1150, /* reg 0x380c,0x380d */
		.frame_length		= 0xe00, /* reg 0x380e,0x380f */
		.min_frame_length	= 0xe00,
		.max_frame_length	= 0xFFFF,
		.min_gain		= 1.0 * 1000 / 16,
						/* / _INT2FLOAT_DIVISOR */
		.max_gain		= 256.0 * 1000 / 16,
						/* / _INT2FLOAT_DIVISOR */
		.inherent_gain		= 1000, /* / _INT2FLOAT_DIVISOR */
		.inherent_gain_bin_en	= 1000, /* / _INT2FLOAT_DIVISOR */
		.support_bin_control	= 0,
		.support_fast_mode	= 0,
		.pll_mult		= 20,
		.pll_div		= 2,
		.mode_sw_wait_frames	= 1500, /* / _INT2FLOAT_DIVISOR */
	},
	.p_mode_i2c			= ov12830_4224x3000_i2c,
};
#endif

#ifdef OV12830_ENABLE_4224x3000_NIGHT
static struct ov12830_mode_data ov12830_4224x3000_NIGHT = {
	.sensor_mode = {
		.res_x			= 4224,
		.res_y			= 3000,
		.active_start_x		= 0,
		.active_stary_y		= 0,
		.peak_frame_rate	= 15000, /* / _INT2FLOAT_DIVISOR */
		.pixel_aspect_ratio	= 1000, /* / _INT2FLOAT_DIVISOR */
		.pll_multiplier		= 12333, /* / _INT2FLOAT_DIVISOR */
		.crop_mode		= NVC_IMAGER_CROPMODE_NONE,
	},
	.sensor_dnvc = {
		.api_version		= NVC_IMAGER_API_DYNAMIC_VER,
		.region_start_x		= 0,
		.region_start_y		= 0,
		.x_scale		= 1,
		.y_scale		= 1,
		.bracket_caps		= 0,
		.flush_count		= 0,
		.init_intra_frame_skip	= 0,
		.ss_intra_frame_skip	= 0,
		.ss_frame_number	= 0,
		.coarse_time		= 0xbd4, /* reg 0x3500,0x3501,0x3502 */
		.max_coarse_diff	= 16,
		.min_exposure_course	= 1,
		.max_exposure_course	= 0xFFFF,
		.diff_integration_time	= 0, /* / _INT2FLOAT_DIVISOR */
		.line_length		= 0x1150, /* reg 0x380c,0x380d */
		.frame_length		= 0xe00, /* reg 0x380e,0x380f */
		.min_frame_length	= 0xe00,
		.max_frame_length	= 0xFFFF,
		.min_gain		= 1.0 * 1000 / 16,
						/* / _INT2FLOAT_DIVISOR */
		.max_gain		= 256.0 * 1000 / 16,
						/* / _INT2FLOAT_DIVISOR */
		.inherent_gain		= 1000, /* / _INT2FLOAT_DIVISOR */
		.inherent_gain_bin_en	= 1000, /* / _INT2FLOAT_DIVISOR */
		.support_bin_control	= 0,
		.support_fast_mode	= 0,
		.pll_mult		= 20,
		.pll_div		= 4,
		.mode_sw_wait_frames	= 1500, /* / _INT2FLOAT_DIVISOR */
	},
	.p_mode_i2c			= ov12830_4224x3000_i2c_NIGHT,
};
#endif

static struct ov12830_mode_data *ov12830_mode_table[] = {
	[0] =
#ifdef OV12830_ENABLE_4224x3000
	&ov12830_4224x3000,
#endif
#ifdef OV12830_ENABLE_1920x1080
	&ov12830_1920x1080,
#endif
#ifdef OV12830_ENABLE_2116x1504
	&ov12830_2116x1504,
#endif
#ifdef OV12830_ENABLE_4224x3000_NIGHT
	&ov12830_4224x3000_NIGHT,
#endif
};


static int ov12830_i2c_rd8(struct ov12830_info *info, u16 reg, u8 *val)
{
	struct i2c_msg msg[2];
	u8 buf[3];


	buf[0] = (reg >> 8);
	buf[1] = (reg & 0x00FF);
	msg[0].addr = info->i2c_client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = &buf[0];
	msg[1].addr = info->i2c_client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = &buf[2];
	*val = 0;
	if (i2c_transfer(info->i2c_client->adapter, msg, 2) != 2)
		return -EIO;

	*val = buf[2];
	return 0;
}

static int ov12830_i2c_rd16(struct ov12830_info *info, u16 reg, u16 *val)
{
	struct i2c_msg msg[2];
	u8 buf[4];

	buf[0] = (reg >> 8);
	buf[1] = (reg & 0x00FF);
	msg[0].addr = info->i2c_client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = &buf[0];
	msg[1].addr = info->i2c_client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = &buf[2];
	*val = 0;
	if (i2c_transfer(info->i2c_client->adapter, msg, 2) != 2)
		return -EIO;

	*val = (((u16)buf[2] << 8) | (u16)buf[3]);
	return 0;
}

static int ov12830_i2c_wr8(struct ov12830_info *info, u16 reg, u8 val)
{
	struct i2c_msg msg;
	u8 buf[3];

	buf[0] = (reg >> 8);
	buf[1] = (reg & 0x00FF);
	buf[2] = val;
	msg.addr = info->i2c_client->addr;
	msg.flags = 0;
	msg.len = 3;
	msg.buf = &buf[0];
	if (i2c_transfer(info->i2c_client->adapter, &msg, 1) != 1)
		return -EIO;

	return 0;
}

static int ov12830_i2c_rd_table(struct ov12830_info *info,
				struct ov12830_reg table[])
{
	struct ov12830_reg *p_table = table;
	u8 val;
	int err = 0;

	while (p_table->addr != OV12830_TABLE_END) {
		err = ov12830_i2c_rd8(info, p_table->addr, &val);
		if (err)
			return err;

		p_table->val = (u16)val;
		p_table++;
	}

	return err;
}

static int ov12830_i2c_wr_blk(struct ov12830_info *info, u8 *buf, int len)
{
	struct i2c_msg msg;

	msg.addr = info->i2c_client->addr;
	msg.flags = 0;
	msg.len = len;
	msg.buf = buf;
	if (i2c_transfer(info->i2c_client->adapter, &msg, 1) != 1)
		return -EIO;

	return 0;
}
extern int g_tmd2771x_lux;
static int ov12830_i2c_wr_table(struct ov12830_info *info,
				struct ov12830_reg table[])
{
	int err;
	const struct ov12830_reg *next;
	const struct ov12830_reg *n_next;
	u8 *b_ptr = info->i2c_buf;
	u16 buf_count = 0;
	u8 reset_status = 1;
	u8 reset_tries_left = OV12830_TABLE_RESET_TIMEOUT;

	for (next = table; next->addr != OV12830_TABLE_END; next++) {
		if (next->addr == OV12830_TABLE_WAIT_MS) {
			msleep(next->val);
			continue;
		} else if (next->addr == OV12830_TABLE_RESET) {
			err = ov12830_i2c_wr8(info, 0x0103, 0x01);
			if (err)
				return err;
			while (reset_status) {
				usleep_range(200, 300);
				if (reset_tries_left < 1)
					return -EIO;
				err = ov12830_i2c_rd8(info, 0x0103,
							&reset_status);
				if (err)
					return err;
				reset_status &= 0x01;
				reset_tries_left -= 1;
			}
			continue;
		}

		if (!buf_count) {
			b_ptr = info->i2c_buf;
			*b_ptr++ = next->addr >> 8;
			*b_ptr++ = next->addr & 0xFF;
			buf_count = 2;
		}
		*b_ptr++ = next->val;
		buf_count++;
		n_next = next + 1;
		if (n_next->addr == next->addr + 1 &&
				n_next->addr != OV12830_TABLE_WAIT_MS &&
				buf_count < OV12830_SIZEOF_I2C_BUF &&
				n_next->addr != OV12830_TABLE_RESET &&
				n_next->addr != OV12830_TABLE_END)
			continue;

		err = ov12830_i2c_wr_blk(info, info->i2c_buf, buf_count);
		if (err)
			return err;

		buf_count = 0;
	}

	return 0;
}


static inline void ov12830_frame_length_reg(struct ov12830_reg *regs,
					u32 frame_length)
{
	if(g_cam_status == 2)
	{
		if(frame_length > 0x1700)
			g_ov12830_nightmode = 1;
		else
			g_ov12830_nightmode = 0;	
	}

       printk("OV12830 _frame_length_reg frame_length %x g_cam_status %d g_ov12830_nightmode %d\n", frame_length, g_cam_status, g_ov12830_nightmode);
	regs->addr = 0x380e;
	regs->val = (frame_length >> 8) & 0xff;
	(regs + 1)->addr = 0x380f;
	(regs + 1)->val = (frame_length) & 0xff;
}

static inline void ov12830_coarse_time_reg(struct ov12830_reg *regs,
					u32 coarse_time)
{
	printk("OV12830 _coarse_time_reg coarse_time %x\n", coarse_time);
	regs->addr = 0x3500;
	regs->val = (coarse_time >> 12) & 0x0F;
	(regs + 1)->addr = 0x3501;
	(regs + 1)->val = (coarse_time >> 4) & 0xFF;
	(regs + 2)->addr = 0x3502;
	(regs + 2)->val = (coarse_time << 4) & 0xFF;
}

static inline void ov12830_gain_reg(struct ov12830_reg *regs, u32 gain)
{
	(regs)->addr = 0x350a;
	(regs)->val = (gain >> 8) & 0x7;
	(regs + 1)->addr = 0x350b;
	(regs + 1)->val = gain & 0xFF;
}

static int ov12830_bin_wr(struct ov12830_info *info, u8 enable)
{
	int err = 0;

	if (enable == info->bin_en)
		return 0;

	if (!info->mode_valid || !ov12830_mode_table[info->mode_index]->
				sensor_dnvc.support_bin_control)
		return -EINVAL;

	if (!err)
		info->bin_en = enable;
	dev_dbg(&info->i2c_client->dev, "%s bin_en=%x err=%d\n",
		__func__, info->bin_en, err);
	return err;
}

static int ov12830_exposure_wr(struct ov12830_info *info,
				struct nvc_imager_bayer *mode)
{
	struct ov12830_reg reg_list[8];
	int err;

	ov12830_frame_length_reg(reg_list, mode->frame_length);
	ov12830_coarse_time_reg(reg_list + 2, mode->coarse_time);
	ov12830_gain_reg(reg_list + 5, mode->gain);
	reg_list[7].addr = OV12830_TABLE_END;
	err = ov12830_i2c_wr_table(info, reg_list);
	if (!err)
		err = ov12830_bin_wr(info, mode->bin_en);
	return err;
}

static int ov12830_gain_wr(struct ov12830_info *info, u32 gain)
{
	int err;
	err = ov12830_i2c_wr8(info, 0x350a, (u8)(gain >> 8) & 0x7);
	err |= ov12830_i2c_wr8(info, 0x350b, (u8)(gain & 0xFF));
	return err;
}

static int ov12830_gain_rd(struct ov12830_info *info, u32 *gain)
{
	int err;

	*gain = 0;
	err = ov12830_i2c_rd16(info, 0x350a, (u16 *)gain);
	return err;
}

static int ov12830_group_hold_wr(struct ov12830_info *info,
				struct nvc_imager_ae *ae)
{
	int err = 0;
	bool groupHoldEnable;
	struct ov12830_reg reg_list[8];
	int count = 0;
	int offset = 0;

	if (ae->gain_enable)
		count += 1;
	if (ae->coarse_time_enable)
		count += 1;
	if (ae->frame_length_enable)
		count += 1;
	groupHoldEnable = (count > 1) ? 1 : 0;

	if (groupHoldEnable)
		err |= ov12830_i2c_wr8(info, 0x3208, 0x01);

	if (ae->gain_enable) {
		ov12830_gain_reg(reg_list + offset, ae->gain);
		offset += 2;
	}
	if (ae->frame_length_enable) {
		ov12830_frame_length_reg(reg_list + offset, ae->frame_length);
		offset += 2;
	}
	if (ae->coarse_time_enable) {
		ov12830_coarse_time_reg(reg_list + offset, ae->coarse_time);
		offset += 3;
	}
	reg_list[offset].addr = OV12830_TABLE_END;
	err |= ov12830_i2c_wr_table(info, reg_list);

	if (groupHoldEnable) {
		err |= ov12830_i2c_wr8(info, 0x3208, 0x11);
		err |= ov12830_i2c_wr8(info, 0x3208, 0xa1);
	}

	return err;
}

static int ov12830_test_pattern_wr(struct ov12830_info *info, unsigned pattern)
{
	if (pattern >= ARRAY_SIZE(test_patterns))
		return -EINVAL;

	return ov12830_i2c_wr_table(info, test_patterns[pattern]);
}

static int ov12830_gpio_rd(struct ov12830_info *info,
			enum ov12830_gpio_type type)
{
	int val = -EINVAL;

	if (info->gpio[type].gpio) {
		val = gpio_get_value_cansleep(info->gpio[type].gpio);
		dev_dbg(&info->i2c_client->dev, "%s %u %d\n",
			__func__, info->gpio[type].gpio, val);
		if (!info->gpio[type].active_high)
			val = !val;
		val &= 1;
	}
	return val; /* return read value or error */
}

static int ov12830_gpio_wr(struct ov12830_info *info,
			enum ov12830_gpio_type type,
			int val) /* val: 0=deassert, 1=assert */
{
	int err = -EINVAL;

	if (info->gpio[type].gpio) {
		if (!info->gpio[type].active_high)
			val = !val;
		val &= 1;
		err = val;
		gpio_set_value_cansleep(info->gpio[type].gpio, val);
		dev_dbg(&info->i2c_client->dev, "%s %u %d\n",
			__func__, info->gpio[type].gpio, val);
	}
	return err; /* return value written or error */
}

static void ov12830_gpio_shutdn(struct ov12830_info *info, int val)
{
	ov12830_gpio_wr(info, OV12830_GPIO_TYPE_SHTDN, val);
}

static void ov12830_gpio_pwrdn(struct ov12830_info *info, int val)
{
	int prev_val;

	prev_val = ov12830_gpio_rd(info, OV12830_GPIO_TYPE_PWRDN);
	if ((prev_val < 0) || (val == prev_val))
		return;

	ov12830_gpio_wr(info, OV12830_GPIO_TYPE_PWRDN, val);
	if (!val && prev_val)
		/* if transition from assert to deassert then delay for I2C */
		msleep(OV12830_STARTUP_DELAY_MS);
}

static void ov12830_gpio_exit(struct ov12830_info *info)
{
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(ov12830_gpio); i++) {
		if (info->gpio[i].gpio && info->gpio[i].own)
			gpio_free(info->gpio[i].gpio);
	}
}

static void ov12830_gpio_init(struct ov12830_info *info)
{
	char label[32];
	unsigned long flags;
	unsigned type;
	unsigned i;
	unsigned j;
	int err;

	if (!info->pdata->gpio_count || !info->pdata->gpio)
		return;

	for (i = 0; i < ARRAY_SIZE(ov12830_gpio); i++) {
		type = ov12830_gpio[i].gpio_type;
		for (j = 0; j < info->pdata->gpio_count; j++) {
			if (type == info->pdata->gpio[j].gpio_type)
				break;
		}
		if (j == info->pdata->gpio_count)
			continue;

		info->gpio[type].gpio = info->pdata->gpio[j].gpio;
		if (ov12830_gpio[i].use_flags) {
			flags = ov12830_gpio[i].flags;
			info->gpio[type].active_high =
						ov12830_gpio[i].active_high;
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

		snprintf(label, sizeof(label), "ov12830_%u_%s",
			 info->pdata->num, ov12830_gpio[i].label);
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

static int ov12830_power_off(struct ov12830_info *info)
{
	struct ov12830_power_rail *pw = &info->regulators;
	int err = 0;

	if (!info->power_on)
		goto ov12830_poweroff_skip;

	if (info->pdata && info->pdata->power_off)
		err = info->pdata->power_off(pw);
	/* if customized design handles the power off process specifically,
	* return is bigger than 0 (normally 1), otherwise 0 or error num.
	*/
	if (err > 0) {
		info->power_on = false;
		return 0;
	}

	if (!err) {
		ov12830_gpio_pwrdn(info, 1);
		ov12830_gpio_shutdn(info, 1);
		if (pw->avdd)
			WARN_ON(IS_ERR_VALUE(
				err = regulator_disable(pw->avdd)));
		if (pw->dvdd)
			WARN_ON(IS_ERR_VALUE(
				err |= regulator_disable(pw->dvdd)));
		if (pw->dovdd)
			WARN_ON(IS_ERR_VALUE(
				err |= regulator_disable(pw->dovdd)));
	}

	if (!err)
		info->power_on = false;

ov12830_poweroff_skip:
	return err;
}

static int ov12830_power_on(struct ov12830_info *info, bool standby)
{
	struct ov12830_power_rail *pw = &info->regulators;
	int err = 0;

	if (info->power_on)
		goto ov12830_poweron_skip;

	if (info->pdata && info->pdata->power_on)
		err = info->pdata->power_on(pw);
	/* if customized design handles the power on process specifically,
	* return is bigger than 0 (normally 1), otherwise 0 or error num.
	*/
	if (!err) {
		if (pw->dvdd)
			WARN_ON(IS_ERR_VALUE(
				err = regulator_enable(pw->dvdd)));
		if (pw->dovdd)
			WARN_ON(IS_ERR_VALUE(
				err |= regulator_enable(pw->dovdd)));
		if (pw->avdd)
			WARN_ON(IS_ERR_VALUE(
				err |= regulator_enable(pw->avdd)));
		ov12830_gpio_shutdn(info, 0);
		ov12830_gpio_pwrdn(info, 0); /* PWRDN off to access I2C */
	}
	if (IS_ERR_VALUE(err))
		return err;
	info->power_on = true;
	err = 0;

ov12830_poweron_skip:
	return err;
}

static void ov12830_edp_lowest(struct ov12830_info *info)
{
	if (!info->edpc)
		return;

	info->edp_state = info->edpc->num_states - 1;
	dev_dbg(&info->i2c_client->dev, "%s %d\n", __func__, info->edp_state);
	if (edp_update_client_request(info->edpc, info->edp_state, NULL)) {
		dev_err(&info->i2c_client->dev, "THIS IS NOT LIKELY HAPPEN!\n");
		dev_err(&info->i2c_client->dev,
				"UNABLE TO SET LOWEST EDP STATE!\n");
	}
}

static int ov12830_edp_req(struct ov12830_info *info, unsigned new_state)
{
	unsigned approved;
	int ret = 0;

	if (!info->edpc)
		return 0;

	dev_dbg(&info->i2c_client->dev, "%s %d\n", __func__, new_state);
	ret = edp_update_client_request(info->edpc, new_state, &approved);
	if (ret) {
		dev_err(&info->i2c_client->dev, "E state transition failed\n");
		return ret;
	}

	if (approved > new_state) {
		dev_err(&info->i2c_client->dev, "EDP no enough current\n");
		return -ENODEV;
	}

	info->edp_state = approved;
	return 0;
}
static int ov12830_pm_wr(struct ov12830_info *info, int pwr)
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
		err = ov12830_power_off(info);
		info->mode_valid = false;
		info->bin_en = 0;
		ov12830_edp_lowest(info);
		break;

	case NVC_PWR_STDBY:
		err = ov12830_power_on(info, true);
		break;

	case NVC_PWR_COMM:
	case NVC_PWR_ON:
		err = ov12830_power_on(info, false);
		err = ov12830_edp_req(info, 0);
		if (err) {
			dev_err(&info->i2c_client->dev,
				"%s: ERROR cannot set edp state! %d\n",
				__func__, err);
			goto mode_wr_full_end;
		}
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
mode_wr_full_end:
	return err;
}

static int ov12830_pm_wr_s(struct ov12830_info *info, int pwr)
{
	int err1 = 0;
	int err2 = 0;

	if ((info->s_mode == NVC_SYNC_OFF) ||
			(info->s_mode == NVC_SYNC_MASTER) ||
			(info->s_mode == NVC_SYNC_STEREO))
		err1 = ov12830_pm_wr(info, pwr);
	if ((info->s_mode == NVC_SYNC_SLAVE) ||
			(info->s_mode == NVC_SYNC_STEREO))
		err2 = ov12830_pm_wr(info->s_info, pwr);
	return err1 | err2;
}

static int ov12830_pm_api_wr(struct ov12830_info *info, int pwr)
{
	int err = 0;

	if (!pwr || (pwr > NVC_PWR_ON))
		return 0;

	if (pwr > info->pwr_dev)
		err = ov12830_pm_wr_s(info, pwr);
	if (!err)
		info->pwr_api = pwr;
	else
		info->pwr_api = NVC_PWR_ERR;
	if (info->pdata->cfg & NVC_CFG_NOERR)
		return 0;

	return err;
}

static int ov12830_pm_dev_wr(struct ov12830_info *info, int pwr)
{
	if (info->mode_enable)
		pwr = NVC_PWR_ON;
	if (pwr < info->pwr_api)
		pwr = info->pwr_api;
	return ov12830_pm_wr(info, pwr);
}

static void ov12830_pm_exit(struct ov12830_info *info)
{
	struct ov12830_power_rail *pw = &info->regulators;

	ov12830_pm_wr(info, NVC_PWR_OFF_FORCE);

	if (pw->avdd)
		regulator_put(pw->avdd);
	if (pw->dvdd)
		regulator_put(pw->dvdd);
	if (pw->dovdd)
		regulator_put(pw->dovdd);
	pw->avdd = NULL;
	pw->dvdd = NULL;
	pw->dovdd = NULL;

	ov12830_gpio_exit(info);
}

static int ov12830_regulator_get(
	struct ov12830_info *info, struct regulator **vreg, char vreg_name[])
{
	struct regulator *reg = NULL;
	int err = 0;

	reg = regulator_get(&info->i2c_client->dev, vreg_name);
	if (IS_ERR(reg)) {
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

static void ov12830_pm_init(struct ov12830_info *info)
{
	struct ov12830_power_rail *pw = &info->regulators;

	ov12830_gpio_init(info);

	ov12830_regulator_get(info, &pw->avdd, "avdd");
	ov12830_regulator_get(info, &pw->dvdd, "dvdd");
	ov12830_regulator_get(info, &pw->dovdd, "dovdd");
	info->power_on = false;
}
static int ov12830_reset(struct ov12830_info *info, int level)
{
	int err;

	if (level == NVC_RESET_SOFT) {
		err = ov12830_pm_wr(info, NVC_PWR_COMM);
		err |= ov12830_i2c_wr8(info, 0x0103, 0x01); /* SW reset */
	} else
		err = ov12830_pm_wr(info, NVC_PWR_OFF_FORCE);
	err |= ov12830_pm_wr(info, info->pwr_api);
	return err;
}
//int update_otp_af(struct ov12830_info * info);
static int ov12830_dev_id(struct ov12830_info *info)
{
	u16 val = 0;
	unsigned i;
	int err;
	ov12830_pm_dev_wr(info, NVC_PWR_COMM);
	err = ov12830_i2c_rd16(info, 0x300a, &val);
      //printk(KERN_ERR"kassey %s %s %d id=0x%x\n", __FILE__, __func__, __LINE__, val);
	if (!err) {
		dev_info(&info->i2c_client->dev, "%s found devId: %x\n",
			__func__, val);
		info->sdata.sensor_id_minor = 0;
		for (i = 0; i < ARRAY_SIZE(ov12830_ids); i++) {
			if (val == ov12830_ids[i]) {
				info->sdata.sensor_id_minor = val;
				break;
			}
		}
		if (!info->sdata.sensor_id_minor) {
			err = -ENODEV;
			dev_dbg(&info->i2c_client->dev, "%s No devId match\n",
				__func__);
		}
	}
      // update_otp_af(info);
	ov12830_pm_dev_wr(info, NVC_PWR_OFF);
	return err;
}
int BG_Ratio_Typical = 0x147;
int RG_Ratio_Typical = 0x12f;
int R_gain, G_gain, B_gain, G_gain_R, G_gain_B;

struct otp_struct {
              u8 module_integrator_id;
              u8 lens_id;
              u8 production_year;
              u8 production_month;
              u8 production_day;
              u16 rg_ratio;
              u16 bg_ratio;
              u16 light_rg;
              u16 light_bg;
              u8 user_data[5];
              u8 lenc[62];
};
 int Bu64291_macro,Bu64291_infinity;
 struct otp_struct current_otp_lenc;
int check_otp_af(struct ov12830_info *info,int index)
{
   int i;
   u8 flag;
   int bank,address;
  // printk("zhangduo test %s +++ \n",__func__);
   //select bank index
              bank = 0xc0 | index;
              ov12830_i2c_wr8(info,0x3d84,bank);

              //read otp into buffer
              ov12830_i2c_wr8(info,0x3d81,0x01);
              mdelay(5);

              //read flag
              address = 0x3d00;
              ov12830_i2c_rd8(info,address,&flag);
            //  printk("zhangduo test otp_af_flag is 0x%x",flag);
               //clear otp buffer
              for(i = 0; i < 16; i++){
                         ov12830_i2c_wr8(info,0x3d00+i,0x00);
              }
             // printk("zhangduo test %s --- \n",__func__);
              return flag;
   
}
//index:index of otp group.(1,2,3)
//return:            0,group index is empty
//                     1,group index has invalid data
//                     2,group index has valid data
int check_otp_wb(struct ov12830_info *info,int index)
{
              int  i;
              u8 flag;
              int bank,address;
             // printk("zhangduo test %s +++ \n",__func__);
              //select bank index
              bank = 0xc0 | index;
              ov12830_i2c_wr8(info,0x3d84,bank);

              //read otp into buffer
              ov12830_i2c_wr8(info,0x3d81,0x01);
              mdelay(5);

              //read flag
              address = 0x3d00;
              ov12830_i2c_rd8(info,address,&flag);
              flag = flag & 0xc0;

              //clear otp buffer
              for(i = 0; i < 16; i++){
                         ov12830_i2c_wr8(info,0x3d00+i,0x00);
              }
             // printk("zhangduo test %s --- \n",__func__);
              if(flag == 0x00){
                          return 0;
              }
              else if(flag & 0x80){
                          return 1;
              }
              else{
                          return 2;
              }
}

// index:index of otp group.(1,2,3)
//return:             0,group index is empty
//                      1,group index has invalid data
//                      2,group index has valid data
int check_otp_lenc(struct ov12830_info *info,int index)
{
               int i,bank;
               u8 flag;
               int address;

               //select bank:4,8,12
               bank = 0xc0 | (index * 4);
               ov12830_i2c_wr8(info,0x3d84,bank);

               //read otp into buffer
               ov12830_i2c_wr8(info,0x3d81,0x01);
               mdelay(5);

               //read flag
               address = 0x3d00;
               ov12830_i2c_rd8(info,address,&flag);
               flag = flag & 0xc0;

               //clear otp buffer
               for(i = 0; i< 16; i++){
                              ov12830_i2c_wr8(info,0x3d00+i,0x00);
               }

               if (flag == 0x00){
                              return 0;
               }
               else if (flag & 0x80){
                              return 1;
               }
               else{
                              return 2;
               }
}
int read_otp_af_start(struct ov12830_info *info,int index, struct otp_struct *otp_ptr){
    int bank;
  //  printk("zhangduo test %s +++,start_index is %d \n",__func__,index);
                //select bank index
                bank = 0xc0 | index;
                ov12830_i2c_wr8(info,0x3d84,bank);

                //read otp into buffer
                ov12830_i2c_wr8(info,0x3d81,0x01);
                mdelay(5);
                ov12830_i2c_rd8(info,0x3d0d,&((*otp_ptr).user_data[2]));
                ov12830_i2c_rd8(info,0x3d0f,&((*otp_ptr).user_data[4]));
                return 0;

}

int read_otp_af_max(struct ov12830_info *info,int index, struct otp_struct *otp_ptr){
   int bank;
   // printk("zhangduo test %s +++,max_index is %d \n",__func__,index);
                //select bank index
                bank = 0xc0 | index;
                ov12830_i2c_wr8(info,0x3d84,bank);

                //read otp into buffer
                ov12830_i2c_wr8(info,0x3d81,0x01);
                mdelay(5);
                ov12830_i2c_rd8(info,0x3d0e,&((*otp_ptr).user_data[3]));
                ov12830_i2c_rd8(info,0x3d0f,&((*otp_ptr).user_data[4]));
                return 0;

}
//index:index of otp group.(1,2,3)
//otp_ptr:pointer of otp_struct
//return:            0,
int read_otp_wb(struct ov12830_info *info,int index,struct otp_struct *otp_ptr){
                int i,bank;
                int address;
                u8 temp8,rg8_temp,bg8_temp,light_rg8_temp,light_bg8_temp;
                u16 temp16,rg16_temp,bg16_temp,light_rg16_temp,light_bg16_temp;
                
               // printk("zhangduo test %s +++ \n",__func__);
                //select bank index
                bank = 0xc0 | index;
                ov12830_i2c_wr8(info,0x3d84,bank);

                //read otp into buffer
                ov12830_i2c_wr8(info,0x3d81,0x01);
                mdelay(5);

                address = 0x3d00;
                ov12830_i2c_rd8(info,address+1,&((*otp_ptr).module_integrator_id));
                ov12830_i2c_rd8(info,address+2,&((*otp_ptr).lens_id));
                ov12830_i2c_rd8(info,address+3,&((*otp_ptr).production_year));
                ov12830_i2c_rd8(info,address+4,&((*otp_ptr).production_month));
                ov12830_i2c_rd8(info,address+5,&((*otp_ptr).production_day));
                ov12830_i2c_rd8(info,address+10,&temp8);
                temp16 = temp8;
                ov12830_i2c_rd8(info,address+6,&rg8_temp);
                rg16_temp = rg8_temp;
                (*otp_ptr).rg_ratio = (rg16_temp << 2)+ ((temp16 >> 6) & 0x03);
                ov12830_i2c_rd8(info,address+7,&bg8_temp);
                bg16_temp = bg8_temp;
                (*otp_ptr).bg_ratio = (bg16_temp << 2) + ((temp16 >> 4) & 0x03);
                ov12830_i2c_rd8(info,address+8,&light_rg8_temp);
                light_rg16_temp = light_rg8_temp;
                (*otp_ptr).light_rg = (light_rg16_temp << 2) + ((temp16 >> 2) & 0x03);
                ov12830_i2c_rd8(info,address+9,&light_bg8_temp);
                light_bg16_temp = light_bg8_temp;
                (*otp_ptr).light_bg =( light_bg16_temp << 2) + (temp8  & 0x03);
                ov12830_i2c_rd8(info,address+11,&((*otp_ptr).user_data[0]));
                ov12830_i2c_rd8(info,address+12,&((*otp_ptr).user_data[1]));
                ov12830_i2c_rd8(info,address+13,&((*otp_ptr).user_data[2]));
                ov12830_i2c_rd8(info,address+14,&((*otp_ptr).user_data[3]));
                ov12830_i2c_rd8(info,address+15,&((*otp_ptr).user_data[4]));

                printk("zhangduo test rg_ratio = %d ,bg_ratio = %d \n",(*otp_ptr).rg_ratio,(*otp_ptr).bg_ratio);
                //clear otp buffer
                for(i = 0;i < 16; i++){
                                 ov12830_i2c_wr8(info,0x3d00+i,0x00);
                }
                //printk("zhangduo test %s --- \n",__func__);
                return 0;
}

//index:index of otp group.(1,2,3)
//otp_ptr:pointer of otp_struct
//return:            0,
int read_otp_lenc(struct ov12830_info *info,int index,struct otp_struct *otp_ptr)
{
                 int bank,i;
                 int address;

                 //clear otp buffer
                 for(i = 0;i < 16;i++){
                                  ov12830_i2c_wr8(info,0x3d00+i,0x00);
                 }

                 //select bank:4,8,12
                 bank = 0xc0 | (index*4);
                 ov12830_i2c_wr8(info,0x3d84,bank);

                 //read otp into buffer
                 ov12830_i2c_wr8(info,0x3d81,0x01);
                 mdelay(5);
                 address = 0x3d01;
                 for(i = 0; i < 15; i++){
                                   ov12830_i2c_rd8(info,address,&((*otp_ptr).lenc[i]));
                                   address++;
                 }

                 //clear otp buffer
                 for(i = 0; i < 16; i++){
                                   ov12830_i2c_wr8(info,0x3d00+i,0x00);
                 }

                 //select 2nd bank
                 bank++;
                 ov12830_i2c_wr8(info,0x3d84,bank);

                 //read otp
                 ov12830_i2c_wr8(info,0x3d81,0x01);
                 mdelay(5);

                 address = 0x3d00;
                 for(i = 15; i < 31; i++){
                                     ov12830_i2c_rd8(info,address,&((*otp_ptr).lenc[i]));
                                     address++;
                 }

                 //clear otp buffer
                 for(i = 0; i < 16; i++){
                                     ov12830_i2c_wr8(info,0x3d00+i,0x00);
                 }

                 //select 3rd bank
                 bank++;
                 ov12830_i2c_wr8(info,0x3d84,bank);

                 //read otp
                 ov12830_i2c_wr8(info,0x3d81,0x01);
                 mdelay(5);

                 address = 0x3d00;
                 for(i = 31; i < 47; i++){
                                     ov12830_i2c_rd8(info,address,&((*otp_ptr).lenc[i]));
                                     address++;
                 }

                 //clear otp buffer
                 for(i = 0; i < 16; i++){
                                    ov12830_i2c_wr8(info,0x3d00+i,0x00);
                 }

                 //select 4th bank
                 bank++;
                 ov12830_i2c_wr8(info,0x3d84,bank);

                 //read otp
                 ov12830_i2c_wr8(info,0x3d81,0x01);
                 mdelay(5);

                 address = 0x3d00;
                 for(i = 47; i < 62; i++){
                                     ov12830_i2c_rd8(info,address,&((*otp_ptr).lenc[i]));
                                     address++;
                 }

                 //clear otp buffer
                 for(i = 0; i < 16; i++){
                                   ov12830_i2c_wr8(info,0x3d00+i,0x00);
                 }

                 return 0;
}

//R_gain,sensor red gain of AWB,0x400 = 1
//G_gain,sensor green gain of AWB,0x400 = 1
//B_gain,sensor blue gain of AWB,0x400 = 1
//return 0;
int update_awb_gain(struct ov12830_info *info,int r_gain,int g_gain,int b_gain)
{

            //u8 r1,r2,g1,g2,b1,b2;
  
            printk("12830  test R_gain = %d,G_gain = %d,B_gain = %d\n",R_gain,G_gain,B_gain);
     
                  if(r_gain > 0x400){
                                ov12830_i2c_wr8(info,0x3400,r_gain >> 8);
                                ov12830_i2c_wr8(info,0x3401,r_gain & 0x00ff);
                  }

                  if(g_gain > 0x400){
			  	
                                ov12830_i2c_wr8(info,0x3402,g_gain >> 8);
                                ov12830_i2c_wr8(info,0x3403,g_gain & 0x00ff);
                  }

                  if(b_gain > 0x400){
	                                ov12830_i2c_wr8(info,0x3404,b_gain >> 8);
                                ov12830_i2c_wr8(info,0x3405,b_gain & 0x00ff);
                  }

         /*
              ov12830_i2c_rd8(info,0x3400,&r1);
              ov12830_i2c_rd8(info,0x3401,&r2);
              ov12830_i2c_rd8(info,0x3402,&g1);
              ov12830_i2c_rd8(info,0x3403,&g2);
              ov12830_i2c_rd8(info,0x3404,&b1);
              ov12830_i2c_rd8(info,0x3405,&b2);
              printk("zhangduo test r1 = %d,r2 = %d,g1 = %d,g2 = %d,b1 = %d,b2 = %d \n",r1,r2,g1,g2,b1,b2);
           */  
            
                  return 0;

                  
}

//otp_ptr:pointer of otp_struct
int update_lenc(struct ov12830_info *info,struct otp_struct *otp_ptr)
{
                  int i;
                  u8 temp;
                  ov12830_i2c_rd8(info,0x5000,&temp);
                  temp = 0x80 | temp;
                  ov12830_i2c_wr8(info,0x5000,temp);
				//printk("__debug: otp_dump LENC_update reg: 0x5800=0x%x\n",temp);

                  for(i = 0; i < 62; i++){
                                 ov12830_i2c_wr8(info,0x5800+i,(*otp_ptr).lenc[i]);
                  }
                  ov12830_i2c_wr8(info,0x5000,temp);				  
                  return 0;
}
int update_otp_af(struct ov12830_info *info)
{
   struct otp_struct current_otp;
   int i,j;
   int temp;
   int af_start_flag,af_max_flag;
   int otp_af_start_index = 0;
   int otp_af_max_index = 0;
   int infinity,macro,af_temp;
   for(i = 1; i <=3; i++){
                  temp = check_otp_af(info,i);
                  af_start_flag = (temp & 0x30) >> 4;
                  if(af_start_flag == 1){
                                  otp_af_start_index = i;
                                  break;
                    }
                  
   }
   for(j = 1; j <=3; j++){
                  temp = check_otp_af(info,j);
                  af_max_flag = (temp & 0x0c) >> 2;
                  if(af_max_flag == 1){
                                  otp_af_max_index = j;
                                  break;
                    }
   }
  if(i > 3) printk("zhangduo test no valid af_start OTP data");
   if(j > 3) printk("zhangduo test no valid af_max OTP data");
   read_otp_af_start(info,otp_af_start_index,&current_otp);
    infinity   = current_otp.user_data[2];
    af_temp = current_otp.user_data[4];
    Bu64291_infinity = (infinity << 2) +(( af_temp >> 2) & 0x03);
   read_otp_af_max(info,otp_af_max_index,&current_otp);
    macro    = current_otp.user_data[3];
    af_temp = current_otp.user_data[4];
     Bu64291_macro = (macro << 2) + (af_temp & 0x03);
   printk("zhangduo test%s,bu64291_macro is %d,bu64291_infinity is %d \n",__func__,Bu64291_macro,Bu64291_infinity);           
   return 0;
}
// call this function after OV12830 initialization
// return value: 0 update success
//                   1 no OTP
 int update_otp_wb(struct ov12830_info *info)
{
                  struct otp_struct current_otp;
                  int i;
                  int otp_index;
                  int temp;
                 // int infinity,macro,af_temp;
                  //int R_gain, G_gain, B_gain, G_gain_R, G_gain_B;
                  int rg,bg;
                //  printk("zhangduo test %s +++ \n",__func__);
                  //R/G and B/G of current camera module is read out from sensor OTP
                  //check first OTP with valid data
                  for(i = 1; i <= 3; i++){
                                  temp = check_otp_wb(info,i);
                                  if(temp == 2){
                                              otp_index = i;
                                              break;
                                  }
                  }
                  
                  if(i > 3){
                                  //no valid wb OTP data
                                  printk("error!zhangduo test no valid wb OTP data\n");
                                  return 1;
                  }
      
                  read_otp_wb(info,otp_index,&current_otp);
                  /*
                  infinity   = current_otp.user_data[2];
                  macro    = current_otp.user_data[3];
                  af_temp = current_otp.user_data[4];
               Bu64291_infinity = (infinity << 2) +(( af_temp >> 2) & 0x03);
               Bu64291_macro = (macro << 2) + (af_temp & 0x03);
               printk("zhangduo test%s,bu64291_macro is %d,bu64291_infinity is %d \n",__func__,Bu64291_macro,Bu64291_infinity);
       */        
                  if(current_otp.light_rg == 0){
                                  //no light source information in OTP,light factor = 1
                                  rg = current_otp.rg_ratio;
                  }
                  else{
                                  rg = current_otp.rg_ratio * ((current_otp.light_rg + 512) / 1024);
                  }

                  if(current_otp.light_bg == 0){
                                  //no light source information in OTP,light factor = 1
                                  bg = current_otp.bg_ratio;
                  }
                  else{
                                  bg = current_otp.bg_ratio * ((current_otp.light_bg + 512) / 1024);
                  }

                  //calculate G gain
                  //0x400 = 1x gain
                  if(bg < BG_Ratio_Typical){
                                  if(rg < RG_Ratio_Typical){
                                              //current_otp.bg_ratio < BG_Ratio_typical && 
                                             //current_otp.rg_ratio < RG_Ratio_typical
                                             G_gain = 0x400;
                                             B_gain = 0x400 * BG_Ratio_Typical /bg;
                                             R_gain = 0x400 * RG_Ratio_Typical / rg;
                                  }
                                  else{
                                             //current_otp.bg_ratio < BG_Ratio_typical &&
                                             //current_otp.rg_ratio >= RG_Ratio_typical
                                             R_gain = 0x400;
                                             G_gain = 0x400 * rg / RG_Ratio_Typical ;
                                             B_gain = G_gain * BG_Ratio_Typical / bg;
                                  }
                  }
                  else{
                                  if(rg < RG_Ratio_Typical){
                                                       //current_otp.bg_ratio >= BG_Ratio_typical &&
                                                       //current_otp.rg_ratio < RG_Ratio_typical
                                                       B_gain = 0x400;
                                                       G_gain = 0x400 * bg / BG_Ratio_Typical;
                                                       R_gain = G_gain * RG_Ratio_Typical / rg;
                                  }
                                  else{
                                                       //current_otp.bg_ratio >= BG_Ratio_typical &&
                                                       //current_otp.rg_ratio >= RG_Ratio_typical
                                                       G_gain_B = 0x400 * bg / BG_Ratio_Typical;
                                                       G_gain_R = 0x400 * rg / RG_Ratio_Typical;

                                                       if(G_gain_B > G_gain_R){
                                                                       B_gain = 0x400;
                                                                       G_gain = G_gain_B;
                                                                       R_gain = G_gain * RG_Ratio_Typical / rg;
                                                       }
                                                       else{
                                                                       R_gain = 0x400;
                                                                       G_gain = G_gain_R;
                                                                       B_gain = G_gain * BG_Ratio_Typical / bg;
                                                       }
                                  }
                  }

                 // update_awb_gain(info,R_gain,G_gain,B_gain);
                 // printk("zhangduo test %s--- \n",__func__);
                  return 0;
}


//call this function after OV12830 initialization
//return value:      0 updata sucess
//                       1,no OTP
int update_otp_lenc(struct ov12830_info *info)
{
                  //struct otp_struct current_otp;
                  int i;
                  int otp_index;
                  int temp;

                  //check first lens correction OTP with valid data
                  for(i = 1; i <= 3; i++){
                                   temp = check_otp_lenc(info,i);
                                   if(temp == 2){
                                                otp_index = i;
                                                break;
                                   }
                  }

                  if(i > 3){
                                   //no valid lens OTP data
                                   printk("error!zhangduo test no valid lens OTP data!\n");
                                   return 1;
                  }

                  read_otp_lenc(info,otp_index,&current_otp_lenc);
                  //update_lenc(info,&current_otp_lenc);

                  //sucess
                  return 0;
}
static int ov12830_mode_able(struct ov12830_info *info, bool mode_enable)
{
	u8 val;
	int err;
     // printk("zhangduo test %s +++ \n",__func__);
	if (mode_enable)
		val = 0x01;
	else
		val = 0x00;
	err = ov12830_i2c_wr8(info, 0x0100, val);
	if (!err) {
		info->mode_enable = mode_enable;
		dev_dbg(&info->i2c_client->dev, "%s streaming=%x\n",
			__func__, info->mode_enable);
		if (!mode_enable)
			ov12830_pm_dev_wr(info, NVC_PWR_STDBY);
	}
    //printk("zhangduo test %s --- \n",__func__);
	return err;
}

static int ov12830_mode_rd(struct ov12830_info *info,
			s32 res_x,
			s32 res_y,
			u32 *index)
{
	int i;

	if (!res_x && !res_y) {
		*index = info->cap->preferred_mode_index;
		return 0;
	}

	for (i = 0; i < OV12830_NUM_MODES; i++) {
		if ((res_x == ov12830_mode_table[i]->sensor_mode.res_x) &&
		   (res_y == ov12830_mode_table[i]->sensor_mode.res_y)) {
			break;
		}
	}
      if(g_ov12830_nightmode==1 && res_x==4224 && (g_tmd2771x_lux<10 && g_tmd2771x_lux>=0))
      {
      	    i = 3;        //night mode
      }
	if (i == OV12830_NUM_MODES) {
		dev_err(&info->i2c_client->dev,
			"%s invalid resolution: %dx%d\n",
			__func__, res_x, res_y);
		return -EINVAL;
	}

	*index = i;
	return 0;
}

static int ov12830_mode_wr_full(struct ov12830_info *info, u32 mode_index)
{
	int err;
      // printk("zhangduo test %s +++ \n",__func__);
	ov12830_pm_dev_wr(info, NVC_PWR_ON);
	ov12830_bin_wr(info, 0);
	err = ov12830_i2c_wr_table(info,
				ov12830_mode_table[mode_index]->p_mode_i2c);
	if (!err) {
		info->mode_index = mode_index;
		info->mode_valid = true;
	} else {
		info->mode_valid = false;
	}
      // printk("zhangduo test %s --- \n",__func__);
	return err;
}
static int ov12830_mode_wr(struct ov12830_info *info,
			struct nvc_imager_bayer *mode)
{
	u32 mode_index;
	int err;
      int val;
	u8 temp;
	
#ifdef OV12830_REGISTER_DUMP
	int i;
	__u8 buf;
	__u16 bufarray[2][6];
	int col;
#endif

	err = ov12830_mode_rd(info, mode->res_x, mode->res_y, &mode_index);
	if (err < 0)
		return err;
       g_cam_status = mode_index;
	pr_info("ov12830: set mode: %dx%d\n", mode->res_x, mode->res_y);

	if (!mode->res_x && !mode->res_y) {
		if (mode->frame_length || mode->coarse_time || mode->gain) {
			/* write exposure only */
			err = ov12830_exposure_wr(info, mode);
			return err;
		} else {
			/* turn off streaming */
			err = ov12830_mode_able(info, false);
			return err;
		}
	}



	if (!info->mode_valid || (info->mode_index != mode_index))
		err = ov12830_mode_wr_full(info, mode_index);
	else
		dev_dbg(&info->i2c_client->dev, "%s short mode\n", __func__);
	err |= ov12830_exposure_wr(info, mode);
	if (err < 0) {
		info->mode_valid = false;
		goto ov12830_mode_wr_err;
	}

	err = ov12830_mode_able(info, true);
	if (err < 0)
		goto ov12830_mode_wr_err;
    /* OTP */
      val = ov12830_cam_clibratiion_flag();
      mdelay(5);
      if (val ==OV12830_OTP_WB_CAL_EN)    // only wb
	{
	       update_awb_gain(info,R_gain,G_gain,B_gain);
		//update_otp_wb(info);
		printk("OV calibration: AWB ON, LSC OFF\n");
	}
	else if (val ==OV12830_OTP_LENS_CAL_EN)  // only lens
	{
	      update_lenc(info,&current_otp_lenc);
		
		printk("OV calibration: AWB OFF, LSC ON\n");
	}
	else if (val ==OV12830_OTP_WB_LENS_CAL_UN )
	{
      ov12830_i2c_rd8(info,0x5000,&temp);
      temp = (0x7f) & (temp);
      ov12830_i2c_wr8(info,0x5000,temp);
		printk("OV calibration: AWB OFF, LSC OFF\n");
	}
	else  // for wb and lens
	{
		update_awb_gain(info,R_gain,G_gain,B_gain);
		 update_lenc(info,&current_otp_lenc);
		printk("OV calibration: AWB ON, LSC ON\n");
	}
	/* otp end */
	return 0;

ov12830_mode_wr_err:
	if (!info->mode_enable)
		ov12830_pm_dev_wr(info, NVC_PWR_OFF);
	return err;
}


static int ov12830_param_rd(struct ov12830_info *info, unsigned long arg)
{
	struct nvc_param params;
	struct ov12830_reg *p_i2c_table;
	const void *data_ptr;
	u32 data_size = 0;
	u32 u32val;
	int err;

	if (copy_from_user(&params,
			(const void __user *)arg,
			sizeof(struct nvc_param))) {
		dev_err(&info->i2c_client->dev,
			"%s copy_from_user err line %d\n", __func__, __LINE__);
		return -EFAULT;
	}

	if (info->s_mode == NVC_SYNC_SLAVE)
		info = info->s_info;
	switch (params.param) {
	case NVC_PARAM_GAIN:
		ov12830_pm_dev_wr(info, NVC_PWR_COMM);
		err = ov12830_gain_rd(info, &u32val);
		ov12830_pm_dev_wr(info, NVC_PWR_OFF);
		dev_dbg(&info->i2c_client->dev, "%s GAIN: %u err: %d\n",
			__func__, u32val, err);
		if (err)
			return err;

		data_ptr = &u32val;
		data_size = sizeof(u32val);
		break;

	case NVC_PARAM_STEREO_CAP:
		if (info->s_info != NULL)
			err = 0;
		else
			err = -ENODEV;
		dev_dbg(&info->i2c_client->dev, "%s STEREO_CAP: %d\n",
			__func__, err);
		data_ptr = &err;
		data_size = sizeof(err);
		break;

	case NVC_PARAM_STEREO:
		dev_dbg(&info->i2c_client->dev, "%s STEREO: %d\n",
			__func__, info->s_mode);
		data_ptr = &info->s_mode;
		data_size = sizeof(info->s_mode);
		break;

	case NVC_PARAM_STS:
		err = ov12830_dev_id(info);
		dev_dbg(&info->i2c_client->dev, "%s STS: %d\n",
			__func__, err);
		data_ptr = &err;
		data_size = sizeof(err);
		break;

	case NVC_PARAM_DEV_ID:
		if (!info->sdata.sensor_id_minor)
			ov12830_dev_id(info);
		data_ptr = &info->sdata.sensor_id;
		data_size = sizeof(info->sdata.sensor_id) * 2;
		dev_dbg(&info->i2c_client->dev, "%s DEV_ID: %x-%x\n",
			__func__, info->sdata.sensor_id,
			info->sdata.sensor_id_minor);
		break;

	case NVC_PARAM_SENSOR_TYPE:
		data_ptr = &info->sdata.sensor_type;
		data_size = sizeof(info->sdata.sensor_type);
		dev_dbg(&info->i2c_client->dev, "%s SENSOR_TYPE: %d\n",
			__func__, info->sdata.sensor_type);
		break;

	case NVC_PARAM_FOCAL_LEN:
		data_ptr = &info->sdata.focal_len;
		data_size = sizeof(info->sdata.focal_len);
		dev_dbg(&info->i2c_client->dev, "%s FOCAL_LEN: %u\n",
			__func__, info->sdata.focal_len);
		break;

	case NVC_PARAM_MAX_APERTURE:
		data_ptr = &info->sdata.max_aperture;
		data_size = sizeof(info->sdata.max_aperture);
		dev_dbg(&info->i2c_client->dev, "%s MAX_APERTURE: %u\n",
			__func__, info->sdata.max_aperture);
		break;

	case NVC_PARAM_FNUMBER:
		data_ptr = &info->sdata.fnumber;
		data_size = sizeof(info->sdata.fnumber);
		dev_dbg(&info->i2c_client->dev, "%s FNUMBER: %u\n",
			__func__, info->sdata.fnumber);
		break;

	case NVC_PARAM_VIEW_ANGLE_H:
		data_ptr = &info->sdata.view_angle_h;
		data_size = sizeof(info->sdata.view_angle_h);
		dev_dbg(&info->i2c_client->dev, "%s VIEW_ANGLE_H: %u\n",
			__func__, info->sdata.view_angle_h);
		break;

	case NVC_PARAM_VIEW_ANGLE_V:
		data_ptr = &info->sdata.view_angle_v;
		data_size = sizeof(info->sdata.view_angle_v);
		dev_dbg(&info->i2c_client->dev, "%s VIEW_ANGLE_V: %u\n",
			__func__, info->sdata.view_angle_v);
		break;

	case NVC_PARAM_I2C:
		dev_dbg(&info->i2c_client->dev, "%s I2C\n", __func__);
		if (params.sizeofvalue > OV12830_I2C_TABLE_MAX_ENTRIES) {
			pr_err("%s: requested size too large\n", __func__);
			return -EINVAL;
		}
		p_i2c_table = kzalloc(sizeof(params.sizeofvalue), GFP_KERNEL);
		if (p_i2c_table == NULL) {
			pr_err("%s: kzalloc error\n", __func__);
			return -ENOMEM;
		}

		if (copy_from_user(p_i2c_table,
				(const void __user *)params.p_value,
				params.sizeofvalue)) {
			dev_err(&info->i2c_client->dev,
				"%s copy_from_user err line %d\n",
				__func__, __LINE__);
			kfree(p_i2c_table);
			return -EINVAL;
		}

		ov12830_pm_dev_wr(info, NVC_PWR_COMM);
		err = ov12830_i2c_rd_table(info, p_i2c_table);
		ov12830_pm_dev_wr(info, NVC_PWR_OFF);
		if (copy_to_user((void __user *)params.p_value,
				 p_i2c_table,
				 params.sizeofvalue)) {
			dev_err(&info->i2c_client->dev,
				"%s copy_to_user err line %d\n",
				__func__, __LINE__);
			err = -EINVAL;
		}
		kfree(p_i2c_table);
		return err;

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

	if (copy_to_user((void __user *)params.p_value,
			 data_ptr,
			 data_size)) {
		dev_err(&info->i2c_client->dev,
			"%s copy_to_user err line %d\n", __func__, __LINE__);
		return -EFAULT;
	}

	return 0;
}

static int ov12830_param_wr_s(struct ov12830_info *info,
			struct nvc_param *params,
			u32 u32val)
{
	struct ov12830_reg *p_i2c_table;
	u8 u8val;
	int err;

	u8val = (u8)u32val;
	switch (params->param) {
	case NVC_PARAM_GAIN:
		dev_dbg(&info->i2c_client->dev, "%s GAIN: %u\n",
			__func__, u32val);
		ov12830_pm_dev_wr(info, NVC_PWR_COMM);
		err = ov12830_gain_wr(info, u32val);
		ov12830_pm_dev_wr(info, NVC_PWR_STDBY);
		return err;

	case NVC_PARAM_RESET:
		err = ov12830_reset(info, u32val);
		dev_dbg(&info->i2c_client->dev, "%s RESET=%d err=%d\n",
			__func__, u32val, err);
		return err;

	case NVC_PARAM_TESTMODE:
		dev_dbg(&info->i2c_client->dev, "%s TESTMODE: %u\n",
			__func__, (unsigned)u8val);
		if (u8val)
			u32val = info->test_pattern;
		else
			u32val = 0;
		ov12830_pm_dev_wr(info, NVC_PWR_ON);
		err = ov12830_test_pattern_wr(info, u32val);
		if (!u8val)
			ov12830_pm_dev_wr(info, NVC_PWR_OFF);
		return err;

	case NVC_PARAM_TEST_PATTERN:
		dev_dbg(&info->i2c_client->dev, "%s TEST_PATTERN: %d\n",
			__func__, u32val);
		info->test_pattern = u32val;
		return 0;

	case NVC_PARAM_SELF_TEST:
		err = ov12830_dev_id(info);
		dev_dbg(&info->i2c_client->dev, "%s SELF_TEST: %d\n",
			__func__, err);
		return err;

	case NVC_PARAM_I2C:
		dev_dbg(&info->i2c_client->dev, "%s I2C\n", __func__);
		if (params->sizeofvalue > OV12830_I2C_TABLE_MAX_ENTRIES) {
			pr_err("%s: requested size too large\n", __func__);
			return -EINVAL;
		}
		p_i2c_table = kzalloc(sizeof(params->sizeofvalue), GFP_KERNEL);
		if (p_i2c_table == NULL) {
			dev_err(&info->i2c_client->dev,
				"%s kzalloc err line %d\n",
				__func__, __LINE__);
			return -ENOMEM;
		}

		if (copy_from_user(p_i2c_table,
				(const void __user *)params->p_value,
				params->sizeofvalue)) {
			dev_err(&info->i2c_client->dev,
				"%s copy_from_user err line %d\n",
				__func__, __LINE__);
			kfree(p_i2c_table);
			return -EFAULT;
		}

		ov12830_pm_dev_wr(info, NVC_PWR_ON);
		err = ov12830_i2c_wr_table(info, p_i2c_table);
		kfree(p_i2c_table);
		return err;

	default:
		dev_err(&info->i2c_client->dev,
			"%s unsupported parameter: %d\n",
			__func__, params->param);
		return -EINVAL;
	}
}

static int ov12830_param_wr(struct ov12830_info *info, unsigned long arg)
{
	struct nvc_param params;
	u8 u8val;
	u32 u32val;
	int err = 0;

	if (copy_from_user(&params, (const void __user *)arg,
			sizeof(struct nvc_param))) {
		dev_err(&info->i2c_client->dev,
			"%s copy_from_user err line %d\n", __func__, __LINE__);
		return -EFAULT;
	}

	if (copy_from_user(&u32val, (const void __user *)params.p_value,
			sizeof(u32val))) {
		dev_err(&info->i2c_client->dev, "%s %d copy_from_user err\n",
			__func__, __LINE__);
		return -EFAULT;
	}

	u8val = (u8)u32val;
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
			if (info->s_info != NULL) {
				info->s_info->s_mode = u8val;
				ov12830_pm_wr(info->s_info, NVC_PWR_OFF);
			}
			break;

		case NVC_SYNC_MASTER:
			info->s_mode = u8val;
			if (info->s_info != NULL)
				info->s_info->s_mode = u8val;
			break;

		case NVC_SYNC_SLAVE:
			if (info->s_info != NULL) {
				/* sync power */
				info->s_info->pwr_api = info->pwr_api;
				err = ov12830_pm_wr(info->s_info,
						info->pwr_dev);
				if (!err) {
					info->s_mode = u8val;
					info->s_info->s_mode = u8val;
				} else {
					if (info->s_mode != NVC_SYNC_STEREO)
						ov12830_pm_wr(info->s_info,
							NVC_PWR_OFF);
					err = -EIO;
				}
			}
			break;

		case NVC_SYNC_STEREO:
			if (info->s_info != NULL) {
				/* sync power */
				info->s_info->pwr_api = info->pwr_api;
				err = ov12830_pm_wr(info->s_info,
						info->pwr_dev);
				if (!err) {
					info->s_mode = u8val;
					info->s_info->s_mode = u8val;
				} else {
					if (info->s_mode != NVC_SYNC_SLAVE)
						ov12830_pm_wr(info->s_info,
								NVC_PWR_OFF);
					err = -EIO;
				}
			}
			break;

		default:
			err = -EINVAL;
		}
		if (info->pdata->cfg & NVC_CFG_NOERR)
			return 0;

		return err;

	case NVC_PARAM_GROUP_HOLD:
	{
		struct nvc_imager_ae ae;
		dev_dbg(&info->i2c_client->dev, "%s GROUP_HOLD\n",
			__func__);
		if (copy_from_user(&ae, (const void __user *)params.p_value,
				sizeof(struct nvc_imager_ae))) {
			dev_err(&info->i2c_client->dev,
				"%s %d copy_from_user err\n",
				__func__, __LINE__);
			return -EFAULT;
		}
		ov12830_pm_dev_wr(info, NVC_PWR_COMM);
		err = ov12830_group_hold_wr(info, &ae);
		ov12830_pm_dev_wr(info, NVC_PWR_STDBY);
		return err;
	}

	default:
	/* parameters dependent on sync mode */
		switch (info->s_mode) {
		case NVC_SYNC_OFF:
		case NVC_SYNC_MASTER:
			return ov12830_param_wr_s(info, &params, u32val);

		case NVC_SYNC_SLAVE:
			return ov12830_param_wr_s(info->s_info,
						 &params,
						 u32val);

		case NVC_SYNC_STEREO:
			err = ov12830_param_wr_s(info, &params, u32val);
			if (!(info->pdata->cfg & NVC_CFG_SYNC_I2C_MUX))
				err |= ov12830_param_wr_s(info->s_info,
							 &params,
							 u32val);
			return err;

		default:
			dev_err(&info->i2c_client->dev, "%s %d internal err\n",
				__func__, __LINE__);
			return -EINVAL;
		}
	}
}

static long ov12830_ioctl(struct file *file,
			 unsigned int cmd,
			 unsigned long arg)
{
	struct ov12830_info *info = file->private_data;
	struct nvc_imager_bayer mode;
	struct nvc_imager_mode_list mode_list;
	struct nvc_imager_mode mode_table[OV12830_NUM_MODES];
	struct nvc_imager_dnvc dnvc;
	const void *data_ptr;
	s32 num_modes;
	u32 i;
	int pwr;
	int err;

	switch (cmd) {
	case NVC_IOCTL_PARAM_WR:
		err = ov12830_param_wr(info, arg);
		return err;

	case NVC_IOCTL_PARAM_RD:
		err = ov12830_param_rd(info, arg);
		return err;

	case NVC_IOCTL_DYNAMIC_RD:
		if (copy_from_user(&dnvc, (const void __user *)arg,
				sizeof(struct nvc_imager_dnvc))) {
			dev_err(&info->i2c_client->dev,
				"%s copy_from_user err line %d\n",
				__func__, __LINE__);
			return -EFAULT;
		}

		dev_dbg(&info->i2c_client->dev, "%s DYNAMIC_RD x=%d y=%d\n",
			__func__, dnvc.res_x, dnvc.res_y);
		err = ov12830_mode_rd(info, dnvc.res_x, dnvc.res_y, &i);
		if (err)
			return -EINVAL;

		if (dnvc.p_mode) {
			if (copy_to_user((void __user *)dnvc.p_mode,
					 &ov12830_mode_table[i]->sensor_mode,
					 sizeof(struct nvc_imager_mode))) {
				dev_err(&info->i2c_client->dev,
					"%s copy_to_user err line %d\n",
					__func__, __LINE__);
				return -EFAULT;
			}
		}

		if (dnvc.p_dnvc) {
			if (copy_to_user((void __user *)dnvc.p_dnvc,
				    &ov12830_mode_table[i]->sensor_dnvc,
				    sizeof(struct nvc_imager_dynamic_nvc))) {
				dev_err(&info->i2c_client->dev,
					"%s copy_to_user err line %d\n",
					__func__, __LINE__);
				return -EFAULT;
			}
		}

		return 0;

	case NVC_IOCTL_MODE_WR:
		if (copy_from_user(&mode, (const void __user *)arg,
				sizeof(struct nvc_imager_bayer))) {
			dev_err(&info->i2c_client->dev,
				"%s copy_from_user err line %d\n",
				__func__, __LINE__);
			return -EFAULT;
		}

		dev_dbg(&info->i2c_client->dev, "%s MODE_WR x=%d y=%d ",
			__func__, mode.res_x, mode.res_y);
		dev_dbg(&info->i2c_client->dev, "coarse=%u frame=%u gain=%u\n",
			mode.coarse_time, mode.frame_length, mode.gain);
		err = ov12830_mode_wr(info, &mode);
		return err;

	case NVC_IOCTL_MODE_RD:
		/*
		 * Return a list of modes that sensor bayer supports.
		 * If called with a NULL ptr to pModes,
		 * then it just returns the count.
		 */
		dev_dbg(&info->i2c_client->dev, "%s MODE_RD n=%d\n",
			__func__, OV12830_NUM_MODES);
		if (copy_from_user(&mode_list, (const void __user *)arg,
				sizeof(struct nvc_imager_mode_list))) {
			dev_err(&info->i2c_client->dev,
				"%s copy_from_user err line %d\n",
				__func__, __LINE__);
			return -EFAULT;
		}

		num_modes = OV12830_NUM_MODES;
		if (mode_list.p_num_mode != NULL) {
			if (copy_to_user((void __user *)mode_list.p_num_mode,
					 &num_modes, sizeof(num_modes))) {
				dev_err(&info->i2c_client->dev,
					"%s copy_to_user err line %d\n",
					__func__, __LINE__);
				return -EFAULT;
			}
		}

		if (mode_list.p_modes != NULL) {
			for (i = 0; i < OV12830_NUM_MODES; i++) {
				mode_table[i] =
					ov12830_mode_table[i]->sensor_mode;
			}
			if (copy_to_user((void __user *)mode_list.p_modes,
					 (const void *)&mode_table,
					 sizeof(mode_table))) {
				dev_err(&info->i2c_client->dev,
					"%s copy_to_user err line %d\n",
					__func__, __LINE__);
				return -EFAULT;
			}
		}

		return 0;

	case NVC_IOCTL_PWR_WR:
		/* This is a Guaranteed Level of Service (GLOS) call */
		pwr = (int)arg * 2;
		dev_dbg(&info->i2c_client->dev, "%s PWR_WR: %d\n",
			__func__, pwr);
		err = ov12830_pm_api_wr(info, pwr);
		return err;

	case NVC_IOCTL_PWR_RD:
		if (info->s_mode == NVC_SYNC_SLAVE)
			pwr = info->s_info->pwr_api / 2;
		else
			pwr = info->pwr_api / 2;
		dev_dbg(&info->i2c_client->dev, "%s PWR_RD: %d\n",
			__func__, pwr);
		if (copy_to_user((void __user *)arg, (const void *)&pwr,
				 sizeof(pwr))) {
			dev_err(&info->i2c_client->dev,
					"%s copy_to_user err line %d\n",
					__func__, __LINE__);
			return -EFAULT;
		}

		return 0;

	case NVC_IOCTL_CAPS_RD:
		dev_dbg(&info->i2c_client->dev, "%s CAPS_RD n=%d\n",
			__func__, sizeof(ov12830_dflt_cap));
		data_ptr = info->cap;
		if (copy_to_user((void __user *)arg,
				 data_ptr,
				 sizeof(ov12830_dflt_cap))) {
			dev_err(&info->i2c_client->dev,
				"%s copy_to_user err line %d\n",
				__func__, __LINE__);
			return -EFAULT;
		}

		return 0;

	case NVC_IOCTL_STATIC_RD:
		dev_dbg(&info->i2c_client->dev, "%s STATIC_RD n=%d\n",
			__func__, sizeof(struct nvc_imager_static_nvc));
		data_ptr = &info->sdata;
		if (copy_to_user((void __user *)arg,
				 data_ptr,
				 sizeof(struct nvc_imager_static_nvc))) {
			dev_err(&info->i2c_client->dev,
				"%s copy_to_user err line %d\n",
				__func__, __LINE__);
			return -EFAULT;
		}

		return 0;

	default:
		dev_err(&info->i2c_client->dev, "%s unsupported ioctl: %x\n",
			__func__, cmd);
	}
	return -EINVAL;
}

static void ov12830_sdata_init(struct ov12830_info *info)
{
	memcpy(&info->sdata, &ov12830_dflt_sdata, sizeof(info->sdata));
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

static int ov12830_sync_en(unsigned num, unsigned sync)
{
	struct ov12830_info *master = NULL;
	struct ov12830_info *slave = NULL;
	struct ov12830_info *pos = NULL;

	rcu_read_lock();
	list_for_each_entry_rcu(pos, &ov12830_info_list, list) {
		if (pos->pdata->num == num) {
			master = pos;
			break;
		}
	}
	pos = NULL;
	list_for_each_entry_rcu(pos, &ov12830_info_list, list) {
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

static int ov12830_sync_dis(struct ov12830_info *info)
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

static int ov12830_open(struct inode *inode, struct file *file)
{
	struct ov12830_info *info = NULL;
	struct ov12830_info *pos = NULL;
	int err;

	rcu_read_lock();
	list_for_each_entry_rcu(pos, &ov12830_info_list, list) {
		if (pos->miscdev.minor == iminor(inode)) {
			info = pos;
			break;
		}
	}
	rcu_read_unlock();
	if (!info)
		return -ENODEV;

	err = ov12830_sync_en(info->pdata->num, info->pdata->sync);
	if (err == -EINVAL)
		dev_err(&info->i2c_client->dev,
			"%s err: invalid num (%u) and sync (%u) instance\n",
			__func__, info->pdata->num, info->pdata->sync);
	if (atomic_xchg(&info->in_use, 1))
		return -EBUSY;

	if (info->s_info != NULL) {
		if (atomic_xchg(&info->s_info->in_use, 1))
			return -EBUSY;
		info->sdata.stereo_cap = 1;
	}

	file->private_data = info;
	dev_dbg(&info->i2c_client->dev, "%s\n", __func__);
	return 0;
}

int ov12830_release(struct inode *inode, struct file *file)
{
	struct ov12830_info *info = file->private_data;
	dev_dbg(&info->i2c_client->dev, "%s\n", __func__);
	ov12830_pm_wr_s(info, NVC_PWR_OFF);
	file->private_data = NULL;
	WARN_ON(!atomic_xchg(&info->in_use, 0));
	if (info->s_info != NULL)
		WARN_ON(!atomic_xchg(&info->s_info->in_use, 0));
	ov12830_sync_dis(info);
	return 0;
}

static const struct file_operations ov12830_fileops = {
	.owner = THIS_MODULE,
	.open = ov12830_open,
	.unlocked_ioctl = ov12830_ioctl,
	.release = ov12830_release,
};

static void ov12830_del(struct ov12830_info *info)
{
	ov12830_pm_exit(info);
	if ((info->s_mode == NVC_SYNC_SLAVE) ||
					(info->s_mode == NVC_SYNC_STEREO))
		ov12830_pm_exit(info->s_info);
	ov12830_sync_dis(info);
	spin_lock(&ov12830_spinlock);
	list_del_rcu(&info->list);
	spin_unlock(&ov12830_spinlock);
	synchronize_rcu();
}



static void ov12830_edp_throttle(unsigned int new_state, void *priv_data)
{
	struct ov12830_info *info = priv_data;
	int err;

	err = ov12830_power_off(info);
	if (err) {
		dev_err(&info->i2c_client->dev,
				"%s: ERROR cannot set edp state! %d\n",
			       __func__, err);
	}
	info->mode_valid = false;
	info->bin_en = 0;
}

static void ov12830_edp_register(struct ov12830_info *info)
{
	struct edp_manager *edp_manager;
	struct edp_client *edpc = &info->pdata->edpc_config;
	int ret;

	info->edpc = edpc;
	if (!edpc->num_states) {
		dev_info(&info->i2c_client->dev,
				"%s: NO edp states defined.\n", __func__);
		return;
	}

	strncpy(edpc->name, "ov12830", EDP_NAME_LEN - 1);
	edpc->name[EDP_NAME_LEN - 1] = 0;
	edpc->private_data = info;
	edpc->throttle = ov12830_edp_throttle;

	dev_dbg(&info->i2c_client->dev, "%s: %s, e0 = %d, p %d\n",
			__func__, edpc->name, edpc->e0_index, edpc->priority);
	for (ret = 0; ret < edpc->num_states; ret++)
		dev_dbg(&info->i2c_client->dev, "e%d = %d mA",
				ret - edpc->e0_index, edpc->states[ret]);

	edp_manager = edp_get_manager("battery");
	if (!edp_manager) {
		dev_err(&info->i2c_client->dev,
				"unable to get edp manager: battery\n");
		return;
	}

	ret = edp_register_client(edp_manager, edpc);
	if (ret) {
		dev_err(&info->i2c_client->dev,
				"unable to register edp client\n");
		return;
	}
}

static int ov12830_remove(struct i2c_client *client)
{
	struct ov12830_info *info = i2c_get_clientdata(client);

	dev_dbg(&info->i2c_client->dev, "%s\n", __func__);
	misc_deregister(&info->miscdev);
	ov12830_del(info);
	return 0;
}

struct clk * tegra_get_clock_by_name(const char *);
static int ov12830_probe(
	struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct ov12830_info *info;
	char dname[16];
	unsigned long clock_probe_rate;
	int err;
	struct clk *vi_sensor;
	struct clk *csus;
	dev_dbg(&client->dev, "%s\n", __func__);
	info = devm_kzalloc(&client->dev, sizeof(*info), GFP_KERNEL);
	if (info == NULL) {
		dev_err(&client->dev, "%s: kzalloc error\n", __func__);
		return -ENOMEM;
	}

	info->i2c_client = client;
	if (client->dev.platform_data) {
		info->pdata = client->dev.platform_data;
	} else {
		info->pdata = &ov12830_dflt_pdata;
		dev_dbg(&client->dev,
			"%s No platform data.  Using defaults.\n", __func__);
	}
	if (info->pdata->cap)
		info->cap = info->pdata->cap;
	else
		info->cap = &ov12830_dflt_cap;
	i2c_set_clientdata(client, info);
	INIT_LIST_HEAD(&info->list);
	spin_lock(&ov12830_spinlock);
	list_add_rcu(&info->list, &ov12830_info_list);
	spin_unlock(&ov12830_spinlock);
	ov12830_pm_init(info);
	ov12830_sdata_init(info);
	if (info->pdata->cfg & (NVC_CFG_NODEV | NVC_CFG_BOOT_INIT)) {
		if (info->pdata->probe_clock) {
			if (info->cap->initial_clock_rate_khz)
				clock_probe_rate = info->cap->
							initial_clock_rate_khz;
			else
				clock_probe_rate = ov12830_dflt_cap.
							initial_clock_rate_khz;
			clock_probe_rate *= 1000;
			info->pdata->probe_clock(clock_probe_rate);
		}
		err = ov12830_dev_id(info);
		if (err < 0) {
			if (info->pdata->cfg & NVC_CFG_NODEV) {
				ov12830_del(info);
				if (info->pdata->probe_clock)
					info->pdata->probe_clock(0);
				return -ENODEV;
			} else {
				dev_err(&client->dev, "%s device not found\n",
					__func__);
			}
		} else {
			dev_dbg(&client->dev, "%s device found\n", __func__);
			if (info->pdata->cfg & NVC_CFG_BOOT_INIT)
				ov12830_mode_wr_full(info, info->cap->
						preferred_mode_index);
		}
		ov12830_pm_dev_wr(info, NVC_PWR_OFF);
		if (info->pdata->probe_clock)
			info->pdata->probe_clock(0);
	}

	ov12830_edp_register(info);

	if (info->pdata->dev_name != 0)
		strcpy(dname, info->pdata->dev_name);
	else
		strcpy(dname, "ov12830");
	if (info->pdata->num)
		snprintf(dname, sizeof(dname), "%s.%u",
			 dname, info->pdata->num);
	info->miscdev.name = dname;
	info->miscdev.fops = &ov12830_fileops;
	info->miscdev.minor = MISC_DYNAMIC_MINOR;
	if (misc_register(&info->miscdev)) {
		dev_err(&client->dev, "%s unable to register misc device %s\n",
			__func__, dname);
		ov12830_del(info);
		return -ENODEV;
	}
      vi_sensor = tegra_get_clock_by_name("vi_sensor");
    if(IS_ERR_OR_NULL(vi_sensor)){
        printk(KERN_ERR"dcdv error %s %s %d \n", __FILE__, __func__, __LINE__);
        return -ENODEV;
        }
    csus = tegra_get_clock_by_name("csus");
    if(IS_ERR_OR_NULL(vi_sensor)){
        printk(KERN_ERR"dcdv error %s %s %d \n", __FILE__, __func__, __LINE__);
        return -ENODEV;
        }
    clk_enable(csus);
    clk_enable(vi_sensor);
    clk_set_rate(vi_sensor, 24000000);
    /* trun all the power for camera sensor here and do read*/
   // ov12830_power_on(info,false);
    //ov12830_mode_wr_full(info,0);
    ov12830_pm_dev_wr(info, NVC_PWR_ON);
    //ov12830_mode_able(info, true);
    //ov12830_dev_id(info);
  err = ov12830_i2c_wr8(info, 0x0100,0x01 );
  if(err)printk("zhangduo test stream i2c error!!!\n ");  
   update_otp_af(info);
   update_otp_lenc(info);
   update_otp_wb(info);
  ov12830_pm_dev_wr(info, NVC_PWR_OFF);
    clk_disable(vi_sensor);
    clk_disable(csus);

    create_cam_cal_proc_file();
	return 0;
}

static const struct i2c_device_id ov12830_id[] = {
	{ "ov12830", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, ov12830_id);

static struct i2c_driver ov12830_i2c_driver = {
	.driver = {
		.name = "ov12830",
		.owner = THIS_MODULE,
	},
	.id_table = ov12830_id,
	.probe = ov12830_probe,
	.remove = ov12830_remove,
};

module_i2c_driver(ov12830_i2c_driver);
MODULE_LICENSE("GPL v2");

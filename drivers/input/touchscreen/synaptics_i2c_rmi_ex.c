/* drivers/input/keyboard/synaptics_i2c_rmi.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (C) 2008 Texas Instrument Inc.
 * Copyright (C) 2009 Synaptics, Inc.
 *
 * provides device files /dev/input/event#
 * for named device files, use udev
 * 2D sensors report ABS_X_FINGER(0), ABS_Y_FINGER(0) through ABS_X_FINGER(7), ABS_Y_FINGER(7)
 * NOTE: requires updated input.h, which should be included with this driver
 * 1D/Buttons report BTN_0 through BTN_0 + button_count
 * TODO: report REL_X, REL_Y for flick, BTN_TOUCH for tap (on 1D/0D; done for 2D)
 * TODO: check ioctl (EVIOCGABS) to query 2D max X & Y, 1D button count
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

#include <linux/module.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/synaptics_i2c_rmi_ex.h>
#include <linux/io.h>
#include <linux/gpio.h>
//#include <linux/earlysuspend.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#include <linux/regulator/consumer.h>  /*ZTE: added by tong.weili for Touch Power 20111116*/
#include "../../../arch/arm/mach-tegra/gpio-names.h" /*ZTE: added by tong.weili for Touch Power 20110725*/

//#define TS_WATCHDOG  /*ZTE: deleted by tong.weili 20120908*/
#define USE_HRTIMER_4WATCHDOG	0

/*ZTE: added by tong.weili for print debug 20120202 ++*/
static int debug = 0;
module_param(debug, int, 0600);
/*ZTE: added by tong.weili for print debug 20120202 ++*/

#define BTN_F19 BTN_0
#define BTN_F30 BTN_0
#define SCROLL_ORIENTATION REL_Y

static struct workqueue_struct *synaptics_wq;
#ifdef TS_WATCHDOG
static struct workqueue_struct *tsp_watchdog_wq;
#endif
static struct i2c_driver synaptics_rmi4_driver;
static struct regulator *ventana_LDO3 = NULL;/*ZTE: added by tong.weili for Touch Power 20111116*/

/* Register: EGR_0 */
#define EGR_PINCH_REG		0
#define EGR_PINCH 		(1 << 6)
#define EGR_PRESS_REG 		0
#define EGR_PRESS 		(1 << 5)
#define EGR_FLICK_REG 		0
#define EGR_FLICK 		(1 << 4)
#define EGR_EARLY_TAP_REG	0
#define EGR_EARLY_TAP		(1 << 3)
#define EGR_DOUBLE_TAP_REG	0
#define EGR_DOUBLE_TAP		(1 << 2)
#define EGR_TAP_AND_HOLD_REG	0
#define EGR_TAP_AND_HOLD	(1 << 1)
#define EGR_SINGLE_TAP_REG	0
#define EGR_SINGLE_TAP		(1 << 0)
/* Register: EGR_1 */
#define EGR_PALM_DETECT_REG	1
#define EGR_PALM_DETECT		(1 << 0)

#ifdef  TS_WATCHDOG
#define TS_WATCHDOG_DURATION  40
#define TS_WATCHDOG_NO_REPORT_TIME 1

#if USE_HRTIMER_4WATCHDOG
	struct hrtimer watchdog_total_timer, watchdog_no_report_timer;
#else
	struct timer_list watchdog_total_timer, watchdog_no_report_timer;
#endif
static u8 ts_watchgod_enable = 0;
static struct work_struct  watchdog_work;
#endif

struct synaptics_function_descriptor {
	__u8 queryBase;
	__u8 commandBase;
	__u8 controlBase;
	__u8 dataBase;
	__u8 intSrc;
	__u8 functionNumber;
};
#define FUNCTION_VERSION(x) ((x >> 5) & 3)
#define INTERRUPT_SOURCE_COUNT(x) (x & 7)

#define FD_ADDR_MAX 0xE9
#define FD_ADDR_MIN 0x05
#define FD_BYTE_COUNT 6

#define MIN_ACTIVE_SPEED 5

#define CONFIG_SYNA_MULTI_TOUCH
#define CONFIG_HAS_VIRTUALKEY

#define USER_DEFINE_MAX_X (1431)
#define USER_DEFINE_MAX_Y (2685)
#define MAX_Y_FOR_LCD (2539)

static char *chip_type[] = {
    "",
    "S2200",
    "S2202",
    "S3200",
    "S3202",
    "S3203",
    "S7020",
    "S7300",
    "S2302",
    "S2306",
    "S3408",
};
static char *sensor_partner[] = {
    "",
    "TPK",
    "Truly",
    "Success",
    "Ofilm",
    "Lead",
    "Wintek",
    "Laibao",
    "CMI",
    "ECW",
    "Goworld",
    "baoming",
    "eachopto",
    "Mutto",
    "Junda",
};


/*ZTE: modified by tong.weili for synaptics sensor identify 20120401 ++*/
static enum Synaptics_Sensor_type 
{
    Type_S3000 = 0, 
    Type_S2202,
    Type_Max,
}s_synaptics_Sensor_type;
static u8 s_synaptics_configID[4] = {0};
static u8 s_F01_CTRL_00 = 0;
static u8 s_F01_QUERY_03 = 0;
static u8 s_F01_CMD_00 = 0;
//static u32 s_F11_Max_X = 0;
//static u32 s_F11_Max_Y = 0;
static u32 s_F11_CTRL_00 = 0;
//static u8 s_F11_QUERY_01 = 0;
//static u8 s_F11_CTRL_14 = 0;
static u8 s_F34_QUERY_00 = 0;//sz
static u8 s_F34_DATA_00 = 0;//sz
/*ZTE: modified by tong.weili for synaptics sensor identify 20120401 --*/

static struct i2c_client *g_synaptics_i2cc;

#ifdef CONFIG_HAS_EARLYSUSPEND
static void synaptics_rmi4_early_suspend(struct early_suspend *h);
static void synaptics_rmi4_late_resume(struct early_suspend *h);
#else
static int synaptics_rmi4_enable(struct input_dev *dev);
static int synaptics_rmi4_disable(struct input_dev *dev);
#endif

extern int prop_add( char *devname, char *item, char *value);/*ZTE: added by tong.weili 增加读取触摸屏型号接口 20110926 */

#ifdef CONFIG_HAS_VIRTUALKEY
/*virtual key support */
#ifdef CONFIG_ZTE_LCD_720_1280
static ssize_t synaptics_vkeys_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf)
{
	return sprintf(buf,
      __stringify(EV_KEY) ":" __stringify(KEY_BACK) ":120:1340:200:120"
	":" __stringify(EV_KEY) ":" __stringify(KEY_HOMEPAGE) ":360:1340:200:120"
	":" __stringify(EV_KEY) ":" __stringify(KEY_MENU) ":600:1340:200:120"
	"\n");
}
#else
static ssize_t synaptics_vkeys_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf)
{
	return sprintf(buf,
      __stringify(EV_KEY) ":" __stringify(KEY_BACK) ":200:1995:250:120"
	":" __stringify(EV_KEY) ":" __stringify(KEY_HOMEPAGE) ":560:1995:250:120"
	":" __stringify(EV_KEY) ":" __stringify(KEY_MENU) ":900:1995:250:120"
	"\n");
}
#endif

static struct kobj_attribute synaptics_vkeys_attr = {
	.attr = {
		.mode = S_IRUGO,
	},
	.show = &synaptics_vkeys_show,
};

static struct attribute *synaptics_properties_attrs[] = {
	&synaptics_vkeys_attr.attr,
	NULL
};

static struct attribute_group synaptics_properties_attr_group = {
	.attrs = synaptics_properties_attrs,
};
 
struct kobject *kobj_synaptics;
static void ts_key_report_synaptics_init(void)
{
	int rc = -EINVAL;

	/* virtual keys */
	synaptics_vkeys_attr.attr.name = "virtualkeys.Synaptics_RMI4";
	kobj_synaptics = kobject_create_and_add("board_properties",
				NULL);
	if (kobj_synaptics)
		rc = sysfs_create_group(kobj_synaptics,
			&synaptics_properties_attr_group);
	if (!kobj_synaptics || rc)
		pr_err("%s: failed to create board_properties\n",
				__func__);
}
#endif

#if (defined(TS_WATCHDOG) || (!defined(CONFIG_HAS_EARLYSUSPEND) ))
static struct i2c_client *client_synap;
#endif

static int synaptics_i2c_write(struct i2c_client *client, int reg, u8 data)
{
    u8 buf[2];
    int rc;
    int ret = 0;

    buf[0] = reg;
    buf[1] = data;
    rc = i2c_master_send(client, buf, 2);
    if (rc != 2)
    {
        dev_err(&client->dev, "synaptics_i2c_write FAILED: writing to reg %d\n", reg);
        ret = -1;
    }
    return ret;
}

static int synaptics_i2c_read(struct i2c_client *client, u8 addr, u8 *buf, u8 len)
{
	struct i2c_msg i2c_msg[2];
	int ret;
	
	i2c_msg[0].addr = client->addr;
	i2c_msg[0].flags = 0;
	i2c_msg[0].buf = &addr;
	i2c_msg[0].len = 1;

	i2c_msg[1].addr = client->addr;
	i2c_msg[1].flags = I2C_M_RD;
	i2c_msg[1].buf = buf;
	i2c_msg[1].len = len;

	ret = i2c_transfer(client->adapter, i2c_msg, 2);
	if (ret < 0) {
		printk(KERN_ERR "[TSP] I2C read failed @ 0x%x\n", addr);
		return ret;
	}
	return 0;
}

static int synaptics_rmi4_read_pdt(struct synaptics_rmi4 *ts)
{
	int ret = 0;
	int nFd = 0;
	int interruptCount = 0;
	__u8 data_length;

	struct i2c_msg fd_i2c_msg[2];
	__u8 fd_reg;
	struct synaptics_function_descriptor fd;

	struct i2c_msg query_i2c_msg[2];
	__u8 query[14];

	fd_i2c_msg[0].addr = ts->client->addr;
	fd_i2c_msg[0].flags = 0;
	fd_i2c_msg[0].buf = &fd_reg;
	fd_i2c_msg[0].len = 1;

	fd_i2c_msg[1].addr = ts->client->addr;
	fd_i2c_msg[1].flags = I2C_M_RD;
	fd_i2c_msg[1].buf = (__u8 *)(&fd);
	fd_i2c_msg[1].len = FD_BYTE_COUNT;

	query_i2c_msg[0].addr = ts->client->addr;
	query_i2c_msg[0].flags = 0;
	query_i2c_msg[0].buf = &fd.queryBase;
	query_i2c_msg[0].len = 1;

	query_i2c_msg[1].addr = ts->client->addr;
	query_i2c_msg[1].flags = I2C_M_RD;
	query_i2c_msg[1].buf = query;
	query_i2c_msg[1].len = sizeof(query);


	ts->hasF11 = false;
	ts->hasF19 = false;
	ts->hasF30 = false;
	ts->data_reg = 0xff;
	ts->data_length = 0;

	for (fd_reg = FD_ADDR_MAX; fd_reg >= FD_ADDR_MIN; fd_reg -= FD_BYTE_COUNT) {
         
		ret = i2c_transfer(ts->client->adapter, fd_i2c_msg, 2);
		if (ret < 0) {
			printk(KERN_ERR "I2C read failed querying RMI4 $%02X capabilities\n", ts->client->addr);
			return ret;
		}

		if (!fd.functionNumber) {
			/* End of PDT */
			ret = nFd;
			printk("Read %d functions from PDT\n", nFd);
			break;
		}
                            
		++nFd;
                #if 0
		printk("fd.functionNumber = %x\n", fd.functionNumber);
		printk("fd.queryBase = %x\n", fd.queryBase);
		printk("fd.commandBase = %x\n", fd.commandBase);
		printk("fd.controlBase = %x\n", fd.controlBase);
		printk("fd.dataBase = %x\n", fd.dataBase);
		printk("fd.intSrc = %x\n", fd.intSrc);
                #endif
		switch (fd.functionNumber) {
			case 0x01: /* Interrupt */
				ts->f01.data_offset = fd.dataBase;
				/*
				 * Can't determine data_length
				 * until whole PDT has been read to count interrupt sources
				 * and calculate number of interrupt status registers.
				 * Setting to 0 safely "ignores" for now.
				 */
				data_length = 0;
                
                          s_F01_CTRL_00 = fd.controlBase;
                          s_F01_QUERY_03 = fd.queryBase + 3;
                          //printk("[tong]:s_F01_CTRL_00 = 0x%x\n", s_F01_CTRL_00);
                          //printk("[tong]:s_F01_QUERY_03 = 0x%x\n", s_F01_QUERY_03);
                          break;
                
			case 0x11: /* 2D */
				ts->hasF11 = true;

				ts->f11.data_offset = fd.dataBase;
				ts->f11.interrupt_offset = interruptCount / 8;
				ts->f11.interrupt_mask = ((1 << INTERRUPT_SOURCE_COUNT(fd.intSrc)) - 1) << (interruptCount % 8);
                                                        
				ret = i2c_transfer(ts->client->adapter, query_i2c_msg, 2);
				if (ret < 0)
					printk(KERN_ERR "Error reading F11 query registers\n");

				ts->f11.points_supported = (query[1] & 7) + 1;
				if (ts->f11.points_supported == 6)
					ts->f11.points_supported = 10;

				printk("%d fingers\n", ts->f11.points_supported);

				ts->f11.data_length = data_length =
					/* finger status, four fingers per register */
					((ts->f11.points_supported + 3) / 4)
					/* absolute data, 5 per finger */
					+ 5 * ts->f11.points_supported
					;

                        {                                      
                            //u8 Read_data[4] = {0};      

                            //write max x
                            synaptics_i2c_write(ts->client, (fd.controlBase+6), USER_DEFINE_MAX_X&0xFF);
                            synaptics_i2c_write(ts->client, (fd.controlBase+7), (USER_DEFINE_MAX_X&0xF00) >> 8);

                            //write max y
                            synaptics_i2c_write(ts->client, (fd.controlBase+8), USER_DEFINE_MAX_Y&0xFF);
                            synaptics_i2c_write(ts->client, (fd.controlBase+9), (USER_DEFINE_MAX_Y&0xF00) >> 8);

                            s_F11_CTRL_00 = fd.controlBase;

                            /*
                            synaptics_i2c_read(ts->client, (fd.controlBase+6), Read_data, 4);           
                            s_F11_Max_X = Read_data[1] * 256 + Read_data[0];
                            s_F11_Max_Y = Read_data[3] * 256 + Read_data[2];
                            
                            printk("[tong]: Read_data = 0x%x, 0x%x, 0x%x, 0x%x",                                    
                                Read_data[0], Read_data[1], Read_data[2], Read_data[3]);     
                            printk("[synaptics]: max x: %d, max y: %d\n", s_F11_Max_X, s_F11_Max_Y);
                            */
                            
                            //U985: Read_data = 56, 4, b6 ,7    //X:0 ~~1110     Y:0~~1974      
                            //U880F: Read_data = 30, 4, 62 ,7    //X:0 ~~1072     Y:0~~1890      
                            //U950: Read_data = 73, 4, 60 ,7    //X:0 ~~1139     Y:0~~1888                               
                        }
                        //s_F11_QUERY_01 = fd.queryBase + 1;                     
                        //printk("[tong]:s_F11_QUERY_01 = 0x%x\n", s_F11_QUERY_01);//tong test
                        //s_F11_CTRL_14 = fd.controlBase + 14;
                        //printk("[tong]:s_F11_CTRL_14 = 0x%x\n", s_F11_CTRL_14);//tong test
                        
				break;
			case 0x19: /* Cap Buttons */
				ts->hasF19 = true;

				ts->f19.data_offset = fd.dataBase;
				ts->f19.interrupt_offset = interruptCount / 8;
				ts->f19.interrupt_mask = ((1 < INTERRUPT_SOURCE_COUNT(fd.intSrc)) - 1) << (interruptCount % 8);
				//ret = i2c_transfer(ts->client->adapter, query_i2c_msg, 2);
				if (ret < 0)
					printk(KERN_ERR "Error reading F19 query registers\n");


				ts->f19.points_supported = query[1] & 0x1F;
				ts->f19.data_length = data_length = (ts->f19.points_supported + 7) / 8;

				printk(KERN_NOTICE "$%02X F19 has %d buttons\n", ts->client->addr, ts->f19.points_supported);

				break;
			case 0x30: /* GPIO */
				ts->hasF30 = true;

				ts->f30.data_offset = fd.dataBase;
				ts->f30.interrupt_offset = interruptCount / 8;
				ts->f30.interrupt_mask = ((1 < INTERRUPT_SOURCE_COUNT(fd.intSrc)) - 1) << (interruptCount % 8);

				ret = i2c_transfer(ts->client->adapter, query_i2c_msg, 2);
				if (ret < 0)
					printk(KERN_ERR "Error reading F30 query registers\n");


				ts->f30.points_supported = query[1] & 0x1F;
				ts->f30.data_length = data_length = (ts->f30.points_supported + 7) / 8;

				break;

                    /*ZTE: added by tong.weili for synaptics sensor identify 20120401 ++*/
                    case 0x34:
                            synaptics_i2c_read(ts->client, fd.controlBase, s_synaptics_configID, 4);                         
                            if(0 == s_synaptics_configID[0] && 0 == s_synaptics_configID[1] 
                                && 0 == s_synaptics_configID[2] && 0 == s_synaptics_configID[3])
                            {
                                s_synaptics_Sensor_type = Type_S3000;
                            }
                            else
                            {
                                s_synaptics_Sensor_type = Type_S2202;
                            }
                            printk("[Synaptics]: sensor type:%d, ConfigID=[0x%x, 0x%x, 0x%x, 0x%x]\n", 
                                    s_synaptics_Sensor_type, s_synaptics_configID[0], s_synaptics_configID[1], 
                                    s_synaptics_configID[2], s_synaptics_configID[3]);
                            //U970 TPK:        ConfigID=[0, 0, 0, 0]?
                            //U970 Truly:      ConfigID=?
                            //U880F 宇顺: ConfigID=[0x32, 0x33, 0x30, 0x37]?
                            //U880F Truly:     ConfigID=?
                            //U985 TPK:        ConfigID=[0x32, 0x31, 0x30, 0x32]?
                            //U950 宇顺:  ConfigID=[0x32, 0x31, 0x30, 0x32]?
                            s_F34_QUERY_00 = fd.queryBase;//sz
                            s_F34_DATA_00 = fd.dataBase;//sz
                            printk("[tong]:s_F34_QUERY_00 = 0x%x\n", s_F34_QUERY_00);//tong test
                            printk("[tong]:s_F34_DATA_00 = 0x%x\n", s_F34_DATA_00);//tong test

				break;
                    /*ZTE: added by tong.weili for synaptics sensor identify 20120401 --*/
                        
			default:
				goto pdt_next_iter;
		}

		// Change to end address for comparison
		// NOTE: make sure final value of ts->data_reg is subtracted
		data_length += fd.dataBase;
		if (data_length > ts->data_length) {
			ts->data_length = data_length;
		}

		if (fd.dataBase < ts->data_reg) {
			ts->data_reg = fd.dataBase;
		}

pdt_next_iter:
		interruptCount += INTERRUPT_SOURCE_COUNT(fd.intSrc);
	}

	// Now that PDT has been read, interrupt count determined, F01 data length can be determined.
	ts->f01.data_length = data_length = 1 + ((interruptCount + 7) / 8);
	// Change to end address for comparison
	// NOTE: make sure final value of ts->data_reg is subtracted
	data_length += ts->f01.data_offset;
	if (data_length > ts->data_length) {
		ts->data_length = data_length;
	}


	// Change data_length back from end address to length
	// NOTE: make sure this was an address
	ts->data_length -= ts->data_reg;

	// Change all data offsets to be relative to first register read
	// TODO: add __u8 *data (= &ts->data[ts->f##.data_offset]) to struct rmi_function_info?
	ts->f01.data_offset -= ts->data_reg;
	ts->f11.data_offset -= ts->data_reg;
	ts->f19.data_offset -= ts->data_reg;
	ts->f30.data_offset -= ts->data_reg;

	ts->data = kcalloc(ts->data_length, sizeof(*ts->data), GFP_KERNEL);
	if (ts->data == NULL) {
		printk(KERN_ERR "Not enough memory to allocate space for RMI4 data\n");
		ret = -ENOMEM;
	}

	ts->data_i2c_msg[0].addr = ts->client->addr;
	ts->data_i2c_msg[0].flags = 0;
	ts->data_i2c_msg[0].len = 1;
	ts->data_i2c_msg[0].buf = &ts->data_reg;

	ts->data_i2c_msg[1].addr = ts->client->addr;
	ts->data_i2c_msg[1].flags = I2C_M_RD;
	ts->data_i2c_msg[1].len = ts->data_length;
	ts->data_i2c_msg[1].buf = ts->data;

	printk(KERN_ERR "RMI4 $%02X data read: $%02X + %d\n",
		ts->client->addr, ts->data_reg, ts->data_length);

	return ret;
}

#ifdef CONFIG_SYNA_BUTTONS_SCROLL
static int first_button(__u8 *button_data, __u8 points_supported)
{
	int b, reg;

	for (reg = 0; reg < ((points_supported + 7) / 8); reg++)
		for (b = 0; b < 8; b++)
			if ((button_data[reg] >> b) & 1)
				return reg * 8 + b;

	return -1;
}

static void synaptics_report_scroll(struct input_dev *dev,
                                    __u8 *button_data,
                                    __u8 points_supported,
                                    int ev_code)
{
	int scroll = 0;
	static int last_button = -1, current_button = -1;

	// This method is slightly problematic
	// It makes no check to find if lift/touch is more likely than slide
	current_button = first_button(button_data, points_supported);

	if (current_button >= 0 && last_button >= 0) {
		scroll = current_button - last_button;
		// This filter mostly works to isolate slide motions from lift/touch
		if (abs(scroll) == 1) {
			//printk("%s(): input_report_rel(): %d\n", __func__, scroll);
			input_report_rel(dev, ev_code, scroll);
		}
	}

	last_button = current_button;
}
#endif

#if 0
static int
proc_read_val(char *page, char **start, off_t off, int count, int *eof,
	  void *data)
{
	int len = 0;
	len += sprintf(page + len, "%s\n", "touchscreen module");
	len += sprintf(page + len, "name     : %s\n", "synaptics");
	#ifdef CONFIG_ZTE_R750_U960
	len += sprintf(page + len, "i2c address  : %x\n", 0x23);
	len += sprintf(page + len, "IC type    : %s\n", "2000 series");
	len += sprintf(page + len, "firmware version    : %s\n", "TM1551");	
	#else
	len += sprintf(page + len, "i2c address  : 0x%x\n", 0x22);
	len += sprintf(page + len, "IC type    : %s\n", "3000 series");
	len += sprintf(page + len, "firmware version    : %s\n", "TM1925");	
	#endif
	len += sprintf(page + len, "module : %s\n", "synaptics + TPK");
	
	if (off + count >= len)
		*eof = 1;
	if (len < off)
		return 0;
	*start = page + off;
	return ((count < len - off) ? count : len - off);
}

static int proc_write_val(struct file *file, const char *buffer,
           unsigned long count, void *data)
{
		unsigned long val;
		sscanf(buffer, "%lu", &val);
		if (val >= 0) {
		if(val==1)
		{
                     //synaptics_i2c_write(ts->client, 0x37, 0x01);      /*reduced reporting mode*/
		}
		else
		{
                     //synaptics_i2c_write(ts->client, 0x37, 0x00);      /*continuous reporting mode*/
		}
			
			return count;
		}
		return -EINVAL;
}
#endif

#ifdef TS_WATCHDOG
#define TOTAL_COUNT 5
#define DEV_THRESHOLD 10

typedef struct saved_poinits_value {
		int x;
		int y;
} POINT_t;

static POINT_t saved_points[5][TOTAL_COUNT];
static int points_Per_Finger[5];
static int checkcount = 0;


static void synaptics_reset( void )
{
	printk("[TSP-TEMP] synaptics_reset\n");

      synaptics_i2c_write(client_synap, (s_F01_CTRL_00+1), 0);     /* disable interrupt, */    //F01_RMI_CTRL01
      synaptics_i2c_write(client_synap, s_F01_CTRL_00, 0x01);      /* deep sleep */            //F01_RMI_CTRL00
  
      msleep(40);
  
      synaptics_i2c_write(client_synap, s_F01_CTRL_00, 0x0);      /* wakeup */ 
      synaptics_i2c_write(client_synap, (s_F01_CTRL_00+1), 4);     /* enable interrupt, */
}

static void synaptics_watchdog_work_func(struct work_struct *work)
{      
	synaptics_reset();
}

#if USE_HRTIMER_4WATCHDOG
static enum hrtimer_restart total_timer_func(struct hrtimer *timer)
{
	ts_watchgod_enable = 0;       
	//hrtimer_cancel(timer);	 
#ifdef TSP_DEBUG
	printk("[TSP]total timer canceled\n");	
#endif
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart no_report_timer_func(struct hrtimer *timer)
{    
#ifdef TSP_DEBUG
	int ret = 0;
#endif
	queue_work(tsp_watchdog_wq, &watchdog_work);
	//if(ts_watchgod_enable)
	//{
		//ret = hrtimer_start(&watchdog_no_report_timer, ktime_set(TS_WATCHDOG_NO_REPORT_TIME, 0), HRTIMER_MODE_REL);
	//}
#ifdef TSP_DEBUG
	printk("[TSP]no_report_timer restarted,ret:%d, \n", ret);		
#endif
	return HRTIMER_NORESTART;
}
#else
static void total_timer_func(unsigned long handle)
{
	ts_watchgod_enable = 0;       
	//hrtimer_cancel(timer);	 
#ifdef TSP_DEBUG
	printk("[TSP]total timer canceled\n");	
#endif
	return;
}

static void no_report_timer_func(unsigned long handle)
{    
#ifdef TSP_DEBUG
	int ret = 0;
#endif
	queue_work(tsp_watchdog_wq, &watchdog_work);
	//if(ts_watchgod_enable)
	//{
		//ret = hrtimer_start(&watchdog_no_report_timer, ktime_set(TS_WATCHDOG_NO_REPORT_TIME, 0), HRTIMER_MODE_REL);
	//}
#ifdef TSP_DEBUG
	printk("[TSP]no_report_timer restarted,ret:%d, \n", ret);		
#endif
	return;
}
#endif
void savePoint(int finger, int x, int y)
{    
      int count = points_Per_Finger[finger];
#ifdef TSP_DEBUG
	printk("[TSP-TEMP] f:%d, x:%d, y:%d\n", finger, x, y);
#endif
	saved_points[finger][count].x = x;
	saved_points[finger][count].y = y;
       points_Per_Finger[finger]++;
}

bool ispointInvalid( void )
{     
      int sumx= 0, sumy = 0, meanx[5], meany[5], devx[5],devy[5]; 
	int devx_total = 0, devy_total = 0, dev = 0;
	int i, j;
	int fingerCount = 0;
	/*calculate mean data of each finger in total*/
	for(i=0; i<5; i++) {
		sumx = 0;
		sumy = 0;
		if( points_Per_Finger[i]) {   
			fingerCount++;
		#ifdef TSP_DEBUG
			printk("[TSP-TEMP] point %d,points_Per_Finger[i]:%d \n", i, points_Per_Finger[i]);
		#endif
			for(j=0; j< points_Per_Finger[i]; j++ ) {
				sumx += saved_points[i][j].x;
				sumy += saved_points[i][j].y;
			#ifdef TSP_DEBUG
				printk("[TSP-TEMP] x: %d, y: %d \n",saved_points[i][j].x, saved_points[i][j].y);
			#endif
			}
			meanx[i] = sumx/ points_Per_Finger[i];
			meany[i] = sumy/ points_Per_Finger[i];
		}
	}
	
	/*calculate deviation of each aixs base on mean value in last step*/
	for(i=0; i<5; i++) {
		sumx = 0;
		sumy = 0;
		if( points_Per_Finger[i]) {
			for(j=0; j< points_Per_Finger[i]; j++ ) {
				sumx += abs(saved_points[i][j].x - meanx[i]);
				sumy += abs(saved_points[i][j].y - meany[i]);
		}
		devx[i] = sumx /  points_Per_Finger[i];
		devy[i] = sumy /  points_Per_Finger[i];
		devx_total += devx[i];
		devy_total += devy[i];
		}
	}
	
	/*dev should be smaller than normal touch when TP through a calibration failure */
      if(fingerCount > 0) /*ZTE: modified by tong.weili 防止除法错误 20120521*/
	    dev = (devx_total + devy_total) / (2*fingerCount);
#ifdef TSP_DEBUG
	printk("[TSP-TEMP] %s dev: %d fingerCount: %d\n", __func__, dev, fingerCount);
#endif
	if(dev < DEV_THRESHOLD && fingerCount > 1){
		printk("[TSP-TEMP] invalid points confirmed\n");
		return true;
	} else if(dev < DEV_THRESHOLD && fingerCount == 1) {
		printk("[TSP-TEMP] fingerconunt =1 ,unable to judge\n");
	}	
	return false;
}
#endif

#if 0
static void synaptics_rmi4_work_func(struct work_struct *work)
{
	int ret;

	struct synaptics_rmi4 *ts = container_of(work, struct synaptics_rmi4, work);

		printk(KERN_ERR "%s enter\n", __func__);     //shihuiqin

	ret = i2c_transfer(ts->client->adapter, ts->data_i2c_msg, 2);

	if (ret < 0) 
	{
		printk(KERN_ERR "%s: i2c_transfer failed\n", __func__);
	} else 
	{
		__u8 *interrupt = &ts->data[ts->f01.data_offset + 1];


      //shihuiqin for test
	printk(KERN_ERR "f01 data  offset:%d,int offset:%d,int mask:%d,hasF11:%d\n", ts->f01.data_offset,ts->f11.interrupt_offset,ts->f11.interrupt_mask,ts->hasF11);

		if (ts->hasF11 && interrupt[ts->f11.interrupt_offset] & ts->f11.interrupt_mask) 
		{

			__u8 finger_status_reg = 0;
			__u8 fsr_len = (ts->f11.points_supported + 3) / 4;
			__u8 finger_status;
			__u8 *f11_data = &ts->data[ts->f11.data_offset];

//#if defined CONFIG_SYNA_MT || defined CONFIG_SYNA_MULTIFINGER
                       int f;
			  static __u8 prev_f=0,prev_finger_status[5]={0};
			  static u12 prev_x[5]={0},prev_y[5]={0};
        			        for (f = 0; f < ts->f11.points_supported; ++f) 
                                     {

        				__u8 reg = fsr_len + 5 * f;
        				__u8 *finger_reg = &f11_data[reg];
					u12 x = (finger_reg[0] * 0x10) | (finger_reg[2] % 0x10);
        				u12 y = (finger_reg[1] * 0x10) | (finger_reg[2] / 0x10);

									 

        				if (!(f % 4))
					{
        					finger_status_reg = f11_data[f / 4];
                                    }
                                                               
        				finger_status = (finger_status_reg >> ((f % 4) * 2)) & 3;

				if((TwoPointsRepeat(prev_x[f],prev_y[f],x,y))&&(finger_status==prev_finger_status[f]))
				{
                                    printk("TwoPointsRepeat\n");
				}
                            else
                            {

				   	     prev_f =(__u8) f;
					     prev_finger_status[f]=(__u8)finger_status;
					     prev_x[f]=x;
					     prev_y[f]=y;

                                 //if(y > 1883)
				     if(y>1995)
				     {
				     
			                  if((x < 300/*364*/))
					    {
				                  input_report_key(ts->input_dev, KEY_MENU, finger_status);
			                   }	
					     else if ((x > 350)&&(x < 555/*364*/))     //(x < 576)
					     {
				                   input_report_key(ts->input_dev, KEY_HOME, finger_status);
			                    }
					     else if ((x > 600)&&(x < 850/*364*/))   //(x < 788)
					     {
				                  input_report_key(ts->input_dev, KEY_BACK, finger_status);
			                   }
					     else if((x > 900)) 
					     {
				                   input_report_key(ts->input_dev, KEY_SEARCH, finger_status);
			                   }	
					     else
					     {
                                             printk("press search, (%d,%d)\n",x,y);
					     }
			//input_sync(ts->input_dev);
		}
		                  else if(y < 1883)
				    {
        				    if (finger_status == 1 )
        				{
        				u4 wx = finger_reg[3] % 0x10;
        				u4 wy = finger_reg[3] / 0x10;
        				
//#ifdef CONFIG_SYNA_MT
                                                        printk( KERN_ERR "shihuiqin f: %d, wx:%d, wy: %d, x: %d, y: %d\n", f,wx,wy,x,y);




                                                        input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, f + 1);
                                                        input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, max(wx, wy));
                                                        //input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, max(wx, wy));
                                                        input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);
                                                        input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);
				                            input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 10);//ZTE_PRESS_WLY_0524
													
                                                        input_mt_sync(ts->input_dev);
//#endif

#ifdef CONFIG_SYNA_MULTIFINGER
        				/* Report multiple fingers for software prior to 2.6.31 - not standard - uses special input.h */
        				input_report_abs(ts->input_dev, ABS_X_FINGER(f), x);
        				input_report_abs(ts->input_dev, ABS_Y_FINGER(f), y);
        				input_report_abs(ts->input_dev, ABS_Z_FINGER(f), z);
#endif
        				}
					//shihuiqin added ++
					else
						input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);//ZTE_PRESS_WLY_0524
					//shihuiqin added --
			             }
				      else
				      	{

					  }
				      // input_sync(ts->input_dev);

                            }		
        			ts->f11_fingers[f].status = finger_status;
		}
                        
                                                if (ts->hasEgrPalmDetect) {
                                                        input_report_key(ts->input_dev,
                                                        BTN_DEAD,
                                                        f11_data[f + EGR_PALM_DETECT_REG] & EGR_PALM_DETECT);
                                                }
                                                if (ts->hasEgrFlick) {
                                                        if (f11_data[f + EGR_FLICK_REG] & EGR_FLICK) {
                                                                input_report_rel(ts->input_dev, REL_X, f11_data[f + 2]);
                                                                input_report_rel(ts->input_dev, REL_Y, f11_data[f + 3]);
                                                        }
                                                }
                                                if (ts->hasEgrSingleTap) {
                                                        input_report_key(ts->input_dev,
                                                        BTN_TOUCH,
                                                        f11_data[f + EGR_SINGLE_TAP_REG] & EGR_SINGLE_TAP);
                                                }
                                                if (ts->hasEgrDoubleTap) {
                                                        input_report_key(ts->input_dev,
                                                        BTN_TOOL_DOUBLETAP,
                                                        f11_data[f + EGR_DOUBLE_TAP_REG] & EGR_DOUBLE_TAP);
                                                }
 //#endif
		}

#ifdef  CONFIG_SYNA_BUTTONS
                        synaptics_report_buttons(ts->input_dev,
                                                 &ts->data[ts->f19.data_offset],
                                                 ts->f19.points_supported, BTN_F19);
#endif
#ifdef  CONFIG_SYNA_BUTTONS_SCROLL
                        synaptics_report_scroll(ts->input_dev,
                                                &ts->data[ts->f19.data_offset],
                                                ts->f19.points_supported,
                                                SCROLL_ORIENTATION);
#endif
                        //printk(KERN_ERR "abs3\n");
                        input_sync(ts->input_dev);
	}

	if (ts->use_irq)
	{
		enable_irq(ts->client->irq);
        }
}

#else
static void synaptics_rmi4_work_func(struct work_struct *work)
{
	int ret;
       /* number of touch points - fingers down in this case */
	int fingerDownCount;
#ifdef TS_WATCHDOG		
	static u32 interval_time, jiffies_now, jiffies_last;
	POINT_t points[5];
#endif

	struct synaptics_rmi4 *ts = container_of(work, struct synaptics_rmi4, work);

	//	printk(KERN_ERR "%s enter\n", __func__);     //shihuiqin


      	fingerDownCount = 0;
	ret = i2c_transfer(ts->client->adapter, ts->data_i2c_msg, 2);

	if (ret < 0) 
	{
		printk(KERN_ERR "%s: i2c_transfer failed\n", __func__);
	} else 
	{
		__u8 *interrupt = &ts->data[ts->f01.data_offset + 1];

		if (ts->hasF11 && (interrupt[ts->f11.interrupt_offset] & ts->f11.interrupt_mask)) 
		{

			__u8 fsr_len = (ts->f11.points_supported + 3) / 4;
			__u8 finger_status;
			__u8 *f11_data = &ts->data[ts->f11.data_offset];
			
                    /*finger data related*/
			__u8 reg;
			__u8 *finger_reg;
			 u12 x;
			 u12 y;
			 
                       /*determine which area(key or lcd) the finger0 is in*/
                       reg = fsr_len;
                       finger_reg = &f11_data[reg];
                       y = (finger_reg[1] * 0x10) | (finger_reg[2] / 0x10);

                      if(0)    //(y>=1893)     /*key area*/
                      {

        				finger_status = f11_data[0]& 3;    //finger0 status
                                   x = (finger_reg[0] * 0x10) | (finger_reg[2] % 0x10);
				     
			                  if((x < 268))
					    {
				                  input_report_key(ts->input_dev, KEY_MENU, finger_status);
						     input_sync(ts->input_dev);
			                   }	
					     else if ((x > 362)&&(x < 562)) 
					     {
				                   input_report_key(ts->input_dev, KEY_HOME, finger_status);
						     input_sync(ts->input_dev);
			                    }
					     else if ((x > 642)&&(x < 842))
					     {
				                  input_report_key(ts->input_dev, KEY_BACK, finger_status);
						     input_sync(ts->input_dev);
			                   }
					     else if((x > 897)) 
					     {
				                   input_report_key(ts->input_dev, KEY_SEARCH, finger_status);
						     input_sync(ts->input_dev);
			                   }	
					     else
					     {
                                             printk("press NULL, (%d,%d)\n",x,y);
					     }
                      }
                      else    /*lcd area*/
                      {
                             int f;
                             __u8 finger_status_reg = 0;

                             /* First we need to count the fingers and generate some events related to that. */
                             for (f = 0; f < ts->f11.points_supported; f++) 
                             {
                                   /*finger status*/
                                   finger_status_reg = f11_data[f / 4];
                                   finger_status = (finger_status_reg >> ((f % 4) * 2)) & 3;

                                   /*finger data*/
                                   reg = fsr_len + 5 * f;
                                   finger_reg = &f11_data[reg];
                                   y = (finger_reg[1] * 0x10) | (finger_reg[2] / 0x10);

                                    if(1)//(y < 1893 )
                                    {
                                           if (finger_status == f11_finger_accurate ) 
                                           {
                                                  fingerDownCount++;
                                                  ts->wasdown = true;
                                           }
                                    }
                             }
                             input_report_key(ts->input_dev,BTN_TOUCH, fingerDownCount);

		#ifdef TS_WATCHDOG		
	         memset(points, 0, sizeof(points));
		#endif
                             for (f = 0; f < ts->f11.points_supported; ++f) 
                             {
                                    /*finger data*/
                                    reg = fsr_len + 5 * f;
                                    finger_reg = &f11_data[reg];
                                    y = (finger_reg[1] * 0x10) | (finger_reg[2] / 0x10);

                                    /*finger status*/
                                    finger_status_reg = f11_data[f / 4];
                                    finger_status = (finger_status_reg >> ((f % 4) * 2)) & 3;

                                    if(1)//(y < 1893)   
                                    {
#if 1
								
                                           if (finger_status == f11_finger_accurate) 
                                           {
                                                 u4 wx = finger_reg[3] % 0x10;
                                                 u4 wy = finger_reg[3] / 0x10;
                                                 int z=0;
								   
					x = (finger_reg[0] * 0x10) | (finger_reg[2] % 0x10);			   
				       z = finger_reg[4];
				#ifdef TS_WATCHDOG		
					points[f].x = x;
					points[f].y = y;
				#endif
                                 /*ZTE: modified by tong.weili for dynamic print 20120202 ++*/
                                 if(debug)
                                 {
                                     printk( KERN_ERR "[tong] f: %d, state:%d, x: %d, y: %d, z: %d, wx: %d, wy: %d\n", 
                                            f,finger_status,x,y,z,wx,wy);
                                 }
                                 /*ZTE: modified by tong.weili for dynamic print 20120202 --*/


				/* if this is the first finger report normal
				ABS_X, ABS_Y, PRESSURE, TOOL_WIDTH events for
				non-MT apps. Apps that support Multi-touch
				will ignore these events and use the MT events.
				Apps that don't support Multi-touch will still
				function.
				*/

#ifdef CONFIG_SYNA_MULTI_TOUCH
				/* Report Multi-Touch events for each finger */
				/* major axis of touch area ellipse */
				input_report_abs(ts->input_dev, ABS_MT_PRESSURE, z);
				//input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, z);
				/* minor axis of touch area ellipse */
				//input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR,
				//		max(wx, wy));
				/* Currently only 2 supported - 1 or 0 */
				//input_report_abs(ts->input_dev, ABS_MT_ORIENTATION,
				//	(wx > wy ? 1 : 0));
				input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);

				/* TODO: Tracking ID needs to be reported but not used yet. */
				/* Could be formed by keeping an id per position and assiging */
				/* a new id when fingerStatus changes for that position.*/
				/*input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID,
						f+1);*/

				/* MT sync between fingers */
				input_mt_sync(ts->input_dev);
#endif

        				}
	#endif
			           }
		}
		#ifdef TS_WATCHDOG
			if(ts_watchgod_enable) {
				if(checkcount <= TOTAL_COUNT ) { //accumulate points
					if(checkcount == 0) {
						jiffies_last = jiffies;
						checkcount = 1 ;
					}
					jiffies_now = jiffies;
					interval_time = jiffies_to_msecs(jiffies_now - jiffies_last);
					if(interval_time > 100) {
						jiffies_last = jiffies_now;
						checkcount++;
						for(f= 0; f<5; f++) {
						if(points[f].x != 0 && points[f].y != 0)
							savePoint(f, points[f].x, points[f].y);		
						}
					}
				}
					 
       			 if(checkcount == TOTAL_COUNT) {
				       //points are bad, reset the ts controller
					if (true == ispointInvalid()) {
						synaptics_reset();
					}
					checkcount = 0;
					for(f= 0; f<5; f++) 
						points_Per_Finger[f] = 0;
				}
				//recieve points, reset the watchdog timer
				#if USE_HRTIMER_4WATCHDOG
					hrtimer_cancel(&watchdog_no_report_timer);
					hrtimer_start(&watchdog_no_report_timer, ktime_set(TS_WATCHDOG_NO_REPORT_TIME, 0), HRTIMER_MODE_REL);
				#else
					//mod_timer(&watchdog_no_report_timer, jiffies + msecs_to_jiffies(TS_WATCHDOG_NO_REPORT_TIME*MSEC_PER_SEC));
				#endif
			}
		#endif
#if 1
	/* if we had a finger down before and now we don't have any send a button up. */
	if ((fingerDownCount == 0) && ts->wasdown) {
		ts->wasdown = false;

#ifdef CONFIG_SYNA_MULTI_TOUCH
	//	input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
	//	input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0);
	//	input_report_abs(ts->input_dev, ABS_MT_POSITION_X, ts->oldX);
	//	input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, ts->oldY);
		//input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, 1);
		input_mt_sync(ts->input_dev);
              input_report_key(ts->input_dev,BTN_TOUCH, 0);
#endif

	//	input_report_abs(ts->input_dev, ABS_X, ts->oldX);
	//	input_report_abs(ts->input_dev, ABS_Y, ts->oldY);
		ts->oldX = ts->oldY = 0;
        
             /*ZTE: modified by tong.weili for dynamic print 20120202 ++*/
             if(debug)
             {
		printk(KERN_ERR "%s: Finger up.", __func__);
	}
             /*ZTE: modified by tong.weili for dynamic print 20120202 --*/
		
	}
#endif
              input_sync(ts->input_dev);     /* sync after groups of events */
		}

	}
	else
	{
              printk(KERN_ERR "mask=%d", interrupt[ts->f11.interrupt_offset]);
	 }
}
	if (ts->use_irq)
	{
		enable_irq(ts->client->irq);
        }
}
#endif
static enum hrtimer_restart synaptics_rmi4_timer_func(struct hrtimer *timer)
{
	struct synaptics_rmi4 *ts = container_of(timer, \
					struct synaptics_rmi4, timer);

	queue_work(synaptics_wq, &ts->work);

	hrtimer_start(&ts->timer, ktime_set(0, 12 * NSEC_PER_MSEC), HRTIMER_MODE_REL);

	return HRTIMER_NORESTART;
}

irqreturn_t synaptics_rmi4_irq_handler(int irq, void *dev_id)
{
	struct synaptics_rmi4 *ts = dev_id;

		//printk(KERN_ERR "%s enter\n", __func__);     //shihuiqin


	disable_irq_nosync(ts->client->irq);
	queue_work(synaptics_wq, &ts->work);

	return IRQ_HANDLED;
}

struct synaptics_fw {
	size_t fw_size;
	char *buff;
	int chip_type;
	int buffer_ready;
};

static struct synaptics_fw g_synaptics_fw;
struct synaptics_rmi4 *ts_f34;
static u8 fw_ver[2];

static int synaptics_dev_open(struct inode *inode, struct file *filp)
{
	int ret = 0;

	disable_irq(g_synaptics_i2cc->irq);

	memset(&g_synaptics_fw, 0, sizeof(struct synaptics_fw));

	return ret;
}

static int synaptics_dev_release(struct inode *inode, struct file *file)
{
	if(g_synaptics_fw.buff){
		kfree(g_synaptics_fw.buff);
		g_synaptics_fw.buff = NULL;
	}

	return 0;
}

static ssize_t synaptics_dev_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *offset)
{
	printk("write data size =%ld-%ld\n", count, offset);
	if(count != g_synaptics_fw.fw_size ){
		pr_err("%s : fw size is not same as ioctl size\n", __func__);
		return -EINVAL;
	}

	if(!g_synaptics_fw.buff){
		pr_err("%s : the receive buff is null\n", __func__);
		return -ENOMEM;
	}

	if (copy_from_user(g_synaptics_fw.buff, buf, count)) {
		pr_err("%s : failed to copy from user space\n", __func__);
		return -EFAULT;
	}

	g_synaptics_fw.buffer_ready |= 0x02;

	return count;
}

static ssize_t synaptics_dev_read(struct file *filp, char __user *buf,
		size_t count, loff_t *offset)
{
	size_t bytes;

	printk("read data size =%ld-%ld\n", count, g_synaptics_fw.fw_size);
	if(count > g_synaptics_fw.fw_size)
		bytes = g_synaptics_fw.fw_size;
	else
		bytes = count;

	if (copy_to_user(buf, g_synaptics_fw.buff, bytes)) {
		pr_warning("%s : failed to copy to user space\n", __func__);
		return -EFAULT;
	}

	return g_synaptics_fw.fw_size;
}

static int synaptics_i2c_multi_write(struct i2c_client *client, int reg, u8 * buf, int count)
{
	int rc;
	int ret = 0;
	int i;
	unsigned char *txbuf;
	unsigned char txbuf_most[17]; /* Use this buffer for fast writes of 16
	bytes or less.  The first byte will
	contain the address at which to start
	the write. */

	if (count < sizeof(txbuf_most)) {
		/* Avoid an allocation if we can help it. */
		txbuf = txbuf_most;
	} else {
		/* over 16 bytes write we'll need to allocate a temp buffer */
		txbuf = kzalloc(count + 1, GFP_KERNEL);
		if (!txbuf)
			return -ENOMEM;
	}

	/* Yes, it stinks here that we have to copy the buffer */
	/* We copy from valp to txbuf leaving
	the first location open for the address */
	for (i = 0; i < count; i++)
		txbuf[i + 1] = buf[i];

	txbuf[0] = reg; /* put the address in the first byte */
	rc = i2c_master_send(client, txbuf, count+1);
	if (rc != (count+1))
	{
		dev_err(&client->dev, "synaptics_i2c_write FAILED: writing to reg %d\n", reg);
		ret = -1;
	}

	return ret;
}

static void flash_program(void)
{
	 u8 buf[20];
	 int ret=0;
	 int i;
	 int blockindex;
	 u8* data = g_synaptics_fw.buff;

       printk( "%s: Synaptics firmware downloading..., will take about 35 seconds!\n", __func__);
		
       synaptics_i2c_write(g_synaptics_i2cc, (s_F01_CTRL_00+1), 0x00);       //disable all  INT  //modified

	   //enable flash programming
	ret = synaptics_i2c_read(g_synaptics_i2cc, (s_F34_QUERY_00), buf, 2);  //modified
	printk(KERN_CRIT "ret=%d,bootloader id=( %x,%x)\n",ret,buf[0],buf[1]);

	synaptics_i2c_multi_write(g_synaptics_i2cc, (s_F34_DATA_00+2), buf,2);//modified

	ret = synaptics_i2c_read(g_synaptics_i2cc, (s_F34_QUERY_00+3), buf, 2); //modified
	printk(KERN_CRIT "ret=%d,block size count=( %x,%x)\n",ret,buf[0],buf[1]);
	ret = synaptics_i2c_read(g_synaptics_i2cc, (s_F34_QUERY_00+5), buf, 2); //modified
	printk(KERN_CRIT "ret=%d,firmware block num count=( %x,%x)\n",ret,buf[0],buf[1]);

	ret = synaptics_i2c_read(g_synaptics_i2cc, (s_F34_QUERY_00+7), buf, 2); //modified
	printk(KERN_CRIT "ret=%d,configuration block num count=( %x,%x)\n",ret,buf[0],buf[1]);

       synaptics_i2c_write(g_synaptics_i2cc, 0x12, 0x0f);       //enable flash programming //modified
      msleep(200);
      //msleep(600);//tong test

      synaptics_rmi4_read_pdt(ts_f34);
      
	i=0;
	do
	{
		ret = synaptics_i2c_read(g_synaptics_i2cc, 0x12, buf, 1); //modified
		i++;
		printk(KERN_CRIT "command in progress,i=%x\n",i);
	}while(((ret!=0)||((buf[0]&0x80)==0)) &&(i<0xff)); 
	
	printk(KERN_CRIT "flash cmd0x12=%x\n",buf[0]);
	synaptics_i2c_read(g_synaptics_i2cc, 0x14, buf, 1); //modified
	printk(KERN_CRIT "INT status 0x14=%x\n",buf[0]);


	//program the firmaware image 
	ret = synaptics_i2c_read(g_synaptics_i2cc, s_F34_QUERY_00, buf, 2);  //modified ***********************
	printk(KERN_CRIT "2 ret=%d,bootloader id=( %x,%x)\n",ret,buf[0],buf[1]);

	synaptics_i2c_multi_write(g_synaptics_i2cc, (s_F34_DATA_00+2), buf,2);//modified
	synaptics_i2c_write(g_synaptics_i2cc, 0x12, 0x03);       //erase flash  //modified
	msleep(250);

	i=0;
	do
	{
		ret = synaptics_i2c_read(g_synaptics_i2cc, 0x12, buf, 1);  //modified
		i++;
		printk(KERN_CRIT "2 command in progress,i=%x\n",i);
	}while(((ret!=0)||((buf[0]&0x80)==0)) &&(i<0xff)); 
	printk(KERN_CRIT " 2 flash cmd0x12=%x\n",buf[0]);
	synaptics_i2c_read(g_synaptics_i2cc, 0x14, buf, 1);//modified
	printk(KERN_CRIT "2 INT status 0x14=%x\n",buf[0]);

	buf[0] = 0;
	buf[1] = 0;
	synaptics_i2c_multi_write(g_synaptics_i2cc, s_F34_DATA_00, buf,2);//modified
	
	//program firmware
	//the frist 0x100 bytes is the header message
	data +=0x100;
      for(blockindex=0;blockindex<0xb00;blockindex++)
	{
		printk(KERN_ALERT "blockindex(%x,%x);syna_bin:%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n", \
			buf[0],buf[1],data[0],data[1], data[2], data[3],	data[4], data[5] , data[6], data[7],data[8], data[9], data[10],data[11]);
		
		synaptics_i2c_multi_write(g_synaptics_i2cc, (s_F34_DATA_00+2), data,16);    //??  //modified
		synaptics_i2c_write(g_synaptics_i2cc, 0x12, 0x02);       //programming firmware  //modified
		msleep(4);
		data +=16;
		i=0;
		do{
			ret = synaptics_i2c_read(g_synaptics_i2cc, 0x12, buf, 1);  //modified
			i++;
			//printk(KERN_CRIT "3 command in progress,i=%x\n",i);
		}while(((ret!=0)||((buf[0]&0x80)==0)) &&(i<0xff)); 
		//printk(KERN_CRIT " 3 flash cmd0x12=%x\n",buf[0]);
		synaptics_i2c_read(g_synaptics_i2cc, 0x14, buf, 1);   //modified
		//printk(KERN_CRIT "3 INT status 0x14=%x\n",buf[0]);
	}
      printk(KERN_CRIT "blockindex=%x\n", blockindex);//0x6E0

	buf[0] = 0;
	buf[1] = 0;
	synaptics_i2c_multi_write(g_synaptics_i2cc, s_F34_DATA_00, buf,2);//modified
	//program the configration image
	for(blockindex=0;blockindex<0x20;blockindex++)
	{
		/*printk(KERN_CRIT "blockindex(%x,%x);syna_bin:%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n", \
				buf[0],buf[1],data[0],data[1], data[2], data[3],	data[4], data[5] , data[6], data[7],data[8], data[9], data[10],data[11]);
		*/
		synaptics_i2c_multi_write(g_synaptics_i2cc, (s_F34_DATA_00+2), data,16);    //??  //modified
		synaptics_i2c_write(g_synaptics_i2cc, 0x12, 0x06);       //enable write configration block  //modified
		msleep(4);
		data +=16;
		i=0;
		do
		{
			ret = synaptics_i2c_read(g_synaptics_i2cc, 0x12, buf, 1);  //modified
			i++;
			//printk(KERN_CRIT "31 command in progress,i=%x\n",i);
		}while(((ret!=0)||((buf[0]&0x80)==0)) &&(i<0xff));
		
		//printk(KERN_CRIT " 31 flash cmd0x12=%x\n",buf[0]);
		synaptics_i2c_read(g_synaptics_i2cc, 0x14, buf, 1);   //modified
		//printk(KERN_CRIT "31 INT status 0x14=%x\n",buf[0]);
	}
      printk(KERN_CRIT "blockindex=%x\n", blockindex);//0x20

      msleep(200);

	//zhangqi add for test
	ret = synaptics_i2c_read(g_synaptics_i2cc, 0x13, buf, 1); //modified
	printk(KERN_CRIT "zhangqi add before reset,0x13=%x\n",buf[0]);
	//reset device 
	synaptics_i2c_write(g_synaptics_i2cc, s_F01_CMD_00, 0x01);       //reset  //modified *************************

      msleep(500);

	//zhangqi add for test
	ret = synaptics_i2c_read(g_synaptics_i2cc, 0x13, buf, 1); //modified
	printk(KERN_CRIT "zhangqi add after reset ,0x13=%x\n",buf[0]);
	i=0;
	do{
		ret = synaptics_i2c_read(g_synaptics_i2cc, 0x13, buf, 1); //modified
		i++;
		printk(KERN_CRIT "4 command in progress,i=%x,0x13=%x\n",i,buf[0]);
	}while(((ret!=0)||((buf[0]&0x40)!=0)) &&(i<0x1ff));
	
	synaptics_i2c_read(g_synaptics_i2cc, 0x14, buf, 1);   //modified
	printk(KERN_CRIT "4 INT status 0x14=%x\n",buf[0]);

        ret = synaptics_rmi4_read_pdt(ts_f34);

      //init
	synaptics_i2c_write(g_synaptics_i2cc, (s_F01_CTRL_00+1), 0x04);       //only eanble abs INT   //modified ************************
	 

	ret = synaptics_i2c_read(g_synaptics_i2cc, (s_F01_QUERY_03-1), fw_ver, 2); //modified ************************
	printk(KERN_CRIT "ret=%d,new firmware version 0x%x 0x%x\n",ret,fw_ver[0],fw_ver[1]);

	enable_irq(g_synaptics_i2cc->irq);

}

#define SYNAPTICS_SET_FWSIZE     _IOW('S', 0x01, unsigned long)
#define SYNAPTICS_SET_CHIPTYPE    _IOW('S', 0x02, unsigned long)
#define SYNAPTICS_FW_UPDATE    _IO('S', 0x03)
#define SYNAPTICS_GET_HW    _IOR('S', 0x04, unsigned char[4])

static long synaptics_dev_ioctl(struct file *file,
							      unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;

	switch (cmd) {
		case SYNAPTICS_SET_FWSIZE:
			if (copy_from_user(&g_synaptics_fw.fw_size, argp, sizeof(size_t))) {
				return -EFAULT;
			}

			if(g_synaptics_fw.buff)
				kfree(g_synaptics_fw.buff);

			g_synaptics_fw.buff = kzalloc(g_synaptics_fw.fw_size, GFP_KERNEL);
			if(!g_synaptics_fw.buff)
				return -ENOMEM;
			g_synaptics_fw.buffer_ready = 0x01;
			break;

		case SYNAPTICS_SET_CHIPTYPE:
			if (copy_from_user(&g_synaptics_fw.chip_type, argp, sizeof(int))) {
				return -EFAULT;
			}
			break;

		case SYNAPTICS_FW_UPDATE:
			if ((g_synaptics_fw.buffer_ready & 0x03) != 0x03){
				pr_err("Firmware buff is not ready.\n");
				return -EACCES;
			}

			flash_program();
			g_synaptics_fw.buffer_ready = 0;
			break;

		case SYNAPTICS_GET_HW:
			if (copy_to_user(argp, s_synaptics_configID, sizeof(s_synaptics_configID))) {
				return -EFAULT;
			}
			break;

		default:
			pr_err("%s bad ioctl %u\n", __func__, cmd);
			return -EINVAL;
	}

	return 0;
}

static struct file_operations synaptics_fops = {
	.owner = THIS_MODULE,
	.open = synaptics_dev_open,
	.llseek	= no_llseek,
	.write	= synaptics_dev_write,
	.read	= synaptics_dev_read,
	.unlocked_ioctl = synaptics_dev_ioctl,
	.release = synaptics_dev_release,
};

static struct miscdevice synaptics_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "synaptics_dev",
	.fops = &synaptics_fops,
};
static int get_type_index(char type)
{
    int result = 0;
    if (type > 'A')
    {
        result = (type - 'A') + 10;
    }
    else if (type >=  '0' && type <=  '9')
    {
        result = type - '0';
    }
    else
    {
        result = 0;
    }
    return result;
}

static int synaptics_rmi4_probe(
	struct i2c_client *client, const struct i2c_device_id *id)
{
	int i;
	int ret = 0;
      int min_x = 0;
      int min_y = 0;
	struct synaptics_rmi4 *ts;

/*ZTE: modified by tong.weili to delete build warning 20120424 ++*/    
	//struct proc_dir_entry *dir, *refresh;
#ifdef CONFIG_ZTE_PROP_BRIDGE      
        char chFirmwareID[20] = {0};
#endif
/*ZTE: modified by tong.weili to delete build warning 20120424 --*/ 

       #if 0
              struct synaptics_ts_platform_data *pdata = pdata = client->dev.platform_data;
              if (!pdata) {
		dev_err(&client->dev, "platform data is required!\n");
		return -EINVAL;
	}
              if (pdata->init_platform_hw)
		pdata->init_platform_hw();
        #endif

	printk(KERN_ERR "probing for Synaptics RMI4 device %s at $%02X...\n", client->name, client->addr);

        gpio_set_value(TEGRA_GPIO_PK4, 0);
        msleep(10);
        gpio_set_value(TEGRA_GPIO_PK4, 1);

       msleep(250);
      
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_ERR "%s: need I2C_FUNC_I2C\n", __func__);
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}
        	
	ts = kzalloc(sizeof(struct synaptics_rmi4), GFP_KERNEL);
	INIT_WORK(&ts->work, synaptics_rmi4_work_func);
	ts->client = client;
	i2c_set_clientdata(client, ts);
	client->driver = &synaptics_rmi4_driver;
#if (defined(TS_WATCHDOG) || (!defined(CONFIG_HAS_EARLYSUSPEND) ))
	client_synap = ts->client;
#endif

	ts_f34=ts;
	g_synaptics_i2cc = ts->client;

      ret = synaptics_rmi4_read_pdt(ts);
	if (ret <= 0) {
		if (ret == 0)
			printk(KERN_ERR "Empty PDT\n");

		printk(KERN_ERR "Error identifying device (%d)\n", ret);
		ret = -ENODEV;
		goto err_pdt_read_failed;
	}

      /*ZTE: modified by tong.weili for synaptics 兼容多种sensor 20120401 ++*/
      if(s_F01_CTRL_00 != 0)
      {
          synaptics_i2c_write(ts->client, (s_F01_CTRL_00+1), 0x04);       //only enable abs INT 
      }      
      /*ZTE: modified by tong.weili for synaptics 兼容多种sensor 20120401 --*/

	ts->input_dev = input_allocate_device();
	if (!ts->input_dev) {
		printk(KERN_ERR "failed to allocate input device.\n");
		ret = -EBUSY;
		goto err_alloc_dev_failed;
	}

	ts->input_dev->name = "Synaptics_RMI4";
	ts->input_dev->phys = client->name;

      /*ZTE: modified by tong.weili for synaptics 兼容多种sensor 20120401 ++*/
      if(Type_S3000 == s_synaptics_Sensor_type)
      {
          ts->f11_max_x = 1077;
          ts->f11_max_y = 1893;
          synaptics_i2c_write(ts->client, 0x78, 0x0F);  /*ZTE: added by tong.weili to enhance 10% sensitivity 20120315*/
      }
      else if(Type_S2202 == s_synaptics_Sensor_type)
      {
          ts->f11_max_x = USER_DEFINE_MAX_X;
          ts->f11_max_y = MAX_Y_FOR_LCD;
      }
      else
      {
          printk(KERN_ERR "Synaptics:error sensor type!\n");
      }
      /*ZTE: modified by tong.weili for synaptics 兼容多种sensor 20120401 --*/
  
      min_x = 0;
      min_y = 0;
              
	set_bit(EV_ABS, ts->input_dev->evbit);
	set_bit(EV_SYN, ts->input_dev->evbit);
	set_bit(EV_KEY, ts->input_dev->evbit);
	
	/* set dummy key to make driver work with virtual keys */
	input_set_capability(ts->input_dev, EV_KEY, KEY_PROG1);
	
      //shihuiqin added ++
//	set_bit(ABS_MT_TOUCH_MAJOR, ts->input_dev->absbit);
//	set_bit(ABS_MT_POSITION_X, ts->input_dev->absbit);
//	set_bit(ABS_MT_POSITION_Y, ts->input_dev->absbit);
//	set_bit(ABS_MT_WIDTH_MAJOR, ts->input_dev->absbit);
	//shihuiqin added --


/*shihuiqin 20110519 added for touchkey++*/
	//set_bit(KEY_BACK, ts->input_dev->keybit);
	//set_bit(KEY_MENU, ts->input_dev->keybit);
	//set_bit(KEY_HOME, ts->input_dev->keybit);
	//set_bit(KEY_SEARCH, ts->input_dev->keybit);
	set_bit(BTN_TOUCH, ts->input_dev->keybit);
	set_bit(BTN_2, ts->input_dev->keybit);

/*shihuiqin 20110519 added for touchkey--*/
	

	if (ts->hasF11) {

	printk( "%s: Set ranges X=[%d..%d] Y=[%d..%d].", __func__, min_x, ts->f11_max_x, min_y, ts->f11_max_y);
//	input_set_abs_params(ts->input_dev, ABS_X, min_x, ts->f11_max_x,0, 0);
//	input_set_abs_params(ts->input_dev, ABS_Y, min_y, ts->f11_max_y,0, 0);
//	input_set_abs_params(ts->input_dev, ABS_PRESSURE, 0, 255, 0, 0);
//	input_set_abs_params(ts->input_dev, ABS_TOOL_WIDTH, 0, 15, 0, 0);

#ifdef CONFIG_SYNA_MULTI_TOUCH
//	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 15, 0, 0);   /*pressure of single-touch*/
//	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MINOR, 0, 15, 0, 0);
////	input_set_abs_params(ts->input_dev, ABS_MT_ORIENTATION, 0, 1, 0, 0);
//	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 1, ts->f11.points_supported, 0, 0);
//	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);   /*ABS_TOOL_WIDTH of single-touch*/
	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);   /*ABS_TOOL_WIDTH of single-touch*/

	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, min_x, ts->f11_max_x,0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, min_y, ts->f11_max_y,0, 0);
#endif

		if (ts->hasEgrPalmDetect)
			set_bit(BTN_DEAD, ts->input_dev->keybit);
		if (ts->hasEgrFlick) {
			set_bit(REL_X, ts->input_dev->keybit);
			set_bit(REL_Y, ts->input_dev->keybit);
		}
		if (ts->hasEgrSingleTap)
			set_bit(BTN_TOUCH, ts->input_dev->keybit);
		if (ts->hasEgrDoubleTap)
			set_bit(BTN_TOOL_DOUBLETAP, ts->input_dev->keybit);
	}
	if (ts->hasF19) {
		set_bit(BTN_DEAD, ts->input_dev->keybit);
#ifdef CONFIG_SYNA_BUTTONS
		/* F19 does not (currently) report ABS_X but setting maximum X is a convenient way to indicate number of buttons */
		input_set_abs_params(ts->input_dev, ABS_X, 0, ts->f19.points_supported, 0, 0);
		for (i = 0; i < ts->f19.points_supported; ++i) {
			set_bit(BTN_F19 + i, ts->input_dev->keybit);
		}
#endif

#ifdef CONFIG_SYNA_BUTTONS_SCROLL
		set_bit(EV_REL, ts->input_dev->evbit);
		set_bit(SCROLL_ORIENTATION, ts->input_dev->relbit);
#endif
	}
	if (ts->hasF30) {
		for (i = 0; i < ts->f30.points_supported; ++i) {
			set_bit(BTN_F30 + i, ts->input_dev->keybit);
		}
	}

/*ZTE: added by tong.weili for autosleep 20130520 ++*/
#ifndef CONFIG_HAS_EARLYSUSPEND
    ts->input_dev->enable = synaptics_rmi4_enable;
    ts->input_dev->disable = synaptics_rmi4_disable;
#endif
/*ZTE: added by tong.weili for autosleep 20130520 --*/

	/*
	 * Device will be /dev/input/event#
	 * For named device files, use udev
	 */
	ret = input_register_device(ts->input_dev);
	if (ret) {
		printk(KERN_ERR "synaptics_rmi4_probe: Unable to register %s \
				input device\n", ts->input_dev->name);
		goto err_input_register_device_failed;
	} else {
		printk("synaptics input device registered\n");
	}

      client->irq = gpio_to_irq(TEGRA_GPIO_PK2);

	if (client->irq) {
		if (request_irq(client->irq, synaptics_rmi4_irq_handler,
				IRQF_TRIGGER_LOW, client->name, ts) >= 0) {
			ts->use_irq = 1;
		} else {
			printk("Failed to request IRQ!\n");
		}
	}
              
	if (!ts->use_irq) {
		printk(KERN_ERR "Synaptics RMI4 device %s in polling mode\n", client->name);
		hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ts->timer.function = synaptics_rmi4_timer_func;
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
	}
	#ifdef TS_WATCHDOG
		#if 	USE_HRTIMER_4WATCHDOG
			hrtimer_init(&watchdog_total_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
			watchdog_total_timer.function = total_timer_func;
			hrtimer_init(&watchdog_no_report_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
			watchdog_no_report_timer.function = no_report_timer_func;
		#else
			setup_timer(&watchdog_total_timer, total_timer_func, (unsigned long) ts);
			//mod_timer(&mxt->timer, jiffies + TOUCHSCREEN_TIMEOUT);
			setup_timer(&watchdog_no_report_timer, no_report_timer_func, (unsigned long) ts);
			//mod_timer(&mxt->timer, jiffies + TOUCHSCREEN_TIMEOUT);
		#endif
		INIT_WORK(&watchdog_work, synaptics_watchdog_work_func);
	#endif

	ts->enable = 1;

	dev_set_drvdata(&ts->input_dev->dev, ts);
#if 0
       dir = proc_mkdir("touchscreen", NULL);
	refresh = create_proc_entry("ts_information", 0644, dir);
	if (refresh) {
		refresh->data		= NULL;
		refresh->read_proc  = proc_read_val;
		refresh->write_proc = proc_write_val;
	}
#endif
	printk(KERN_INFO "synaptics_ts_probe: Start touchscreen %s in %s mode\n", ts->input_dev->name, ts->use_irq ? "interrupt" : "polling");

	#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = synaptics_rmi4_early_suspend;
	ts->early_suspend.resume = synaptics_rmi4_late_resume;
	register_early_suspend(&ts->early_suspend);
	#endif

#ifdef CONFIG_HAS_VIRTUALKEY
	ts_key_report_synaptics_init();
#endif

 /*ZTE: modified by tong.weili for synaptics 兼容多种sensor 20120401 ++*/
#ifdef CONFIG_ZTE_PROP_BRIDGE      
      if(Type_S3000 == s_synaptics_Sensor_type)
      {
          if(s_F01_QUERY_03 != 0)
          {
              u8 firmwareID = 0;
              synaptics_i2c_read(client, s_F01_QUERY_03, &firmwareID, 1);     
              printk("[Synaptics]: firmwareID = 0x%02x\n", firmwareID);
              sprintf(chFirmwareID, "0x%02x", firmwareID);
          }      
      }
      else if(Type_S2202 == s_synaptics_Sensor_type)
      {                
          printk("[Synaptics]: firmwareID = 0x%02x, 0x%02x\n", s_synaptics_configID[2], s_synaptics_configID[3]);
          sprintf(chFirmwareID, "0x%02x, 0x%02x", s_synaptics_configID[2], s_synaptics_configID[3]);
      }
              
      if(prop_add("Touchscreen", "fw-version", chFirmwareID))
      {
            printk("[tong]: prop_add touchscreen failed!");
      }

      if (s_synaptics_configID[0] != 0) {
            int chip_type_id =  get_type_index(s_synaptics_configID[0]);
            if (chip_type_id >= (sizeof(chip_type)/sizeof(chip_type[0]))) {
                 chip_type_id = 0;
            }
            if (prop_add("Touchscreen", "chip-type", chip_type[chip_type_id])) {
                 printk("[tong]: prop_add touchscreen chip type failed!");
            }
      }

      if (s_synaptics_configID[1] != 0) {
            char sen_partner[30] = {0};
            int chip_sensor_id = get_type_index(s_synaptics_configID[1]);
            if (chip_sensor_id >= (sizeof(sensor_partner)/sizeof(sensor_partner[0]))) {
                 chip_sensor_id = 0;
            }
            snprintf(sen_partner, sizeof(sen_partner), "Synaptics+%s", sensor_partner[chip_sensor_id]);
            if (prop_add("Touchscreen", "sensor-partner", sen_partner)) {
                 printk("[tong]: prop_add touchscreen sensor partner failed!");
            }
      }

      if (client->addr != 0) {
            char i2c_buf[10] = {0};
            snprintf(i2c_buf, sizeof(i2c_buf), "0x%x", client->addr);
            if (prop_add("Touchscreen", "i2c-address", i2c_buf)) {
                 printk("[tong]: prop_add touchscreen sensor partner failed!");
            }
      }
#endif
 /*ZTE: modified by tong.weili for synaptics 兼容多种sensor 20120401 --*/

	ret = misc_register(&synaptics_device);
	if (ret) {
		pr_err("%s : misc_register failed\n", __FILE__);
	}

	return 0;

err_input_register_device_failed:
	input_free_device(ts->input_dev);

err_alloc_dev_failed:
err_pdt_read_failed:
                
              if(ts != NULL)
              {
                        kfree(ts);
              }
err_check_functionality_failed:

	return ret;
}

static int synaptics_rmi4_remove(struct i2c_client *client)
{
       struct synaptics_rmi4 *ts = i2c_get_clientdata(client);
	//unregister_early_suspend(&ts->early_suspend);
	if (ts->use_irq)
		free_irq(client->irq, ts);
	else
		hrtimer_cancel(&ts->timer);
	input_unregister_device(ts->input_dev);
       if(ts != NULL)
       {
                kfree(ts);
        }

       misc_deregister(&synaptics_device);

	return 0;
}

static int synaptics_rmi4_suspend(struct i2c_client *client, pm_message_t mesg)
{
       int ret;
	struct synaptics_rmi4 *ts = i2c_get_clientdata(client);

      printk("[tong]:%s\n", __func__);

      if (0 == ts->enable)
      {
          printk("[tong]:synaptics has been suspended.\n");
          return 0;
      }

	if (ts->use_irq)
       {
		disable_irq(client->irq);
	}
	else
	{
		hrtimer_cancel(&ts->timer);
	}
	ret = cancel_work_sync(&ts->work);
	if (ret && ts->use_irq) /* if work was pending disable-count is now 2 */
	{
	    printk("cancel_work_sync ret=%d",ret);
	    enable_irq(client->irq);
	}

        /*ZTE:modified by tong.weili 修正寄存器地址20111124 ++*/
        synaptics_i2c_write(ts->client, (s_F01_CTRL_00+1), 0);     /* disable interrupt, */    //F01_RMI_CTRL01
        synaptics_i2c_write(client, s_F01_CTRL_00, 0x01);      /* deep sleep */            //F01_RMI_CTRL00
        /*ZTE:modified by tong.weili 修正寄存器地址20111124 --*/

	ts->enable = 0;
#ifdef TS_WATCHDOG
#if USE_HRTIMER_4WATCHDOG
	   hrtimer_cancel(&watchdog_total_timer);
	   hrtimer_cancel(&watchdog_no_report_timer);
#endif
#endif
	return 0;
}

static int synaptics_rmi4_resume(struct i2c_client *client)
{
    struct synaptics_rmi4 *ts = i2c_get_clientdata(client);

      printk("[tong]:%s\n", __func__);

      if (1 == ts->enable)
      {
          printk("[tong]:synaptics has been resumed.\n");
          return 0;
      }

      //write max x
      synaptics_i2c_write(ts->client, (s_F11_CTRL_00+6), USER_DEFINE_MAX_X&0xFF);
      synaptics_i2c_write(ts->client, (s_F11_CTRL_00+7), (USER_DEFINE_MAX_X&0xF00) >> 8);
  
      //write max y
      synaptics_i2c_write(ts->client, (s_F11_CTRL_00+8), USER_DEFINE_MAX_Y&0xFF);
      synaptics_i2c_write(ts->client, (s_F11_CTRL_00+9), (USER_DEFINE_MAX_Y&0xF00) >> 8);
                                
      synaptics_i2c_write(client, s_F01_CTRL_00, 0x0);      /* wakeup */ /*ZTE:modified by tong.weili 修正寄存器地址20111124*/    

	if (ts->use_irq)
	{
	    enable_irq(ts->client->irq);
	    synaptics_i2c_write(ts->client, (s_F01_CTRL_00+1), 4);     /* enable interrupt, */ /*ZTE:modified by tong.weili 修正寄存器地址20111124*/
	}
	else
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);

	ts->enable = 1;
#ifdef TS_WATCHDOG
	ts_watchgod_enable = 1;
	checkcount = 0;
	#if USE_HRTIMER_4WATCHDOG
		hrtimer_start(&watchdog_total_timer, ktime_set(TS_WATCHDOG_DURATION, 0), HRTIMER_MODE_REL);
		//   hrtimer_start(&watchdog_no_report_timer, ktime_set(TS_WATCHDOG_NO_REPORT_TIME, 0), HRTIMER_MODE_REL);
	#else
		mod_timer(&watchdog_total_timer, jiffies + msecs_to_jiffies(TS_WATCHDOG_DURATION*MSEC_PER_SEC));
		//mod_timer(&watchdog_no_report_timer, jiffies + msecs_to_jiffies(TS_WATCHDOG_DURATION*MSEC_PER_SEC));
	#endif
#endif
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void synaptics_rmi4_early_suspend(struct early_suspend *h)
{
	struct synaptics_rmi4 *ts;
	ts = container_of(h, struct synaptics_rmi4, early_suspend);
	synaptics_rmi4_suspend(ts->client, PMSG_SUSPEND);
}

static void synaptics_rmi4_late_resume(struct early_suspend *h)
{
	struct synaptics_rmi4 *ts;
	ts = container_of(h, struct synaptics_rmi4, early_suspend);
	synaptics_rmi4_resume(ts->client);
}

#else
static int synaptics_rmi4_enable(struct input_dev *dev)
{
    return synaptics_rmi4_resume(client_synap);
}
static int synaptics_rmi4_disable(struct input_dev *dev)
{
    return synaptics_rmi4_suspend(client_synap, PMSG_SUSPEND);
}

#endif


static const struct i2c_device_id synaptics_rmi4_id[] = {
	{ "synaptics-rmi-ts", 0 },
	{ }
};

static struct i2c_driver synaptics_rmi4_driver = {
	.probe		= synaptics_rmi4_probe,
	.remove		= synaptics_rmi4_remove,
/*	
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend	= synaptics_rmi4_suspend,
	.resume		= synaptics_rmi4_resume,
#endif
*/
	.id_table	= synaptics_rmi4_id,
	.driver = {
		.name	= "synaptics-rmi-ts",
	},
};
 
static int __devinit synaptics_rmi4_init(void)
{
	//synaptics_wq = create_rt_workqueue("synaptics_wq");//定义在最新workqueue.h中
      synaptics_wq = create_singlethread_workqueue("synaptics_wq");/*ZTE: modified by tong.weili for Touch 20110725*/
	if (!synaptics_wq)
	{
		printk(KERN_ERR "Could not create work queue synaptics_wq: no memory");
		return -ENOMEM;
	}
    
#ifdef TS_WATCHDOG
      tsp_watchdog_wq = create_singlethread_workqueue("synaptics_watchdog_wq");	
      if (!tsp_watchdog_wq)	
      {       
          printk(KERN_ERR "[TSP] Could not create work queue synaptics_watchdog_wq: no memory");      
          return -ENOMEM; 
      }
#endif
	return i2c_add_driver(&synaptics_rmi4_driver);
}

static void __exit synaptics_rmi4_exit(void)
{
	i2c_del_driver(&synaptics_rmi4_driver);
	if (synaptics_wq)
		destroy_workqueue(synaptics_wq);
    
#ifdef TS_WATCHDOG
      if (tsp_watchdog_wq)		
          destroy_workqueue(tsp_watchdog_wq);
#endif
}

module_init(synaptics_rmi4_init);
module_exit(synaptics_rmi4_exit);

MODULE_DESCRIPTION("Synaptics RMI4 Driver");
MODULE_LICENSE("GPL");


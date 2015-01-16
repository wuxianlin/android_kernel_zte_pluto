/*
 * drivers/misc/Sensors-check.c
 * check whether the sensors are successfully mounted.
 *
 * Copyright (C) 2011 ZTE.
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
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/ioctl.h>

enum sensors_type {
    SENSORS_TYPE_ACCEL,	
    SENSORS_TYPE_COMPASS,
    SENSORS_TYPE_GYRO,
    SENSORS_TYPE_LIGHT,
};

/* ZTE:add SENSORS_VENDOR_KIONIX by liyongzhuang for accel kxtf9 ++*/
enum sensors_vendor {
    SENSORS_VENDOR_ST,
    SENSORS_VENDOR_KIONIX,
    SENSORS_VENDOR_AKM,
    SENSORS_VENDOR_FREESCALE,
    SENSORS_VENDOR_INVENSENSE,
    SENSORS_VENDOR_TAOS,
    SENSORS_VENDOR_RHOM,    
};
/* ZTE:add SENSORS_VENDOR_KIONIX by liyongzhuang for accel kxtf9 --*/

struct sensors_item_info {
	int type;	/* consumer */
	int vendor;	/* consumer */
	int addr;	/* consumer */
	int id_exist;	/* consumer */
	int id_reg;	/* consumer */
	int id_val;	/* consumer */
	int i2c_nr;	/* consumer */
};

#define ID_EXIST      1
#define ID_NOEXIST  0

#define SENSORS_INFO(_addr, _id_exist, _id_reg, _id_val, _i2c_nr, _type, _vendor) \
{								\
	.type              = _type,	 \
	.vendor          = _vendor, \
	.addr              = _addr, \
	.id_exist         = _id_exist, \
	.id_reg           = _id_reg, \
	.id_val           = _id_val,	 \
	.i2c_nr           = _i2c_nr, \
}

#define SENSORS_CHECK_GET_ACCEL     _IOR('S', 0x00, int)
#define SENSORS_CHECK_GET_COMPASS       _IOR('S', 0x01, int)
#define SENSORS_CHECK_GET_GYRO      _IOR('S', 0x02, int)
#define SENSORS_CHECK_GET_LIGHT     _IOR('S', 0x03, int)

/* ZTE:modify by liyongzhuang for accel kxtf9 ++*/
static struct sensors_item_info sensors_info_list[] = {
	SENSORS_INFO(0x0F, ID_EXIST, 0x0F, 0x05, 0, SENSORS_TYPE_ACCEL, SENSORS_VENDOR_KIONIX),
	SENSORS_INFO(0x18, ID_EXIST, 0x0F, 0x33, 0, SENSORS_TYPE_ACCEL, SENSORS_VENDOR_ST),
	SENSORS_INFO(0x19, ID_EXIST, 0x0F, 0x33, 0, SENSORS_TYPE_ACCEL, SENSORS_VENDOR_ST),
	SENSORS_INFO(0x0C, ID_EXIST, 0x00, 0x48, 0, SENSORS_TYPE_COMPASS, SENSORS_VENDOR_AKM),
	SENSORS_INFO(0x69, ID_NOEXIST, 0x00, 0x00, 0, SENSORS_TYPE_GYRO, SENSORS_VENDOR_ST),
	SENSORS_INFO(0x6A, ID_EXIST, 0x0F, 0xD4, 0, SENSORS_TYPE_GYRO, SENSORS_VENDOR_ST),
	SENSORS_INFO(0x39, ID_EXIST, 0x92, 0x29, 0, SENSORS_TYPE_LIGHT, SENSORS_VENDOR_TAOS),
};
 /* ZTE:modify by liyongzhuang for accel kxtf9 --*/

static int sensor_check(struct sensors_item_info * sensor_item)
{
        int ret;
        char msgbuf;
        struct i2c_adapter *sensor_adapter;
        
        struct i2c_msg msgs[] = {
            {
                .addr = sensor_item->addr,
                .flags = 0,
                .len = 1,
                .buf = &msgbuf,
            },
            {
                .addr = sensor_item->addr,
                .flags = I2C_M_RD,
                .len = 1,
                .buf = &msgbuf,
            },
        };

        msgbuf = sensor_item->id_reg;
        sensor_adapter = i2c_get_adapter(sensor_item->i2c_nr);
        ret = i2c_transfer(sensor_adapter, msgs, 2);
        if(ret < 2){
            printk("%s:[i2c-dev 0x%02x]i2c_transfer erro(%d)\n", __func__, sensor_item->addr, ret);
            return -1;
        }

        if((ID_EXIST == sensor_item->id_exist) && (msgbuf != sensor_item->id_val)){
            printk("%s:[i2c-dev 0x%02x]device id is not match(0x%02x<->0x%02x)\n", __func__, \
                sensor_item->addr, msgbuf, sensor_item->id_val);
            return -1;
        }        

        return 0;
}

static int sensors_check_open(struct inode *inode, struct file *file)
{
        return 0;
}

static int sensors_check_release(struct inode *inode, struct file *file)
{	
        return 0;
}

static long sensors_check_ioctl(struct file *file,
		      unsigned int cmd, unsigned long arg)
{
        int sensors_num;
        int i;
        int ret;
        int sensor_sate=0;

        sensors_num = ARRAY_SIZE(sensors_info_list);
        
        switch (cmd) {
            case SENSORS_CHECK_GET_ACCEL:
                for(i = 0; i < sensors_num; i++){
                    if(SENSORS_TYPE_ACCEL == sensors_info_list[i].type){
                        ret = sensor_check(&sensors_info_list[i]);
                        if(!ret){
                            sensor_sate = 1;                            
                        }
                    }
                }
                break;
            case SENSORS_CHECK_GET_COMPASS:
                for(i = 0; i < sensors_num; i++){
                    if(SENSORS_TYPE_COMPASS== sensors_info_list[i].type){
                        ret = sensor_check(&sensors_info_list[i]);
                        if(!ret){
                            sensor_sate = 1;
                        }
                    }
                }
                break;
            case SENSORS_CHECK_GET_GYRO:
                for(i = 0; i < sensors_num; i++){
                    if(SENSORS_TYPE_GYRO== sensors_info_list[i].type){
                        ret = sensor_check(&sensors_info_list[i]);
                        if(!ret){
                            sensor_sate = 1;
                        }
                    }
                }
                break;
            case SENSORS_CHECK_GET_LIGHT:
                for(i = 0; i < sensors_num; i++){
                    if(SENSORS_TYPE_LIGHT== sensors_info_list[i].type){
                        ret = sensor_check(&sensors_info_list[i]);
                        if(!ret){
                            sensor_sate = 1;
                        }
                    }
                }
                break;            
            default:
                printk("%s:unkown command\n", __func__);
        }

        if(copy_to_user((unsigned char __user *) arg, &sensor_sate, sizeof(int))){
            printk("%s:copy_to_user error\n", __func__);
        }
        
        return 0;
}


static struct file_operations sensors_check_fops = {
	.owner = THIS_MODULE,
	.open = sensors_check_open,
	.release = sensors_check_release,
	.unlocked_ioctl = sensors_check_ioctl,
};

static struct miscdevice sensors_check_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "sensors_check",
	.fops = &sensors_check_fops,
};

static int __init sensors_check_init(void)
{
        if (misc_register(&sensors_check_device)) {
            printk(KERN_ERR "%s: sensors_check_device register failed!\n", __func__);
        }
        return 0;
}

static void __exit sensors_check_exit(void)
{
        misc_deregister(&sensors_check_device);
}

module_init(sensors_check_init);
module_exit(sensors_check_exit);

MODULE_AUTHOR("Lu poyuan <lu.poyuan@zte.com.cn>");
MODULE_DESCRIPTION("sensors prod_test driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.1");


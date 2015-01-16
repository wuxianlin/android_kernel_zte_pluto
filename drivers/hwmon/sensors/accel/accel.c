/*
 * drivers/misc/accel/accel.c
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
#include <linux/delay.h>
#include <linux/suspend.h>
#include <linux/module.h>

#define ACCEL_DEFUALT_ADDR 0x0F //kxtf9

#define G_MAX 16000
#define FUZZ 32
#define FLAT 32

#define DEBUG 1

struct accel_data{
    unsigned char i2c_address;
    struct i2c_adapter *adapter;
    struct i2c_client *i2c_client;
    struct platform_device *pdev;
    struct accel_platform_data *pdata;
    struct accel_driver_descr *driver_descr;
    struct input_polled_dev *input_poll_dev;

    struct mutex lock;
    atomic_t enabled;
    unsigned int poll_inerval;	
    unsigned int g_range;
#ifdef DEBUG
    u8 reg_addr;
#endif
    int shutdown_complete;
};

atomic_t suspend_enabled;
struct accel_data *g_accel_driver_data = NULL;

#define RW_RETRIES 5

int accel_i2c_write(unsigned int len, unsigned char *data)
{
	struct i2c_msg msg;
	int res;
	int loop;

	if (NULL == data)
		return -EINVAL;
      
	 if(g_accel_driver_data && g_accel_driver_data->shutdown_complete){
         return -ENODEV;
     }
	msg.addr = g_accel_driver_data->i2c_address;
	msg.flags = 0;	/* write */
	msg.buf = data;
	msg.len = len;

	for(loop = 0; loop< RW_RETRIES; loop++){
		res = i2c_transfer(g_accel_driver_data->adapter, &msg, 1);
		if(res == 1)
			break;
		mdelay(10);
	}

	if (res < 1)
		return -1;
	else
		return 0;
}

int accel_i2c_read(unsigned char reg,
		    unsigned int len, unsigned char *data)
{
	struct i2c_msg msgs[2];
	int res;
	int loop;

	if (NULL == data)
		return -EINVAL;
    
	 if(g_accel_driver_data && g_accel_driver_data->shutdown_complete){
         return -ENODEV;
     }
	msgs[0].addr = g_accel_driver_data->i2c_address;
	msgs[0].flags = 0;	/* write */
	msgs[0].buf = &reg;
	msgs[0].len = 1;

	msgs[1].addr = g_accel_driver_data->i2c_address;
	msgs[1].flags = I2C_M_RD; /* read */
	msgs[1].buf = data;
	msgs[1].len = len;

	for(loop = 0; loop< RW_RETRIES; loop++){
		res = i2c_transfer(g_accel_driver_data->adapter, msgs, 2);
		if(res == 2)
			break;
		mdelay(10);
	}

	if (res < 2)
		return -1;
	else
		return 0;
}

int accel_i2c_write_register(unsigned char reg, unsigned char value)
{
    unsigned char data[2];

    data[0] = reg;
    data[1] = value;        
    return accel_i2c_write(2, data);
}

int accel_i2c_read_register(unsigned char reg, unsigned char *value)
{
	return accel_i2c_read(reg, 1, value);
}

static int accel_enable(void)
{
    int err;

    if (!atomic_cmpxchg(&g_accel_driver_data->enabled, 0, 1)) {
        
        mutex_lock(&g_accel_driver_data->lock);
        err = g_accel_driver_data->driver_descr->enable();
        if (err < 0) {
            atomic_set(&g_accel_driver_data->enabled, 0);
            mutex_unlock(&g_accel_driver_data->lock);
            return err;
        }
        mutex_unlock(&g_accel_driver_data->lock);
        
        //start the poll work
        input_polldev_resume(g_accel_driver_data->input_poll_dev);
    }

    return 0;
}

static int accel_disable(void)
{
    if (atomic_cmpxchg(&g_accel_driver_data->enabled, 1, 0)) {
        //stop the poll work
        input_polldev_suspend(g_accel_driver_data->input_poll_dev);

        mutex_lock(&g_accel_driver_data->lock);
        g_accel_driver_data->driver_descr->disable();
        mutex_unlock(&g_accel_driver_data->lock);
    }

    return 0;
}

static ssize_t attr_get_polling_rate(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
    unsigned int val;
    
    mutex_lock(&g_accel_driver_data->lock);	
    val = g_accel_driver_data->poll_inerval;
    mutex_unlock(&g_accel_driver_data->lock);

    return sprintf(buf, "poll interval: %dms\n", val);
}

static ssize_t attr_set_polling_rate(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
    unsigned long interval_ms;
    
    if (strict_strtoul(buf, 10, &interval_ms))
    	return -EINVAL;
    if (!interval_ms)
    	return -EINVAL;

    mutex_lock(&g_accel_driver_data->lock);
    
    g_accel_driver_data->poll_inerval = (unsigned int)interval_ms;
        
    //update the device register
    g_accel_driver_data->driver_descr->update_odr((unsigned int)interval_ms);
    
    mutex_unlock(&g_accel_driver_data->lock);

    //updtate the polled device
    input_polldev_update_poll(g_accel_driver_data->input_poll_dev, (unsigned int)interval_ms);    

    return size;
}

static ssize_t attr_get_range(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
    int range = 0;

    mutex_lock(&g_accel_driver_data->lock);
    switch (g_accel_driver_data->g_range) {
        case ACCEL_FS_2G:
            range = 2;
            break;
        case ACCEL_FS_4G:
            range = 4;
            break;
        case ACCEL_FS_8G:
            range = 8;
            break;
        case ACCEL_FS_16G:
            range = 16;
            break;
        default:
            range = 2;
    }
    mutex_unlock(&g_accel_driver_data->lock);

    return sprintf(buf, "full scale: %dg \n", range);
}

static ssize_t attr_set_range(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
    unsigned long val;
    unsigned int range;

    if (strict_strtoul(buf, 10, &val))
        return -EINVAL;

    switch (val) {
        case 2://+/-2G
            range = ACCEL_FS_2G;
            break;
        case 4://+/-4G
            range = ACCEL_FS_4G;
            break;
        case 8://+/-8G
            range = ACCEL_FS_8G;
            break;
        default:
            val = 2;
            range = ACCEL_FS_2G;
    }

    mutex_lock(&g_accel_driver_data->lock);        
    g_accel_driver_data->g_range = range;
    g_accel_driver_data->driver_descr->update_fs(g_accel_driver_data->g_range);
    mutex_unlock(&g_accel_driver_data->lock);
       
    return size;
}

static ssize_t attr_get_enable(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	int val = atomic_read(&g_accel_driver_data->enabled);
	return sprintf(buf, "%s is %s\n", g_accel_driver_data->driver_descr->name, val ? "enabled" : "disabled");
}

static ssize_t attr_set_enable(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	
	printk("%s:val(%ld) for %s\n", __func__, val, g_accel_driver_data->driver_descr->name);
    
	if (val){
		atomic_set(&suspend_enabled, 1);
		accel_enable();
	}else{
		atomic_set(&suspend_enabled, 0);
		accel_disable();
	}

	return size;
}

static ssize_t attr_get_axis(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s axis convert matrix is %ld\n", g_accel_driver_data->driver_descr->name, \
		*(g_accel_driver_data->driver_descr->axis_map));
}

static ssize_t attr_set_axis(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	*(g_accel_driver_data->driver_descr->axis_map) = val;

	return size;
}

#ifdef DEBUG
/* PAY ATTENTION: These DEBUG funtions don't manage resume_state */
static ssize_t attr_reg_set(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
    int rc;
    u8 reg;
    unsigned long val;

    if (strict_strtoul(buf, 16, &val))
    	return -EINVAL;

    mutex_lock(&g_accel_driver_data->lock);
    reg = g_accel_driver_data->reg_addr;
    mutex_unlock(&g_accel_driver_data->lock);

    rc = accel_i2c_write_register(reg, val);
    if(rc<0){
        printk("accel:register write failed\n");
    }
       
    return size;
}

static ssize_t attr_reg_get(struct device *dev, struct device_attribute *attr,
				char *buf)
{
    int rc;
    u8 reg;
    u8 val;

    mutex_lock(&g_accel_driver_data->lock);
    reg = g_accel_driver_data->reg_addr;
    mutex_unlock(&g_accel_driver_data->lock);

    rc = accel_i2c_read_register(reg, &val);
    if(rc<0){
        printk("accel:register read failed\n");
    }
    
    return sprintf(buf, "addr=0x%02x, val=0x%02x\n", reg, val);
}

static ssize_t attr_addr_set(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long val;
	if (strict_strtoul(buf, 16, &val))
		return -EINVAL;
    
	mutex_lock(&g_accel_driver_data->lock);
	g_accel_driver_data->reg_addr = (u8)val;
	mutex_unlock(&g_accel_driver_data->lock);
    
	return size;
}
#endif

static struct device_attribute attributes[] = {
	__ATTR(pollrate_ms, 0600, attr_get_polling_rate, attr_set_polling_rate),
	__ATTR(range, 0600, attr_get_range, attr_set_range),
	__ATTR(enable, 0600, attr_get_enable, attr_set_enable),
	__ATTR(axis, 0600, attr_get_axis, attr_set_axis),
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
	for ( ; i >= 0; i--)
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


static void accel_input_poll_func(struct input_polled_dev *dev)
{
    int data_out[3];
    int err;
    
    if(atomic_read(&g_accel_driver_data->enabled)){
        mutex_lock(&g_accel_driver_data->lock);
        err = g_accel_driver_data->driver_descr->get_axis_data(data_out);
        if (err < 0)
            printk("Read accel data failed\n");
        else{
            input_report_abs(dev->input, ABS_X, data_out[0]);
            input_report_abs(dev->input, ABS_Y, data_out[1]);
            input_report_abs(dev->input, ABS_Z, data_out[2]);
            input_sync(dev->input);
        }
        
        mutex_unlock(&g_accel_driver_data->lock);
    }    
}

static int accel_input_init(struct accel_data *pdriverdata)
{
	int err = -1;
	struct input_dev *input;


	pdriverdata->input_poll_dev = input_allocate_polled_device();
	if (!pdriverdata->input_poll_dev) {
		err = -ENOMEM;
		dev_err(&pdriverdata->pdev->dev,
			"input device allocate failed\n");
		goto err0;
	}

	pdriverdata->input_poll_dev->private = pdriverdata;
	pdriverdata->input_poll_dev->poll = accel_input_poll_func;
//	pdriverdata->input_poll_dev->open = NULL;
//	pdriverdata->input_poll_dev->close = NULL;
	pdriverdata->input_poll_dev->poll_interval = pdriverdata->pdata->poll_inerval;	
	pdriverdata->input_poll_dev->poll_interval_min = 0;
	pdriverdata->input_poll_dev->poll_interval_max = 1000;

	input = pdriverdata->input_poll_dev->input;

	input->id.bustype = BUS_I2C;
	input->dev.parent = &pdriverdata->i2c_client->dev;

	set_bit(EV_ABS, input->evbit);

	input_set_abs_params(input, ABS_X, -G_MAX, G_MAX, FUZZ, FLAT);
	input_set_abs_params(input, ABS_Y, -G_MAX, G_MAX, FUZZ, FLAT);
	input_set_abs_params(input, ABS_Z, -G_MAX, G_MAX, FUZZ, FLAT);

	input->name = "Accelerometer";

	err = input_register_polled_device(pdriverdata->input_poll_dev);
	if (err) {
		dev_err(&pdriverdata->pdev->dev,
			"unable to register input polled device %s\n",
			pdriverdata->input_poll_dev->input->name);
		goto err1;
	}

	return 0;

err1:
	input_free_polled_device(pdriverdata->input_poll_dev);
err0:
	return err;
}

static void accel_input_cleanup(struct accel_data *pdriverdata)
{
	input_unregister_polled_device(pdriverdata->input_poll_dev);
	input_free_polled_device(pdriverdata->input_poll_dev);
}

static int accel_pm_notify(struct notifier_block *nb,
			unsigned long event, void *nouse)
{
	switch (event) {
		case PM_SUSPEND_PREPARE:
			if(atomic_read(&suspend_enabled))
				accel_disable();
			break;
		case PM_POST_SUSPEND:
			if(atomic_read(&suspend_enabled))
				accel_enable();
			break;
	}

	return NOTIFY_OK;
}

static struct notifier_block accel_i2c_notify = {
	.notifier_call = accel_pm_notify,
};

int accel_i2c_probe(struct i2c_client *clientp, const struct i2c_device_id *dev_id){
    int ret;
    
    g_accel_driver_data->i2c_client = clientp;
    
    /* As default, do not report information */
    atomic_set(&g_accel_driver_data->enabled, 0);

    /*init the device parameter*/
    mutex_lock(&g_accel_driver_data->lock);   
    //init the full scale
    g_accel_driver_data->driver_descr->update_fs(g_accel_driver_data->g_range);

    //init the odr
    g_accel_driver_data->driver_descr->update_odr(g_accel_driver_data->poll_inerval);

    //init the counts per gravity (fix the 1024 counts/g)
    g_accel_driver_data->driver_descr->update_countsperg(1024);
    
    mutex_unlock(&g_accel_driver_data->lock);    
 
    ret= accel_input_init(g_accel_driver_data);
    if (ret< 0) {
    	dev_err(&clientp->dev, "input init failed\n");
    	goto failed_1;
    }

    ret = create_sysfs_interfaces(&clientp->dev);
    if (ret < 0) {
    	dev_err(&clientp->dev,
    		"%s device register failed\n", clientp->name);
    	goto failed_2;
    }

    register_pm_notifier(&accel_i2c_notify);

    dev_info(&clientp->dev, "driver probe successfull!!!\n");
    return 0;

failed_1:
    accel_input_cleanup(g_accel_driver_data);
failed_2:
    return ret;
    
}

int accel_i2c_remove(struct i2c_client *clientp){
    
    accel_input_cleanup(g_accel_driver_data);
    
    remove_sysfs_interfaces(&clientp->dev);
    
    return 0;
}

static int accel_i2c_suspend(struct device *dev)
{
	return 0;
}

static int accel_i2c_resume(struct device *dev)
{
	return 0;
}

static void accel_i2c_shutdown(struct i2c_client *clientp)
{
	mutex_lock(&g_accel_driver_data->lock);
	g_accel_driver_data->shutdown_complete =1;
	mutex_unlock(&g_accel_driver_data->lock);
}

static const struct dev_pm_ops accel_i2c_pm_ops = {
	.suspend = accel_i2c_suspend,
	.resume = accel_i2c_resume,
};

static const struct i2c_device_id accel_i2c_id[] = {
	{ "auto_accel", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, accel_i2c_id);

static struct i2c_driver accel_i2c_driver = {
	.driver = {
		.name	= "auto_accel",
		.owner	= THIS_MODULE,
		.pm = &accel_i2c_pm_ops,
	},
	.probe	= accel_i2c_probe,
	.remove	= __devexit_p(accel_i2c_remove),
	.shutdown = accel_i2c_shutdown,
	.id_table = accel_i2c_id,
};

static struct accel_driver_descr * get_accel_driver(unsigned short i2c_addr)
{
    switch(i2c_addr){
#ifdef CONFIG_ACCEL_SENSORS_LIS3DH
        case 0x18:
        case 0x19:
            return lis3dh_get_driver_descr();
#endif
#ifdef CONFIG_ACCEL_SENSORS_LSM330D_A
        case 0x18:
        case 0x19:
            return lsm330da_get_driver_descr();
#endif
#ifdef CONFIG_ACCEL_SENSORS_MMA845X
        case 0x1C:
        case 0x1D:
            return mma845x_get_driver_descr();
#endif
#ifdef CONFIG_ACCEL_SENSORS_KXTF9
        case 0x0F:
            return kxtf9_get_driver_descr();
#endif            
        default:
            printk("Not support the address[0x%02x] chip\n", i2c_addr);
            return NULL;
    }
}

static int accel_i2c_probe_func(struct i2c_adapter *adap, unsigned short addr){
        int ret;
        
        struct accel_driver_descr * driver_descr;
        char msgbuf;
        struct i2c_msg msgs[] = {
            {
                .addr = addr,
                .flags = 0,
                .len = 1,
                .buf = &msgbuf,
            },
            {
                .addr = addr,
                .flags = I2C_M_RD,
                .len = 1,
                .buf = &msgbuf,
            },
        };

        driver_descr = get_accel_driver(addr);
        if(NULL == driver_descr)
            return false;
        
        msgbuf = driver_descr->id_reg;
            
        ret = i2c_transfer(adap, msgs, 2);
        if(ret < 2){
            printk("%s:[i2c-dev 0x%02x]i2c_transfer erro(%d)\n", __func__, addr, ret);
            return false;
        }

        if((ID_CHECK == driver_descr->check_type) && (msgbuf != driver_descr->dev_id)){
            return false;
        }

        return true;
}

struct i2c_board_info accel_board_info = {
    I2C_BOARD_INFO("auto_accel", ACCEL_DEFUALT_ADDR),
};

/*
unsigned short const addr_list[] = {
    TAOS_DEVICE_ADDR2, 
    I2C_CLIENT_END
};
*/

static int __devinit accel_probe(struct platform_device *pdev)
{
    int ret;
    struct i2c_adapter *accel_adapter;
    struct i2c_client *accel_clientp;
    struct accel_platform_data *pdata;
     
    /* Check platform data*/    
    if (pdev->dev.platform_data == NULL) {
        dev_err(&pdev->dev, "platform data is NULL. exiting.\n");
        ret = -EINVAL;
        goto failed;
    }
    
    pdata = pdev->dev.platform_data;
    
    accel_adapter = i2c_get_adapter(pdata->adapt_nr);
   
    /*check whether the i2c adaper support the devices*/
    if (!i2c_check_functionality(accel_adapter, I2C_FUNC_I2C)) {
        dev_err(&pdev->dev, "adapter not  capable for I2C_FUNC_I2C\n");
        ret = -EIO;
        goto failed;
    }
    
    /*check which accel is mounted on the board*/
    accel_clientp = i2c_new_probed_device(accel_adapter, &accel_board_info, pdata->accel_list, accel_i2c_probe_func);

    if(NULL==accel_clientp){
        dev_err(&pdev->dev, "Not find the accel chip\n");
        ret = -ENXIO;
        goto failed;
    }
    
    /* Allocate memory for driver private data */
    g_accel_driver_data = kzalloc(sizeof(struct accel_data), GFP_KERNEL);
    if (g_accel_driver_data == NULL) {
        ret = -ENOMEM;
        dev_err(&pdev->dev,
                        "failed to allocate memory for module data: "
                            "%d\n", ret);
        goto failed;
    }
       
    accel_board_info.platform_data = pdata;
    g_accel_driver_data->i2c_address = accel_board_info.addr;
    g_accel_driver_data->adapter = accel_adapter;
    g_accel_driver_data->pdev = pdev;
    g_accel_driver_data->pdata = pdata;
    g_accel_driver_data->poll_inerval = pdata->poll_inerval;
    g_accel_driver_data->g_range = pdata->g_range;
    g_accel_driver_data->driver_descr = get_accel_driver(accel_board_info.addr);

    mutex_init(&g_accel_driver_data->lock);
    g_accel_driver_data->shutdown_complete =0;
    dev_info(&pdev->dev, "find the accel chip : (0x%02x)%s\n", 
                            accel_clientp->addr,  g_accel_driver_data->driver_descr->name);
    
    if ((ret = (i2c_add_driver(&accel_i2c_driver))) < 0) {
        dev_err(&pdev->dev, "Accel driver register failed\n");
        ret = -ENXIO;
        goto mem_failed;
    }
    
    return 0;
mem_failed:
    mutex_destroy(&g_accel_driver_data->lock);
    kfree(g_accel_driver_data);
failed:
	return ret;
}

static int accel_remove(struct platform_device *pdev)
{
    i2c_del_driver(&accel_i2c_driver);
    mutex_destroy(&g_accel_driver_data->lock);
    kfree(g_accel_driver_data);
    return 0;
}

static struct platform_driver accel_platform_driver = {
	.probe = accel_probe,
	.remove = accel_remove,
	.driver = {
		.name = "accel_platform",
		.owner = THIS_MODULE,
	},
};

static int __init accel_init_driver(void)
{
	return platform_driver_register(&accel_platform_driver);
}

static void __exit accel_exit_driver(void)
{
	platform_driver_unregister(&accel_platform_driver);
}


late_initcall(accel_init_driver);
module_exit(accel_exit_driver);

MODULE_AUTHOR("ZTE Corporation");
MODULE_DESCRIPTION("auto adaptive accelemeter driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("ACCEL");	

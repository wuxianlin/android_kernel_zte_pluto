/*****************************************************************************
 * Copyright(C) 2011, ZTE Corporation
 *
 *  Module Name	: 
 *  File Name		:  modem_control.c
 *  Description : 
 *  Author      		:  lixingyuan	
 *  Version     : 
 *  Data        		:  2011-07-22
 *  Others      : 
 *  Revision Details1£º
 *     Modify Data£º
 *     Version£º
 *     Author£º
 *     Modification£º
 *  Revision Details2£º
 *****************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
/*ZTE:added by zhanming for modem wakeup ap  20111104 ++*/
#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/sched.h>

#include <linux/gpio.h>

#include "board.h"
#include "clock.h"
#include "board-pluto.h"
#include "devices.h"
#include "gpio-names.h"
#include "fuse.h"

static int usb_switch_state = 0;
static int wakeup_irq;

#if 0
static int modem_reset_enable(int enable)
{
    printk(KERN_INFO "modem_power_enable %d\n", enable);
    if(enable)
        gpio_set_value(P903u_modem_reset_en, 1);
    else
        gpio_set_value(P903u_modem_reset_en, 0);

    return 0;
}
#endif

static int modem_power_enable(int enable)
{
    printk(KERN_INFO "modem_power_enable %d\n", enable);
    if(enable)
        gpio_set_value(modem_power_enb, 1);
    else
        gpio_set_value(modem_power_enb, 0);

    return 0;
}

#if 0
static int modem_power_key_set(int enable)
{
    printk(KERN_INFO "modem_power_key %d\n", enable);
    if(enable)
        gpio_set_value(modem_power_key, 1);
    else
        gpio_set_value(modem_power_key, 0);

    return 0;
}

static int modem_download_enable(int enable)
{
    printk(KERN_INFO "modem_download_enable %d\n", enable);
    if(enable)
        gpio_set_value(p903u_modem_download_en, 1);
    else
        gpio_set_value(p903u_modem_download_en, 0);

    return 0;
}
#endif

#define SWITCH_TO_AP          0
#define SWITCH_TO_MDM       1
/*bjk del for P040T30 GPIO change++*/
//#define SWITCH_TO_MHL       2
/*bjk del for P040T30 GPIO change--*/

/* +++ZTE: modified by liliguo for removing "static" 20130118+++ */
int modem_usb_enable(int enable)
{      
    printk(KERN_INFO "modem_usb_enable 0x%x\n", enable);
    gpio_set_value(usb_port_ctrl0, enable & 0x01);
    
    /*bjk del for P040T30 GPIO change++*/
    //gpio_set_value(usb_port_ctrl1, (enable >> 1) & 0x01);
    /*bjk del for P040T30 GPIO change--*/
    return 0;
}
/* ---ZTE: modified by liliguo for removing "static" 20130118--- */

static int modem_usb3v3_enable(int enable)
{
    printk(KERN_INFO "modem_usb3v3_enable %d\n", enable);
    if(enable)
        gpio_set_value(modem_usb3v3_enb, 1);
    else
        gpio_set_value(modem_usb3v3_enb, 0);
    
    return 0;
}

extern void tegra_udc_connect(void);
extern void tegra_udc_disconnect(void);

static ssize_t modem_usb_switch_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t status;
	long value;
       char *usb_ap[2] = { "USB_SWITCH=AP", NULL };
       char *usb_modem[2] = { "USB_SWITCH=MODEM", NULL };    
       char *usb_mhl[2] = { "USB_SWITCH=MHL", NULL };    
        
	status = strict_strtol(buf, 0, &value);
	if (status == 0) {
        status = 1;
        if (value == 0) {
            printk(KERN_NOTICE "switch usb to ap\n");
            kobject_uevent_env( &dev->kobj, KOBJ_CHANGE, usb_ap);
            modem_usb_enable(SWITCH_TO_AP);
            msleep(200);
            tegra_udc_connect();
        } else if (value == 1) {
            printk(KERN_NOTICE "switch usb to modem\n");
            kobject_uevent_env( &dev->kobj, KOBJ_CHANGE, usb_modem);
            tegra_udc_disconnect();
            msleep(200);
            modem_usb_enable(SWITCH_TO_MDM);
        } 
        /*bjk del for P040T30 GPIO change++*/
        /*else if (value == 2) {
            printk(KERN_NOTICE "switch usb to mhl\n");
            kobject_uevent_env( &dev->kobj, KOBJ_CHANGE, usb_mhl);
            modem_usb_enable(SWITCH_TO_MHL);
        }*/ 
        /*bjk del for P040T30 GPIO change--*/
        else {
            status = -EINVAL;
        }
	}

    return status;
}
static const DEVICE_ATTR(usb_switch, 0600, NULL, modem_usb_switch_store);

static ssize_t modem_usb_ldo_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t status;
	long value;

	status = strict_strtol(buf, 0, &value);
	if (status == 0) {
        status = 1;
        if (value == 0) {
            printk(KERN_NOTICE "disable usb3v3 ldo\n");
            modem_usb3v3_enable(0);
        } else if (value == 1) {
            printk(KERN_NOTICE "enable usb3v3 ldo\n");
            modem_usb3v3_enable(1);
        } else {
            status = -EINVAL;
        }
	}

    return status;
}
static const DEVICE_ATTR(usb_ldo, 0600, NULL, modem_usb_ldo_store);

static ssize_t modem_power_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t status;
	long value;

	status = strict_strtol(buf, 0, &value);
	if (status == 0) {
        status = 1;
        if (value == 0) {
            printk(KERN_NOTICE "poweroff modem\n");
            modem_power_enable(0);
        } else if (value == 1) {
            printk(KERN_NOTICE "poweron modem\n");
            modem_power_enable(1);
        } else {
            status = -EINVAL;
        }
	}

    return status;
}
static const DEVICE_ATTR(poweron, 0600, NULL, modem_power_store);

static ssize_t modem_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t status;
	long value;
    char *modem_reset[2] = { "MODEM_STATE=RESET", NULL };

	status = strict_strtol(buf, 0, &value);
	if (status == 0) {
        status = 1;
        if (value == 1) {
            kobject_uevent_env( &dev->kobj, KOBJ_CHANGE, modem_reset);
            
            printk(KERN_NOTICE "reset modem\n");
            modem_power_enable(0);
            modem_usb3v3_enable(0);
            msleep(1000);
            modem_usb3v3_enable(1);
            modem_power_enable(1);
        } else {
            status = -EINVAL;
        }
	}

    return status;
}
static const DEVICE_ATTR(reset, 0600, NULL, modem_reset_store);

/*ZTE:added by zhanming for modem wakeup ap  20111104 ++*/
static ssize_t modem_apwakeup_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
    int value;
    
    value = gpio_get_value(MODEM_TO_AP_WKUP);
    if (value < 0)
        return value;

    if (value)
        strcpy(buf, "1\n");
    else
        strcpy(buf, "0\n");
    
    return 2;
}
static const DEVICE_ATTR(apwakeup, 0600, modem_apwakeup_show, NULL);

static ssize_t modem_apsleep_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t status;
	long value;

	status = strict_strtol(buf, 0, &value);
	if (status == 0) {
        status = 1;
        if (value == 1) {
            gpio_set_value(AP_TO_MODEM_SLP, 1);
        } else if (value == 0) {
            gpio_set_value(AP_TO_MODEM_SLP, 0);
        } else {
            status = -EINVAL;
        }
	}

    return status;
}
static const DEVICE_ATTR(apsleep, 0600, NULL, modem_apsleep_store);
/*ZTE:added by zhanming for modem wakeup ap  20111104  --*/


static ssize_t modem_download_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t status;
	long value;
        char *modem_download[2] = { "MODEM_STATE=DOWNLOAD", NULL };

	status = strict_strtol(buf, 0, &value);
	if (status == 0) {
        status = 1;
        if (value == 1) {
            kobject_uevent_env( &dev->kobj, KOBJ_CHANGE, modem_download);
            disable_irq(wakeup_irq);
            printk(KERN_NOTICE "download modem\n");
            schedule_timeout_uninterruptible(msecs_to_jiffies(1000));
            modem_usb3v3_enable(0);
            modem_power_enable(0);
            tegra_udc_disconnect();
            msleep(200);
            modem_usb_enable(SWITCH_TO_MDM);            
            mdelay(10000);           
            modem_power_enable(1);
            modem_usb3v3_enable(1);
            
        } else {
            status = -EINVAL;
        }
	}

    return status;
}
static const DEVICE_ATTR(download, 0600, NULL, modem_download_store);

static ssize_t modem_sleep_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
    int value;
    
    value = gpio_get_value(MODEM_TO_AP_SLP);
    if (value < 0)
        return value;

    if (value)
        strcpy(buf, "1\n");
    else
        strcpy(buf, "0\n");
    
    return 2;
}
static const DEVICE_ATTR(sleep, 0600, modem_sleep_show, NULL);

/*ZTE:add by caixiaoguang 20111021 for modem dead++*/
static ssize_t modem_dead_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
    int value;
    
    value = gpio_get_value(modem_dead);
    if (value < 0)
        return value;

    if (value)
        strcpy(buf, "0\n");//modem not dead
    else
        strcpy(buf, "1\n");//modem is dead
    
    return 2;
}
static const DEVICE_ATTR(dead, 0600, modem_dead_show, NULL);
/*ZTE:add by caixiaoguang 20111021 for modem dead--*/

static ssize_t modem_wakeup_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t status;
	long value;

	status = strict_strtol(buf, 0, &value);
	if (status == 0) {
        status = 1;
        if (value == 1) {
            printk(KERN_NOTICE "wakeup modem\n");
            gpio_set_value(AP_TO_MODEM_WKUP, 1);
            msleep(100);
            gpio_set_value(AP_TO_MODEM_WKUP, 0);
            msleep(100);
            gpio_set_value(AP_TO_MODEM_WKUP, 1);
        } else {
            status = -EINVAL;
        }
	}

    return status;
}
static const DEVICE_ATTR(wakeup, 0600, NULL, modem_wakeup_store);

static const struct attribute *modem_control_attrs[] = {
	&dev_attr_usb_switch.attr,
	&dev_attr_usb_ldo.attr,
	&dev_attr_poweron.attr,
	&dev_attr_reset.attr,
	&dev_attr_apwakeup.attr,/*ZTE:added by zhanming for modem wakeup ap  20111104 ++*/
	&dev_attr_apsleep.attr, /*ZTE:added by zhanming for modem wakeup ap  20111104 ++*/
	&dev_attr_download.attr,
	&dev_attr_sleep.attr,
	&dev_attr_wakeup.attr,
	&dev_attr_dead.attr,/*ZTE:add by caixiaoguang 20111021 for modem dead*/
	NULL,
};

static const struct attribute_group modem_control_attr_group = {
	.attrs = (struct attribute **) modem_control_attrs,
};

/*ZTE:added by zhanming for modem wakeup ap  20111104 ++*/

static struct wake_lock modem_wake_lock;
static struct work_struct modem_env_work;
static struct kobject *pkobj;
static int suspended;
volatile static int lc_dead_level = 0;

extern void dc_present_start(void);//add for low int from modem by zhanming 20120220

static void modem_dead_work(struct work_struct *work)
{
    char *modem_dead_ind[2] = { "MODEM_STATE=DEAD", NULL };

    kobject_uevent_env( pkobj, KOBJ_CHANGE, modem_dead_ind);    
}

int modem_dead_check(void){
    return !lc_dead_level;
}
EXPORT_SYMBOL(modem_dead_check);

static irqreturn_t modem_wakeup_irq(int irq, void *data)
{
    struct platform_device *pdev = data;
    int value;
    static int ignore_fake_dead = 0;
    
    //printk(KERN_INFO "modem_wakeup_irq......\n");
    value = gpio_get_value(modem_dead);
    if (!value){
        lc_dead_level = 0;
        if (ignore_fake_dead){
            ignore_fake_dead--;
            printk(KERN_INFO "modem_boot!\n");
        }else{
            printk(KERN_INFO "modem_dead!\n");
            pkobj = &pdev->dev.kobj;
            if (!work_pending(&modem_env_work))
        		schedule_work(&modem_env_work);
        }
    }else{
        lc_dead_level = 1;
        if (ignore_fake_dead)
            printk(KERN_INFO "modem_boot over!\n");
        ignore_fake_dead = 0;
    }
    //dc_present_start();//add for low int from modem by zhanming 20120220
    
    wake_lock_timeout(&modem_wake_lock, msecs_to_jiffies(5000));
    
    return IRQ_HANDLED;
}

/*ZTE:added by zhanming for modem wakeup ap  20111104  --*/
static int __init parse_usb_swtich(char *arg)
{
	ssize_t status;
	long value;

    printk(KERN_INFO "parse_usb_swtich: %s\n", arg);

	status = strict_strtol(arg, 0, &value);
	if (status == 0) {
		if (value == 1)
			usb_switch_state = 1;
	}
	return 1;
}
__setup("usb_port_mode=", parse_usb_swtich);

static int __devinit modem_control_probe(struct platform_device *pdev)
{
    int ret;
    printk(KERN_INFO "modem_control_probe\n");

    ret = gpio_request(modem_power_enb, "modem_power");
    if (ret < 0) {
        printk(KERN_WARNING "failed to request gpio modem_power %d\n", ret);
    }
    gpio_direction_output(modem_power_enb, 1);

#if 0    
    ret = gpio_request(modem_reset_en, "modem_reset");
    if (ret < 0) {
        printk(KERN_WARNING "failed to request gpio modem_reset %d\n", ret);
    }
    gpio_direction_output(modem_reset_en, 1);
    gpio_set_value(modem_reset_en, 0);
#endif
    
    ret = gpio_request(usb_port_ctrl0, "usb_switch0");	
    if (ret < 0) {
        printk(KERN_WARNING "failed to request gpio usb_switch %d\n", ret);
    }


    /*bjk del for P040T30 GPIO change++*/
    /*ret = gpio_request(usb_port_ctrl1, "usb_switch1");	
    if (ret < 0) {
        printk(KERN_WARNING "failed to request gpio usb_switch %d\n", ret);
    }*/
    /*bjk del for P040T30 GPIO change--*/
    printk(KERN_INFO "usb_switch_state: %d\n", usb_switch_state);
    gpio_direction_output(usb_port_ctrl0, usb_switch_state == 1);

    /*bjk del for P040T30 GPIO change++*/
    //gpio_direction_output(usb_port_ctrl1, 0);
    /*bjk del for P040T30 GPIO change--*/
    
    ret =  gpio_request(modem_usb3v3_enb, "modem_usb3v3");	
    if (ret < 0) {
        printk(KERN_WARNING "failed to request gpio modem_usb3v3 %d\n", ret);
    }
    //tegra_gpio_enable(modem_usb3v3_enb);
    gpio_direction_output(modem_usb3v3_enb, 1);

#if 0
    ret =  gpio_request(modem_power_key, "modem_powerkey");	
    if (ret < 0) {
        printk(KERN_WARNING "failed to request gpio modem_powerkey %d\n", ret);
    }
    //tegra_gpio_enable(modem_power_key);
    gpio_direction_output(modem_power_key, 1);

    ret =  gpio_request(modem_download_en, "modem_download");	
    if (ret < 0) {
        printk(KERN_WARNING "failed to request gpio modem_download %d\n", ret);
    }
    //tegra_gpio_enable(modem_download_en);
    gpio_direction_output(modem_download_en, 1);
#endif

/*ZTE:added by zhanming for modem wakeup ap  20111104 ++*/
    ret =  gpio_request(MODEM_TO_AP_SLP, "modem_sleep");	
    if (ret < 0) {
        printk(KERN_WARNING "failed to request gpio MODEM_TO_AP_SLP %d\n", ret);
    }
    //tegra_gpio_enable(MODEM_TO_AP_SLP);
    gpio_direction_input(MODEM_TO_AP_SLP);
    /*++ cuijian modify init sequence, must keep ap_sleep init before modem_wakeup 20120401++*/
    ret =  gpio_request(AP_TO_MODEM_SLP, "ap_sleep");	
    if (ret < 0) {
        printk(KERN_WARNING "failed to request gpio AP_TO_MODEM_SLP %d\n", ret);
    }
    //tegra_gpio_enable(AP_TO_MODEM_SLP);
    gpio_direction_output(AP_TO_MODEM_SLP, 0);

    ret =  gpio_request(AP_TO_MODEM_WKUP, "modem_wakeup");	
    if (ret < 0) {
        printk(KERN_WARNING "failed to request gpio AP_TO_MODEM_WKUP %d\n", ret);
    }
    //tegra_gpio_enable(AP_TO_MODEM_WKUP);
    gpio_direction_output(AP_TO_MODEM_WKUP, 1);
    /*-- cuijian modify init sequence, must keep ap_sleep init before modem_wakeup 20120401--*/
    ret =  gpio_request(MODEM_TO_AP_WKUP, "ap_wakeup");	
    if (ret < 0) {
        printk(KERN_WARNING "failed to request gpio MODEM_TO_AP_WKUP %d\n", ret);
    }
   // tegra_gpio_enable(MODEM_TO_AP_WKUP);
    gpio_direction_input(MODEM_TO_AP_WKUP);
/*ZTE:added by zhanming for modem wakeup ap  20111104  --*/
    
     /*ZTE:add by caixiaoguang 20111021 for modem dead++*/   
    ret =  gpio_request(modem_dead, "modem_dead");	
    if (ret < 0) {
        printk(KERN_WARNING "failed to request gpio modem_dead %d\n", ret);
    }
    //tegra_gpio_enable(modem_dead); 
    gpio_direction_input(modem_dead);
    /*ZTE:add by caixiaoguang 20111021 for modem dead--*/  

/*ZTE:added by zhanming for modem wakeup ap  20111104 ++*/
    wake_lock_init(&modem_wake_lock, WAKE_LOCK_SUSPEND, "modem_lock");
    INIT_WORK(&modem_env_work, modem_dead_work);

    wakeup_irq = gpio_to_irq(MODEM_TO_AP_WKUP);
	ret = request_irq(wakeup_irq, modem_wakeup_irq,
		IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, "modem_wakeup_irq", pdev);
    if (ret < 0) {
        printk(KERN_WARNING "failed to request modem_wakeup_irq %d\n", ret);
    }    

/*ZTE:added by zhanming for modem wakeup ap  20111104  --*/

    printk(KERN_INFO "create sysfs interface\n");
    ret = sysfs_create_group(&pdev->dev.kobj, &modem_control_attr_group);
    if (ret)
        printk(KERN_WARNING "sysfs_create_group ret %d\n", ret);

/*ZTE:added by zhanming for modem wakeup ap  20111104 ++*/
    device_init_wakeup(&pdev->dev, 1);
    suspended = 0;

    return ret;
}

static int __devexit modem_control_remove(struct platform_device *pdev)
{
    printk(KERN_NOTICE "modem_control_remove\n");
    
    sysfs_remove_group(&pdev->dev.kobj, &modem_control_attr_group);
    device_init_wakeup(&pdev->dev, 0);
    gpio_free(modem_power_enb);
    gpio_free(usb_port_ctrl0);

    /*bjk del for P040T30 GPIO change++*/
    //gpio_free(usb_port_ctrl1);
    /*bjk del for P040T30 GPIO change--*/
    
    gpio_free(modem_usb3v3_enb);
    gpio_free(MODEM_TO_AP_SLP);
    gpio_free(AP_TO_MODEM_SLP);
    gpio_free(AP_TO_MODEM_WKUP);
    gpio_free(MODEM_TO_AP_WKUP);    
    gpio_free(modem_dead);
    return 0;
}

static int modem_control_suspend(struct platform_device *pdev, pm_message_t state)
{
    /*ZTE:added by zhanming for modem wakeup ap  20111104 ++*/
    if (device_may_wakeup(&pdev->dev))
    {
        enable_irq_wake(wakeup_irq);
    }
    suspended = 1;
    //modem_uart_enable(0);
    return 0;
}

static int modem_control_resume(struct platform_device *pdev)
{
/*ZTE:added by zhanming for modem wakeup ap  20111104 ++*/
    //modem_uart_enable(1);
    if (device_may_wakeup(&pdev->dev))
    {
        disable_irq_wake(wakeup_irq);
    }
    suspended = 0;
    return 0;
}

static struct platform_driver modem_control_driver = {
	.remove = modem_control_remove,
    .suspend = modem_control_suspend,
    .resume = modem_control_resume,
	.driver = {
		   .name = "modem_control",
	},
};

static int __devinit modem_control_init(void)
{
    printk(KERN_NOTICE "modem_control_init\n");
	return platform_driver_probe(&modem_control_driver, modem_control_probe);
}

static void __devexit modem_control_exit(void)
{
    printk(KERN_NOTICE "modem_control_exit\n");
	platform_driver_unregister(&modem_control_driver);
}

module_init(modem_control_init);
module_exit(modem_control_exit);

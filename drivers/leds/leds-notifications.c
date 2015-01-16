/*
 * LED driver for Android notifications
 *
 * Copyright (C) 2012 ZTE Corporation.
 *	Poyuan Lu <lu.poyuan@zte.com.cn>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/ctype.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/suspend.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/module.h>
#define LED_ALPH_MASK 0xFF000000
#define LED_RED_MASK 0x00FF0000
#define LED_GREEN_MASK 0x0000FF00
#define LED_BLUE_MASK 0x000000FF

struct notifications_leds {
	struct led_classdev cdev;
	struct mutex lock;
	struct work_struct work;

	unsigned long color;
	unsigned long blink;
	unsigned long onMs;
	unsigned long offMs;
	int brightness;

	void (*red_control)(int on_off);
	void (*green_control)(int on_off);
	void (*blue_control)(int on_off);

	struct notifier_block leds_reboot_nb;
	struct notifier_block leds_pm_nb;
};

struct notifications_leds_pdata {
	void (*red_control)(int on_off);
	void (*green_control)(int on_off);
	void (*blue_control)(int on_off);
};

static void led_red_control(struct notifications_leds *data, int on_off)
{
	if(data->red_control)
		data->red_control(on_off);

}

static void led_green_control(struct notifications_leds *data, int on_off)
{
	if(data->green_control)
		data->green_control(on_off);
}

static void led_blue_control(struct notifications_leds *data, int on_off)
{
	if(data->blue_control)
		data->blue_control(on_off);
}

static ssize_t led_color_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct notifications_leds *data;

	data = container_of(led_cdev, struct notifications_leds, cdev);

	return sprintf(buf, "0x%08lx\n", data->color);
}

static ssize_t led_color_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	int ret = -EINVAL;
	char *after;
	unsigned long state = simple_strtoul(buf, &after, 16);
	size_t count = after - buf;
	struct notifications_leds *data;

	data = container_of(led_cdev, struct notifications_leds, cdev);

	if (isspace(*after))
		count++;

	if (count == size) {
		data->color = state;
		if(!(data->color & LED_RED_MASK))
			led_red_control(data, 0);

		if(!(data->color & LED_GREEN_MASK))
			led_green_control(data, 0);

		if(!(data->color & LED_BLUE_MASK))
			led_blue_control(data, 0);
			
		ret = count;
	}

	return ret;
}

static ssize_t led_blink_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct notifications_leds *data;

	data = container_of(led_cdev, struct notifications_leds, cdev);

	return sprintf(buf, "%lu\n", data->blink);
}

static ssize_t led_blink_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	int ret = -EINVAL;
	char *after;
	unsigned long state = simple_strtoul(buf, &after, 10);
	size_t count = after - buf;
	struct notifications_leds *data;

	data = container_of(led_cdev, struct notifications_leds, cdev);

	if (isspace(*after))
		count++;

	if (count == size) {
		unsigned long color;

		color = data->color;
		data->color = 0x00FFFFFF;
		led_brightness_set(led_cdev, LED_OFF);
		data->color = color;

		data->blink = state;
		if(data->blink){
			led_blink_set(led_cdev, &data->onMs, &data->offMs);
		}else{
			if(data->color)
				led_brightness_set(led_cdev, LED_FULL);
			else
				led_brightness_set(led_cdev, LED_OFF);
		}

		ret = count;
	}

	return ret;
}

static ssize_t led_delay_on_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);

	return sprintf(buf, "%lu\n", led_cdev->blink_delay_on);
}

static ssize_t led_delay_on_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	int ret = -EINVAL;
	char *after;
	unsigned long state = simple_strtoul(buf, &after, 10);
	size_t count = after - buf;
	struct notifications_leds *data;

	data = container_of(led_cdev, struct notifications_leds, cdev);

	if (isspace(*after))
		count++;

	if (count == size) {
		data->onMs = state;
		if(data->blink){
			led_blink_set(led_cdev, &data->onMs, &data->offMs);
		}
		ret = count;
	}

	return ret;
}

static ssize_t led_delay_off_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);

	return sprintf(buf, "%lu\n", led_cdev->blink_delay_off);
}

static ssize_t led_delay_off_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	int ret = -EINVAL;
	char *after;
	unsigned long state = simple_strtoul(buf, &after, 10);
	size_t count = after - buf;
	struct notifications_leds *data;

	data = container_of(led_cdev, struct notifications_leds, cdev);

	if (isspace(*after))
		count++;

	if (count == size) {
		data->offMs = state;
		if(data->blink){
			led_blink_set(led_cdev, &data->onMs, &data->offMs);
		}
		ret = count;
	}

	return ret;
}

static struct device_attribute notifications_leds_attrs[] = {
	__ATTR(color, 0664, led_color_show, led_color_store),
	__ATTR(blink, 0664, led_blink_show, led_blink_store),
	__ATTR(delay_on, 0664, led_delay_on_show, led_delay_on_store),
	__ATTR(delay_off, 0664, led_delay_off_show, led_delay_off_store),
};

static int leds_notifier_reboot(struct notifier_block *nb, unsigned long code,
			  void *unused)
{
	struct notifications_leds *data;
	unsigned long color;

	data = container_of(nb, struct notifications_leds, leds_reboot_nb);

	switch (code) {
		case SYS_DOWN:
		case SYS_POWER_OFF:
			color = data->color;
			data->color = 0x00FFFFFF;
			led_brightness_set(&data->cdev, LED_OFF);
			data->color = color;
			break;

		default:
			break;
	}

	return NOTIFY_OK;
}

static int leds_notifier_pm(struct notifier_block *nb, unsigned long code,
			  void *unused)
{
	struct notifications_leds *data;
	unsigned long color;

	data = container_of(nb, struct notifications_leds, leds_pm_nb);

	switch (code) {
		case PM_SUSPEND_PREPARE:
			color = data->color;
			data->color = 0x00FFFFFF;
			led_brightness_set(&data->cdev, LED_OFF);
			data->color = color;
			break;

		case PM_POST_SUSPEND:
			if(data->blink){
				led_blink_set(&data->cdev, &data->onMs, &data->offMs);
			}else{
				if(data->color)
					led_brightness_set(&data->cdev, LED_FULL);
				else
					led_brightness_set(&data->cdev, LED_OFF);
			}
			break;

		default:
			break;
	}

	return NOTIFY_OK;
}

static void notifications_leds_control(struct notifications_leds *data)
{
	if(data->color & LED_RED_MASK){
		led_red_control(data, !!(data->brightness));
	}

	if(data->color & LED_GREEN_MASK){
		led_green_control(data, !!(data->brightness));
	}

	if(data->color & LED_BLUE_MASK){
		led_blue_control(data, !!(data->brightness));
	}
}

static void notifications_leds_work(struct work_struct *work)
{
	struct notifications_leds *data;
	//printk("%s in \n", __func__);
	data = container_of(work, struct notifications_leds, work);

	mutex_lock(&data->lock);
	
	notifications_leds_control(data);

	mutex_unlock(&data->lock);
}

static void notifications_leds_set(struct led_classdev *cdev,
			   enum led_brightness value)
{
	struct notifications_leds *data;

	data = container_of(cdev, struct notifications_leds, cdev);
	data->brightness = value;

	schedule_work(&data->work);
}

static int notifications_leds_probe(struct platform_device *pdev)
{
	int ret;
	int i;
	struct notifications_leds *data;
	struct notifications_leds_pdata *pdata;

	pdata = pdev->dev.platform_data;
	if (pdata == NULL) {
		dev_err(&pdev->dev, "No platform data!\n");
		return -EINVAL;
	}

	data = kzalloc(sizeof(struct notifications_leds), GFP_KERNEL);
	if (data == NULL)
		return -ENOMEM;

	if(pdata->red_control)
		data->red_control = pdata->red_control;
	if(pdata->green_control)
		data->green_control = pdata->green_control;
	if(pdata->blue_control)
		data->blue_control = pdata->blue_control;

	data->onMs = 500;
	data->offMs = 500;

	mutex_init(&data->lock);
	INIT_WORK(&data->work, notifications_leds_work);

	data->cdev.name = "notifications";
	data->cdev.brightness_set = notifications_leds_set;
	ret = led_classdev_register(&pdev->dev, &data->cdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register LED: %d\n", ret);
		goto out;
	}

	for (i = 0; i < ARRAY_SIZE(notifications_leds_attrs); i++) {
		ret = device_create_file(data->cdev.dev,
					 &notifications_leds_attrs[i]);
		if (ret)
			goto err_create_file;
	}

	data->leds_reboot_nb.notifier_call = leds_notifier_reboot;
	ret = register_reboot_notifier(&data->leds_reboot_nb);
	data->leds_pm_nb.notifier_call = leds_notifier_pm;
	ret = register_pm_notifier(&data->leds_pm_nb);

	//close all leds
	data->color = 0x00ffffff;
	notifications_leds_set(&data->cdev, LED_OFF);
	data->color = 0;

	return 0;
err_create_file:
	while (--i >= 0)
		device_remove_file(data->cdev.dev, &notifications_leds_attrs[i]);
out:
	kfree(data);
	return ret;
}

static int notifications_leds_remove(struct platform_device *pdev)
{
	int i;
	int ret;
	struct notifications_leds *data = platform_get_drvdata(pdev);

	for (i = 0; i < ARRAY_SIZE(notifications_leds_attrs); i++) {
		device_remove_file(data->cdev.dev,
					 &notifications_leds_attrs[i]);
	}

	ret = unregister_reboot_notifier(&data->leds_reboot_nb);
	ret = unregister_pm_notifier(&data->leds_pm_nb);

	led_classdev_unregister(&data->cdev);
	kfree(data);

	return 0;
}

static struct platform_driver notifications_leds_driver = {
	.driver	= {
		.name	= "leds-notifications",
		.owner	= THIS_MODULE,
	},
	.probe	= notifications_leds_probe,
	.remove	= notifications_leds_remove,
};

static int __devinit notifications_leds_init(void)
{
	return platform_driver_register(&notifications_leds_driver);
}
module_init(notifications_leds_init);

static void __devexit notifications_leds_exit(void)
{
	platform_driver_unregister(&notifications_leds_driver);
}
module_exit(notifications_leds_exit);

MODULE_DESCRIPTION("LED driver for Android notifications");
MODULE_AUTHOR("Poyuan Lu <lu.poyuan@zte.com.cn>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:leds-notifications");

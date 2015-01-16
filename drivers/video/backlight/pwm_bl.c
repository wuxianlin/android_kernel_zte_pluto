/*
 * linux/drivers/video/backlight/pwm_bl.c
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION, All rights reserved.
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
 * simple PWM based backlight control, board code has to setup
 * 1) pin configuration so PWM waveforms can output
 * 2) platform_data being correctly configured
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/pwm.h>
#include <linux/pwm_backlight.h>
#include <linux/slab.h>
#include <linux/edp.h>

struct pwm_bl_data {
	struct pwm_device	*pwm;
	struct device		*dev;
	unsigned int		period;
	unsigned int		lth_brightness;
	unsigned int		pwm_gpio;
	int			(*notify)(struct device *,
					  int brightness);
	void			(*notify_after)(struct device *,
					int brightness);
	int			(*check_fb)(struct device *, struct fb_info *);
	int (*display_init)(struct device *);
	struct edp_client *pwm_edp_client;
	int *edp_brightness_states;
};


static int debug = 0;
module_param(debug, int, 0600);

#ifdef CONFIG_BACKLIGHT_1_WIRE_MODE
extern int tps61165_set_backlight(int brightness);
#endif

static int pwm_brightness_set(struct backlight_device *bl, int brightness)
{
	struct pwm_bl_data *pb = dev_get_drvdata(&bl->dev);
	int max = bl->props.max_brightness;
	static int old_brightness = -1; 

	if(debug)
	{
		printk("[tong]:pwm_backlight_update_status, brightness = %d\n", brightness);
	}

	if (pb->display_init && !pb->display_init(pb->dev))
		brightness = 0;

	if (bl->props.power != FB_BLANK_UNBLANK)
		brightness = 0;

	if (bl->props.fb_blank != FB_BLANK_UNBLANK)
		brightness = 0;

	if (pb->notify)
		brightness = pb->notify(pb->dev, brightness);

    if (brightness > max)
       {
		    dev_err(&bl->dev, "Invalid brightness value: %d max: %d\n",
		        brightness, max);
            return 0;
       }

      if(old_brightness == brightness)
      {
          if(debug)
          {
              printk("[tong]:pwm_backlight_update_status: the same level as before, nothing done!brightness=%d\n", brightness);
          }
          return 0;
      }
      old_brightness = brightness;

#if defined (CONFIG_BACKLIGHT_1_WIRE_MODE)
        tps61165_set_backlight(brightness);
#else
	if (brightness == 0) {
		pwm_config(pb->pwm, 0, pb->period);
		pwm_disable(pb->pwm);
	} else {
		brightness = pb->lth_brightness +
			(brightness * (pb->period - pb->lth_brightness) / max);
		pwm_config(pb->pwm, brightness, pb->period);
		pwm_enable(pb->pwm);
	}
#endif

	if (pb->notify_after)
		pb->notify_after(pb->dev, brightness);

	return 0;
}

static int pwm_backlight_set_with_edp(struct backlight_device *bl,
		int brightness)
{
	struct pwm_bl_data *data = dev_get_drvdata(&bl->dev);
	struct device *dev = data->dev;
    unsigned int approved;
	int ret;
	unsigned int edp_state;
	unsigned int i;
	if (data->pwm_edp_client) {
		for (i = 0; i < PWM_EDP_NUM_STATES; i++) {
			if (brightness >= data->edp_brightness_states[i])
				break;
		}
		edp_state = i;
		ret = edp_update_client_request(data->pwm_edp_client,
							edp_state, &approved);
		if (ret) {
			dev_err(dev, "E state transition failed\n");
			return ret;
		}
	}
	/* if user is requesting brightness > 0, don't turn off backlight. */
	if (brightness > 0 && !data->edp_brightness_states[approved]
			&& approved)
		approved--;

	//pwm_brightness_set(bl, data->edp_brightness_states[approved]);
	pwm_brightness_set(bl, brightness);

	return 0;
}

int  adjust_brightness(int brightness)
{
        int adjust_value;
        int temp_value;

      // 80%
 #if 0
	adjust_value = (((brightness-10)*100 /245)*((brightness-10)*100 /245)*11)/1000 + (((brightness-10)*100 /245)*27)/20 +10;

      // 85%
	adjust_value = (((brightness-10)*100 /245)*((brightness-10)*100 /245)*7)/1000 + (((brightness-10)*100 /245)*7)/4 +10;
#endif

	// 90%
	adjust_value = (((brightness-10)*100 /245)*((brightness-10)*100 /245))/200 + (((brightness-10)*100 /245)*39)/20 +10;

       return adjust_value;
}

static int pwm_backlight_update_status(struct backlight_device *bl)
{
	struct pwm_bl_data *pb = dev_get_drvdata(&bl->dev);
	int brightness = bl->props.brightness;

#if 0
	if (bl->props.power != FB_BLANK_UNBLANK)
		brightness = 0;

	if (bl->props.fb_blank != FB_BLANK_UNBLANK)
		brightness = 0;

	if (pb->notify)
		brightness = pb->notify(pb->dev, brightness);
#endif
       printk("before brightness = %d\n" ,brightness);
	brightness = adjust_brightness(brightness);
       printk("after  brightness = %d\n" ,brightness);

	pwm_backlight_set_with_edp(bl, brightness);

	return 0;
}

static void pwm_backlight_edpcb(unsigned int new_state, void *priv_data)
{
	struct backlight_device *bl = (struct backlight_device *) priv_data;
	struct pwm_bl_data *data = dev_get_drvdata(&bl->dev);

	pwm_brightness_set(bl, data->edp_brightness_states[new_state]);
}

static int pwm_backlight_get_brightness(struct backlight_device *bl)
{
	return bl->props.brightness;
}

static int pwm_backlight_check_fb(struct backlight_device *bl,
				  struct fb_info *info)
{
	struct pwm_bl_data *pb = dev_get_drvdata(&bl->dev);

	return !pb->check_fb || pb->check_fb(pb->dev, info);
}

static const struct backlight_ops pwm_backlight_ops = {
	.update_status	= pwm_backlight_update_status,
	.get_brightness	= pwm_backlight_get_brightness,
	.check_fb	= pwm_backlight_check_fb,
};

static int pwm_backlight_probe(struct platform_device *pdev)
{
	struct backlight_properties props;
	struct platform_pwm_backlight_data *data = pdev->dev.platform_data;
	struct backlight_device *bl;
	struct pwm_bl_data *pb;
	int ret;
	struct edp_manager *battery_manager = NULL;

	if (!data) {
		dev_err(&pdev->dev, "failed to find platform data\n");
		return -EINVAL;
	}

	if (data->init) {
		ret = data->init(&pdev->dev);
		if (ret < 0)
			return ret;
	}

	pb = devm_kzalloc(&pdev->dev, sizeof(*pb), GFP_KERNEL);
	if (!pb) {
		dev_err(&pdev->dev, "no memory for state\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	pb->period = data->pwm_period_ns;
	pb->notify = data->notify;
	pb->notify_after = data->notify_after;
	pb->check_fb = data->check_fb;
	pb->lth_brightness = data->lth_brightness *
		(data->pwm_period_ns / data->max_brightness);
	pb->dev = &pdev->dev;
	pb->display_init = data->init;
	pb->pwm_gpio = data->pwm_gpio;
	pb->edp_brightness_states = data->edp_brightness;

	pb->pwm = pwm_request(data->pwm_id, "backlight");
	if (IS_ERR(pb->pwm)) {
		dev_err(&pdev->dev, "unable to request PWM for backlight\n");
		ret = PTR_ERR(pb->pwm);
		goto err_alloc;
	} else
		dev_dbg(&pdev->dev, "got pwm for backlight\n");

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = data->max_brightness;

	if (gpio_is_valid(pb->pwm_gpio)) {
		ret = gpio_request(pb->pwm_gpio, "disp_bl");
		if (ret)
			dev_err(&pdev->dev, "backlight gpio request failed\n");
	}

	bl = backlight_device_register(dev_name(&pdev->dev), &pdev->dev, pb,
				       &pwm_backlight_ops, &props);
	if (IS_ERR(bl)) {
		dev_err(&pdev->dev, "failed to register backlight\n");
		ret = PTR_ERR(bl);
		goto err_bl;
	}

	pb->pwm_edp_client = devm_kzalloc(&pdev->dev,
				sizeof(struct edp_client), GFP_KERNEL);
	if (IS_ERR_OR_NULL(pb->pwm_edp_client)) {
		dev_err(&pdev->dev, "could not allocate edp client\n");
		return PTR_ERR(pb->pwm_edp_client);
	}

	strncpy(pb->pwm_edp_client->name, "backlight", EDP_NAME_LEN - 1);
	pb->pwm_edp_client->name[EDP_NAME_LEN - 1] = '\0';
	pb->pwm_edp_client->states = data->edp_states;
	pb->pwm_edp_client->num_states = PWM_EDP_NUM_STATES;
	pb->pwm_edp_client->e0_index = PWM_EDP_ZERO;
	pb->pwm_edp_client->private_data = bl;
	pb->pwm_edp_client->priority = EDP_MIN_PRIO - 2;
	pb->pwm_edp_client->throttle = pwm_backlight_edpcb;
	pb->pwm_edp_client->notify_promotion = pwm_backlight_edpcb;

	battery_manager = edp_get_manager("battery");
	if (!battery_manager) {
		dev_err(&pdev->dev, "unable to get edp manager\n");
	} else {
		ret = edp_register_client(battery_manager,
					pb->pwm_edp_client);
		if (ret) {
			dev_err(&pdev->dev,
				"unable to register edp client:%d\n", ret);
		} else {
			ret = edp_update_client_request(
						pb->pwm_edp_client,
						PWM_EDP_ZERO, NULL);
			if (ret) {
				dev_err(&pdev->dev,
				  "unable to set E0 EDP state\n");
				  edp_unregister_client(pb->pwm_edp_client);
			} else {
				goto edp_success;
			}
		}
	}

	devm_kfree(&pdev->dev, pb->pwm_edp_client);
	pb->pwm_edp_client = NULL;

edp_success:
	bl->props.brightness = data->dft_brightness;
	backlight_update_status(bl);

	if (gpio_is_valid(pb->pwm_gpio))
		gpio_free(pb->pwm_gpio);

	platform_set_drvdata(pdev, bl);
	return 0;

err_bl:
	pwm_free(pb->pwm);
err_alloc:
	if (data->exit)
		data->exit(&pdev->dev);
	return ret;
}

static int pwm_backlight_remove(struct platform_device *pdev)
{
	struct platform_pwm_backlight_data *data = pdev->dev.platform_data;
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct pwm_bl_data *pb = dev_get_drvdata(&bl->dev);

	backlight_device_unregister(bl);
	pwm_config(pb->pwm, 0, pb->period);
	pwm_disable(pb->pwm);
	pwm_free(pb->pwm);
	if (data->exit)
		data->exit(&pdev->dev);
	return 0;
}

#ifdef CONFIG_PM
static int pwm_backlight_suspend(struct device *dev)
{
	struct backlight_device *bl = dev_get_drvdata(dev);
	struct pwm_bl_data *pb = dev_get_drvdata(&bl->dev);

	if (pb->notify)
		pb->notify(pb->dev, 0);
	pwm_config(pb->pwm, 0, pb->period);
	pwm_disable(pb->pwm);
	if (pb->notify_after)
		pb->notify_after(pb->dev, 0);
	pwm_backlight_set_with_edp(bl, 0);
	return 0;
}

static int pwm_backlight_resume(struct device *dev)
{
	struct backlight_device *bl = dev_get_drvdata(dev);

	backlight_update_status(bl);
	return 0;
}

static SIMPLE_DEV_PM_OPS(pwm_backlight_pm_ops, pwm_backlight_suspend,
			 pwm_backlight_resume);

#endif

static struct platform_driver pwm_backlight_driver = {
	.driver		= {
		.name	= "pwm-backlight",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &pwm_backlight_pm_ops,
#endif
	},
	.probe		= pwm_backlight_probe,
	.remove		= pwm_backlight_remove,
};

module_platform_driver(pwm_backlight_driver);

MODULE_DESCRIPTION("PWM based Backlight Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:pwm-backlight");


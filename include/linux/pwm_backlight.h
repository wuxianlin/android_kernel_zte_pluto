/*
 * Generic PWM backlight driver data - see drivers/video/backlight/pwm_bl.c
 */
#ifndef __LINUX_PWM_BACKLIGHT_H
#define __LINUX_PWM_BACKLIGHT_H

#include <linux/backlight.h>

enum pwm_edp_states {
	PWM_EDP_NEG_3,
	PWM_EDP_NEG_2,
	PWM_EDP_NEG_1,
	PWM_EDP_ZERO,
	PWM_EDP_1,
	PWM_EDP_2,
	PWM_EDP_3,
	PWM_EDP_4,
	PWM_EDP_5,
	PWM_EDP_6,
	PWM_EDP_7,
	PWM_EDP_NUM_STATES,
};

#define PWM_EDP_BRIGHTNESS_UNIT    25
struct platform_pwm_backlight_data {
	int pwm_id;
	unsigned int max_brightness;
	unsigned int dft_brightness;
	unsigned int lth_brightness;
	unsigned int pwm_period_ns;
	unsigned int pwm_gpio;
	int (*init)(struct device *dev);
	int (*notify)(struct device *dev, int brightness);
	void (*notify_after)(struct device *dev, int brightness);
	void (*exit)(struct device *dev);
	int (*check_fb)(struct device *dev, struct fb_info *info);
	unsigned int edp_states[PWM_EDP_NUM_STATES];
	unsigned int edp_brightness[PWM_EDP_NUM_STATES];
};

#endif

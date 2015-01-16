/*
 * arch/arm/mach-tegra/panel-yushun-1080p-5-7.c
 *
  * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
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

#include <mach/dc.h>
#include <mach/iomap.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/tegra_pwm_bl.h>
#include <linux/regulator/consumer.h>
#include <linux/pwm_backlight.h>
#include <linux/ioport.h>

#include "devices.h"
#include "gpio-names.h"
#include "board-panel.h"
#include "NT35596.h"


#define DSI_PANEL_RESET         1
#define DSI_PANEL_RST_GPIO      TEGRA_GPIO_PH5
#define DSI_PANEL_BL_PWM        TEGRA_GPIO_PH1
#define DC_CTRL_MODE            (TEGRA_DC_OUT_CONTINUOUS_MODE|TEGRA_DC_OUT_INITIALIZED_MODE)

static struct regulator *avdd_lcd_3v0_2v8;
static bool dsi_yushun_1080p_5_reg_requested;
static bool dsi_yushun_1080p_5_gpio_requested;
static bool is_bl_powered;

static struct platform_device *disp_device;
static int initlcdflag = 0;
#if CONFIG_ESD_READ_TE
extern void te_set_SuspendFlag(int flag);
extern void te_set_WatchdogTimer(int mSecs);
#endif

#if 0
static tegra_dc_bl_output dsi_yushun_1080p_5_bl_response_curve = {
	0, 1, 3, 5, 7, 9, 11, 13,
	15, 17, 19, 21, 22, 23, 25, 26,
	28, 29, 30, 32, 33, 34, 36, 37,
	39, 40, 42, 43, 45, 46, 48, 49,
	50, 51, 52, 53, 54, 55, 56, 57,
	58, 59, 60, 61, 62, 63, 64, 65,
	66, 67, 68, 70, 71, 72, 73, 74,
	75, 77, 78, 79, 80, 81, 82, 83,
	84, 85, 86, 87, 88, 89, 90, 91,
	92, 93, 94, 95, 96, 97, 98, 99,
	100, 101, 101, 102, 102, 103, 103, 104,
	105, 105, 106, 107, 108, 108, 109, 110,
	111, 112, 113, 114, 115, 116, 117, 118,
	119, 120, 121, 121, 122, 123, 124, 125,
	126, 127, 128, 129, 130, 131, 132, 133,
	134, 135, 135, 136, 137, 138, 139, 140,
	141, 142, 143, 144, 145, 146, 147, 148,
	149, 150, 151, 152, 153, 154, 155, 156,
	156, 157, 158, 159, 160, 161, 162, 162,
	163, 163, 164, 164, 165, 165, 166, 167,
	167, 168, 169, 170, 171, 172, 173, 173,
	174, 175, 176, 177, 178, 179, 180, 181,
	182, 183, 184, 185, 186, 187, 188, 188,
	189, 190, 191, 192, 193, 194, 194, 195,
	196, 197, 198, 199, 200, 201, 202, 203,
	204, 204, 205, 206, 206, 207, 207, 208,
	209, 209, 210, 211, 212, 213, 214, 215,
	216, 217, 218, 219, 220, 221, 222, 223,
	223, 224, 225, 226, 227, 228, 229, 230,
	231, 232, 233, 234, 235, 236, 237, 238,
	239, 240, 241, 242, 243, 244, 245, 246,
	247, 247, 248, 250, 251, 252, 253, 255
};
#endif
static int __maybe_unused dsi_yushun_1080p_5_bl_notify(struct device *unused,
							int brightness)
{
	//int cur_sd_brightness = atomic_read(&sd_brightness);

	/* Apply any backlight response curve */
	if (brightness > 255)
		pr_info("Error: Brightness > 255!\n");
#if 0
	else
		brightness = dsi_yushun_1080p_5_bl_response_curve[brightness];

	/* SD brightness is a percentage */
	brightness = (brightness * cur_sd_brightness) / 255;
#endif
	return brightness;
}

static bool __maybe_unused dsi_yushun_1080p_5_check_bl_power(void)
{
	return is_bl_powered;
}

static int __maybe_unused dsi_yushun_1080p_5_check_fb(struct device *dev,
					     struct fb_info *info)
{
	return info->device == &disp_device->dev;
}

/*
	jingdongfang uses pwm blacklight device
*/
static struct platform_pwm_backlight_data dsi_yushun_1080p_5_bl_data = {
	.pwm_id         = 1,
	.max_brightness = 255,
	.dft_brightness = 132,
	.pwm_period_ns  = 40000,
	.notify         = dsi_yushun_1080p_5_bl_notify,
	/* Only toggle backlight on fb blank notifications for disp1 */
	.check_fb       = dsi_yushun_1080p_5_check_fb,
    .edp_states = {886, 556, 544, 452, 374, 302, 216, 172, 112, 68, 0}, /* mW. P040T30 LCM 2013-6-4*/
    .edp_brightness = {255, 230, 204, 179, 153, 128, 102, 77, 51, 19, 0}, /* brightness */
};

static struct platform_device dsi_yushun_1080p_5_bl_device = {
	.name   = "pwm-backlight",
	.id     = -1,
	.dev    = {
		.platform_data = &dsi_yushun_1080p_5_bl_data,
	},
};

static struct platform_device __maybe_unused
			*dsi_yushun_1080p_5_bl_devices[] __initdata = {
	&tegra_pwfm1_device,
	&dsi_yushun_1080p_5_bl_device,
};

static int __init dsi_yushun_1080p_5_register_bl_dev(void)
{
	int err = 0;
	err = platform_add_devices(dsi_yushun_1080p_5_bl_devices,
				ARRAY_SIZE(dsi_yushun_1080p_5_bl_devices));
	if (err) {
		pr_err("disp1 bl device registration failed");
		return err;
	}
	return err;
}

struct tegra_dc_mode dsi_yushun_1080p_5_modes[] = {
	{
		.pclk = 143700000,
		.h_ref_to_sync = 4,
		.v_ref_to_sync = 1,
		.h_sync_width = 10,
		.v_sync_width = 2,
		.h_back_porch = 50,
		.v_back_porch = 3,
		.h_active = 1080,
		.v_active = 1920,
		.h_front_porch = 100,
		.v_front_porch = 4,
	},
};

static int dsi_yushun_1080p_5_reg_get(struct device *dev)
{
	int err = 0;

	if (dsi_yushun_1080p_5_reg_requested)
		return 0;

	avdd_lcd_3v0_2v8 = regulator_get(dev, "avdd_lcd");
	if (IS_ERR_OR_NULL(avdd_lcd_3v0_2v8)) {
		pr_err("avdd_lcd regulator get failed\n");
		err = PTR_ERR(avdd_lcd_3v0_2v8);
		avdd_lcd_3v0_2v8 = NULL;
		goto fail;
	}

	dsi_yushun_1080p_5_reg_requested = true;
	return 0;
fail:
	return err;
}

static int dsi_yushun_1080p_5_gpio_get(void)
{
	int err = 0;

	if (dsi_yushun_1080p_5_gpio_requested)
		return 0;

	err = gpio_request(DSI_PANEL_RST_GPIO, "panel rst");
	if (err < 0) {
		pr_err("panel reset gpio request failed err =%d \n",err);
		goto fail;
	}
#if 0
	err = gpio_request(DSI_PANEL_BL_PWM, "panel pwm");
	if (err < 0) {
		pr_err("panel backlight pwm gpio request failed\n");
		goto fail;
	}
    gpio_direction_output(DSI_PANEL_BL_PWM, 1);
#endif
	dsi_yushun_1080p_5_gpio_requested = true;
	return 0;
fail:
	return err;
}

static int dsi_yushun_1080p_5_enable(struct device *dev)
{
	int err = 0;

	err = dsi_yushun_1080p_5_reg_get(dev);
	if (err < 0) {
		pr_err("dsi regulator get failed\n");
		goto fail;
	}

	err = dsi_yushun_1080p_5_gpio_get();
	if (err < 0) {
		pr_err("dsi gpio request failed\n");
		goto fail;
	}
#if DSI_PANEL_RESET
    printk("liyibo : initlcdflag = %d %s %d\n", initlcdflag, __func__, __LINE__);
    if(initlcdflag != 0)
    {
	    gpio_direction_output(DSI_PANEL_RST_GPIO, 0);
    }
#endif

    gpio_set_value(pluto_LCD_1V8_EN,1);
	if (avdd_lcd_3v0_2v8) {
		err = regulator_enable(avdd_lcd_3v0_2v8);
		if (err < 0) {
			pr_err("avdd_lcd regulator enable failed\n");
			goto fail;
		}
		regulator_set_voltage(avdd_lcd_3v0_2v8, 2800000, 2800000);
	}

	usleep_range(3000, 5000);

#if DSI_PANEL_RESET
//	gpio_set_value(DSI_PANEL_RST_GPIO, 1);  /*  deleted begin by liyibo for wake up delay 2014-03-18*/
	/*delay(10);
	gpio_set_value(DSI_PANEL_RST_GPIO, 0);
	mdelay(10);
	gpio_set_value(DSI_PANEL_RST_GPIO, 1);
	mdelay(120);*/
//	msleep(20);
#endif

	is_bl_powered = true;
#if CONFIG_ESD_READ_TE
    te_set_SuspendFlag(0);
    if(initlcdflag != 0)
    {
        te_set_WatchdogTimer(2000);
    }
#endif

    initlcdflag = 1;
	return 0;
fail:
	return err;
}

static int dsi_yushun_1080p_5_disable(void)
{
#if CONFIG_ESD_READ_TE
    te_set_SuspendFlag(1);
#endif
	is_bl_powered = false;

	gpio_set_value(DSI_PANEL_RST_GPIO, 0);
    mdelay(100);
 //   gpio_set_value(DSI_PANEL_RST_GPIO, 1); /*  deleted begin by liyibo for wake up delay 2014-03-18*/
//    mdelay(120);
	/*if (avdd_lcd_3v0_2v8)
		regulator_disable(avdd_lcd_3v0_2v8);
    usleep_range(3000, 5000);
	gpio_set_value(pluto_LCD_1V8_EN,0);*/
	return 0;
}

static int dsi_yushun_1080p_5_postsuspend(void)
{
	return 0;
}

static struct tegra_dsi_cmd NT35596_AUO_dsi_init_cmd[]= {
/* modify begin by liyibo for wake up delay 2014-03-18*/
    DSI_CMD_SHORT(0x15, 0xff, 0xEE),    
    DSI_CMD_SHORT(0x15, 0xfb, 0x01), 
    DSI_CMD_SHORT(0x15, 0x18, 0x40),
    DSI_DLY_MS(10), 
    DSI_CMD_SHORT(0x15, 0x18, 0x00), 
    DSI_DLY_MS(20),
/* modify end  by liyibo for wake up delay 2014-03-18*/
    DSI_CMD_LONG(0x39,NT35596_AUO_param0),
    DSI_CMD_LONG(0x39,NT35596_AUO_param1),
    DSI_CMD_LONG(0x39,NT35596_AUO_param2),
    DSI_DLY_MS(21),
    DSI_CMD_LONG(0x39,NT35596_AUO_param3),
    DSI_CMD_LONG(0x39,NT35596_AUO_param4),
    DSI_CMD_LONG(0x39,NT35596_AUO_param5),
    DSI_CMD_LONG(0x39,NT35596_AUO_param6),
    DSI_CMD_LONG(0x39,NT35596_AUO_param7),
    DSI_CMD_LONG(0x39,NT35596_AUO_param8),
    DSI_CMD_LONG(0x39,NT35596_AUO_param9),
    DSI_CMD_LONG(0x39,NT35596_AUO_param10),
    DSI_CMD_LONG(0x39,NT35596_AUO_param11),
    DSI_CMD_LONG(0x39,NT35596_AUO_param12),
    DSI_CMD_LONG(0x39,NT35596_AUO_param13),
    DSI_CMD_LONG(0x39,NT35596_AUO_param14),
    DSI_CMD_LONG(0x39,NT35596_AUO_param15),
    DSI_CMD_LONG(0x39,NT35596_AUO_param16),
    DSI_CMD_LONG(0x39,NT35596_AUO_param17),
    DSI_CMD_LONG(0x39,NT35596_AUO_param18),
    DSI_CMD_LONG(0x39,NT35596_AUO_param19),
    DSI_CMD_LONG(0x39,NT35596_AUO_param20),
    DSI_CMD_LONG(0x39,NT35596_AUO_param21),
    DSI_CMD_LONG(0x39,NT35596_AUO_param22),
    DSI_CMD_LONG(0x39,NT35596_AUO_param23),
    DSI_CMD_LONG(0x39,NT35596_AUO_param24),
    DSI_CMD_LONG(0x39,NT35596_AUO_param25),
    DSI_CMD_LONG(0x39,NT35596_AUO_param26),
    DSI_CMD_LONG(0x39,NT35596_AUO_param27),
    DSI_CMD_LONG(0x39,NT35596_AUO_param28),
    DSI_CMD_LONG(0x39,NT35596_AUO_param29),
    DSI_CMD_LONG(0x39,NT35596_AUO_param30),
    DSI_CMD_LONG(0x39,NT35596_AUO_param31),
    DSI_CMD_LONG(0x39,NT35596_AUO_param32),
    DSI_CMD_LONG(0x39,NT35596_AUO_param33),
    DSI_CMD_LONG(0x39,NT35596_AUO_param34),
    DSI_CMD_LONG(0x39,NT35596_AUO_param35),
    DSI_CMD_LONG(0x39,NT35596_AUO_param36),
    DSI_CMD_LONG(0x39,NT35596_AUO_param37),
    DSI_CMD_LONG(0x39,NT35596_AUO_param38),
    DSI_CMD_LONG(0x39,NT35596_AUO_param39),
    DSI_CMD_LONG(0x39,NT35596_AUO_param40),
    DSI_CMD_LONG(0x39,NT35596_AUO_param41),
    DSI_CMD_LONG(0x39,NT35596_AUO_param42),
    DSI_CMD_LONG(0x39,NT35596_AUO_param43),
    DSI_CMD_LONG(0x39,NT35596_AUO_param44),
    DSI_CMD_LONG(0x39,NT35596_AUO_param45),
    DSI_CMD_LONG(0x39,NT35596_AUO_param46),
    DSI_CMD_LONG(0x39,NT35596_AUO_param47),
    DSI_CMD_LONG(0x39,NT35596_AUO_param48),
    DSI_CMD_LONG(0x39,NT35596_AUO_param49),
    DSI_CMD_LONG(0x39,NT35596_AUO_param50),
    DSI_CMD_LONG(0x39,NT35596_AUO_param51),
    DSI_CMD_LONG(0x39,NT35596_AUO_param52),
    DSI_CMD_LONG(0x39,NT35596_AUO_param53),
    DSI_CMD_LONG(0x39,NT35596_AUO_param54),
    DSI_CMD_LONG(0x39,NT35596_AUO_param55),
    DSI_CMD_LONG(0x39,NT35596_AUO_param56),
    DSI_CMD_LONG(0x39,NT35596_AUO_param57),
    DSI_CMD_LONG(0x39,NT35596_AUO_param58),
    DSI_CMD_LONG(0x39,NT35596_AUO_param59),
    DSI_CMD_LONG(0x39,NT35596_AUO_param60),
    DSI_CMD_LONG(0x39,NT35596_AUO_param61),
    DSI_CMD_LONG(0x39,NT35596_AUO_param62),
    DSI_CMD_LONG(0x39,NT35596_AUO_param63),
    DSI_CMD_LONG(0x39,NT35596_AUO_param64),
    DSI_CMD_LONG(0x39,NT35596_AUO_param65),
    DSI_CMD_LONG(0x39,NT35596_AUO_param66),
    DSI_CMD_LONG(0x39,NT35596_AUO_param67),
    DSI_CMD_LONG(0x39,NT35596_AUO_param68),
    DSI_CMD_LONG(0x39,NT35596_AUO_param69),
    DSI_CMD_LONG(0x39,NT35596_AUO_param70),
    DSI_CMD_LONG(0x39,NT35596_AUO_param71),
    DSI_CMD_LONG(0x39,NT35596_AUO_param72),
    DSI_CMD_LONG(0x39,NT35596_AUO_param73),
    DSI_CMD_LONG(0x39,NT35596_AUO_param74),
    DSI_CMD_LONG(0x39,NT35596_AUO_param75),
    DSI_CMD_LONG(0x39,NT35596_AUO_param76),
    DSI_CMD_LONG(0x39,NT35596_AUO_param77),
    DSI_CMD_LONG(0x39,NT35596_AUO_param78),
    DSI_CMD_LONG(0x39,NT35596_AUO_param79),
    DSI_CMD_LONG(0x39,NT35596_AUO_param80),
    DSI_CMD_LONG(0x39,NT35596_AUO_param81),
    DSI_CMD_LONG(0x39,NT35596_AUO_param82),
    DSI_CMD_LONG(0x39,NT35596_AUO_param83),
    DSI_CMD_LONG(0x39,NT35596_AUO_param84),
    DSI_CMD_LONG(0x39,NT35596_AUO_param85),
    DSI_CMD_LONG(0x39,NT35596_AUO_param86),
    DSI_CMD_LONG(0x39,NT35596_AUO_param87),
    DSI_CMD_LONG(0x39,NT35596_AUO_param88),
    DSI_CMD_LONG(0x39,NT35596_AUO_param89),
    DSI_CMD_LONG(0x39,NT35596_AUO_param90),
    DSI_CMD_LONG(0x39,NT35596_AUO_param91),
    DSI_CMD_LONG(0x39,NT35596_AUO_param92),
    DSI_CMD_LONG(0x39,NT35596_AUO_param93),
    DSI_CMD_LONG(0x39,NT35596_AUO_param94),
    DSI_CMD_LONG(0x39,NT35596_AUO_param95),
    DSI_CMD_LONG(0x39,NT35596_AUO_param96),
    DSI_CMD_LONG(0x39,NT35596_AUO_param97),
    DSI_CMD_LONG(0x39,NT35596_AUO_param98),
    DSI_CMD_LONG(0x39,NT35596_AUO_param99),
    DSI_CMD_LONG(0x39,NT35596_AUO_param100),
    DSI_CMD_LONG(0x39,NT35596_AUO_param101),
    DSI_CMD_LONG(0x39,NT35596_AUO_param102),
    DSI_CMD_LONG(0x39,NT35596_AUO_param103),
    DSI_CMD_LONG(0x39,NT35596_AUO_param104),
    DSI_CMD_LONG(0x39,NT35596_AUO_param105),
    DSI_CMD_LONG(0x39,NT35596_AUO_param106),
    DSI_CMD_LONG(0x39,NT35596_AUO_param107),
    DSI_CMD_LONG(0x39,NT35596_AUO_param108),
    DSI_CMD_LONG(0x39,NT35596_AUO_param109),
    DSI_CMD_LONG(0x39,NT35596_AUO_param110),
    DSI_CMD_LONG(0x39,NT35596_AUO_param111),
    DSI_CMD_LONG(0x39,NT35596_AUO_param112),
    DSI_CMD_LONG(0x39,NT35596_AUO_param113),
    DSI_CMD_LONG(0x39,NT35596_AUO_param114),
    DSI_CMD_LONG(0x39,NT35596_AUO_param115),
    DSI_CMD_LONG(0x39,NT35596_AUO_param116),
    DSI_CMD_LONG(0x39,NT35596_AUO_param117),
    DSI_CMD_LONG(0x39,NT35596_AUO_param118),
    DSI_CMD_LONG(0x39,NT35596_AUO_param119),
    DSI_CMD_LONG(0x39,NT35596_AUO_param120),
    DSI_CMD_LONG(0x39,NT35596_AUO_param121),
    DSI_CMD_LONG(0x39,NT35596_AUO_param122),
    DSI_CMD_LONG(0x39,NT35596_AUO_param123),
    DSI_CMD_LONG(0x39,NT35596_AUO_param124),
    DSI_CMD_LONG(0x39,NT35596_AUO_param125),
    DSI_CMD_LONG(0x39,NT35596_AUO_param126),
    DSI_CMD_LONG(0x39,NT35596_AUO_param127),
    DSI_CMD_LONG(0x39,NT35596_AUO_param128),
    DSI_CMD_LONG(0x39,NT35596_AUO_param129),
    DSI_CMD_LONG(0x39,NT35596_AUO_param130),
    DSI_CMD_LONG(0x39,NT35596_AUO_param131),
    DSI_CMD_LONG(0x39,NT35596_AUO_param132),
    DSI_CMD_LONG(0x39,NT35596_AUO_param133),
    DSI_CMD_LONG(0x39,NT35596_AUO_param134),
    DSI_CMD_LONG(0x39,NT35596_AUO_param135),
    DSI_CMD_LONG(0x39,NT35596_AUO_param136),
    DSI_CMD_LONG(0x39,NT35596_AUO_param137),
    DSI_CMD_LONG(0x39,NT35596_AUO_param138),
    DSI_CMD_LONG(0x39,NT35596_AUO_param139),
    DSI_CMD_LONG(0x39,NT35596_AUO_param140),
    DSI_CMD_LONG(0x39,NT35596_AUO_param141),
    DSI_CMD_LONG(0x39,NT35596_AUO_param142),
    DSI_CMD_LONG(0x39,NT35596_AUO_param143),
    DSI_CMD_LONG(0x39,NT35596_AUO_param144),
    DSI_CMD_LONG(0x39,NT35596_AUO_param146),
    DSI_CMD_LONG(0x39,NT35596_AUO_param147),
    DSI_CMD_LONG(0x39,NT35596_AUO_param148),
    DSI_CMD_LONG(0x39,NT35596_AUO_param149),
    DSI_CMD_LONG(0x39,NT35596_AUO_param150),
    DSI_CMD_LONG(0x39,NT35596_AUO_param151),
    DSI_CMD_LONG(0x39,NT35596_AUO_param152),
    DSI_CMD_LONG(0x39,NT35596_AUO_param153),
    DSI_CMD_LONG(0x39,NT35596_AUO_param154),
    DSI_CMD_LONG(0x39,NT35596_AUO_param155),
    DSI_CMD_LONG(0x39,NT35596_AUO_param156),
    DSI_CMD_LONG(0x39,NT35596_AUO_param157),
    DSI_CMD_LONG(0x39,NT35596_AUO_param158),
    DSI_CMD_LONG(0x39,NT35596_AUO_param159),
    DSI_CMD_LONG(0x39,NT35596_AUO_param160),
    DSI_CMD_LONG(0x39,NT35596_AUO_param161),
    DSI_CMD_LONG(0x39,NT35596_AUO_param162),
    DSI_CMD_LONG(0x39,NT35596_AUO_param163),
    DSI_CMD_LONG(0x39,NT35596_AUO_param164),
    DSI_CMD_LONG(0x39,NT35596_AUO_param165),
    DSI_CMD_LONG(0x39,NT35596_AUO_param166),
    DSI_CMD_LONG(0x39,NT35596_AUO_param167),
    DSI_CMD_LONG(0x39,NT35596_AUO_param168),
    DSI_CMD_LONG(0x39,NT35596_AUO_param169),
    DSI_CMD_LONG(0x39,NT35596_AUO_param170),
    DSI_CMD_LONG(0x39,NT35596_AUO_param171),
    DSI_CMD_LONG(0x39,NT35596_AUO_param172),
    DSI_CMD_LONG(0x39,NT35596_AUO_param173),
    DSI_CMD_LONG(0x39,NT35596_AUO_param174),
    DSI_CMD_LONG(0x39,NT35596_AUO_param175),
    DSI_CMD_LONG(0x39,NT35596_AUO_param176),
    DSI_CMD_LONG(0x39,NT35596_AUO_param177),
    DSI_CMD_LONG(0x39,NT35596_AUO_param178),
    DSI_CMD_LONG(0x39,NT35596_AUO_param179),
    DSI_CMD_LONG(0x39,NT35596_AUO_param180),
    DSI_CMD_LONG(0x39,NT35596_AUO_param181),
    DSI_CMD_LONG(0x39,NT35596_AUO_param182),
    DSI_CMD_LONG(0x39,NT35596_AUO_param183),
    DSI_CMD_LONG(0x39,NT35596_AUO_param184),
    DSI_CMD_LONG(0x39,NT35596_AUO_param185),
    DSI_CMD_LONG(0x39,NT35596_AUO_param186),
    DSI_CMD_LONG(0x39,NT35596_AUO_param187),
    DSI_CMD_LONG(0x39,NT35596_AUO_param188),
    DSI_CMD_LONG(0x39,NT35596_AUO_param189),
    DSI_CMD_LONG(0x39,NT35596_AUO_param190),
    DSI_CMD_LONG(0x39,NT35596_AUO_param191),
    DSI_CMD_LONG(0x39,NT35596_AUO_param192),
    DSI_CMD_LONG(0x39,NT35596_AUO_param193),
    DSI_CMD_LONG(0x39,NT35596_AUO_param194),
    DSI_CMD_LONG(0x39,NT35596_AUO_param195),
    DSI_CMD_LONG(0x39,NT35596_AUO_param196),
    DSI_CMD_LONG(0x39,NT35596_AUO_param197),
    DSI_CMD_LONG(0x39,NT35596_AUO_param198),
    DSI_CMD_LONG(0x39,NT35596_AUO_param199),
    DSI_CMD_LONG(0x39,NT35596_AUO_param200),
    DSI_CMD_LONG(0x39,NT35596_AUO_param201),
    DSI_CMD_LONG(0x39,NT35596_AUO_param202),
    DSI_CMD_LONG(0x39,NT35596_AUO_param203),
    DSI_CMD_LONG(0x39,NT35596_AUO_param204),
    DSI_CMD_LONG(0x39,NT35596_AUO_param205),
    DSI_CMD_LONG(0x39,NT35596_AUO_param206),
    DSI_CMD_LONG(0x39,NT35596_AUO_param207),
    DSI_CMD_LONG(0x39,NT35596_AUO_param208),
    DSI_CMD_LONG(0x39,NT35596_AUO_param209),
    DSI_CMD_LONG(0x39,NT35596_AUO_param210),
    DSI_CMD_LONG(0x39,NT35596_AUO_param211),
    DSI_CMD_LONG(0x39,NT35596_AUO_param212),
    DSI_CMD_LONG(0x39,NT35596_AUO_param213),
    DSI_CMD_LONG(0x39,NT35596_AUO_param214),
    DSI_CMD_LONG(0x39,NT35596_AUO_param215),
    DSI_CMD_LONG(0x39,NT35596_AUO_param216),
    DSI_CMD_LONG(0x39,NT35596_AUO_param217),
    DSI_CMD_LONG(0x39,NT35596_AUO_param218),
    DSI_CMD_LONG(0x39,NT35596_AUO_param219),
    DSI_CMD_LONG(0x39,NT35596_AUO_param220),
    DSI_CMD_LONG(0x39,NT35596_AUO_param221),
    DSI_CMD_LONG(0x39,NT35596_AUO_param222),
    DSI_CMD_LONG(0x39,NT35596_AUO_param223),
    DSI_CMD_LONG(0x39,NT35596_AUO_param224),
    DSI_CMD_LONG(0x39,NT35596_AUO_param225),
    DSI_CMD_LONG(0x39,NT35596_AUO_param226),
    DSI_CMD_LONG(0x39,NT35596_AUO_param227),
    DSI_CMD_LONG(0x39,NT35596_AUO_param228),
    DSI_CMD_LONG(0x39,NT35596_AUO_param229),
    DSI_CMD_LONG(0x39,NT35596_AUO_param230),
    DSI_CMD_LONG(0x39,NT35596_AUO_param231),
    DSI_CMD_LONG(0x39,NT35596_AUO_param232),
    DSI_CMD_LONG(0x39,NT35596_AUO_param233),
    DSI_CMD_LONG(0x39,NT35596_AUO_param234),
    DSI_CMD_LONG(0x39,NT35596_AUO_param235),
    DSI_CMD_LONG(0x39,NT35596_AUO_param236),
    DSI_CMD_LONG(0x39,NT35596_AUO_param237),
    DSI_CMD_LONG(0x39,NT35596_AUO_param238),
    DSI_CMD_LONG(0x39,NT35596_AUO_param239),
    DSI_CMD_LONG(0x39,NT35596_AUO_param240),
    DSI_CMD_LONG(0x39,NT35596_AUO_param241),
    DSI_CMD_LONG(0x39,NT35596_AUO_param242),
    DSI_CMD_LONG(0x39,NT35596_AUO_param243),
    DSI_CMD_LONG(0x39,NT35596_AUO_param244),
    DSI_CMD_LONG(0x39,NT35596_AUO_param245),
    DSI_CMD_LONG(0x39,NT35596_AUO_param246),
    DSI_CMD_LONG(0x39,NT35596_AUO_param247),
    DSI_CMD_LONG(0x39,NT35596_AUO_param248),
    DSI_CMD_LONG(0x39,NT35596_AUO_param249),
    DSI_CMD_LONG(0x39,NT35596_AUO_param250),
    DSI_CMD_LONG(0x39,NT35596_AUO_param251),
    DSI_CMD_LONG(0x39,NT35596_AUO_param252),
    DSI_CMD_LONG(0x39,NT35596_AUO_param253),
    DSI_CMD_LONG(0x39,NT35596_AUO_param254),
    DSI_CMD_LONG(0x39,NT35596_AUO_param255),
    DSI_CMD_LONG(0x39,NT35596_AUO_param256),
    DSI_CMD_LONG(0x39,NT35596_AUO_param257),
    DSI_CMD_LONG(0x39,NT35596_AUO_param258),
    DSI_CMD_LONG(0x39,NT35596_AUO_param259),
    DSI_CMD_LONG(0x39,NT35596_AUO_param260),
    DSI_CMD_LONG(0x39,NT35596_AUO_param261),
    DSI_CMD_LONG(0x39,NT35596_AUO_param262),
    DSI_CMD_LONG(0x39,NT35596_AUO_param263),
    DSI_CMD_LONG(0x39,NT35596_AUO_param264),
    DSI_CMD_LONG(0x39,NT35596_AUO_param265),
    DSI_CMD_LONG(0x39,NT35596_AUO_param266),
    DSI_CMD_LONG(0x39,NT35596_AUO_param267),
    DSI_CMD_LONG(0x39,NT35596_AUO_param268),
    DSI_CMD_LONG(0x39,NT35596_AUO_param269),
    DSI_CMD_LONG(0x39,NT35596_AUO_param270),
    DSI_CMD_LONG(0x39,NT35596_AUO_param271),
    DSI_CMD_LONG(0x39,NT35596_AUO_param272),
    DSI_CMD_LONG(0x39,NT35596_AUO_param273),
    DSI_CMD_LONG(0x39,NT35596_AUO_param274),
    DSI_CMD_LONG(0x39,NT35596_AUO_param275),
    DSI_CMD_LONG(0x39,NT35596_AUO_param276),
    DSI_CMD_LONG(0x39,NT35596_AUO_param277),
    DSI_CMD_LONG(0x39,NT35596_AUO_param278),
    DSI_CMD_LONG(0x39,NT35596_AUO_param279),
    DSI_CMD_LONG(0x39,NT35596_AUO_param280),
    DSI_CMD_LONG(0x39,NT35596_AUO_param281),
    DSI_CMD_LONG(0x39,NT35596_AUO_param282),
    DSI_CMD_LONG(0x39,NT35596_AUO_param283),
    DSI_CMD_LONG(0x39,NT35596_AUO_param284),
    DSI_CMD_LONG(0x39,NT35596_AUO_param285),
    DSI_CMD_LONG(0x39,NT35596_AUO_param286),
    DSI_CMD_LONG(0x39,NT35596_AUO_param287),
    DSI_CMD_LONG(0x39,NT35596_AUO_param288),
    DSI_CMD_LONG(0x39,NT35596_AUO_param289),
    DSI_CMD_LONG(0x39,NT35596_AUO_param290),
    DSI_CMD_LONG(0x39,NT35596_AUO_param291),
    DSI_CMD_LONG(0x39,NT35596_AUO_param292),
    DSI_CMD_LONG(0x39,NT35596_AUO_param293),
    DSI_CMD_LONG(0x39,NT35596_AUO_param294),
    DSI_CMD_LONG(0x39,NT35596_AUO_param295),
    DSI_CMD_LONG(0x39,NT35596_AUO_param296),
    DSI_CMD_LONG(0x39,NT35596_AUO_param297),
    DSI_CMD_LONG(0x39,NT35596_AUO_param298),
    DSI_CMD_LONG(0x39,NT35596_AUO_param299),
    DSI_CMD_LONG(0x39,NT35596_AUO_param300),
    DSI_CMD_LONG(0x39,NT35596_AUO_param301),
    DSI_CMD_LONG(0x39,NT35596_AUO_param302),
    DSI_CMD_LONG(0x39,NT35596_AUO_param303),
    DSI_CMD_LONG(0x39,NT35596_AUO_param304),
    DSI_CMD_LONG(0x39,NT35596_AUO_param305),
    DSI_CMD_LONG(0x39,NT35596_AUO_param306),
    DSI_CMD_LONG(0x39,NT35596_AUO_param307),
    DSI_CMD_LONG(0x39,NT35596_AUO_param308),
    DSI_CMD_LONG(0x39,NT35596_AUO_param309),
    DSI_CMD_LONG(0x39,NT35596_AUO_param310),
    DSI_CMD_LONG(0x39,NT35596_AUO_param311),
    DSI_CMD_LONG(0x39,NT35596_AUO_param312),
    DSI_CMD_LONG(0x39,NT35596_AUO_param313),
    DSI_CMD_LONG(0x39,NT35596_AUO_param314),
    DSI_CMD_LONG(0x39,NT35596_AUO_param315),
    DSI_CMD_LONG(0x39,NT35596_AUO_param316),
    DSI_CMD_LONG(0x39,NT35596_AUO_param317),
    DSI_CMD_LONG(0x39,NT35596_AUO_param318),
    DSI_CMD_LONG(0x39,NT35596_AUO_param319),
    DSI_CMD_LONG(0x39,NT35596_AUO_param320),
    DSI_CMD_LONG(0x39,NT35596_AUO_param321),
    DSI_CMD_LONG(0x39,NT35596_AUO_param322),
    DSI_CMD_LONG(0x39,NT35596_AUO_param323),
    DSI_CMD_LONG(0x39,NT35596_AUO_param324),
    DSI_CMD_LONG(0x39,NT35596_AUO_param325),
    DSI_CMD_LONG(0x39,NT35596_AUO_param326),
    DSI_CMD_LONG(0x39,NT35596_AUO_param327),
    DSI_CMD_LONG(0x39,NT35596_AUO_param328),
    DSI_CMD_LONG(0x39,NT35596_AUO_param329),
    DSI_CMD_LONG(0x39,NT35596_AUO_param330),
    DSI_CMD_LONG(0x39,NT35596_AUO_param331),
    DSI_CMD_LONG(0x39,NT35596_AUO_param332),
    DSI_CMD_LONG(0x39,NT35596_AUO_param333),
    DSI_CMD_LONG(0x39,NT35596_AUO_param334),
    DSI_CMD_LONG(0x39,NT35596_AUO_param335),
    DSI_CMD_LONG(0x39,NT35596_AUO_param336),
    DSI_CMD_LONG(0x39,NT35596_AUO_param337),
    DSI_CMD_LONG(0x39,NT35596_AUO_param338),
    DSI_CMD_LONG(0x39,NT35596_AUO_param339),
    DSI_CMD_LONG(0x39,NT35596_AUO_param340),
    DSI_CMD_LONG(0x39,NT35596_AUO_param341),
    DSI_CMD_LONG(0x39,NT35596_AUO_param342),
    DSI_CMD_LONG(0x39,NT35596_AUO_param343),
    DSI_CMD_LONG(0x39,NT35596_AUO_param344),
    DSI_CMD_LONG(0x39,NT35596_AUO_param345),
    DSI_CMD_LONG(0x39,NT35596_AUO_param346),
    DSI_CMD_LONG(0x39,NT35596_AUO_param347),
    DSI_CMD_LONG(0x39,NT35596_AUO_param348),
    DSI_CMD_LONG(0x39,NT35596_AUO_param349),
    DSI_CMD_LONG(0x39,NT35596_AUO_param350),
    DSI_CMD_LONG(0x39,NT35596_AUO_param351),
    DSI_CMD_LONG(0x39,NT35596_AUO_param352),
    DSI_CMD_LONG(0x39,NT35596_AUO_param353),
    DSI_CMD_LONG(0x39,NT35596_AUO_param354),
    DSI_CMD_LONG(0x39,NT35596_AUO_param355),
    DSI_CMD_LONG(0x39,NT35596_AUO_param356),
    DSI_CMD_LONG(0x39,NT35596_AUO_param357),
    DSI_CMD_LONG(0x39,NT35596_AUO_param358),
    DSI_CMD_LONG(0x39,NT35596_AUO_param359),
    DSI_CMD_LONG(0x39,NT35596_AUO_param360),
    DSI_CMD_LONG(0x39,NT35596_AUO_param361),
    DSI_CMD_LONG(0x39,NT35596_AUO_param362),
    DSI_CMD_LONG(0x39,NT35596_AUO_param363),
    DSI_CMD_LONG(0x39,NT35596_AUO_param364),
    DSI_CMD_LONG(0x39,NT35596_AUO_param365),
    DSI_CMD_LONG(0x39,NT35596_AUO_param366),
    DSI_CMD_LONG(0x39,NT35596_AUO_param367),
    DSI_CMD_LONG(0x39,NT35596_AUO_param368),
    DSI_CMD_LONG(0x39,NT35596_AUO_param369),
    DSI_CMD_LONG(0x39,NT35596_AUO_param370),
    DSI_CMD_LONG(0x39,NT35596_AUO_param371),
    DSI_CMD_LONG(0x39,NT35596_AUO_param372),
    DSI_CMD_LONG(0x39,NT35596_AUO_param373),
    DSI_CMD_LONG(0x39,NT35596_AUO_param374),
    DSI_CMD_LONG(0x39,NT35596_AUO_param375),
    DSI_CMD_LONG(0x39,NT35596_AUO_param376),
    DSI_CMD_LONG(0x39,NT35596_AUO_param377),
    DSI_CMD_LONG(0x39,NT35596_AUO_param378),
    DSI_CMD_LONG(0x39,NT35596_AUO_param379),
    DSI_CMD_LONG(0x39,NT35596_AUO_param380),
    DSI_CMD_LONG(0x39,NT35596_AUO_param381),
    DSI_CMD_LONG(0x39,NT35596_AUO_param382),
    DSI_CMD_LONG(0x39,NT35596_AUO_param383),
    DSI_CMD_LONG(0x39,NT35596_AUO_param384),
    DSI_CMD_LONG(0x39,NT35596_AUO_param385),
    DSI_CMD_LONG(0x39,NT35596_AUO_param386),
    DSI_CMD_LONG(0x39,NT35596_AUO_param387),
    DSI_CMD_LONG(0x39,NT35596_AUO_param388),
    DSI_CMD_LONG(0x39,NT35596_AUO_param389),
    DSI_CMD_LONG(0x39,NT35596_AUO_param390),
    DSI_CMD_LONG(0x39,NT35596_AUO_param391),
    DSI_CMD_LONG(0x39,NT35596_AUO_param392),
    DSI_CMD_LONG(0x39,NT35596_AUO_param393),
    DSI_CMD_LONG(0x39,NT35596_AUO_param394),
    DSI_CMD_LONG(0x39,NT35596_AUO_param395),
    DSI_CMD_LONG(0x39,NT35596_AUO_param396),
    DSI_CMD_LONG(0x39,NT35596_AUO_param397),
    DSI_CMD_LONG(0x39,NT35596_AUO_param398),
    DSI_CMD_LONG(0x39,NT35596_AUO_param399),
    DSI_CMD_LONG(0x39,NT35596_AUO_param400),
    DSI_CMD_LONG(0x39,NT35596_AUO_param401),
    DSI_CMD_LONG(0x39,NT35596_AUO_param402),
    DSI_CMD_LONG(0x39,NT35596_AUO_param403),
    DSI_CMD_LONG(0x39,NT35596_AUO_param404),
    DSI_CMD_LONG(0x39,NT35596_AUO_param405),
    DSI_CMD_LONG(0x39,NT35596_AUO_param406),
    DSI_CMD_LONG(0x39,NT35596_AUO_param407),
    DSI_CMD_LONG(0x39,NT35596_AUO_param408),
    DSI_CMD_LONG(0x39,NT35596_AUO_param409),
    DSI_CMD_LONG(0x39,NT35596_AUO_param410),
    DSI_CMD_LONG(0x39,NT35596_AUO_param411),
    DSI_CMD_LONG(0x39,NT35596_AUO_param412),
    DSI_CMD_LONG(0x39,NT35596_AUO_param413),
    DSI_CMD_LONG(0x39,NT35596_AUO_param414),
    DSI_CMD_LONG(0x39,NT35596_AUO_param415),
    DSI_CMD_LONG(0x39,NT35596_AUO_param416),
    DSI_CMD_LONG(0x39,NT35596_AUO_param417),
    DSI_CMD_LONG(0x39,NT35596_AUO_param418),
    DSI_CMD_LONG(0x39,NT35596_AUO_param419),
    DSI_CMD_LONG(0x39,NT35596_AUO_param420),
    DSI_CMD_LONG(0x39,NT35596_AUO_param421),
    DSI_CMD_LONG(0x39,NT35596_AUO_param422),
    DSI_CMD_LONG(0x39,NT35596_AUO_param423),
    DSI_CMD_LONG(0x39,NT35596_AUO_param424),
    DSI_CMD_LONG(0x39,NT35596_AUO_param425),
    DSI_CMD_LONG(0x39,NT35596_AUO_param426),
    DSI_CMD_LONG(0x39,NT35596_AUO_param427),
    DSI_CMD_LONG(0x39,NT35596_AUO_param428),
    DSI_CMD_LONG(0x39,NT35596_AUO_param429),
    DSI_CMD_LONG(0x39,NT35596_AUO_param430),
    DSI_CMD_LONG(0x39,NT35596_AUO_param431),
    DSI_CMD_LONG(0x39,NT35596_AUO_param432),
    DSI_CMD_LONG(0x39,NT35596_AUO_param433),
    DSI_CMD_LONG(0x39,NT35596_AUO_param434),
    DSI_CMD_LONG(0x39,NT35596_AUO_param435),
    DSI_CMD_LONG(0x39,NT35596_AUO_param436),
    DSI_CMD_LONG(0x39,NT35596_AUO_param437),
    DSI_CMD_LONG(0x39,NT35596_AUO_param438),
    DSI_CMD_LONG(0x39,NT35596_AUO_param439),
    DSI_CMD_LONG(0x39,NT35596_AUO_param440),
    DSI_CMD_LONG(0x39,NT35596_AUO_param441),
    DSI_CMD_LONG(0x39,NT35596_AUO_param442),
    DSI_CMD_LONG(0x39,NT35596_AUO_param443),
    DSI_CMD_LONG(0x39,NT35596_AUO_param444),
    DSI_CMD_LONG(0x39,NT35596_AUO_param445),
    DSI_CMD_LONG(0x39,NT35596_AUO_param446),
    DSI_CMD_LONG(0x39,NT35596_AUO_param447),
    DSI_CMD_LONG(0x39,NT35596_AUO_param448),
    DSI_CMD_LONG(0x39,NT35596_AUO_param449),
    DSI_CMD_LONG(0x39,NT35596_AUO_param450),
    DSI_CMD_LONG(0x39,NT35596_AUO_param451),
    DSI_CMD_LONG(0x39,NT35596_AUO_param452),
    DSI_CMD_LONG(0x39,NT35596_AUO_param453),
    DSI_CMD_LONG(0x39,NT35596_AUO_param454),
    DSI_CMD_LONG(0x39,NT35596_AUO_param455),
    DSI_CMD_LONG(0x39,NT35596_AUO_param456),
    DSI_CMD_LONG(0x39,NT35596_AUO_param457),
    DSI_CMD_LONG(0x39,NT35596_AUO_param458),
    DSI_CMD_LONG(0x39,NT35596_AUO_param459),
    DSI_CMD_LONG(0x39,NT35596_AUO_param460),
    DSI_CMD_LONG(0x39,NT35596_AUO_param461),
    DSI_CMD_LONG(0x39,NT35596_AUO_param462),
    DSI_CMD_LONG(0x39,NT35596_AUO_param463),
    DSI_CMD_LONG(0x39,NT35596_AUO_param464),
    DSI_CMD_LONG(0x39,NT35596_AUO_param465),
    DSI_CMD_LONG(0x39,NT35596_AUO_param466),
    DSI_CMD_LONG(0x39,NT35596_AUO_param467),
    DSI_CMD_LONG(0x39,NT35596_AUO_param468),
    DSI_CMD_LONG(0x39,NT35596_AUO_param469),
    DSI_CMD_LONG(0x39,NT35596_AUO_param470),
    DSI_CMD_LONG(0x39,NT35596_AUO_param471),
    DSI_CMD_LONG(0x39,NT35596_AUO_param472),
    DSI_CMD_LONG(0x39,NT35596_AUO_param473),
    DSI_CMD_LONG(0x39,NT35596_AUO_param474),
    DSI_CMD_LONG(0x39,NT35596_AUO_param475),
    DSI_CMD_LONG(0x39,NT35596_AUO_param476),
    DSI_CMD_LONG(0x39,NT35596_AUO_param477),
    DSI_CMD_LONG(0x39,NT35596_AUO_param478),
    DSI_CMD_LONG(0x39,NT35596_AUO_param479),
    DSI_CMD_LONG(0x39,NT35596_AUO_param480),
    DSI_CMD_LONG(0x39,NT35596_AUO_param481),
    DSI_CMD_LONG(0x39,NT35596_AUO_param482),
    DSI_CMD_LONG(0x39,NT35596_AUO_param483),
    DSI_CMD_LONG(0x39,NT35596_AUO_param484),
    DSI_CMD_LONG(0x39,NT35596_AUO_param485),
    DSI_CMD_LONG(0x39,NT35596_AUO_param486),
    DSI_CMD_LONG(0x39,NT35596_AUO_param487),
    DSI_CMD_LONG(0x39,NT35596_AUO_param488),
    DSI_CMD_LONG(0x39,NT35596_AUO_param489),
    DSI_CMD_LONG(0x39,NT35596_AUO_param490),
    DSI_CMD_LONG(0x39,NT35596_AUO_param491),
    DSI_CMD_LONG(0x39,NT35596_AUO_param492),
    DSI_CMD_LONG(0x39,NT35596_AUO_param493),
    DSI_CMD_LONG(0x39,NT35596_AUO_param494),
    DSI_CMD_LONG(0x39,NT35596_AUO_param495),
    DSI_CMD_LONG(0x39,NT35596_AUO_param496),
    DSI_CMD_LONG(0x39,NT35596_AUO_param497),
    DSI_CMD_LONG(0x39,NT35596_AUO_param498),
    DSI_CMD_LONG(0x39,NT35596_AUO_param499),
    DSI_CMD_LONG(0x39,NT35596_AUO_param500),
    DSI_CMD_LONG(0x39,NT35596_AUO_param501),
    DSI_CMD_LONG(0x39,NT35596_AUO_param502),
    DSI_CMD_LONG(0x39,NT35596_AUO_param503),
    DSI_CMD_LONG(0x39,NT35596_AUO_param504),
    DSI_CMD_LONG(0x39,NT35596_AUO_param505),
    DSI_CMD_LONG(0x39,NT35596_AUO_param506),
    DSI_CMD_LONG(0x39,NT35596_AUO_param507),
    DSI_CMD_LONG(0x39,NT35596_AUO_param508),
#ifdef NT35596_AUO_COLOR_ENHANCE
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param0),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param1),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param2),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param3),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param4),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param5),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param6),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param7),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param8),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param9),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param10),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param11),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param12),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param13),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param14),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param15),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param16),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param17),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param18),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param19),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param20),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param21),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param22),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param23),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param24),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param25),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param26),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param27),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param28),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param29),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param30),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param31),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param32),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param33),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param34),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param35),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param36),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param37),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param38),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param39),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param40),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param41),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param42),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param43),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param44),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param45),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param46),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param47),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param48),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param49),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param50),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param51),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param52),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param53),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param54),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param55),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param56),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param57),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param58),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param59),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param60),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param61),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param62),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param63),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param64),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param65),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param66),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param67),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param68),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param69),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param70),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param71),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param72),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param73),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param74),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param75),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param76),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param77),
    DSI_CMD_LONG(0x39,NT35596_AUO_color_enhance_param78),
#endif
    DSI_CMD_LONG(0x39,NT35596_AUO_param510),
    DSI_CMD_LONG(0x39,NT35596_AUO_param511),
    DSI_CMD_LONG(0x39,NT35596_AUO_param512),
    DSI_CMD_LONG(0x39,NT35596_AUO_param513),
    DSI_CMD_SHORT(0x05, 0x11, 0x00),
	DSI_DLY_MS(80),      /* modify  by liyibo for wake up delay 2014-03-18*/
	DSI_CMD_SHORT(0x15, 0xff, 0x00),
#if CONFIG_ESD_READ_TE
	DSI_CMD_SHORT(0x15, 0x35, 0x00),
#endif
	DSI_CMD_SHORT(0x05, 0x29, 0x00),
	DSI_DLY_MS(10),
};

static struct tegra_dsi_cmd dsi_suspend_cmd[] = {
    //DSI_CMD_SHORT(0x05, 0x22, 0x00),
    //DSI_DLY_MS(50),
	DSI_CMD_SHORT(0x05, 0x28, 0x00),
	DSI_DLY_MS(100),
	DSI_CMD_SHORT(0x05, 0x10, 0x00),
	DSI_DLY_MS(120),
};


static struct tegra_dsi_out dsi_yushun_1080p_5_pdata = {
	.n_data_lanes = 4,

	.dsi_instance = DSI_INSTANCE_0,

	.refresh_rate = 60,
	.video_data_type = TEGRA_DSI_VIDEO_TYPE_VIDEO_MODE,
	.video_clock_mode = TEGRA_DSI_VIDEO_CLOCK_CONTINUOUS,
	.video_burst_mode = TEGRA_DSI_VIDEO_NONE_BURST_MODE_WITH_SYNC_END,

	.controller_vs = DSI_VS_1,
	.pixel_format = TEGRA_DSI_PIXEL_FORMAT_24BIT_P,
	.virtual_channel = TEGRA_DSI_VIRTUAL_CHANNEL_0,

	.panel_reset = DSI_PANEL_RESET,
	.power_saving_suspend = true,
	.dsi_init_cmd = NT35596_AUO_dsi_init_cmd,
	.n_init_cmd = ARRAY_SIZE(NT35596_AUO_dsi_init_cmd),
	.n_suspend_cmd = ARRAY_SIZE(dsi_suspend_cmd),
	.dsi_suspend_cmd = dsi_suspend_cmd,
	#if CONFIG_ESD_READ_TE
    .n_reset_cmd = ARRAY_SIZE(NT35596_AUO_dsi_init_cmd),
    .dsi_reset_cmd = NT35596_AUO_dsi_init_cmd,
    #endif
};

static void dsi_yushun_1080p_5_resources_init(struct resource *
resources, int n_resources)
{
	int i;
	for (i = 0; i < n_resources; i++) {
		struct resource *r = &resources[i];
		if (resource_type(r) == IORESOURCE_MEM &&
			!strcmp(r->name, "dsi_regs")) {
			r->start = TEGRA_DSI_BASE;
			r->end = TEGRA_DSI_BASE + TEGRA_DSI_SIZE - 1;
		}
	}
}

static void dsi_yushun_1080p_5_dc_out_init(struct tegra_dc_out *dc)
{
	dc->dsi = &dsi_yushun_1080p_5_pdata;
	dc->parent_clk = "pll_d_out0";
	dc->modes = dsi_yushun_1080p_5_modes;
	dc->n_modes = ARRAY_SIZE(dsi_yushun_1080p_5_modes);
	dc->enable = dsi_yushun_1080p_5_enable;
	dc->disable = dsi_yushun_1080p_5_disable;
	dc->postsuspend	= dsi_yushun_1080p_5_postsuspend,
	dc->width = 62;
	dc->height = 110;
	dc->flags = DC_CTRL_MODE;
}
static void dsi_yushun_1080p_5_fb_data_init(struct tegra_fb_data *fb)
{
	fb->xres = dsi_yushun_1080p_5_modes[0].h_active;
	fb->yres = dsi_yushun_1080p_5_modes[0].v_active;
}

static void dsi_yushun_1080p_5_sd_settings_init(struct tegra_dc_sd_settings *settings)
{
	settings->bl_device_name = "pwm-backlight";
}

static void dsi_yushun_1080p_5_set_disp_device(
	struct platform_device *display_device)
{
	disp_device = display_device;
}

struct tegra_panel __initdata dsi_yushun_1080p_5 = {
	.init_sd_settings = dsi_yushun_1080p_5_sd_settings_init,
	.init_dc_out = dsi_yushun_1080p_5_dc_out_init,
	.init_fb_data = dsi_yushun_1080p_5_fb_data_init,
	.init_resources = dsi_yushun_1080p_5_resources_init,
	.register_bl_dev = dsi_yushun_1080p_5_register_bl_dev,
	.set_disp_device = dsi_yushun_1080p_5_set_disp_device,
};
EXPORT_SYMBOL(dsi_yushun_1080p_5);

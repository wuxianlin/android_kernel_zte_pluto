/*
 * arch/arm/mach-tegra/panel-cmi-720p-5.c
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
#include "NT35590.h"


#define DSI_PANEL_RESET         1
#define DSI_PANEL_RST_GPIO      TEGRA_GPIO_PH5
#define DSI_PANEL_BL_PWM        TEGRA_GPIO_PH1
#define DC_CTRL_MODE            (TEGRA_DC_OUT_CONTINUOUS_MODE|TEGRA_DC_OUT_INITIALIZED_MODE)

static struct regulator *avdd_lcd_3v0_2v8;
static bool dsi_auo_720p_5_reg_requested;
static bool dsi_auo_720p_5_gpio_requested;
static bool is_bl_powered;

static struct platform_device *disp_device;
static int initlcdflag = 0;
#if CONFIG_ESD_READ_TE
extern void te_set_SuspendFlag(int flag);
extern void te_set_WatchdogTimer(int mSecs);
#endif

static int __maybe_unused dsi_auo_720p_5_bl_notify(struct device *unused,
							int brightness)
{
	//int cur_sd_brightness = atomic_read(&sd_brightness);

	/* Apply any backlight response curve */
	if (brightness > 255)
		pr_info("Error: Brightness > 255!\n");
	return brightness;
}

static bool __maybe_unused dsi_auo_720p_5_check_bl_power(void)
{
	return is_bl_powered;
}

static int __maybe_unused dsi_auo_720p_5_check_fb(struct device *dev,
					     struct fb_info *info)
{
	return info->device == &disp_device->dev;
}

/*
	jingdongfang uses pwm blacklight device
*/
static struct platform_pwm_backlight_data dsi_auo_720p_5_bl_data = {
	.pwm_id         = 1,
	.max_brightness = 255,
	.dft_brightness = 132,
	.pwm_period_ns  = 40000,
	.notify         = dsi_auo_720p_5_bl_notify,
	/* Only toggle backlight on fb blank notifications for disp1 */
	.check_fb       = dsi_auo_720p_5_check_fb,
    .edp_states = {886, 556, 544, 452, 374, 302, 216, 172, 112, 68, 0}, /* mW. P040T30 LCM 2013-6-4*/
    .edp_brightness = {255, 230, 204, 179, 153, 128, 102, 77, 51, 19, 0}, /* brightness */
};

static struct platform_device dsi_auo_720p_5_bl_device = {
	.name   = "pwm-backlight",
	.id     = -1,
	.dev    = {
		.platform_data = &dsi_auo_720p_5_bl_data,
	},
};

static struct platform_device __maybe_unused
			*dsi_auo_720p_5_bl_devices[] __initdata = {
	&tegra_pwfm1_device,
	&dsi_auo_720p_5_bl_device,
};

static int __init dsi_auo_720p_5_register_bl_dev(void)
{
	int err = 0;
	err = platform_add_devices(dsi_auo_720p_5_bl_devices,
				ARRAY_SIZE(dsi_auo_720p_5_bl_devices));
	if (err) {
		pr_err("disp1 bl device registration failed");
		return err;
	}
	return err;
}

struct tegra_dc_mode dsi_auo_720p_5_modes[] = {
	{
		.pclk = 66700000,
		.h_ref_to_sync = 4,
		.v_ref_to_sync = 1,
		.h_sync_width = 10,
		.v_sync_width = 3,
		.h_back_porch = 60,
		.v_back_porch = 11,
		.h_active = 720,
		.v_active = 1280,
		.h_front_porch = 60,
		.v_front_porch = 8,
	},
};

static int dsi_auo_720p_5_reg_get(struct device *dev)
{
	int err = 0;

	if (dsi_auo_720p_5_reg_requested)
		return 0;

	avdd_lcd_3v0_2v8 = regulator_get(dev, "avdd_lcd");
	if (IS_ERR_OR_NULL(avdd_lcd_3v0_2v8)) {
		pr_err("avdd_lcd regulator get failed\n");
		err = PTR_ERR(avdd_lcd_3v0_2v8);
		avdd_lcd_3v0_2v8 = NULL;
		goto fail;
	}

	dsi_auo_720p_5_reg_requested = true;
	return 0;
fail:
	return err;
}

static int dsi_auo_720p_5_gpio_get(void)
{
	int err = 0;

	if (dsi_auo_720p_5_gpio_requested)
		return 0;

	err = gpio_request(DSI_PANEL_RST_GPIO, "panel rst");
	if (err < 0) {
		pr_err("panel reset gpio request failed err =%d \n",err);
		goto fail;
	}
	dsi_auo_720p_5_gpio_requested = true;
	return 0;
fail:
	return err;
}

static int dsi_auo_720p_5_enable(struct device *dev)
{
	int err = 0;

	err = dsi_auo_720p_5_reg_get(dev);
	if (err < 0) {
		pr_err("dsi regulator get failed\n");
		goto fail;
	}

	err = dsi_auo_720p_5_gpio_get();
	if (err < 0) {
		pr_err("dsi gpio request failed\n");
		goto fail;
	}
#if DSI_PANEL_RESET
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
	gpio_set_value(DSI_PANEL_RST_GPIO, 1);
	/*delay(10);
	gpio_set_value(DSI_PANEL_RST_GPIO, 0);
	mdelay(10);
	gpio_set_value(DSI_PANEL_RST_GPIO, 1);
	mdelay(120);*/
	msleep(20);
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

static int dsi_auo_720p_5_disable(void)
{
#if CONFIG_ESD_READ_TE
    te_set_SuspendFlag(1);
#endif
	is_bl_powered = false;

	gpio_set_value(DSI_PANEL_RST_GPIO, 0);
    mdelay(100);
    gpio_set_value(DSI_PANEL_RST_GPIO, 1);
    mdelay(120);
	/*if (avdd_lcd_3v0_2v8)
		regulator_disable(avdd_lcd_3v0_2v8);
    usleep_range(3000, 5000);
	gpio_set_value(pluto_LCD_1V8_EN,0);*/
	return 0;
}

static int dsi_auo_720p_5_postsuspend(void)
{
	return 0;
}

static struct tegra_dsi_cmd NT35590_AUO_dsi_init_cmd[]= {
    DSI_CMD_LONG(0x39,NT35590_AUO_Param1),  
    DSI_CMD_LONG(0x39,NT35590_AUO_Param2),  
    DSI_CMD_LONG(0x39,NT35590_AUO_Param3),  
    DSI_CMD_LONG(0x39,NT35590_AUO_Param4),  
    DSI_CMD_LONG(0x39,NT35590_AUO_Param5),  
    DSI_CMD_LONG(0x39,NT35590_AUO_Param6),  
    DSI_CMD_LONG(0x39,NT35590_AUO_Param7),  
    DSI_CMD_LONG(0x39,NT35590_AUO_Param8),  
    DSI_CMD_LONG(0x39,NT35590_AUO_Param9),  
    DSI_CMD_LONG(0x39,NT35590_AUO_Param10), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param11), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param12), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param13), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param14), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param15), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param16), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param17), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param18), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param19), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param20), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param21), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param22), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param23), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param24), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param25), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param26), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param27), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param28), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param29), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param30), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param31), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param32), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param33), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param34), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param35), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param36), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param37), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param38), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param39), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param40), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param41), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param42), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param43), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param44), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param45), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param46), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param47), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param48), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param49), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param50), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param51), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param52), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param53), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param54), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param55), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param56), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param57), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param58), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param59), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param60), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param61), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param62), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param63), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param64), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param65), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param66), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param67), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param68), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param69), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param70), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param71), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param72), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param73), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param74), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param75), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param76), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param77), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param78), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param79), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param80), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param81), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param82), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param83), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param84), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param85), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param86), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param87), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param88), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param89), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param90), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param91), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param92), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param93), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param94), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param95), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param96), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param97), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param98), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param99), 
    DSI_CMD_LONG(0x39,NT35590_AUO_Param100),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param101),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param102),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param103),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param104),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param105),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param106),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param107),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param108),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param109),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param110),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param111),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param112),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param113),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param114),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param115),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param116),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param117),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param118),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param119),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param120),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param121),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param122),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param123),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param124),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param125),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param126),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param127),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param128),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param129),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param130),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param131),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param132),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param133),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param134),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param135),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param136),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param137),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param138),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param139),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param140),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param141),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param142),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param143),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param144),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param145),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param146),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param147),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param148),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param149),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param150),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param151),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param152),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param153),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param154),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param155),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param156),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param157),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param158),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param159),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param160),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param161),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param162),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param163),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param164),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param165),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param166),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param167),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param168),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param169),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param170),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param171),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param172),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param173),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param174),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param175),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param176),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param177),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param178),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param179),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param180),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param181),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param182),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param183),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param184),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param185),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param186),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param187),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param188),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param189),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param190),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param191),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param192),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param193),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param194),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param195),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param196),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param197),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param198),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param199),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param200),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param201),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param202),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param203),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param204),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param205),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param206),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param207),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param208),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param209),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param210),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param211),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param212),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param213),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param214),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param215),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param216),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param217),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param218),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param219),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param220),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param221),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param222),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param223),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param224),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param225),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param226),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param227),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param228),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param229),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param230),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param231),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param232),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param233),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param234),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param235),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param236),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param237),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param238),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param239),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param240),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param241),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param242),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param243),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param244),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param245),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param246),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param247),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param248),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param249),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param250),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param251),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param252),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param253),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param254),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param255),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param256),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param257),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param258),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param259),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param260),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param261),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param262),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param263),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param264),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param265),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param266),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param267),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param268),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param269),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param270),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param271),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param272),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param273),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param274),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param275),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param276),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param277),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param278),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param279),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param280),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param281),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param282),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param283),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param284),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param285),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param286),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param287),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param288),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param289),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param290),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param291),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param292),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param293),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param294),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param295),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param296),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param297),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param298),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param299),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param300),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param301),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param302),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param303),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param304),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param305),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param306),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param307),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param308),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param309),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param310),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param311),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param312),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param313),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param314),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param315),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param316),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param317),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param318),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param319),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param320),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param321),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param322),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param323),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param324),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param325),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param326),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param327),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param328),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param329),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param330),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param331),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param332),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param333),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param334),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param335),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param336),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param337),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param338),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param339),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param340),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param341),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param342),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param343),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param344),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param345),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param346),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param347),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param348),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param349),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param350),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param351),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param352),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param353),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param354),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param355),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param356),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param357),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param358),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param359),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param360),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param361),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param362),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param363),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param364),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param365),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param366),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param367),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param368),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param369),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param370),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param371),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param372),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param373),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param374),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param375),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param376),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param377),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param378),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param379),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param380),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param381),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param382),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param383),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param384),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param385),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param386),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param387),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param388),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param389),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param390),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param391),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param392),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param393),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param394),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param395),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param396),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param397),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param398),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param399),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param400),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param401),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param402),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param403),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param404),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param405),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param406),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param407),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param408),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param409),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param410),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param411),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param412),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param413),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param414),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param415),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param416),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param417),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param418),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param419),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param420),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param421),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param422),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param423),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param424),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param425),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param426),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param427),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param428),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param429),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param430),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param431),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param432),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param433),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param434),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param435),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param436),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param437),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param438),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param439),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param440),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param441),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param442),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param443),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param444),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param445),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param446),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param447),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param448),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param449),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param450),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param451),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param452),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param453),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param454),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param455),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param456),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param457),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param458),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param459),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param460),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param461),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param462),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param463),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param464),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param465),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param466),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param467),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param468),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param469),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param470),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param471),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param472),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param473),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param474),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param475),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param476),
    DSI_CMD_LONG(0x39,NT35590_AUO_Param477),
    DSI_CMD_SHORT(0x05, 0x11, 0x00),
	DSI_DLY_MS(120),
#if CONFIG_ESD_READ_TE
	DSI_CMD_SHORT(0x15, 0x35, 0x00),
#endif
	DSI_CMD_SHORT(0x05, 0x29, 0x00),
	DSI_DLY_MS(10),
};

static struct tegra_dsi_cmd dsi_suspend_cmd[] = {
    DSI_CMD_SHORT(0x05, 0x28, 0x00),
    DSI_DLY_MS(50),
	DSI_CMD_SHORT(0x05, 0x34, 0x00),
	DSI_CMD_SHORT(0x05, 0x10, 0x00),
	DSI_DLY_MS(200),
};


static struct tegra_dsi_out dsi_auo_720p_5_pdata = {
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
	.dsi_init_cmd = NT35590_AUO_dsi_init_cmd,
	.n_init_cmd = ARRAY_SIZE(NT35590_AUO_dsi_init_cmd),
	.n_suspend_cmd = ARRAY_SIZE(dsi_suspend_cmd),
	.dsi_suspend_cmd = dsi_suspend_cmd,
	#if CONFIG_ESD_READ_TE
    .n_reset_cmd = ARRAY_SIZE(NT35590_AUO_dsi_init_cmd),
    .dsi_reset_cmd = NT35590_AUO_dsi_init_cmd,
    #endif
};

static void dsi_auo_720p_5_resources_init(struct resource *
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

static void dsi_auo_720p_5_dc_out_init(struct tegra_dc_out *dc)
{
	dc->dsi = &dsi_auo_720p_5_pdata;
	dc->parent_clk = "pll_d_out0";
	dc->modes = dsi_auo_720p_5_modes;
	dc->n_modes = ARRAY_SIZE(dsi_auo_720p_5_modes);
	dc->enable = dsi_auo_720p_5_enable;
	dc->disable = dsi_auo_720p_5_disable;
	dc->postsuspend	= dsi_auo_720p_5_postsuspend,
	dc->width = 62;
	dc->height = 110;
	dc->flags = DC_CTRL_MODE;
}
static void dsi_auo_720p_5_fb_data_init(struct tegra_fb_data *fb)
{
	fb->xres = dsi_auo_720p_5_modes[0].h_active;
	fb->yres = dsi_auo_720p_5_modes[0].v_active;
}

static void dsi_auo_720p_5_sd_settings_init(struct tegra_dc_sd_settings *settings)
{
	settings->bl_device_name = "pwm-backlight";
}

static void dsi_auo_720p_5_set_disp_device(
	struct platform_device *display_device)
{
	disp_device = display_device;
}

struct tegra_panel __initdata dsi_auo_720p_5 = {
	.init_sd_settings = dsi_auo_720p_5_sd_settings_init,
	.init_dc_out = dsi_auo_720p_5_dc_out_init,
	.init_fb_data = dsi_auo_720p_5_fb_data_init,
	.init_resources = dsi_auo_720p_5_resources_init,
	.register_bl_dev = dsi_auo_720p_5_register_bl_dev,
	.set_disp_device = dsi_auo_720p_5_set_disp_device,
};
EXPORT_SYMBOL(dsi_auo_720p_5);

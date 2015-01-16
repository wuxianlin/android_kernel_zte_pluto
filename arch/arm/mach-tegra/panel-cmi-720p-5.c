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
static bool dsi_cmi_720p_5_reg_requested;
static bool dsi_cmi_720p_5_gpio_requested;
static bool is_bl_powered;

static struct platform_device *disp_device;
static int initlcdflag = 0;
#if CONFIG_ESD_READ_TE
extern void te_set_SuspendFlag(int flag);
extern void te_set_WatchdogTimer(int mSecs);
#endif

static int __maybe_unused dsi_cmi_720p_5_bl_notify(struct device *unused,
							int brightness)
{
	//int cur_sd_brightness = atomic_read(&sd_brightness);

	/* Apply any backlight response curve */
	if (brightness > 255)
		pr_info("Error: Brightness > 255!\n");
	return brightness;
}

static bool __maybe_unused dsi_cmi_720p_5_check_bl_power(void)
{
	return is_bl_powered;
}

static int __maybe_unused dsi_cmi_720p_5_check_fb(struct device *dev,
					     struct fb_info *info)
{
	return info->device == &disp_device->dev;
}

/*
	jingdongfang uses pwm blacklight device
*/
static struct platform_pwm_backlight_data dsi_cmi_720p_5_bl_data = {
	.pwm_id         = 1,
	.max_brightness = 255,
	.dft_brightness = 132,
	.pwm_period_ns  = 40000,
	.notify         = dsi_cmi_720p_5_bl_notify,
	/* Only toggle backlight on fb blank notifications for disp1 */
	.check_fb       = dsi_cmi_720p_5_check_fb,
    .edp_states = {886, 556, 544, 452, 374, 302, 216, 172, 112, 68, 0}, /* mW. P040T30 LCM 2013-6-4*/
    .edp_brightness = {255, 230, 204, 179, 153, 128, 102, 77, 51, 19, 0}, /* brightness */
};

static struct platform_device dsi_cmi_720p_5_bl_device = {
	.name   = "pwm-backlight",
	.id     = -1,
	.dev    = {
		.platform_data = &dsi_cmi_720p_5_bl_data,
	},
};

static struct platform_device __maybe_unused
			*dsi_cmi_720p_5_bl_devices[] __initdata = {
	&tegra_pwfm1_device,
	&dsi_cmi_720p_5_bl_device,
};

static int __init dsi_cmi_720p_5_register_bl_dev(void)
{
	int err = 0;
	err = platform_add_devices(dsi_cmi_720p_5_bl_devices,
				ARRAY_SIZE(dsi_cmi_720p_5_bl_devices));
	if (err) {
		pr_err("disp1 bl device registration failed");
		return err;
	}
	return err;
}

struct tegra_dc_mode dsi_cmi_720p_5_modes[] = {
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

static int dsi_cmi_720p_5_reg_get(struct device *dev)
{
	int err = 0;

	if (dsi_cmi_720p_5_reg_requested)
		return 0;

	avdd_lcd_3v0_2v8 = regulator_get(dev, "avdd_lcd");
	if (IS_ERR_OR_NULL(avdd_lcd_3v0_2v8)) {
		pr_err("avdd_lcd regulator get failed\n");
		err = PTR_ERR(avdd_lcd_3v0_2v8);
		avdd_lcd_3v0_2v8 = NULL;
		goto fail;
	}

	dsi_cmi_720p_5_reg_requested = true;
	return 0;
fail:
	return err;
}

static int dsi_cmi_720p_5_gpio_get(void)
{
	int err = 0;

	if (dsi_cmi_720p_5_gpio_requested)
		return 0;

	err = gpio_request(DSI_PANEL_RST_GPIO, "panel rst");
	if (err < 0) {
		pr_err("panel reset gpio request failed err =%d \n",err);
		goto fail;
	}
	dsi_cmi_720p_5_gpio_requested = true;
	return 0;
fail:
	return err;
}

static int dsi_cmi_720p_5_enable(struct device *dev)
{
	int err = 0;

	err = dsi_cmi_720p_5_reg_get(dev);
	if (err < 0) {
		pr_err("dsi regulator get failed\n");
		goto fail;
	}

	err = dsi_cmi_720p_5_gpio_get();
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

static int dsi_cmi_720p_5_disable(void)
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

static int dsi_cmi_720p_5_postsuspend(void)
{
	return 0;
}

static struct tegra_dsi_cmd NT35590_CMI_dsi_init_cmd[]= {
    DSI_CMD_LONG(0x39,NT35590_CMI_param1),
    DSI_CMD_LONG(0x39,NT35590_CMI_param2),
    DSI_CMD_LONG(0x39,NT35590_CMI_param3),
    DSI_CMD_LONG(0x39,NT35590_CMI_param4),
    DSI_CMD_LONG(0x39,NT35590_CMI_param5),
    DSI_CMD_LONG(0x39,NT35590_CMI_param6),
    DSI_CMD_LONG(0x39,NT35590_CMI_param7),
    DSI_CMD_LONG(0x39,NT35590_CMI_param8),
    DSI_CMD_LONG(0x39,NT35590_CMI_param9),
    DSI_CMD_LONG(0x39,NT35590_CMI_param10),
    DSI_CMD_LONG(0x39,NT35590_CMI_param11),
    DSI_CMD_LONG(0x39,NT35590_CMI_param12),
    DSI_CMD_LONG(0x39,NT35590_CMI_param13),
    DSI_CMD_LONG(0x39,NT35590_CMI_param14),
    DSI_CMD_LONG(0x39,NT35590_CMI_param15),
    DSI_CMD_LONG(0x39,NT35590_CMI_param16),
    DSI_CMD_LONG(0x39,NT35590_CMI_param17),
    DSI_CMD_LONG(0x39,NT35590_CMI_param18),
    DSI_CMD_LONG(0x39,NT35590_CMI_param19),
    DSI_CMD_LONG(0x39,NT35590_CMI_param20),
    DSI_CMD_LONG(0x39,NT35590_CMI_param21),
    DSI_CMD_LONG(0x39,NT35590_CMI_param22),
    DSI_CMD_LONG(0x39,NT35590_CMI_param23),
    DSI_CMD_LONG(0x39,NT35590_CMI_param24),
    DSI_CMD_LONG(0x39,NT35590_CMI_param25),
    DSI_CMD_LONG(0x39,NT35590_CMI_param26),
    DSI_CMD_LONG(0x39,NT35590_CMI_param27),
    DSI_CMD_LONG(0x39,NT35590_CMI_param28),
    DSI_CMD_LONG(0x39,NT35590_CMI_param29),
    DSI_CMD_LONG(0x39,NT35590_CMI_param30),
    DSI_CMD_LONG(0x39,NT35590_CMI_param31),
    DSI_CMD_LONG(0x39,NT35590_CMI_param32),
    DSI_CMD_LONG(0x39,NT35590_CMI_param33),
    DSI_CMD_LONG(0x39,NT35590_CMI_param34),
    DSI_CMD_LONG(0x39,NT35590_CMI_param35),
    DSI_CMD_LONG(0x39,NT35590_CMI_param36),
    DSI_CMD_LONG(0x39,NT35590_CMI_param37),
    DSI_CMD_LONG(0x39,NT35590_CMI_param38),
    DSI_CMD_LONG(0x39,NT35590_CMI_param39),
    DSI_CMD_LONG(0x39,NT35590_CMI_param40),
    DSI_CMD_LONG(0x39,NT35590_CMI_param41),
    DSI_CMD_LONG(0x39,NT35590_CMI_param42),
    DSI_CMD_LONG(0x39,NT35590_CMI_param43),
    DSI_CMD_LONG(0x39,NT35590_CMI_param44),
    DSI_CMD_LONG(0x39,NT35590_CMI_param45),
    DSI_CMD_LONG(0x39,NT35590_CMI_param46),
    DSI_CMD_LONG(0x39,NT35590_CMI_param47),
    DSI_CMD_LONG(0x39,NT35590_CMI_param48),
    DSI_CMD_LONG(0x39,NT35590_CMI_param49),
    DSI_CMD_LONG(0x39,NT35590_CMI_param50),
    DSI_CMD_LONG(0x39,NT35590_CMI_param51),
    DSI_CMD_LONG(0x39,NT35590_CMI_param52),
    DSI_CMD_LONG(0x39,NT35590_CMI_param53),
    DSI_CMD_LONG(0x39,NT35590_CMI_param54),
    DSI_CMD_LONG(0x39,NT35590_CMI_param55),
    DSI_CMD_LONG(0x39,NT35590_CMI_param56),
    DSI_CMD_LONG(0x39,NT35590_CMI_param57),
    DSI_CMD_LONG(0x39,NT35590_CMI_param58),
    DSI_CMD_LONG(0x39,NT35590_CMI_param59),
    DSI_CMD_LONG(0x39,NT35590_CMI_param60),
    DSI_CMD_LONG(0x39,NT35590_CMI_param61),
    DSI_CMD_LONG(0x39,NT35590_CMI_param62),
    DSI_CMD_LONG(0x39,NT35590_CMI_param63),
    DSI_CMD_LONG(0x39,NT35590_CMI_param64),
    DSI_CMD_LONG(0x39,NT35590_CMI_param65),
    DSI_CMD_LONG(0x39,NT35590_CMI_param66),
    DSI_CMD_LONG(0x39,NT35590_CMI_param67),
    DSI_CMD_LONG(0x39,NT35590_CMI_param68),
    DSI_CMD_LONG(0x39,NT35590_CMI_param69),
    DSI_CMD_LONG(0x39,NT35590_CMI_param70),
    DSI_CMD_LONG(0x39,NT35590_CMI_param71),
    DSI_CMD_LONG(0x39,NT35590_CMI_param72),
    DSI_CMD_LONG(0x39,NT35590_CMI_param73),
    DSI_CMD_LONG(0x39,NT35590_CMI_param74),
    DSI_CMD_LONG(0x39,NT35590_CMI_param75),
    DSI_CMD_LONG(0x39,NT35590_CMI_param76),
    DSI_CMD_LONG(0x39,NT35590_CMI_param77),
    DSI_CMD_LONG(0x39,NT35590_CMI_param78),
    DSI_CMD_LONG(0x39,NT35590_CMI_param79),
    DSI_CMD_LONG(0x39,NT35590_CMI_param80),
    DSI_CMD_LONG(0x39,NT35590_CMI_param81),
    DSI_CMD_LONG(0x39,NT35590_CMI_param82),
    DSI_CMD_LONG(0x39,NT35590_CMI_param83),
    DSI_CMD_LONG(0x39,NT35590_CMI_param84),
    DSI_CMD_LONG(0x39,NT35590_CMI_param85),
    DSI_CMD_LONG(0x39,NT35590_CMI_param86),
    DSI_CMD_LONG(0x39,NT35590_CMI_param87),
    DSI_CMD_LONG(0x39,NT35590_CMI_param88),
    DSI_CMD_LONG(0x39,NT35590_CMI_param89),
    DSI_CMD_LONG(0x39,NT35590_CMI_param90),
    DSI_CMD_LONG(0x39,NT35590_CMI_param91),
    DSI_CMD_LONG(0x39,NT35590_CMI_param92),
    DSI_CMD_LONG(0x39,NT35590_CMI_param93),
    DSI_CMD_LONG(0x39,NT35590_CMI_param94),
    DSI_CMD_LONG(0x39,NT35590_CMI_param95),
    DSI_CMD_LONG(0x39,NT35590_CMI_param96),
    DSI_CMD_LONG(0x39,NT35590_CMI_param97),
    DSI_CMD_LONG(0x39,NT35590_CMI_param98),
    DSI_CMD_LONG(0x39,NT35590_CMI_param99),
    DSI_CMD_LONG(0x39,NT35590_CMI_param100),
    DSI_CMD_LONG(0x39,NT35590_CMI_param101),
    DSI_CMD_LONG(0x39,NT35590_CMI_param102),
    DSI_CMD_LONG(0x39,NT35590_CMI_param103),
    DSI_CMD_LONG(0x39,NT35590_CMI_param104),
    DSI_CMD_LONG(0x39,NT35590_CMI_param105),
    DSI_CMD_LONG(0x39,NT35590_CMI_param106),
    DSI_CMD_LONG(0x39,NT35590_CMI_param107),
    DSI_CMD_LONG(0x39,NT35590_CMI_param108),
    DSI_CMD_LONG(0x39,NT35590_CMI_param109),
    DSI_CMD_LONG(0x39,NT35590_CMI_param110),
    DSI_CMD_LONG(0x39,NT35590_CMI_param111),
    DSI_CMD_LONG(0x39,NT35590_CMI_param112),
    DSI_CMD_LONG(0x39,NT35590_CMI_param113),
    DSI_CMD_LONG(0x39,NT35590_CMI_param114),
    DSI_CMD_LONG(0x39,NT35590_CMI_param115),
    DSI_CMD_LONG(0x39,NT35590_CMI_param116),
    DSI_CMD_LONG(0x39,NT35590_CMI_param117),
    DSI_CMD_LONG(0x39,NT35590_CMI_param118),
    DSI_CMD_LONG(0x39,NT35590_CMI_param119),
    DSI_CMD_LONG(0x39,NT35590_CMI_param120),
    DSI_CMD_LONG(0x39,NT35590_CMI_param121),
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


static struct tegra_dsi_out dsi_cmi_720p_5_pdata = {
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
	.dsi_init_cmd = NT35590_CMI_dsi_init_cmd,
	.n_init_cmd = ARRAY_SIZE(NT35590_CMI_dsi_init_cmd),
	.n_suspend_cmd = ARRAY_SIZE(dsi_suspend_cmd),
	.dsi_suspend_cmd = dsi_suspend_cmd,
	#if CONFIG_ESD_READ_TE
    .n_reset_cmd = ARRAY_SIZE(NT35590_CMI_dsi_init_cmd),
    .dsi_reset_cmd = NT35590_CMI_dsi_init_cmd,
    #endif
};

static void dsi_cmi_720p_5_resources_init(struct resource *
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

static void dsi_cmi_720p_5_dc_out_init(struct tegra_dc_out *dc)
{
	dc->dsi = &dsi_cmi_720p_5_pdata;
	dc->parent_clk = "pll_d_out0";
	dc->modes = dsi_cmi_720p_5_modes;
	dc->n_modes = ARRAY_SIZE(dsi_cmi_720p_5_modes);
	dc->enable = dsi_cmi_720p_5_enable;
	dc->disable = dsi_cmi_720p_5_disable;
	dc->postsuspend	= dsi_cmi_720p_5_postsuspend,
	dc->width = 62;
	dc->height = 110;
	dc->flags = DC_CTRL_MODE;
}
static void dsi_cmi_720p_5_fb_data_init(struct tegra_fb_data *fb)
{
	fb->xres = dsi_cmi_720p_5_modes[0].h_active;
	fb->yres = dsi_cmi_720p_5_modes[0].v_active;
}

static void dsi_cmi_720p_5_sd_settings_init(struct tegra_dc_sd_settings *settings)
{
	settings->bl_device_name = "pwm-backlight";
}

static void dsi_cmi_720p_5_set_disp_device(
	struct platform_device *display_device)
{
	disp_device = display_device;
}

struct tegra_panel __initdata dsi_cmi_720p_5 = {
	.init_sd_settings = dsi_cmi_720p_5_sd_settings_init,
	.init_dc_out = dsi_cmi_720p_5_dc_out_init,
	.init_fb_data = dsi_cmi_720p_5_fb_data_init,
	.init_resources = dsi_cmi_720p_5_resources_init,
	.register_bl_dev = dsi_cmi_720p_5_register_bl_dev,
	.set_disp_device = dsi_cmi_720p_5_set_disp_device,
};
EXPORT_SYMBOL(dsi_cmi_720p_5);

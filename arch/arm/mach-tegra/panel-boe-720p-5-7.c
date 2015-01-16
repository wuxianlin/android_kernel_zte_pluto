/*
 * arch/arm/mach-tegra/panel-boe-720p-5-7.c
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
#include "otm1283a.h"

#define DSI_PANEL_RESET         1
#define DSI_PANEL_RST_GPIO      TEGRA_GPIO_PH5
#define DSI_PANEL_BL_PWM        TEGRA_GPIO_PH1

#define DC_CTRL_MODE            TEGRA_DC_OUT_CONTINUOUS_MODE

static struct regulator *avdd_lcd_3v0_2v8;
static bool dsi_boe_720p_5_7_reg_requested;
static bool dsi_boe_720p_5_7_gpio_requested;
static bool is_bl_powered;

static struct platform_device *disp_device;
/*
static tegra_dc_bl_output dsi_boe_720p_5_7_bl_response_curve = {
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
*/
static int __maybe_unused dsi_boe_720p_5_7_bl_notify(struct device *unused,
							int brightness)
{
	//int cur_sd_brightness = atomic_read(&sd_brightness);

	/* Apply any backlight response curve */
	if (brightness > 255)
		pr_info("Error: Brightness > 255!\n");
	//else
		//brightness = dsi_boe_720p_5_7_bl_response_curve[brightness];

	/* SD brightness is a percentage */
	//brightness = (brightness * cur_sd_brightness) / 255;

	return brightness;
}

static int __maybe_unused dsi_boe_720p_5_7_check_fb(struct device *dev,
					     struct fb_info *info)
{
	return info->device == &disp_device->dev;
}

/*
	jingdongfang uses pwm blacklight device
*/
static struct platform_pwm_backlight_data dsi_boe_720p_5_7_bl_data = {
	.pwm_id         = 1,
	.max_brightness = 255,
	.dft_brightness = 132, /*ZTE: modified by tong.weili for default brightness 20130315*/
	.pwm_period_ns  = 40000,
	.notify         = dsi_boe_720p_5_7_bl_notify,
	/* Only toggle backlight on fb blank notifications for disp1 */
	.check_fb       = dsi_boe_720p_5_7_check_fb,
};

static struct platform_device dsi_boe_720p_5_7_bl_device = {
	.name   = "pwm-backlight",
	.id     = -1,
	.dev    = {
		.platform_data = &dsi_boe_720p_5_7_bl_data,
	},
};

static struct platform_device __maybe_unused
			*dsi_boe_720p_5_7_bl_devices[] __initdata = {
	&tegra_pwfm1_device,
	&dsi_boe_720p_5_7_bl_device,
};

static int __init dsi_boe_720p_5_7_register_bl_dev(void)
{
	int err = 0;
	err = platform_add_devices(dsi_boe_720p_5_7_bl_devices,
				ARRAY_SIZE(dsi_boe_720p_5_7_bl_devices));
	if (err) {
		pr_err("disp1 bl device registration failed");
		return err;
	}

#ifndef CONFIG_BACKLIGHT_1_WIRE_MODE
        err = gpio_request(DSI_PANEL_BL_PWM, "panel pwm");
	if (err < 0) {
		pr_err("panel backlight pwm gpio request failed\n");
		return err;
	}
	gpio_free(DSI_PANEL_BL_PWM);
#endif

	return err;
}

struct tegra_dc_mode dsi_boe_720p_5_7_modes[] = {
	{
		.pclk = 10000000,
		.h_ref_to_sync = 4,
		.v_ref_to_sync = 4,
		.h_sync_width = 6,
		.v_sync_width = 6,
		.h_back_porch = 109,
		.v_back_porch = 20,
		.h_active = 720,
		.v_active = 1280,
		.h_front_porch = 10,
		.v_front_porch = 15,
	},
};

static int dsi_boe_720p_5_7_reg_get(void)
{
	int err = 0;

	if (dsi_boe_720p_5_7_reg_requested)
		return 0;

        avdd_lcd_3v0_2v8 = regulator_get(NULL, "avdd_lcd");
	if (IS_ERR_OR_NULL(avdd_lcd_3v0_2v8)) {
		pr_err("avdd_lcd regulator get failed\n");
		err = PTR_ERR(avdd_lcd_3v0_2v8);
		avdd_lcd_3v0_2v8 = NULL;
		goto fail;
	}

	dsi_boe_720p_5_7_reg_requested = true;
	return 0;
fail:
	return err;
}

static int dsi_boe_720p_5_7_gpio_get(void)
{
	int err = 0;

	if (dsi_boe_720p_5_7_gpio_requested)
		return 0;

	err = gpio_request(DSI_PANEL_RST_GPIO, "panel rst");
	if (err < 0) {
		pr_err("panel reset gpio request failed\n");
		goto fail;
	}

	dsi_boe_720p_5_7_gpio_requested = true;
	return 0;
fail:
	return err;
}

static int dsi_boe_720p_5_7_enable(struct device *dev)
{
	int err = 0;

      err = dsi_boe_720p_5_7_reg_get();
	if (err < 0) {
		pr_err("dsi regulator get failed\n");
		goto fail;
	}

	err = dsi_boe_720p_5_7_gpio_get();
	if (err < 0) {
		pr_err("dsi gpio request failed\n");
		goto fail;
	}
	gpio_direction_output(DSI_PANEL_RST_GPIO, 1);

	if (avdd_lcd_3v0_2v8) {
		err = regulator_enable(avdd_lcd_3v0_2v8);
		if (err < 0) {
			pr_err("avdd_lcd regulator enable failed\n");
			goto fail;
		}
                regulator_set_voltage(avdd_lcd_3v0_2v8, 3000000, 3000000);
	}

	usleep_range(3000, 5000);

#if DSI_PANEL_RESET
	gpio_direction_output(DSI_PANEL_RST_GPIO, 0);
	gpio_set_value(DSI_PANEL_RST_GPIO, 0);
	mdelay(10);
	gpio_set_value(DSI_PANEL_RST_GPIO, 1);
	mdelay(120);
#endif

	is_bl_powered = true;
	return 0;
fail:
	return err;
}

static struct tegra_dsi_cmd dsi_boe_720p_5_7_init_cmd[] = {
    DSI_CMD_LONG(0x39,OTM1283A_boe_param0),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param1),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param2),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param3),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param4),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param5),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param6),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param7),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param8),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param9),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param10),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param11),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param12),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param13),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param14),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param15),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param16),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param17),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param18),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param19),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param20),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param21),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param22),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param23),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param24),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param25),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param26),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param27),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param28),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param29),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param30),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param31),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param32),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param33),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param34),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param35),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param36),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param37),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param38),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param39),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param40),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param41),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param42),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param43),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param44),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param45),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param46),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param47),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param48),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param49),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param50),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param51),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param52),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param53),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param54),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param55),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param56),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param57),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param58),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param59),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param60),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param61),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param62),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param63),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param64),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param65),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param66),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param67),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param68),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param69),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param70),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param71),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param72),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param73),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param74),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param75),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param76),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param77),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param78),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param79),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param80),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param81),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param82),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param83),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param84),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param85),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param86),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param87),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param88),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param89),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param90),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param91),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param92),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param93),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param94),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param95),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param96),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param97),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param98),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param99),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param100),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param101),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param102),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param103),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param104),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param105),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param106),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param107),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param108),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param109),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param110),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param111),
	DSI_CMD_LONG(0x39,OTM1283A_boe_param112),

        DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM, DSI_DCS_EXIT_SLEEP_MODE, 0x00),
	DSI_DLY_MS(100),
	DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM, DSI_DCS_SET_DISPLAY_ON, 0x00),
	DSI_DLY_MS(10),
};

static struct tegra_dsi_out dsi_boe_720p_5_7_pdata = {
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
	.dsi_init_cmd = dsi_boe_720p_5_7_init_cmd,
	.n_init_cmd = ARRAY_SIZE(dsi_boe_720p_5_7_init_cmd),
};

static int dsi_boe_720p_5_7_disable(void)
{
	is_bl_powered = false;
	gpio_set_value(DSI_PANEL_RST_GPIO, 0);

	if (avdd_lcd_3v0_2v8)
		regulator_disable(avdd_lcd_3v0_2v8);

	return 0;
}

static void dsi_boe_720p_5_7_resources_init(struct resource *
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

static void dsi_boe_720p_5_7_dc_out_init(struct tegra_dc_out *dc)
{
	dc->dsi = &dsi_boe_720p_5_7_pdata;
	dc->parent_clk = "pll_d_out0";
	dc->modes = dsi_boe_720p_5_7_modes;
	dc->n_modes = ARRAY_SIZE(dsi_boe_720p_5_7_modes);
	dc->enable = dsi_boe_720p_5_7_enable;
	dc->disable = dsi_boe_720p_5_7_disable;
	dc->width = 71;
	dc->height = 127;
	dc->flags = DC_CTRL_MODE;
}
static void dsi_boe_720p_5_7_fb_data_init(struct tegra_fb_data *fb)
{
	fb->xres = dsi_boe_720p_5_7_modes[0].h_active;
	fb->yres = dsi_boe_720p_5_7_modes[0].v_active;
}

static void dsi_boe_720p_5_7_sd_settings_init(struct tegra_dc_sd_settings *settings)
{
	settings->bl_device_name = "pwm-backlight";
}

static void dsi_boe_720p_5_7_set_disp_device(
	struct platform_device *display_device)
{
	disp_device = display_device;
}

struct tegra_panel __initdata dsi_boe_720p_5_7 = {
	.init_sd_settings = dsi_boe_720p_5_7_sd_settings_init,
	.init_dc_out = dsi_boe_720p_5_7_dc_out_init,
	.init_fb_data = dsi_boe_720p_5_7_fb_data_init,
	.init_resources = dsi_boe_720p_5_7_resources_init,
	.register_bl_dev = dsi_boe_720p_5_7_register_bl_dev,
	.set_disp_device = dsi_boe_720p_5_7_set_disp_device,
};
EXPORT_SYMBOL(dsi_boe_720p_5_7);

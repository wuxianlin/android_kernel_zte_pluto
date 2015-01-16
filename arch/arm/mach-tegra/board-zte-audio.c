/*
 * arch/arm/mach-tegra/board-zte-codecs.c
 *
 *
 * Copyright (C) 2013 ZTE.
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


#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/spi-tegra.h>
#include <mach/tegra_asoc_pdata.h>
#include <mach/gpio.h>
#include "board-pluto.h"
#include "devices.h"
#include "gpio-names.h"
#include "clock.h"
#include <linux/gpio.h>

#include <sound/max98095.h>
#include <linux/spi-tegra.h>


/*ZTE: added for CODEC, Begin*/
/* Equalizer filter coefs generated from the MAXIM MAX9809X
 * Evaluation Kit (EVKIT) software tool */
static struct max9809X_eq_cfg eq_cfg[] = {
	{ /* Flat response */
	        .name = "SPK_MUSIC",
	        .rate = 48000,
	        .band1 = {0x268B, 0xC008, 0x3F08, 0x01EB, 0x0B16},
	        .band2 = {0x6601, 0xC5C2, 0x3506, 0x1A87, 0x23D6},
	        .band3 = {0x0A50, 0xC35F, 0x2146, 0x147E, 0x36AB},
	        .band4 = {0x7FFE, 0xD606, 0x1E77, 0x304F, 0x3848},
	        .band5 = {0x2594, 0xC01D, 0x3E37, 0x03C2, 0x0F02},
	},
	{ /* Low pass Fc=1KHz */
	        .name = "HP_MUSIC",
	        .rate = 48000,
	        .band1 = {0x2997, 0xC002, 0x3F7E, 0x00E3, 0x0804},
	        .band2 = {0x2405, 0xC009, 0x3F1D, 0x0218, 0x0A9D},
	        .band3 = {0x2045, 0xC06B, 0x3F1A, 0x0745, 0x0AAA},
	        .band4 = {0x2638, 0xC3AC, 0x32FF, 0x155D, 0x26AB},
	        .band5 = {0x293E, 0xF89B, 0x0DE2, 0x3F92, 0x3E79},
	},
	{ /* BASS=-12dB, TREBLE=+9dB, Fc=5KHz */
		.name = "HIBOOST",
		.rate = 44100,
		.band1 = {0x0815, 0xC001, 0x3AA4, 0x0003, 0x19A2},
		.band2 = {0x0815, 0xC103, 0x092F, 0x0B55, 0x3F56},
		.band3 = {0x0E0A, 0xC306, 0x1E5C, 0x136E, 0x3856},
		.band4 = {0x2459, 0xF665, 0x0CAA, 0x3F46, 0x3EBB},
		.band5 = {0x5BBB, 0x3FFF, 0xCEB0, 0x0000, 0x28CA},
	},
	{ /* BASS=12dB, TREBLE=+12dB */
		.name = "LOUD12DB",
		.rate = 44100,
		.band1 = {0x7FC1, 0xC001, 0x3EE8, 0x0020, 0x0BC7},
		.band2 = {0x51E9, 0xC016, 0x3C7C, 0x033F, 0x14E9},
		.band3 = {0x1745, 0xC12C, 0x1680, 0x0C2F, 0x3BE9},
		.band4 = {0x4536, 0xD7E2, 0x0ED4, 0x31DD, 0x3E42},
		.band5 = {0x7FEF, 0x3FFF, 0x0BAB, 0x0000, 0x3EED},
	},
	{
		.name = "FLAT",
		.rate = 16000,
		.band1 = {0x2000, 0xC004, 0x4000, 0x0141, 0x0000},
		.band2 = {0x2000, 0xC033, 0x4000, 0x0505, 0x0000},
		.band3 = {0x2000, 0xC268, 0x4000, 0x115F, 0x0000},
		.band4 = {0x2000, 0xDA62, 0x4000, 0x33C6, 0x0000},
		.band5 = {0x2000, 0x4000, 0x4000, 0x0000, 0x0000},
	},
	{
		.name = "LOWPASS1K",
		.rate = 16000,
		.band1 = {0x2000, 0xC004, 0x4000, 0x0141, 0x0000},
		.band2 = {0x5BE8, 0xC3E0, 0x3307, 0x15ED, 0x26A0},
		.band3 = {0x0F71, 0xD15A, 0x08B3, 0x2BD0, 0x3F67},
		.band4 = {0x0815, 0x3FFF, 0xCF78, 0x0000, 0x29B7},
		.band5 = {0x0815, 0x3FFF, 0xCF78, 0x0000, 0x29B7},
	},
	{ /* BASS=-12dB, TREBLE=+9dB, Fc=2KHz */
		.name = "HIBOOST",
		.rate = 16000,
		.band1 = {0x0815, 0xC001, 0x3BD2, 0x0009, 0x16BF},
		.band2 = {0x080E, 0xC17E, 0xF653, 0x0DBD, 0x3F43},
		.band3 = {0x0F80, 0xDF45, 0xEE33, 0x36FE, 0x3D79},
		.band4 = {0x590B, 0x3FF0, 0xE882, 0x02BD, 0x3B87},
		.band5 = {0x4C87, 0xF3D0, 0x063F, 0x3ED4, 0x3FB1},
	},
	{ /* BASS=12dB, TREBLE=+12dB */
		.name = "LOUD12DB",
		.rate = 16000,
		.band1 = {0x7FC1, 0xC001, 0x3D07, 0x0058, 0x1344},
		.band2 = {0x2DA6, 0xC013, 0x3CF1, 0x02FF, 0x138B},
		.band3 = {0x18F1, 0xC08E, 0x244D, 0x0863, 0x34B5},
		.band4 = {0x2BE0, 0xF385, 0x04FD, 0x3EC5, 0x3FCE},
		.band5 = {0x7FEF, 0x4000, 0x0BAB, 0x0000, 0x3EED},
	},
};

static struct max9809X_biquad_cfg bq_cfg[] = {
	{
		.name = "LP4K",
		.rate = 44100,
		.band1 = {0x5019, 0xe0de, 0x03c2, 0x0784, 0x03c2},
		.band2 = {0x5013, 0xe0e5, 0x03c1, 0x0783, 0x03c1},
	},
	{
		.name = "HP4K",
		.rate = 44100,
		.band1 = {0x5019, 0xe0de, 0x2e4b, 0xa36a, 0x2e4b},
		.band2 = {0x5013, 0xe0e5, 0x2e47, 0xa371, 0x2e47},
	},
};

static struct max9809X_pdata enterprise_max9809X_pdata = {
	/* equalizer configuration */
	.eq_cfg = eq_cfg,
	.eq_cfgcnt = ARRAY_SIZE(eq_cfg), 

	/* biquad filter configuration */
	.bq_cfg = bq_cfg,
	.bq_cfgcnt = ARRAY_SIZE(bq_cfg), 

	/* microphone configuration */
	.digmic_left_mode = 0,  /* 0 = normal analog mic */
	.digmic_right_mode = 0, /* 0 = normal analog mic */
};

static void audio_codec_enbale(bool enable)
{
	gpio_request(CODEC_CEN_PIN, "codec_enable");	

	if (enable)
		gpio_direction_output(CODEC_CEN_PIN, 1);
	else
		gpio_direction_output(CODEC_CEN_PIN, 0);
}

#ifdef CONFIG_SWITCH
static void audio_codec_irqgpio_init(void)
{
	 printk("[codec]audio_codec_irqgpio_init\n");
	 gpio_request(TEGRA_GPIO_CDC_IRQ, "hook detect");	
	 gpio_direction_input(TEGRA_GPIO_CDC_IRQ);  
}
#endif
	
static struct spi_clk_parent spi_codec_parent_clk[] = {
	[0] = {.name = "pll_p"},        
};

static struct tegra_spi_platform_data spi_codec_pdata = {
	.is_dma_based           = true, 
	.max_dma_buffer         = (16 * 1024),
	.is_clkon_always        = false,
	.max_rate               = 16000000,			// Change by Danny to enhance the rate 
};

static struct tegra_spi_device_controller_data codec_spi_control_info = {
	.rx_clk_tap_delay = 0,
	.tx_clk_tap_delay = 0,
};
static struct spi_board_info codec_spi_board_info[] = {
	/* spi master */
	{
		.modalias = "max9809X",
		.bus_num = 1,
		.chip_select = 1,
		.mode = SPI_MODE_1,
		.max_speed_hz = 16000000,
		.platform_data = &enterprise_max9809X_pdata,
		.controller_data = &codec_spi_control_info,
	},
};

static void codec_spi_bus_init(void)
{
	int err;
	int i;
	struct clk *c;

	printk("[codec] codec_spi_bus_init\n");
	for (i = 0; i < ARRAY_SIZE(spi_codec_parent_clk); ++i) {
		c = tegra_get_clock_by_name(spi_codec_parent_clk[i].name);
		if (IS_ERR_OR_NULL(c)) {
			pr_err("Not able to get the clock for %s\n",
			spi_codec_parent_clk[i].name);
			continue;
		}
		spi_codec_parent_clk[i].parent_clk = c;
		spi_codec_parent_clk[i].fixed_clk_rate = clk_get_rate(c);
	}
	spi_codec_pdata.parent_clk_list = spi_codec_parent_clk;
	spi_codec_pdata.parent_clk_count = ARRAY_SIZE(spi_codec_parent_clk);


	tegra11_spi_device2.dev.platform_data = &spi_codec_pdata;
	platform_device_register(&tegra11_spi_device2);

	//ZTE: different from Enterprise
	codec_spi_board_info[0].irq = gpio_to_irq(TEGRA_GPIO_CDC_IRQ);
	err = spi_register_board_info(codec_spi_board_info,
	ARRAY_SIZE(codec_spi_board_info));
	if (err < 0){
		pr_err("%s: spi_register_board returned error %d\n",__func__, err);
	}
}

static struct tegra_asoc_platform_data enterprise_audio_max9809X_spi_pdata = {
	.gpio_spkr_en	= -1,
	.gpio_hp_det	= TEGRA_GPIO_HP_DET,
	.gpio_hp_mute	= -1,
	.gpio_int_mic_en	= -1,
	.gpio_ext_mic_en = -1,
	.gpio_spk_head_mic_switch = TEGRA_GPIO_SPK_HEAD_MIC_SWITCH,
	.i2s_param[HIFI_CODEC]	= {
		.audio_port_id	= 0,
		.is_i2s_master	= 1,
		.i2s_mode	= TEGRA_DAIFMT_I2S,
		.sample_size	= 16,
	},
	.i2s_param[BASEBAND]	= {
		.audio_port_id	= 2,
		.is_i2s_master	= 1,
		.i2s_mode	= TEGRA_DAIFMT_DSP_A,
		.sample_size	= 16,
		.rate		= 8000,
		.channels	= 1,
	},
	.i2s_param[BT_SCO]	= {
		.audio_port_id	= 3,    
		.is_i2s_master	= 1,
		.i2s_mode	= TEGRA_DAIFMT_DSP_A,
		.sample_size	= 16,        
	},
};

static struct platform_device enterprise_audio_max9809X_spi_device = {
	.name	= "tegra-snd-max9809X",
	.id	= 0,
	.dev	= {
		.platform_data  = &enterprise_audio_max9809X_spi_pdata,
	},
};

static int codce_DC_control_gpio_init(void)
{
	int ret = 0;

	ret = gpio_request(TEGRA_GPIO_HARD_VERSION0, "HARD_VERSION0");
	if (ret) {
		pr_err("%s: cannot get TEGRA_GPIO_HARD_VERSION0 gpio\n", __func__);
		return ret;
	}
	ret = gpio_direction_input(TEGRA_GPIO_HARD_VERSION0);
	if (ret < 0) {
		pr_err("%s: gpio set direction failed\n", __func__);
		gpio_free(TEGRA_GPIO_HARD_VERSION0);
		return ret;
	}

	ret = gpio_request(TEGRA_GPIO_HARD_VERSION1, "HARD_VERSION1");
	if (ret) {
		pr_err("%s: cannot get TEGRA_GPIO_HARD_VERSION1 gpio\n", __func__);
		return ret;
	}
	ret = gpio_direction_input(TEGRA_GPIO_HARD_VERSION1);
	if (ret < 0) {
		pr_err("%s: gpio set direction failed\n", __func__);
		gpio_free(TEGRA_GPIO_HARD_VERSION1);
		return ret;
	}

	ret = gpio_request(TEGRA_GPIO_HARD_VERSION2, "HARD_VERSION2");
	if (ret) {
		pr_err("%s: cannot get TEGRA_GPIO_HARD_VERSION2 gpio\n", __func__);
		return ret;
	}
	ret = gpio_direction_input(TEGRA_GPIO_HARD_VERSION2);
	if (ret < 0) {
		pr_err("%s: gpio set direction failed\n", __func__);
		gpio_free(TEGRA_GPIO_HARD_VERSION2);
		return ret;
	}
}
 int read_hard_version_gpio(void)
{
	int dc_gpio_val = 0;

      dc_gpio_val |= (gpio_get_value(TEGRA_GPIO_HARD_VERSION0)<<0);
      dc_gpio_val |= (gpio_get_value(TEGRA_GPIO_HARD_VERSION1)<<1);
      dc_gpio_val |= (gpio_get_value(TEGRA_GPIO_HARD_VERSION2)<<2);

	return dc_gpio_val;
}

static int codec_DC_gpio_init(void)
{
	int ret;

	printk("%s\n", __func__);
	ret = gpio_request(TEGRA_GPIO_SPK_DC_EN_PIN, "SPK_DC_EN");
	if (ret) {
		pr_err("%s: cannot get TEGRA_GPIO_SPK_DC_EN_PIN gpio\n", __func__);
		return ret;
	}
	ret = gpio_direction_output(TEGRA_GPIO_SPK_DC_EN_PIN, 1);
	if (ret < 0) {
		pr_err("%s: gpio set direction failed\n", __func__);
		gpio_free(TEGRA_GPIO_SPK_DC_EN_PIN);
		return ret;
	}
	return 0;
}


int zte_audio_init(void)
{
	int dc_gpio_val = 0;

	audio_codec_enbale(1);
#ifdef CONFIG_SWITCH
        audio_codec_irqgpio_init();
#endif	
      codec_spi_bus_init();
      codce_DC_control_gpio_init();
      dc_gpio_val = read_hard_version_gpio();
	if (dc_gpio_val >= 0x02)
	{
		codec_DC_gpio_init();
	}
	platform_device_register(&enterprise_audio_max9809X_spi_device);
return 0;
}

EXPORT_SYMBOL(zte_audio_init);


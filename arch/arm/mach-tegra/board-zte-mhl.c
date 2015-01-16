/*
 * arch/arm/mach-tegra/board-zte-mhl.c
 *
 *
 * Copyright (C) 2011 ZTE.
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

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include "gpio-names.h"

#ifdef CONFIG_SII8240_MHL_TX
#define CI2CA  true  // CI2CA depend on the CI2CA pin's level
#ifdef CI2CA 
#define SII8240_plus 0x02  //Define sii8334's I2c Address of all pages by the status of CI2CA.
#else
#define SII8240_plus 0x00  //Define sii8334's I2c Address of all pages by the status of CI2CA.
#endif
#endif

#ifdef CONFIG_SII8240_MHL_TX
#define AP_MHL_INT		TEGRA_GPIO_PR4
#define AP_MHL_RESET	TEGRA_GPIO_PS0
#define AP_MHL_3V3_EN   TEGRA_GPIO_PX7
#endif

int Sii8240_reset(void)
{	
	int ret;
	printk("[MHL] sii8240 reset sequence\n");
	
	ret = gpio_request(AP_MHL_RESET, "mhl_reset");
	if (ret < 0) {
		pr_err("%s: gpio_request failed %d, line: %d\n", __func__, ret, __LINE__);
		return ret;
	}

	ret = gpio_direction_output(AP_MHL_RESET, 1);
	if (ret < 0) {
		pr_err("%s: gpio_direction_input failed %d, line: %d\n", __func__, ret, __LINE__);
		gpio_free(AP_MHL_RESET);
		return ret;
	}

	mdelay(10);
	gpio_set_value(AP_MHL_RESET, 0);
	mdelay(10);
	gpio_set_value(AP_MHL_RESET, 1);

	return 0;
}

struct tegra_mhl_gpios {
	const char *name;
	int gpio;
	bool internal_gpio;
	bool output_dir;
	int init_value;
};

#define TEGRA_MHL_GPIO(_name, _gpio, _internal_gpio, _output_dir, _init_value)	\
	{									\
		.name = _name,					\
		.gpio = _gpio,					\
		.internal_gpio = _internal_gpio,		\
		.output_dir = _output_dir,			\
		.init_value = _init_value,			\
	}

static struct tegra_mhl_gpios pluto_mhl_gpios[] = {
	//TEGRA_SENSOR_GPIO("temp_alert", TEGRA_GPIO_PH7, 1, 0, 0),
	TEGRA_MHL_GPIO("mhl-irq", AP_MHL_INT, 1, 0, 0),
	TEGRA_MHL_GPIO("mhl_power_en", AP_MHL_3V3_EN, 1, 0, 0),
};

static int __init pluto_mhl_init(void)
{    
	int ret;
	printk("[MHL] sii8240 init sequence\n");

	ret = gpio_request(AP_MHL_INT, "mhl-irq");
	if (ret < 0) {
		pr_err("%s: gpio_request failed %d, line: %d\n", __func__, ret, __LINE__);
		return ret;
	}

	ret = gpio_direction_input(AP_MHL_INT);
	if (ret < 0) {
		pr_err("%s: gpio_direction_input failed %d, line: %d\n", __func__, ret, __LINE__);
		gpio_free(AP_MHL_INT);
		return ret;
	}

	ret = gpio_request(AP_MHL_3V3_EN, "mhl_power_en");
	if (ret < 0) {
		pr_err("%s: gpio_request failed %d, line: %d\n", __func__, ret, __LINE__);
		return ret;
	}

	ret = gpio_direction_output(AP_MHL_3V3_EN, 1);
	if (ret < 0) {
		pr_err("%s: gpio_direction_input failed %d, line: %d\n", __func__, ret, __LINE__);
		gpio_free(AP_MHL_RESET);
		return ret;
	}

     return 0;
}

static struct i2c_board_info Sii8240_i2c0_board_info[] = {
	{
		  I2C_BOARD_INFO("Sil-8240", 0x39 + SII8240_plus),
		 .irq = -1,
	},
};

int __init zte_mhl_init(void)
{
	int ret;
	
	ret = pluto_mhl_init();
	if (ret < 0) {
		pr_err("%s: pluto_mhl_init failed %d, line: %d\n", __func__, ret, __LINE__);
		return ret;
	}
	
	Sii8240_i2c0_board_info[0].irq = gpio_to_irq(AP_MHL_INT);
	i2c_register_board_info(0, Sii8240_i2c0_board_info,
  			ARRAY_SIZE(Sii8240_i2c0_board_info));

	return 0;
}


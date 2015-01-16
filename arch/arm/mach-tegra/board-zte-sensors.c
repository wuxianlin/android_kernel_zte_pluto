/*
 * arch/arm/mach-tegra/board-zte-sensors.c
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
#include <mach/gpio.h>
#include <linux/gpio.h>
#include "gpio-names.h"
#include <linux/platform_device.h>
/*sensor header files*/
#include <linux/nct1008.h>
#include <linux/i2c/tmd2771x.h>
#include <linux/i2c/l3g4200d.h>
#include <linux/i2c/akm8962.h>
#include <linux/accel.h>

#define AP_COMPASS_INT_GPIO       TEGRA_GPIO_PQ4
#define AP_LIGHT_INT_GPIO            TEGRA_GPIO_PC7

#ifdef CONFIG_SENSORS_TMD2771
static struct prox_config prox_config_pdata = {
	.pulse_count = 5,
	.led_drive = PROX_LED_DRIVE_25MA,
	.threshold_lo = 500,
	.threshold_hi = 560,
	.thresh_offset = 0,
	.thresh_gain_hi = 200,
	.thresh_gain_lo = 170,
};


static struct tmd2771x_platform_data tmd2771_pdata = {
	.prox_config = &prox_config_pdata,
};
#endif

#ifdef CONFIG_ACCEL_SENSORS
static unsigned short accel_addr_list[] = {
    0x0F, // kionix
    0x19, // st
    I2C_CLIENT_END
};

static struct accel_platform_data accel_pdata ={
	.adapt_nr = 0,
	.accel_list = accel_addr_list,
	.poll_inerval = 10,
	.g_range = ACCEL_FS_2G,
};

static struct platform_device accel_platform_device = {
    .name		= "accel_platform",
    .id = -1,
    .dev = {
        .platform_data = &accel_pdata,
    },
};
#endif

#ifdef CONFIG_SENSORS_L3G4200D
static struct l3g4200d_gyr_platform_data l3g4200d_pdata = {
	.poll_interval = 10,
	.min_interval = 2,
	.fs_range      = L3G4200D_FS_2000DPS,
	#ifdef CONFIG_PROJECT_U950
	.axis_map_x = 1,
	.axis_map_y = 0,	
	#else
	.axis_map_x = 1,
	.axis_map_y = 0,
	#endif
	.axis_map_z = 2,

	.negate_x = 0,
	#ifdef CONFIG_PROJECT_U950
	.negate_y = 0,
	#else
	.negate_y = 1,
	#endif
	.negate_z = 0,
};
#endif

#ifdef CONFIG_SENSORS_AK8962
static struct akm8962_platform_data akm8962_pdata = {
	.gpio_DRDY = AP_COMPASS_INT_GPIO,
};
#endif

static struct i2c_board_info pluto_sensor_i2c0_board_info[] = {
#ifdef CONFIG_SENSORS_AK8962
	{
		I2C_BOARD_INFO(AKM8962_I2C_NAME, 0x0C),
		.platform_data = &akm8962_pdata,
		.irq = -1,
	},
#endif
#ifdef CONFIG_SENSORS_TMD2771
	{
		I2C_BOARD_INFO("tmd2771x", 0x39),
		.platform_data = &tmd2771_pdata,
		.irq = -1,
	},
#endif
#ifdef CONFIG_SENSORS_L3G4200D
	{
		I2C_BOARD_INFO(L3G4200D_I2C_NAME, 0x69),
		.platform_data = &l3g4200d_pdata,
	},
#endif
#ifdef CONFIG_SENSORS_LSM330D_G
	{
		I2C_BOARD_INFO("lsm330dlc_gyr", 0x6A),
	},
	{
		I2C_BOARD_INFO("lsm330dlc_sec_gyr", 0x6B),
	},
#endif
};

#if 0
static struct i2c_board_info pluto_compass_i2c0_board_info[] = {
#ifdef CONFIG_SENSORS_AK8962
	{
		I2C_BOARD_INFO(AKM8962_I2C_NAME, 0x0C),
		.platform_data = &akm8962_pdata,
		.irq = -1,
	},
#endif
};

static struct i2c_board_info pluto_light_i2c0_board_info[] = {
#ifdef CONFIG_SENSORS_TMD2771
	{
		I2C_BOARD_INFO("tmd2771x", 0x39),
		.platform_data = &tmd2771_pdata,
		.irq = -1,
	},
#endif
};

static struct i2c_board_info pluto_gyro_i2c0_board_info[] = {
#ifdef CONFIG_SENSORS_L3G4200D
	{
		I2C_BOARD_INFO(L3G4200D_I2C_NAME, 0x69),
		.platform_data = &l3g4200d_pdata,
	},
#endif
#ifdef CONFIG_SENSORS_LSM330D_G
	{
		I2C_BOARD_INFO("lsm330dlc_gyr", 0x6B),
	},
#endif
};
#endif

struct tegra_sensor_gpios {
	const char *name;
	int gpio;
	bool internal_gpio;
	bool output_dir;
	int init_value;
};

#define TEGRA_SENSOR_GPIO(_name, _gpio, _internal_gpio, _output_dir, _init_value)	\
	{									\
		.name = _name,					\
		.gpio = _gpio,					\
		.internal_gpio = _internal_gpio,		\
		.output_dir = _output_dir,			\
		.init_value = _init_value,			\
	}

static struct tegra_sensor_gpios enterprise_sensor_gpios[] = {
	//TEGRA_SENSOR_GPIO("temp_alert", TEGRA_GPIO_PH7, 1, 0, 0),
	TEGRA_SENSOR_GPIO("taos_irq", AP_LIGHT_INT_GPIO, 1, 0, 0),
	TEGRA_SENSOR_GPIO("akm_int", AP_COMPASS_INT_GPIO, 1, 0, 0),
};

static void pluto_gpios_init(void)
{
	int ret;
	int i;

	for (i = 0; i < ARRAY_SIZE(enterprise_sensor_gpios); i++) {

		ret = gpio_request(enterprise_sensor_gpios[i].gpio,
			enterprise_sensor_gpios[i].name);
		if (ret < 0) {
			pr_err("gpio_request failed for gpio #%d(%s)\n",
				i, enterprise_sensor_gpios[i].name);
			continue;
		}

		if(enterprise_sensor_gpios[i].output_dir){
			ret = gpio_direction_output(enterprise_sensor_gpios[i].gpio,
				enterprise_sensor_gpios[i].init_value);
			if (ret < 0) {
				pr_err("gpio_direction_output failed for gpio #%d(%s)\n",
					i, enterprise_sensor_gpios[i].name);
				gpio_free(enterprise_sensor_gpios[i].gpio);
				continue;
			}
		}else{
			ret = gpio_direction_input(enterprise_sensor_gpios[i].gpio);
			if (ret < 0) {
				pr_err("gpio_direction_input failed for gpio #%d(%s)\n",
					i, enterprise_sensor_gpios[i].name);
				gpio_free(enterprise_sensor_gpios[i].gpio);
				continue;
			}
		}

		gpio_export(enterprise_sensor_gpios[i].gpio, false);
	}
}

int __init zte_sensors_init(void)
{
	int i=0;
	
	pluto_gpios_init();

#ifdef CONFIG_SENSORS_AK8962
	pluto_sensor_i2c0_board_info[i++].irq = gpio_to_irq(AP_COMPASS_INT_GPIO);
#endif
#ifdef CONFIG_SENSORS_TMD2771
	pluto_sensor_i2c0_board_info[i++].irq = gpio_to_irq(AP_LIGHT_INT_GPIO);
#endif

	i2c_register_board_info(0, pluto_sensor_i2c0_board_info,
				ARRAY_SIZE(pluto_sensor_i2c0_board_info));

#ifdef CONFIG_ACCEL_SENSORS
	platform_device_register(&accel_platform_device);
#endif

	return 0;
}
//EXPORT_SYMBOL(zte_sensors_init);

/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
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
 */
#include <linux/export.h>
#include <linux/resource.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/usb/ulpi.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_data/tegra_usb.h>
#include <mach/clk.h>
#include <mach/iomap.h>
#include <mach/pinmux.h>
#include <mach/pinmux-t11.h>
#include <asm/mach-types.h>
#include <mach/tegra_usb_pmc.h>

static void __iomem *pmc_base;
static unsigned long flags;
static DEFINE_SPINLOCK(pmc_lock);

#ifdef KERNEL_WARNING
static void usb_phy_power_down_pmc(struct tegra_usb_pmc_data *pmc_data)
{
	unsigned long val;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, pmc_data->instance);

	spin_lock_irqsave(&pmc_lock, flags);

	/* power down all 3 UTMIP interfaces */
	val = readl(pmc_base + PMC_UTMIP_MASTER_CONFIG);
	val |= UTMIP_PWR(0) | UTMIP_PWR(1) | UTMIP_PWR(2);
	writel(val, pmc_base + PMC_UTMIP_MASTER_CONFIG);

	/* turn on pad detectors */
	writel(PMC_POWER_DOWN_MASK, pmc_base + PMC_USB_AO);

	/* setup sleep walk fl all 3 usb controllers */
	val = UTMIP_USBOP_RPD_A | UTMIP_USBON_RPD_A | UTMIP_HIGHZ_A |
		UTMIP_USBOP_RPD_B | UTMIP_USBON_RPD_B | UTMIP_HIGHZ_B |
		UTMIP_USBOP_RPD_C | UTMIP_USBON_RPD_C | UTMIP_HIGHZ_C |
		UTMIP_USBOP_RPD_D | UTMIP_USBON_RPD_D | UTMIP_HIGHZ_D;
	writel(val, pmc_base + PMC_SLEEPWALK_REG(0));
	writel(val, pmc_base + PMC_SLEEPWALK_REG(1));
	writel(val, pmc_base + PMC_SLEEPWALK_REG(2));

	/* enable pull downs on HSIC PMC */
	val = UHSIC_STROBE_RPD_A | UHSIC_DATA_RPD_A | UHSIC_STROBE_RPD_B |
		UHSIC_DATA_RPD_B | UHSIC_STROBE_RPD_C | UHSIC_DATA_RPD_C |
		UHSIC_STROBE_RPD_D | UHSIC_DATA_RPD_D;
	writel(val, pmc_base + PMC_SLEEPWALK_UHSIC);

	/* Turn over pad configuration to PMC */
	val = readl(pmc_base + PMC_SLEEP_CFG);
	val &= ~UTMIP_WAKE_VAL(0, ~0);
	val &= ~UTMIP_WAKE_VAL(1, ~0);
	val &= ~UTMIP_WAKE_VAL(2, ~0);
	val &= ~UHSIC_WAKE_VAL_P0(~0);
	val |= UTMIP_WAKE_VAL(0, WAKE_VAL_NONE) |
	UHSIC_WAKE_VAL_P0(WAKE_VAL_NONE) |
	UTMIP_WAKE_VAL(1, WAKE_VAL_NONE) | UTMIP_WAKE_VAL(2, WAKE_VAL_NONE) |
	UTMIP_RCTRL_USE_PMC(0) | UTMIP_RCTRL_USE_PMC(1) |
	UTMIP_RCTRL_USE_PMC(2) |
	UTMIP_TCTRL_USE_PMC(0) | UTMIP_TCTRL_USE_PMC(1) |
	UTMIP_TCTRL_USE_PMC(2) |
	UTMIP_FSLS_USE_PMC(0) | UTMIP_FSLS_USE_PMC(1) |
	UTMIP_FSLS_USE_PMC(2) |
	UTMIP_MASTER_ENABLE(0) | UTMIP_MASTER_ENABLE(1) |
	UTMIP_MASTER_ENABLE(2) |
	UHSIC_MASTER_ENABLE_P0;
	writel(val, pmc_base + PMC_SLEEP_CFG);

	spin_unlock_irqrestore(&pmc_lock, flags);
}
#endif

static void utmip_setup_pmc_wake_detect(struct tegra_usb_pmc_data *pmc_data)
{
	unsigned long val, pmc_pad_cfg_val;
	unsigned  int inst = pmc_data->instance;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, pmc_data->instance);

	spin_lock_irqsave(&pmc_lock, flags);

	/*Set PMC MASTER bits to do the following
	* a. Take over the UTMI drivers
	* b. set up such that it will take over resume
	*	 if remote wakeup is detected
	* Prepare PMC to take over suspend-wake detect-drive resume until USB
	* controller ready
	*/

	/* disable master enable in PMC */
	val = readl(pmc_base + PMC_SLEEP_CFG);
	val &= ~UTMIP_MASTER_ENABLE(inst);
	writel(val, pmc_base + PMC_SLEEP_CFG);

	/* UTMIP_PWR_PX=1 for power savings mode */
	val = readl(pmc_base + PMC_UTMIP_MASTER_CONFIG);
	val |= UTMIP_PWR(inst);
	writel(val, pmc_base + PMC_UTMIP_MASTER_CONFIG);

	/* config debouncer */
	val = readl(pmc_base + PMC_USB_DEBOUNCE);
	val &= ~UTMIP_LINE_DEB_CNT(~0);
	val |= UTMIP_LINE_DEB_CNT(1);
	val |= PMC_USB_DEBOUNCE_VAL(2);
	writel(val, pmc_base + PMC_USB_DEBOUNCE);

	/* Make sure nothing is happening on the line with respect to PMC */
	val = readl(pmc_base + PMC_UTMIP_FAKE);
	val &= ~USBOP_VAL(inst);
	val &= ~USBON_VAL(inst);
	writel(val, pmc_base + PMC_UTMIP_FAKE);

	/* Make sure wake value for line is none */
	val = readl(pmc_base + PMC_SLEEPWALK_CFG);
	val &= ~UTMIP_LINEVAL_WALK_EN(inst);
	writel(val, pmc_base + PMC_SLEEPWALK_CFG);
	val = readl(pmc_base + PMC_SLEEP_CFG);
	val &= ~UTMIP_WAKE_VAL(inst, ~0);
	val |= UTMIP_WAKE_VAL(inst, WAKE_VAL_NONE);
	writel(val, pmc_base + PMC_SLEEP_CFG);

	/* turn off pad detectors */
	val = readl(pmc_base + PMC_USB_AO);
	val |= (USBOP_VAL_PD(inst) | USBON_VAL_PD(inst));
	writel(val, pmc_base + PMC_USB_AO);

	/* Remove fake values and make synchronizers work a bit */
	val = readl(pmc_base + PMC_UTMIP_FAKE);
	val &= ~USBOP_VAL(inst);
	val &= ~USBON_VAL(inst);
	writel(val, pmc_base + PMC_UTMIP_FAKE);

	/* Enable which type of event can trigger a walk,
	* in this case usb_line_wake */
	val = readl(pmc_base + PMC_SLEEPWALK_CFG);
	val |= UTMIP_LINEVAL_WALK_EN(inst);
	writel(val, pmc_base + PMC_SLEEPWALK_CFG);

	/* Capture FS/LS pad configurations */
	pmc_pad_cfg_val = readl(pmc_base + PMC_PAD_CFG);
	val = readl(pmc_base + PMC_TRIGGERS);
	val |= UTMIP_CAP_CFG(inst);
	writel(val, pmc_base + PMC_TRIGGERS);
	udelay(1);
	pmc_pad_cfg_val = readl(pmc_base + PMC_PAD_CFG);

	/* BIAS MASTER_ENABLE=0 */
	val = readl(pmc_base + PMC_UTMIP_BIAS_MASTER_CNTRL);
	val &= ~BIAS_MASTER_PROG_VAL;
	writel(val, pmc_base + PMC_UTMIP_BIAS_MASTER_CNTRL);

	/* program walk sequence for remote or hotplug wakeup */
	if ((pmc_data->port_speed < USB_PMC_PORT_SPEED_UNKNOWN) ||
		(pmc_data->port_speed == USB_PMC_PORT_SPEED_SUPER)) {
		/* program walk sequence, maintain a J, followed by a driven K
		* to signal a resume once an wake event is detected */
		val = readl(pmc_base + PMC_SLEEPWALK_REG(inst));
		val &= ~UTMIP_AP_A;
		val |= UTMIP_USBOP_RPD_A | UTMIP_USBON_RPD_A | UTMIP_AN_A |
					UTMIP_HIGHZ_A |
			UTMIP_USBOP_RPD_B | UTMIP_USBON_RPD_B | UTMIP_AP_B |
					UTMIP_AN_B |
			UTMIP_USBOP_RPD_C | UTMIP_USBON_RPD_C | UTMIP_AP_C |
					UTMIP_AN_C |
			UTMIP_USBOP_RPD_D | UTMIP_USBON_RPD_D | UTMIP_AP_D |
					UTMIP_AN_D;
		writel(val, pmc_base + PMC_SLEEPWALK_REG(inst));

		if (pmc_data->port_speed == USB_PMC_PORT_SPEED_LOW) {
			val = readl(pmc_base + PMC_SLEEPWALK_REG(inst));
			val &= ~(UTMIP_AN_B | UTMIP_HIGHZ_B | UTMIP_AN_C |
				UTMIP_HIGHZ_C | UTMIP_AN_D | UTMIP_HIGHZ_D);
			writel(val, pmc_base + PMC_SLEEPWALK_REG(inst));
		} else {
			val = readl(pmc_base + PMC_SLEEPWALK_REG(inst));
			val &= ~(UTMIP_AP_B | UTMIP_HIGHZ_B | UTMIP_AP_C |
				UTMIP_HIGHZ_C | UTMIP_AP_D | UTMIP_HIGHZ_D |
				UTMIP_AN_A);
				val |= UTMIP_AP_A;
			writel(val, pmc_base + PMC_SLEEPWALK_REG(inst));
		}
	} else {
		/* program walk sequence, pull down both dp and dn lines,
		* tristate lines once an hotplug-in wake event is detected */
		val = readl(pmc_base + PMC_SLEEPWALK_REG(inst));
		val |= UTMIP_USBOP_RPD_A | UTMIP_USBON_RPD_A | UTMIP_HIGHZ_A;
		val &= ~UTMIP_AP_A;
		val &= ~UTMIP_AN_A;
		val |= UTMIP_USBOP_RPD_B | UTMIP_USBON_RPD_B | UTMIP_HIGHZ_B;
		val &= ~UTMIP_AP_B;
		val &= ~UTMIP_AN_B;
		val |= UTMIP_USBOP_RPD_C | UTMIP_USBON_RPD_C | UTMIP_HIGHZ_C;
		val &= ~UTMIP_AP_C;
		val &= ~UTMIP_AN_C;
		val |= UTMIP_USBOP_RPD_D | UTMIP_USBON_RPD_D | UTMIP_HIGHZ_D;
		val &= ~UTMIP_AP_D;
		val &= ~UTMIP_AN_D;
		writel(val, pmc_base + PMC_SLEEPWALK_REG(inst));
	}
	/* turn on pad detectors */
	val = readl(pmc_base + PMC_USB_AO);
	val &= ~(USBOP_VAL_PD(inst) | USBON_VAL_PD(inst));
	writel(val, pmc_base + PMC_USB_AO);

	spin_unlock_irqrestore(&pmc_lock, flags);

	/* Add small delay before usb detectors provide stable line values */
	mdelay(1);

	spin_lock_irqsave(&pmc_lock, flags);

	/* Program thermally encoded RCTRL_VAL, TCTRL_VAL into PMC space */
	val = readl(pmc_base + PMC_UTMIP_TERM_PAD_CFG);
	val = PMC_TCTRL_VAL(pmc_data->utmip_tctrl_val) |
		PMC_RCTRL_VAL(pmc_data->utmip_rctrl_val);
	writel(val, pmc_base + PMC_UTMIP_TERM_PAD_CFG);

	/* Turn over pad configuration to PMC  for line wake events*/
	val = readl(pmc_base + PMC_SLEEP_CFG);
	val &= ~UTMIP_WAKE_VAL(inst, ~0);
	val |= UTMIP_WAKE_VAL(inst, WAKE_VAL_ANY);
	val |= UTMIP_RCTRL_USE_PMC(inst) | UTMIP_TCTRL_USE_PMC(inst);
	val |= UTMIP_MASTER_ENABLE(inst) | UTMIP_FSLS_USE_PMC(inst);
	writel(val, pmc_base + PMC_SLEEP_CFG);

	spin_unlock_irqrestore(&pmc_lock, flags);
}

static void utmip_phy_disable_pmc_bus_ctrl(struct tegra_usb_pmc_data *pmc_data)
{
	unsigned long val;
	unsigned  int inst = pmc_data->instance;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, pmc_data->instance);

	spin_lock_irqsave(&pmc_lock, flags);

	val = readl(pmc_base + PMC_SLEEP_CFG);
	val &= ~UTMIP_WAKE_VAL(inst, 0xF);
	val |= UTMIP_WAKE_VAL(inst, WAKE_VAL_NONE);
	writel(val, pmc_base + PMC_SLEEP_CFG);

	/* Disable PMC master mode by clearing MASTER_EN */
	val = readl(pmc_base + PMC_SLEEP_CFG);
	val &= ~(UTMIP_RCTRL_USE_PMC(inst) | UTMIP_TCTRL_USE_PMC(inst) |
			UTMIP_FSLS_USE_PMC(inst) | UTMIP_MASTER_ENABLE(inst));
	writel(val, pmc_base + PMC_SLEEP_CFG);

	val = readl(pmc_base + PMC_TRIGGERS);
	val &= ~UTMIP_CAP_CFG(inst);
	writel(val, pmc_base + PMC_TRIGGERS);

	/* turn off pad detectors */
	val = readl(pmc_base + PMC_USB_AO);
	val |= (USBOP_VAL_PD(inst) | USBON_VAL_PD(inst));
	writel(val, pmc_base + PMC_USB_AO);

	val = readl(pmc_base + PMC_TRIGGERS);
	val |= UTMIP_CLR_WALK_PTR(inst);
	val |= UTMIP_CLR_WAKE_ALARM(inst);
	writel(val, pmc_base + PMC_TRIGGERS);

	spin_unlock_irqrestore(&pmc_lock, flags);
}


static void utmip_powerdown_pmc_wake_detect(struct tegra_usb_pmc_data *pmc_data)
{
	unsigned long val;
	unsigned  int inst = pmc_data->instance;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, pmc_data->instance);

	spin_lock_irqsave(&pmc_lock, flags);

	/* power down UTMIP interfaces */
	val = readl(pmc_base + PMC_UTMIP_MASTER_CONFIG);
	val |= UTMIP_PWR(inst);
	writel(val, pmc_base + PMC_UTMIP_MASTER_CONFIG);

	/* setup sleep walk usb controller */
	val = UTMIP_USBOP_RPD_A | UTMIP_USBON_RPD_A | UTMIP_HIGHZ_A |
		UTMIP_USBOP_RPD_B | UTMIP_USBON_RPD_B | UTMIP_HIGHZ_B |
		UTMIP_USBOP_RPD_C | UTMIP_USBON_RPD_C | UTMIP_HIGHZ_C |
		UTMIP_USBOP_RPD_D | UTMIP_USBON_RPD_D | UTMIP_HIGHZ_D;
	writel(val, pmc_base + PMC_SLEEPWALK_REG(inst));

	/* Program thermally encoded RCTRL_VAL, TCTRL_VAL into PMC space */
	val = readl(pmc_base + PMC_UTMIP_TERM_PAD_CFG);
	val = PMC_TCTRL_VAL(pmc_data->utmip_tctrl_val) |
		PMC_RCTRL_VAL(pmc_data->utmip_rctrl_val);
	writel(val, pmc_base + PMC_UTMIP_TERM_PAD_CFG);

	/* Turn over pad configuration to PMC */
	val = readl(pmc_base + PMC_SLEEP_CFG);
	val &= ~UTMIP_WAKE_VAL(inst, ~0);
	val |= UTMIP_WAKE_VAL(inst, WAKE_VAL_NONE) |
		UTMIP_RCTRL_USE_PMC(inst) | UTMIP_TCTRL_USE_PMC(inst) |
		UTMIP_FSLS_USE_PMC(inst) | UTMIP_MASTER_ENABLE(inst);
	writel(val, pmc_base + PMC_SLEEP_CFG);

	spin_unlock_irqrestore(&pmc_lock, flags);
}

static void utmip_powerup_pmc_wake_detect(struct tegra_usb_pmc_data *pmc_data)
{
	unsigned long val;
	unsigned  int inst = pmc_data->instance;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, pmc_data->instance);

	spin_lock_irqsave(&pmc_lock, flags);

	/* Disable PMC master mode by clearing MASTER_EN */
	val = readl(pmc_base + PMC_SLEEP_CFG);
	val &= ~(UTMIP_RCTRL_USE_PMC(inst) | UTMIP_TCTRL_USE_PMC(inst) |
			UTMIP_FSLS_USE_PMC(inst) | UTMIP_MASTER_ENABLE(inst));
	writel(val, pmc_base + PMC_SLEEP_CFG);

	spin_unlock_irqrestore(&pmc_lock, flags);
	mdelay(1);
}

static void uhsic_powerup_pmc_wake_detect(struct tegra_usb_pmc_data *pmc_data)
{
	unsigned long val;
	unsigned int inst = pmc_data->instance;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, pmc_data->instance);

	spin_lock_irqsave(&pmc_lock, flags);

	/* turn on pad detectors for HSIC*/
	val = readl(pmc_base + PMC_USB_AO);
	val &= ~(HSIC_RESERVED(inst) | STROBE_VAL_PD(inst) | DATA_VAL_PD(inst));
	writel(val, pmc_base + PMC_USB_AO);

	/* Disable PMC master mode by clearing MASTER_EN */
	val = readl(pmc_base + PMC_UHSIC_SLEEP_CFG(inst));
	val &= ~(UHSIC_MASTER_ENABLE(inst));
	writel(val, pmc_base + PMC_UHSIC_SLEEP_CFG(inst));

	spin_unlock_irqrestore(&pmc_lock, flags);
	mdelay(1);
}

static void uhsic_powerdown_pmc_wake_detect(struct tegra_usb_pmc_data *pmc_data)
{
	unsigned long val;
	unsigned int inst = pmc_data->instance;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, pmc_data->instance);

	spin_lock_irqsave(&pmc_lock, flags);

	/* turn off pad detectors for HSIC*/
	val = readl(pmc_base + PMC_USB_AO);
	val |= (HSIC_RESERVED(inst) | STROBE_VAL_PD(inst) | DATA_VAL_PD(inst));
	writel(val, pmc_base + PMC_USB_AO);

	/* enable pull downs on HSIC PMC */
	val = UHSIC_STROBE_RPD_A | UHSIC_DATA_RPD_A | UHSIC_STROBE_RPD_B |
		UHSIC_DATA_RPD_B | UHSIC_STROBE_RPD_C | UHSIC_DATA_RPD_C |
		UHSIC_STROBE_RPD_D | UHSIC_DATA_RPD_D;
	writel(val, pmc_base + PMC_SLEEPWALK_UHSIC(inst));

	/* Turn over pad configuration to PMC */
	val = readl(pmc_base + PMC_UHSIC_SLEEP_CFG(inst));
	val &= ~UHSIC_WAKE_VAL(inst, ~0);
	val |= UHSIC_WAKE_VAL(inst, WAKE_VAL_NONE) | UHSIC_MASTER_ENABLE(inst);
	writel(val, pmc_base + PMC_UHSIC_SLEEP_CFG(inst));

	spin_unlock_irqrestore(&pmc_lock, flags);
}

static void uhsic_setup_pmc_wake_detect(struct tegra_usb_pmc_data *pmc_data)
{
	unsigned long val;
	unsigned int inst = pmc_data->instance;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, pmc_data->instance);

	spin_lock_irqsave(&pmc_lock, flags);

	/*Set PMC MASTER bits to do the following
	* a. Take over the hsic drivers
	* b. set up such that it will take over resume
	*	 if remote wakeup is detected
	* Prepare PMC to take over suspend-wake detect-drive resume until USB
	* controller ready
	*/

	/* disable master enable in PMC */
	val = readl(pmc_base + PMC_UHSIC_SLEEP_CFG(inst));
	val &= ~UHSIC_MASTER_ENABLE(inst);
	writel(val, pmc_base + PMC_UHSIC_SLEEP_CFG(inst));

	/* UTMIP_PWR_PX=1 for power savings mode */
	val = readl(pmc_base + PMC_UHSIC_MASTER_CONFIG(inst));
	val |= UHSIC_PWR(inst);
	writel(val, pmc_base + PMC_UHSIC_MASTER_CONFIG(inst));

	/* config debouncer */
	val = readl(pmc_base + PMC_USB_DEBOUNCE);
	val |= PMC_USB_DEBOUNCE_VAL(2);
	writel(val, pmc_base + PMC_USB_DEBOUNCE);

	/* Make sure nothing is happening on the line with respect to PMC */
	val = readl(pmc_base + PMC_UHSIC_FAKE(inst));
	val &= ~UHSIC_FAKE_STROBE_VAL(inst);
	val &= ~UHSIC_FAKE_DATA_VAL(inst);
	writel(val, pmc_base + PMC_UHSIC_FAKE(inst));

	/* Clear walk enable */
	val = readl(pmc_base + PMC_UHSIC_SLEEPWALK_CFG(inst));
	val &= ~UHSIC_LINEVAL_WALK_EN(inst);
	writel(val, pmc_base + PMC_UHSIC_SLEEPWALK_CFG(inst));

	/* Make sure wake value for line is none */
	val = readl(pmc_base + PMC_UHSIC_SLEEP_CFG(inst));
	val &= ~UHSIC_WAKE_VAL(inst, WAKE_VAL_ANY);
	val |= UHSIC_WAKE_VAL(inst, WAKE_VAL_NONE);
	writel(val, pmc_base + PMC_UHSIC_SLEEP_CFG(inst));

	/* turn on pad detectors */
	val = readl(pmc_base + PMC_USB_AO);
	val &= ~(STROBE_VAL_PD(inst) | DATA_VAL_PD(inst));
	writel(val, pmc_base + PMC_USB_AO);

	/* Add small delay before usb detectors provide stable line values */
	udelay(1);

	/* Enable which type of event can trigger a walk,
	* in this case usb_line_wake */
	val = readl(pmc_base + PMC_UHSIC_SLEEPWALK_CFG(inst));
	val |= UHSIC_LINEVAL_WALK_EN(inst);
	writel(val, pmc_base + PMC_UHSIC_SLEEPWALK_CFG(inst));

	/* program walk sequence, maintain a J, followed by a driven K
	* to signal a resume once an wake event is detected */

	val = readl(pmc_base + PMC_SLEEPWALK_UHSIC(inst));

	val &= ~UHSIC_DATA_RPU_A;
	val |=  UHSIC_DATA_RPD_A;
	val &= ~UHSIC_STROBE_RPD_A;
	val |=  UHSIC_STROBE_RPU_A;

	val &= ~UHSIC_DATA_RPD_B;
	val |=  UHSIC_DATA_RPU_B;
	val &= ~UHSIC_STROBE_RPU_B;
	val |=  UHSIC_STROBE_RPD_B;

	val &= ~UHSIC_DATA_RPD_C;
	val |=  UHSIC_DATA_RPU_C;
	val &= ~UHSIC_STROBE_RPU_C;
	val |=  UHSIC_STROBE_RPD_C;

	val &= ~UHSIC_DATA_RPD_D;
	val |=  UHSIC_DATA_RPU_D;
	val &= ~UHSIC_STROBE_RPU_D;
	val |=  UHSIC_STROBE_RPD_D;
	writel(val, pmc_base + PMC_SLEEPWALK_UHSIC(inst));

	/* Setting Wake event*/
	val = readl(pmc_base + PMC_UHSIC_SLEEP_CFG(inst));
	val &= ~UHSIC_WAKE_VAL(inst, WAKE_VAL_ANY);
	val |= UHSIC_WAKE_VAL(inst, WAKE_VAL_SD10);
	writel(val, pmc_base + PMC_UHSIC_SLEEP_CFG(inst));

	/* Clear the walk pointers and wake alarm */
	val = readl(pmc_base + PMC_UHSIC_TRIGGERS(inst));
	val |= UHSIC_CLR_WAKE_ALARM(inst) | UHSIC_CLR_WALK_PTR(inst);
	writel(val, pmc_base + PMC_UHSIC_TRIGGERS(inst));

	/* Turn over pad configuration to PMC  for line wake events*/
	val = readl(pmc_base + PMC_UHSIC_SLEEP_CFG(inst));
	val |= UHSIC_MASTER_ENABLE(inst);
	writel(val, pmc_base + PMC_UHSIC_SLEEP_CFG(inst));

	spin_unlock_irqrestore(&pmc_lock, flags);
	DBG("%s:PMC enabled for HSIC remote wakeup\n", __func__);
}

static void uhsic_phy_disable_pmc_bus_ctrl(struct tegra_usb_pmc_data *pmc_data)
{
	unsigned long val;
	unsigned int inst = pmc_data->instance;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, pmc_data->instance);

	spin_lock_irqsave(&pmc_lock, flags);

	val = readl(pmc_base + PMC_UHSIC_SLEEP_CFG(inst));
	val &= ~UHSIC_WAKE_VAL(inst, WAKE_VAL_ANY);
	val |= UHSIC_WAKE_VAL(inst, WAKE_VAL_NONE);
	writel(val, pmc_base + PMC_UHSIC_SLEEP_CFG(inst));

	/* Disable PMC master mode by clearing MASTER_EN */
	val = readl(pmc_base + PMC_UHSIC_SLEEP_CFG(inst));
	val &= ~(UHSIC_MASTER_ENABLE(inst));
	writel(val, pmc_base + PMC_UHSIC_SLEEP_CFG(inst));

	/* turn off pad detectors */
	val = readl(pmc_base + PMC_USB_AO);
	val |= (STROBE_VAL_PD(inst) | DATA_VAL_PD(inst));
	writel(val, pmc_base + PMC_USB_AO);

	val = readl(pmc_base + PMC_UHSIC_TRIGGERS(inst));
	val |= (UHSIC_CLR_WALK_PTR(inst) | UHSIC_CLR_WAKE_ALARM(inst));
	writel(val, pmc_base + PMC_UHSIC_TRIGGERS(inst));

	spin_unlock_irqrestore(&pmc_lock, flags);
}

static struct tegra_usb_pmc_ops usb2_utmi_pmc_ops = {
	.setup_pmc_wake_detect = utmip_setup_pmc_wake_detect,
	.disable_pmc_bus_ctrl = utmip_phy_disable_pmc_bus_ctrl,
	.powerup_pmc_wake_detect = utmip_powerup_pmc_wake_detect,
	.powerdown_pmc_wake_detect = utmip_powerdown_pmc_wake_detect,
};

static struct tegra_usb_pmc_ops usb2_hsic_pmc_ops = {
	.setup_pmc_wake_detect = uhsic_setup_pmc_wake_detect,
	.disable_pmc_bus_ctrl = uhsic_phy_disable_pmc_bus_ctrl,
	.powerup_pmc_wake_detect = uhsic_powerup_pmc_wake_detect,
	.powerdown_pmc_wake_detect = uhsic_powerdown_pmc_wake_detect,
};

static struct tegra_usb_pmc_ops *pmc_ops[] = {
	[TEGRA_USB_PHY_INTF_UTMI] = &usb2_utmi_pmc_ops,
	[TEGRA_USB_PHY_INTF_HSIC] = &usb2_hsic_pmc_ops,
};

void tegra_usb_pmc_init(struct tegra_usb_pmc_data *pmc_data)
{
	static u32 utmip_rctrl_val;
	static u32 utmip_tctrl_val;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, pmc_data->instance);

	if (!pmc_base)
		pmc_base = IO_ADDRESS(TEGRA_PMC_BASE);

#ifdef KERNEL_WARNING
	pmc_data->pmc_ops.power_down_pmc = usb_phy_power_down_pmc;
#endif
	if (pmc_data->phy_type == TEGRA_USB_PHY_INTF_UTMI) {
		if (pmc_data->instance == 0) {
			utmip_rctrl_val = pmc_data->utmip_rctrl_val;
			utmip_tctrl_val = pmc_data->utmip_tctrl_val;
		} else {
			pmc_data->utmip_rctrl_val = utmip_rctrl_val;
			pmc_data->utmip_tctrl_val = utmip_tctrl_val;
		}
	}
	pmc_data->pmc_ops = pmc_ops[pmc_data->phy_type];
}
EXPORT_SYMBOL_GPL(tegra_usb_pmc_init);

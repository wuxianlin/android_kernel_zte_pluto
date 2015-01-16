/**********************************************************************************
 * Si8240 Linux Driver
 *
 * Copyright (C) 2011-2012 Silicon Image Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed .as is. WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the
 * GNU General Public License for more details.
 *
 **********************************************************************************/


/**
 * @file sii_hal_linux_gpio.c
 *
 * @brief Linux implementation of GPIO pin support needed by Silicon Image
 *        MHL devices.
 *
 * $Author: Dave Canfield
 * $Rev: $
 * $Date: Feb. 9, 2011
 *
 *****************************************************************************/

#define SII_HAL_LINUX_GPIO_C

/***** #include statements ***************************************************/
#include "sii_hal.h"
#include "sii_hal_priv.h"
#include "si_c99support.h"
#include "si_osdebug.h"

/***** local macro definitions ***********************************************/


/***** local type definitions ************************************************/


/***** local variable declarations *******************************************/


/***** local function prototypes *********************************************/


/***** global variable declarations *******************************************/


// Simulate the DIP switches
bool	pinDbgMsgs	= false;	// simulated pinDbgSw2 0=Print
bool	pinAllowD3	= true;	// false allows debugging
bool	pinOverrideTiming = true;	// simulated pinDbgSw2
bool	pinDataLaneL	= true;		// simulated pinDbgSw3
bool	pinDataLaneH	= true;		// simulated pinDbgSw4
bool	pinDoHdcp	= true;		// simulated pinDbgSw5
bool    pinWakePulseEn = true;   // wake pulses enabled by default
bool	pinPackedPixelUserControl=false;
bool	pinPackedPixelUserOn=true;
bool	pinTranscodeMode=false;

//	 Simulate the GPIO pins
bool	 pinTxHwReset	= false;	// simulated reset pin %%%% TODO possible on Beagle?
bool	 pinM2uVbusCtrlM	= true;		// Active high, needs to be low in MHL connected state, high otherwise.
bool	 pinVbusEnM	= true;		// Active high input. If high sk is providing power, otherwise not.
bool	pinMhlVbusSense=false;

// Simulate the LEDs
bool	pinMhlConn	= true;		// MHL Connected LED 
bool	pinUsbConn	= false;	// USB connected LED
bool	pinSourceVbusOn	= true;		// Active low LED. On when pinMhlVbusSense and pinVbusEnM are active
bool	pinSinkVbusOn	= true;		// Active low LED. On when pinMhlVbusSense is active and pinVbusEnM is not active


/***** local functions *******************************************************/


/***** public functions ******************************************************/


/*****************************************************************************/
/**
 * @brief Configure platform GPIOs needed by the MHL device.
 *
 *****************************************************************************/
halReturn_t HalGpioInit(void)
{
	int status;

	/* Configure GPIO used to perform a hard reset of the device. */
	status = gpio_request(W_RST_GPIO, "W_RST#");
	if (status < 0)
	{
    	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
    			"HalInit gpio_request for GPIO %d (H/W Reset) failed, status: %d\n",
    			W_RST_GPIO, status);
		return HAL_RET_FAILURE;
	}

	status = gpio_direction_output(W_RST_GPIO, 1);
	if (status < 0)
	{
    	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
    			"HalInit gpio_direction_output for GPIO %d (H/W Reset) failed, status: %d\n",
    			W_RST_GPIO, status);
		gpio_free(W_RST_GPIO);
		return HAL_RET_FAILURE;
	}

#ifdef MAKE_8240_DRIVER //(
    // don't do the stuff in the else branch
#elif defined(MAKE_833X_DRIVER) //)(
    // don't do the stuff in the else branch
#else //)(

	/* Configure GPIO used to control USB VBUS power. */
	status = gpio_request(M2U_VBUS_CTRL_M, "W_RST#");
	if (status < 0)
	{
    	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
    			"HalInit gpio_request for GPIO %d (VBUS) failed, status: %d\n",
    			M2U_VBUS_CTRL_M, status);
		return HAL_RET_FAILURE;
	}

	status = gpio_direction_output(M2U_VBUS_CTRL_M, 0);
	if (status < 0)
	{
    	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
    			"HalInit gpio_direction_output for GPIO %d (VBUS) failed, status: %d\n",
    			M2U_VBUS_CTRL_M, status);
		gpio_free(W_RST_GPIO);
		gpio_free(M2U_VBUS_CTRL_M);
		return HAL_RET_FAILURE;
	}
#endif	//)

	/*
	 * Configure the GPIO used as an interrupt input from the device
	 * NOTE: GPIO support should probably be initialized BEFORE enabling
	 * interrupt support
	 */
	status = gpio_request(W_INT_GPIO, "W_INT");
	if(status < 0)
	{
    	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
    			"HalInitGpio gpio_request for GPIO %d (interrupt)failed, status: %d\n",
    			W_INT_GPIO, status);
		gpio_free(W_RST_GPIO);
		return HAL_RET_FAILURE;
	}

	status = gpio_direction_input(W_INT_GPIO);
	if(status < 0)
	{
    	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
    			"HalInitGpio gpio_direction_input for GPIO %d (interrupt)failed, status: %d",
    			W_INT_GPIO, status);
		gpio_free(W_INT_GPIO);
		gpio_free(W_RST_GPIO);
#ifdef MAKE_8240_DRIVER //(
    // don't do the stuff in the else branch
#elif defined(MAKE_833X_DRIVER) //)(
    // don't do the stuff in the else branch
#else //)(
		gpio_free(M2U_VBUS_CTRL_M);
#endif //)
		return HAL_RET_FAILURE;
	}

	return HAL_RET_SUCCESS;
}



/*****************************************************************************/
/**
 * @brief Release GPIO pins needed by the MHL device.
 *
 *****************************************************************************/
halReturn_t HalGpioTerm(void)
{
	halReturn_t 	halRet;

	halRet = HalInitCheck();
	if(halRet != HAL_RET_SUCCESS)
	{
		return halRet;
	}

	gpio_free(W_INT_GPIO);
	gpio_free(W_RST_GPIO);
#ifdef MAKE_8240_DRIVER //(
    // don't do the stuff in the else branch
#elif defined(MAKE_833X_DRIVER) //)(
    // don't do the stuff in the else branch
#else //)(
	gpio_free(M2U_VBUS_CTRL_M);
#endif //)

	return HAL_RET_SUCCESS;
}


/*****************************************************************************/
/**
 * @brief Platform specific function to control the reset pin of the MHL
 * 		  transmitter device.
 *
 *****************************************************************************/
int HalGpioGetTxIntPin(void)
{
	halReturn_t 	halRet;

	halRet = HalInitCheck();
	if(halRet != HAL_RET_SUCCESS)
	{
		return -1;
	}

	return gpio_get_value(W_INT_GPIO);
}


/*****************************************************************************/
/**
 * @brief Platform specific function to control the reset pin of the MHL
 * 		  transmitter device.
 *
 *****************************************************************************/
halReturn_t HalGpioSetTxResetPin(bool value)
{
	halReturn_t 	halRet;

	halRet = HalInitCheck();
	if(halRet != HAL_RET_SUCCESS)
	{
		return halRet;
	}

	gpio_set_value(W_RST_GPIO, value);
	return HAL_RET_SUCCESS;
}



/*****************************************************************************/
/**
 * @brief Platform specific function to control power on the USB port.
 *
 *****************************************************************************/
halReturn_t HalGpioSetUsbVbusPowerPin(bool value)
{
	halReturn_t 	halRet;

	halRet = HalInitCheck();
	if(halRet != HAL_RET_SUCCESS)
	{
		return halRet;
	}

	gpio_set_value(M2U_VBUS_CTRL_M, value);
	return HAL_RET_SUCCESS;
}



/*****************************************************************************/
/**
 * @brief Platform specific function to control Vbus power on the MHL port.
 *
 *****************************************************************************/
halReturn_t HalGpioSetVbusPowerPin(bool powerOn)
{
	halReturn_t 	halRet;

	halRet = HalInitCheck();
	if(halRet != HAL_RET_SUCCESS)
	{
		return halRet;
	}

	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
			"HalGpioSetVbusPowerPin called but this function is not implemented yet!\n");

	return HAL_RET_SUCCESS;
}

/*
 * Platform data for MAX98096
 *
 * Copyright 2012 Maxim Integrated Products
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef __SOUND_MAX98096_PDATA_H__
#define __SOUND_MAX98096_PDATA_H__

/* This determines which interface, SPI or I2C is used */
#define INTERFACE_SPI
//#undef INTERFACE_SPI

#ifdef INTERFACE_SPI
#warning "<><><><><> BUILDING FOR SPI <><><><><>"
#else
#warning "<><><><><> BUILDING FOR I2C <><><><><>"
#endif

#include "max9809X.h"

#endif

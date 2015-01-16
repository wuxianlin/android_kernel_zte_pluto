/*
 * max9809X.c -- MAX9809X ALSA SoC Audio driver
 *
 * Copyright 2011 Maxim Integrated Products
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/spi/spi.h>
#include <linux/spinlock.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <asm/div64.h>
#include <sound/max9809X.h>
#include "max9809X.h"
#include "max9809X-dsp.h"
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/err.h>
#include <linux/version.h>
#ifdef CONFIG_SWITCH
#include <linux/switch.h>
#include <linux/input.h>
#include <mach/dma.h>
#endif
#include <linux/spi/spi.h>
#include <linux/spi-tegra.h>
#include "../../../../arch/arm/mach-tegra/board-pluto.h"
#include "../../../../arch/arm/mach-tegra/gpio-names.h"
struct snd_soc_codec *max9809X_codec;
extern void dsp_fs_setup(struct snd_soc_codec *codec);

extern long max9809X_hp_time;
extern int state_hp;
static long max9809X_mic_time=0;

#define DEBUG 1

static const u8 max9809X_reg_def[M9809X_REG_CNT] = {
    0x00, /* 00 */
    0x00, /* 01 */
    0x00, /* 02 */
    0x00, /* 03 */
    0x00, /* 04 */
    0x00, /* 05 */
    0x00, /* 06 */
    0x00, /* 07 */
    0x00, /* 08 */
    0x00, /* 09 */
    0x00, /* 0A */
    0x00, /* 0B */
    0x00, /* 0C */
    0x00, /* 0D */
    0x00, /* 0E */
    0x00, /* 0F */
    0x00, /* 10 */
    0x00, /* 11 */
    0x00, /* 12 */
    0x00, /* 13 */
    0x00, /* 14 */
    0x00, /* 15 */
    0x00, /* 16 */
    0x00, /* 17 */
    0x00, /* 18 */
    0x00, /* 19 */
    0x00, /* 1A */
    0x00, /* 1B */
    0x00, /* 1C */
    0x00, /* 1D */
    0x00, /* 1E */
    0x00, /* 1F */
    0x00, /* 20 */
    0x00, /* 21 */
    0x00, /* 22 */
    0x00, /* 23 */
    0x00, /* 24 */
    0x00, /* 25 */
    0x00, /* 26 */
    0x00, /* 27 */
    0x00, /* 28 */
    0x00, /* 29 */
    0x00, /* 2A */
    0x00, /* 2B */
    0x00, /* 2C */
    0x00, /* 2D */
    0x00, /* 2E */
    0x00, /* 2F */
    0x00, /* 30 */
    0x00, /* 31 */
    0x00, /* 32 */
    0x00, /* 33 */
    0x00, /* 34 */
    0x00, /* 35 */
    0x00, /* 36 */
    0x00, /* 37 */
    0x00, /* 38 */
    0x00, /* 39 */
    0x00, /* 3A */
    0x00, /* 3B */
    0x00, /* 3C */
    0x00, /* 3D */
    0x00, /* 3E */
    0x00, /* 3F */
    0x00, /* 40 */
    0x00, /* 41 */
    0x00, /* 42 */
    0x00, /* 43 */
    0x00, /* 44 */
    0x00, /* 45 */
    0x00, /* 46 */
    0x00, /* 47 */
    0x00, /* 48 */
    0x00, /* 49 */
    0x00, /* 4A */
    0x00, /* 4B */
    0x00, /* 4C */
    0x00, /* 4D */
    0x00, /* 4E */
    0x00, /* 4F */
    0x00, /* 50 */
    0x00, /* 51 */
    0x00, /* 52 */
    0x00, /* 53 */
    0x00, /* 54 */
    0x00, /* 55 */
    0x00, /* 56 */
    0x00, /* 57 */
    0x00, /* 58 */
    0x00, /* 59 */
    0x00, /* 5A */
    0x00, /* 5B */
    0x00, /* 5C */
    0x00, /* 5D */
    0x00, /* 5E */
    0x00, /* 5F */
    0x00, /* 60 */
    0x00, /* 61 */
    0x00, /* 62 */
    0x00, /* 63 */
    0x00, /* 64 */
    0x00, /* 65 */
    0x00, /* 66 */
    0x00, /* 67 */
    0x00, /* 68 */
    0x00, /* 69 */
    0x00, /* 6A */
    0x00, /* 6B */
    0x00, /* 6C */
    0x00, /* 6D */
    0x00, /* 6E */
    0x00, /* 6F */
    0x00, /* 70 */
    0x00, /* 71 */
    0x00, /* 72 */
    0x00, /* 73 */
    0x00, /* 74 */
    0x00, /* 75 */
    0x00, /* 76 */
    0x00, /* 77 */
    0x00, /* 78 */
    0x00, /* 79 */
    0x00, /* 7A */
    0x00, /* 7B */
    0x00, /* 7C */
    0x00, /* 7D */
    0x00, /* 7E */
    0x00, /* 7F */
    0x00, /* 80 */
    0x00, /* 81 */
    0x00, /* 82 */
    0x00, /* 83 */
    0x00, /* 84 */
    0x00, /* 85 */
    0x00, /* 86 */
    0x00, /* 87 */
    0x00, /* 88 */
    0x00, /* 89 */
    0x00, /* 8A */
    0x00, /* 8B */
    0x00, /* 8C */
    0x00, /* 8D */
    0x00, /* 8E */
    0x00, /* 8F */
    0x00, /* 90 */
    0x00, /* 91 */
    0x30, /* 92 */
    0xF0, /* 93 */
    0x00, /* 94 */
    0x00, /* 95 */
    0x3F, /* 96 */
    0x00, /* 97 */
    0x00, /* 98 */
    0x00, /* 99 */
    0x00, /* 9A */
    0x00, /* 9B */
    0x00, /* 9C */
    0x00, /* 9D */
    0x00, /* 9E */
    0x00, /* 9F */
    0x00, /* A0 */
    0x00, /* A1 */
    0x00, /* A2 */
    0x00, /* A3 */
    0x00, /* A4 */
    0x00, /* A5 */
    0x00, /* A6 */
    0x00, /* A7 */
    0x00, /* A8 */
    0x00, /* A9 */
    0x00, /* AA */
    0x00, /* AB */
    0x00, /* AC */
    0x00, /* AD */
    0x00, /* AE */
    0x00, /* AF */
    0x00, /* B0 */
    0x00, /* B1 */
    0x00, /* B2 */
    0x00, /* B3 */
    0x00, /* B4 */
    0x00, /* B5 */
    0x00, /* B6 */
    0x00, /* B7 */
    0x00, /* B8 */
    0x00, /* B9 */
    0x00, /* BA */
    0x00, /* BB */
    0x00, /* BC */
    0x00, /* BD */
    0x00, /* BE */
    0x00, /* BF */
    0x00, /* C0 */
    0x00, /* C1 */
    0x00, /* C2 */
    0x00, /* C3 */
    0x00, /* C4 */
    0x00, /* C5 */
    0x00, /* C6 */
    0x00, /* C7 */
    0x00, /* C8 */
    0x00, /* C9 */
    0x00, /* CA */
    0x00, /* CB */
    0x00, /* CC */
    0x00, /* CD */
    0x00, /* CE */
    0x00, /* CF */
    0x00, /* D0 */
    0x00, /* D1 */
    0x00, /* D2 */
    0x00, /* D3 */
    0x00, /* D4 */
    0x00, /* D5 */
    0x00, /* D6 */
    0x00, /* D7 */
    0x00, /* D8 */
    0x00, /* D9 */
    0x00, /* DA */
    0x00, /* DB */
    0x00, /* DC */
    0x00, /* DD */
    0x00, /* DE */
    0x00, /* DF */
    0x00, /* E0 */
    0x00, /* E1 */
    0x00, /* E2 */
    0x00, /* E3 */
    0x00, /* E4 */
    0x00, /* E5 */
    0x00, /* E6 */
    0x00, /* E7 */
    0x00, /* E8 */
    0x00, /* E9 */
    0x00, /* EA */
    0x00, /* EB */
    0x00, /* EC */
    0x00, /* ED */
    0x00, /* EE */
    0x00, /* EF */
    0x00, /* F0 */
    0x00, /* F1 */
    0x00, /* F2 */
    0x00, /* F3 */
    0x00, /* F4 */
    0x00, /* F5 */
    0x00, /* F6 */
    0x00, /* F7 */
    0x00, /* F8 */
    0x00, /* F9 */
    0x00, /* FA */
    0x00, /* FB */
    0x00, /* FC */
    0x00, /* FD */
    0x00, /* FE */
    0x00, /* FF */
};

static struct {
    int readable;
    int writable;
} max9809X_access[M9809X_REG_CNT] = {
    { 0x00, 0x00 }, /* 00 */
    { 0xFF, 0x00 }, /* 01 */
    { 0xFF, 0x00 }, /* 02 */
    { 0xFF, 0x00 }, /* 03 */
    { 0xFF, 0x00 }, /* 04 */
    { 0xFF, 0x00 }, /* 05 */
    { 0xFF, 0x00 }, /* 06 */
    { 0xFF, 0x00 }, /* 07 */
    { 0xFF, 0x00 }, /* 08 */
    { 0xFF, 0x00 }, /* 09 */
    { 0xFF, 0x00 }, /* 0A */
    { 0xFF, 0x00 }, /* 0B */
    { 0xFF, 0x00 }, /* 0C */
    { 0xFF, 0x00 }, /* 0D */
    { 0xFF, 0x00 }, /* 0E */
    { 0xFF, 0x9F }, /* 0F */
    { 0xFF, 0xFF }, /* 10 */
    { 0xFF, 0xFF }, /* 11 */
    { 0xFF, 0xFF }, /* 12 */
    { 0xFF, 0xFF }, /* 13 */
    { 0xFF, 0xFF }, /* 14 */
    { 0xFF, 0xFF }, /* 15 */
    { 0xFF, 0xFF }, /* 16 */
    { 0xFF, 0xFF }, /* 17 */
    { 0xFF, 0xFF }, /* 18 */
    { 0xFF, 0xFF }, /* 19 */
    { 0xFF, 0xFF }, /* 1A */
    { 0xFF, 0xFF }, /* 1B */
    { 0xFF, 0xFF }, /* 1C */
    { 0xFF, 0xFF }, /* 1D */
    { 0xFF, 0x77 }, /* 1E */
    { 0xFF, 0x77 }, /* 1F */
    { 0xFF, 0x77 }, /* 20 */
    { 0xFF, 0x77 }, /* 21 */
    { 0xFF, 0x77 }, /* 22 */
    { 0xFF, 0x77 }, /* 23 */
    { 0xFF, 0xFF }, /* 24 */
    { 0xFF, 0x7F }, /* 25 */
    { 0xFF, 0x31 }, /* 26 */
    { 0xFF, 0xFF }, /* 27 */
    { 0xFF, 0xFF }, /* 28 */
    { 0xFF, 0xFF }, /* 29 */
    { 0xFF, 0xF7 }, /* 2A */
    { 0xFF, 0x2F }, /* 2B */
    { 0xFF, 0xEF }, /* 2C */
    { 0xFF, 0xFF }, /* 2D */
    { 0xFF, 0xFF }, /* 2E */
    { 0xFF, 0xFF }, /* 2F */
    { 0xFF, 0xFF }, /* 30 */
    { 0xFF, 0xFF }, /* 31 */
    { 0xFF, 0xFF }, /* 32 */
    { 0xFF, 0xFF }, /* 33 */
    { 0xFF, 0xF7 }, /* 34 */
    { 0xFF, 0x2F }, /* 35 */
    { 0xFF, 0xCF }, /* 36 */
    { 0xFF, 0xFF }, /* 37 */
    { 0xFF, 0xFF }, /* 38 */
    { 0xFF, 0xFF }, /* 39 */
    { 0xFF, 0xFF }, /* 3A */
    { 0xFF, 0xFF }, /* 3B */
    { 0xFF, 0xFF }, /* 3C */
    { 0xFF, 0xFF }, /* 3D */
    { 0xFF, 0xF7 }, /* 3E */
    { 0xFF, 0x2F }, /* 3F */
    { 0xFF, 0xCF }, /* 40 */
    { 0xFF, 0xFF }, /* 41 */
    { 0xFF, 0x77 }, /* 42 */
    { 0xFF, 0xFF }, /* 43 */
    { 0xFF, 0xFF }, /* 44 */
    { 0xFF, 0xFF }, /* 45 */
    { 0xFF, 0xFF }, /* 46 */
    { 0xFF, 0xFF }, /* 47 */
    { 0xFF, 0xFF }, /* 48 */
    { 0xFF, 0x0F }, /* 49 */
    { 0xFF, 0xFF }, /* 4A */
    { 0xFF, 0xFF }, /* 4B */
    { 0xFF, 0x3F }, /* 4C */
    { 0xFF, 0x3F }, /* 4D */
    { 0xFF, 0x3F }, /* 4E */
    { 0xFF, 0xFF }, /* 4F */
    { 0xFF, 0x7F }, /* 50 */
    { 0xFF, 0x7F }, /* 51 */
    { 0xFF, 0x0F }, /* 52 */
    { 0xFF, 0x3F }, /* 53 */
    { 0xFF, 0x3F }, /* 54 */
    { 0xFF, 0x3F }, /* 55 */
    { 0xFF, 0xFF }, /* 56 */
    { 0xFF, 0xFF }, /* 57 */
    { 0xFF, 0xBF }, /* 58 */
    { 0xFF, 0x1F }, /* 59 */
    { 0xFF, 0xBF }, /* 5A */
    { 0xFF, 0x1F }, /* 5B */
    { 0xFF, 0xBF }, /* 5C */
    { 0xFF, 0x3F }, /* 5D */
    { 0xFF, 0x3F }, /* 5E */
    { 0xFF, 0x7F }, /* 5F */
    { 0xFF, 0x7F }, /* 60 */
    { 0xFF, 0x47 }, /* 61 */
    { 0xFF, 0x9F }, /* 62 */
    { 0xFF, 0x9F }, /* 63 */
    { 0xFF, 0x9F }, /* 64 */
    { 0xFF, 0x9F }, /* 65 */
    { 0xFF, 0x9F }, /* 66 */
    { 0xFF, 0xBF }, /* 67 */
    { 0xFF, 0xBF }, /* 68 */
    { 0xFF, 0xFF }, /* 69 */
    { 0xFF, 0xFF }, /* 6A */
    { 0xFF, 0x7F }, /* 6B */
    { 0xFF, 0xF7 }, /* 6C */
    { 0xFF, 0xFF }, /* 6D */
    { 0xFF, 0xFF }, /* 6E */
    { 0xFF, 0x1F }, /* 6F */
    { 0xFF, 0xF7 }, /* 70 */
    { 0xFF, 0xFF }, /* 71 */
    { 0xFF, 0xFF }, /* 72 */
    { 0xFF, 0x1F }, /* 73 */
    { 0xFF, 0xF7 }, /* 74 */
    { 0xFF, 0xFF }, /* 75 */
    { 0xFF, 0xFF }, /* 76 */
    { 0xFF, 0x1F }, /* 77 */
    { 0xFF, 0xF7 }, /* 78 */
    { 0xFF, 0xFF }, /* 79 */
    { 0xFF, 0xFF }, /* 7A */
    { 0xFF, 0x1F }, /* 7B */
    { 0xFF, 0xF7 }, /* 7C */
    { 0xFF, 0xFF }, /* 7D */
    { 0xFF, 0xFF }, /* 7E */
    { 0xFF, 0x1F }, /* 7F */
    { 0xFF, 0xF7 }, /* 80 */
    { 0xFF, 0xFF }, /* 81 */
    { 0xFF, 0xFF }, /* 82 */
    { 0xFF, 0x1F }, /* 83 */
    { 0xFF, 0x7F }, /* 84 */
    { 0xFF, 0x0F }, /* 85 */
    { 0xFF, 0xD8 }, /* 86 */
    { 0xFF, 0xFF }, /* 87 */
    { 0xFF, 0xEF }, /* 88 */
    { 0xFF, 0xFE }, /* 89 */
    { 0xFF, 0xFE }, /* 8A */
    { 0xFF, 0xFF }, /* 8B */
    { 0xFF, 0xFF }, /* 8C */
    { 0xFF, 0x3F }, /* 8D */
    { 0xFF, 0xFF }, /* 8E */
    { 0xFF, 0x3F }, /* 8F */
    { 0xFF, 0x8F }, /* 90 */
    { 0xFF, 0xFF }, /* 91 */
    { 0xFF, 0x3F }, /* 92 */
    { 0xFF, 0xFF }, /* 93 */
    { 0xFF, 0xFF }, /* 94 */
    { 0xFF, 0x0F }, /* 95 */
    { 0xFF, 0x3F }, /* 96 */
    { 0xFF, 0x8C }, /* 97 */
    { 0x00, 0x00 }, /* 98 */
    { 0x00, 0x00 }, /* 99 */
    { 0x00, 0x00 }, /* 9A */
    { 0x00, 0x00 }, /* 9B */
    { 0x00, 0x00 }, /* 9C */
    { 0x00, 0x00 }, /* 9D */
    { 0x00, 0x00 }, /* 9E */
    { 0x00, 0x00 }, /* 9F */
    { 0x00, 0x00 }, /* A0 */
    { 0x00, 0x00 }, /* A1 */
    { 0x00, 0x00 }, /* A2 */
    { 0x00, 0x00 }, /* A3 */
    { 0x00, 0x00 }, /* A4 */
    { 0x00, 0x00 }, /* A5 */
    { 0x00, 0x00 }, /* A6 */
    { 0x00, 0x00 }, /* A7 */
    { 0x00, 0x00 }, /* A8 */
    { 0x00, 0x00 }, /* A9 */
    { 0x00, 0x00 }, /* AA */
    { 0x00, 0x00 }, /* AB */
    { 0x00, 0x00 }, /* AC */
    { 0x00, 0x00 }, /* AD */
    { 0x00, 0x00 }, /* AE */
    { 0x00, 0x00 }, /* AF */
    { 0x00, 0x00 }, /* B0 */
    { 0x00, 0x00 }, /* B1 */
    { 0x00, 0x00 }, /* B2 */
    { 0x00, 0x00 }, /* B3 */
    { 0x00, 0x00 }, /* B4 */
    { 0x00, 0x00 }, /* B5 */
    { 0x00, 0x00 }, /* B6 */
    { 0x00, 0x00 }, /* B7 */
    { 0x00, 0x00 }, /* B8 */
    { 0x00, 0x00 }, /* B9 */
    { 0x00, 0x00 }, /* BA */
    { 0x00, 0x00 }, /* BB */
    { 0x00, 0x00 }, /* BC */
    { 0x00, 0x00 }, /* BD */
    { 0x00, 0x00 }, /* BE */
    { 0x00, 0x00 }, /* BF */
    { 0x00, 0x00 }, /* C0 */
    { 0x00, 0x00 }, /* C1 */
    { 0x00, 0x00 }, /* C2 */
    { 0x00, 0x00 }, /* C3 */
    { 0x00, 0x00 }, /* C4 */
    { 0x00, 0x00 }, /* C5 */
    { 0x00, 0x00 }, /* C6 */
    { 0x00, 0x00 }, /* C7 */
    { 0x00, 0x00 }, /* C8 */
    { 0x00, 0x00 }, /* C9 */
    { 0x00, 0x00 }, /* CA */
    { 0x00, 0x00 }, /* CB */
    { 0x00, 0x00 }, /* CC */
    { 0x00, 0x00 }, /* CD */
    { 0x00, 0x00 }, /* CE */
    { 0x00, 0x00 }, /* CF */
    { 0x00, 0x00 }, /* D0 */
    { 0x00, 0x00 }, /* D1 */
    { 0x00, 0x00 }, /* D2 */
    { 0x00, 0x00 }, /* D3 */
    { 0x00, 0x00 }, /* D4 */
    { 0x00, 0x00 }, /* D5 */
    { 0x00, 0x00 }, /* D6 */
    { 0x00, 0x00 }, /* D7 */
    { 0x00, 0x00 }, /* D8 */
    { 0x00, 0x00 }, /* D9 */
    { 0x00, 0x00 }, /* DA */
    { 0x00, 0x00 }, /* DB */
    { 0x00, 0x00 }, /* DC */
    { 0x00, 0x00 }, /* DD */
    { 0x00, 0x00 }, /* DE */
    { 0x00, 0x00 }, /* DF */
    { 0x00, 0x00 }, /* E0 */
    { 0x00, 0x00 }, /* E1 */
    { 0x00, 0x00 }, /* E2 */
    { 0x00, 0x00 }, /* E3 */
    { 0x00, 0x00 }, /* E4 */
    { 0x00, 0x00 }, /* E5 */
    { 0x00, 0x00 }, /* E6 */
    { 0x00, 0x00 }, /* E7 */
    { 0x00, 0x00 }, /* E8 */
    { 0x00, 0x00 }, /* E9 */
    { 0x00, 0x00 }, /* EA */
    { 0x00, 0x00 }, /* EB */
    { 0x00, 0x00 }, /* EC */
    { 0x00, 0x00 }, /* ED */
    { 0x00, 0x00 }, /* EE */
    { 0x00, 0x00 }, /* EF */
    { 0x00, 0x00 }, /* F0 */
    { 0x00, 0x00 }, /* F1 */
    { 0x00, 0x00 }, /* F2 */
    { 0x00, 0x00 }, /* F3 */
    { 0x00, 0x00 }, /* F4 */
    { 0x00, 0x00 }, /* F5 */
    { 0x00, 0x00 }, /* F6 */
    { 0x00, 0x00 }, /* F7 */
    { 0x00, 0x00 }, /* F8 */
    { 0x00, 0x00 }, /* F9 */
    { 0x00, 0x00 }, /* FA */
    { 0x00, 0x00 }, /* FB */
    { 0x00, 0x00 }, /* FC */
    { 0x00, 0x00 }, /* FD */
    { 0x00, 0x00 }, /* FE */
    { 0xFF, 0x00 }, /* FF */
};

static int max9809X_readable(struct snd_soc_codec *codec, unsigned int reg)
{
	if (reg >= M9809X_REG_CNT)
		return 0;
	return max9809X_access[reg].readable != 0;
}

static int max9809X_volatile_register(struct snd_soc_codec *codec, unsigned int reg)
{
	if (reg > M9809X_REG_MAX_CACHED)
		return 1;

	switch (reg) {
	case M9809X_000_HOST_DATA:
	case M9809X_001_HOST_INT_STS:
	case M9809X_002_HOST_RSP_STS:
	case M9809X_003_HOST_CMD_STS:
	case M9809X_004_CODEC_STS:
	case M9809X_005_DAI1_ALC_STS:
	case M9809X_006_DAI2_ALC_STS:
	case M9809X_007_JACK_AUTO_STS:
	case M9809X_008_JACK_MANUAL_STS:
	case M9809X_009_JACK_VBAT_STS:
	case M9809X_00A_ACC_ADC_STS:
	case M9809X_00B_MIC_NG_AGC_STS:
	case M9809X_00C_SPK_L_VOLT_STS:
	case M9809X_00D_SPK_R_VOLT_STS:
	case M9809X_00E_TEMP_SENSOR_STS:
	case M9809X_015_DEC:
		return 1;
	}

    return 0;
}

/*override spi hardware write operation,since maxim audio codec has slight difference on issueing register address
* which is not compatible with what defined by upper layer
* here assume the first byte is register one-byte address,which needs to be set by "snd_soc_codec_set_cache_io(codec, 8, 8, max98095->control_type)"
* added by Jelphi.zhang
replace hunan SPI write function Mon Jul 23 17:26:23 2012 +0800
*/
#define BCOUNT				8
static int max9809X_do_spi_write(void *control, char *data, int len)
{
    struct spi_device *spi = control;
    u8 regaddr;
    int ret;
    int tx_8bytes_times=0;
    int tx_left=0;
    int tx_xfer_times=0;
    int i=0;
    struct spi_message msg;
    struct spi_transfer *xfers;
    u8 *tx_buf;

    if(len<2)
        return 0;

    regaddr = data[0];/*first byte is register address, then followed by (len-1) data bytes*/
    tx_8bytes_times = (len-1)/BCOUNT;
    tx_left = (len-1)%BCOUNT;

    //CS would be toggled tx_xfer_times to tx all data in issueing one message
    tx_xfer_times = tx_8bytes_times + 1;
    xfers = kcalloc(tx_xfer_times,sizeof(struct spi_transfer),GFP_KERNEL);
    if (xfers == NULL)
        return -ENOMEM;
    //prepare all data with format as codec expected
    tx_buf = kcalloc(3*(len-1),1,GFP_KERNEL);
    if (tx_buf == NULL){
        kfree(xfers);
        return -ENOMEM;
    }

    if(regaddr == 0x00){	//for write Fifo
        for(i=0;i<(len-1);i++){
            tx_buf[3*i] = 0x80;
            tx_buf[3*i+1] = 0;
            tx_buf[3*i+2] = data[i+1];
        }
    }else{				//genera register write
        for(i=0;i<(len-1);i++){
            tx_buf[i*3]=0x80|((data[0]+i)>>3);
            tx_buf[i*3+1]=(data[0]+i)<<5;
            tx_buf[i*3+2]=data[i+1];
        }
    }
    spi_message_init(&msg);
    //prepare all spi_transfer
    for(i=0;i<tx_8bytes_times;i++){
        memset(&xfers[i],0,sizeof(struct spi_transfer));
        xfers[i].len = 3*BCOUNT;
        xfers[i].tx_buf = tx_buf + 3*BCOUNT*i;
        xfers[i].delay_usecs = 0;
        xfers[i].cs_change = 1;
        spi_message_add_tail(&xfers[i],&msg);
    }
    if(tx_left>0){
        i=tx_8bytes_times;
        memset(&xfers[i],0,sizeof(struct spi_transfer));
        xfers[i].len = 3*tx_left;
        xfers[i].tx_buf = tx_buf + 3*BCOUNT*i;
        xfers[i].delay_usecs = 0;
        xfers[i].cs_change = 1;
        spi_message_add_tail(&xfers[i],&msg);
    }

    ret = spi_sync(spi,&msg);
    if(ret<0){
        kfree(xfers);
        kfree(tx_buf);     
        return ret;
    }
    kfree(xfers);
    kfree(tx_buf);
    return len;
}

static unsigned int max9809X_do_spi_read(struct snd_soc_codec *codec, unsigned int reg)
{
    struct spi_device *spi = codec->control_data;
    u8 buf[2];
    u8 data=0;
    int ret;

    buf[0]=(u8)(reg>>3);
    buf[1]=(u8)(reg<<5);

    ret = spi_write_then_read(spi,buf,2,&data,1);

    if (ret < 0)
        return 0;

    return (unsigned int)data;
}

/*
 * Filter coefficients are in a separate register segment
 * and they share the address space of the normal registers.
 * The coefficient registers do not need or share the cache.
 */

static int max9809X_hw_write(struct snd_soc_codec *codec, unsigned int reg,
			     unsigned int value)
{
    u8 data[2];

    data[0] = reg;
    data[1] = value;
    if (codec->hw_write(codec->control_data, data, 2) == 2)
        return 0;
    else
        return -EIO;
}

static int max9809X_write(struct snd_soc_codec *codec, unsigned int reg,
			     unsigned int value)
{
	u8 data[2] = {0};
        int ret = 0;
        
	reg &= 0xff;
	data[0] = reg;
	data[1] = value & 0xff;

	if (!snd_soc_codec_volatile_register(codec, reg) &&
	    reg < codec->driver->reg_cache_size &&
	    !codec->cache_bypass) {
		ret = snd_soc_cache_write(codec, reg, value);
		if (ret < 0)
			return -1;
	}

	if (codec->cache_only) {
		codec->cache_sync = 1;
		return 0;
	}

	ret = codec->hw_write(codec->control_data, data, 2);
	if (ret == 2)
		return 0;
	if (ret < 0)
		return ret;
	else
		return -EIO;
}

static unsigned int max9809X_read(struct snd_soc_codec *codec, unsigned int reg)
{
	int ret;
	unsigned int val;

	if (reg >= codec->driver->reg_cache_size ||
	    snd_soc_codec_volatile_register(codec, reg) ||
	    codec->cache_bypass) {
		if (codec->cache_only)
			return -1;

		BUG_ON(!codec->hw_read);
		return codec->hw_read(codec, reg);
	}

	ret = snd_soc_cache_read(codec, reg, &val);
	if (ret < 0)
		return -1;
	return val;
}
/*
 * Load equalizer DSP coefficient configurations registers
 */
static void m9809X_eq_band(struct snd_soc_codec *codec, unsigned int dai,
		    unsigned int band, u16 *coefs)
{
    unsigned int eq_reg;
    unsigned int i;

    BUG_ON(band > 4);
    BUG_ON(dai > 1);

	/* Load the base register address */
	eq_reg = dai ? M9809X_142_DAI2_EQ_BASE : M9809X_110_DAI1_EQ_BASE;

	/* Add the band address offset, note adjustment for word address */
	eq_reg += band * (M9809X_COEFS_PER_BAND << 1);

	/* Step through the registers and coefs */
	for (i = 0; i < M9809X_COEFS_PER_BAND; i++) {
		max9809X_hw_write(codec, eq_reg++, M9809X_BYTE1(coefs[i]));
		max9809X_hw_write(codec, eq_reg++, M9809X_BYTE0(coefs[i]));
	}
}

/*
 * Load biquad filter coefficient configurations registers
 */
static void m9809X_biquad_band(struct snd_soc_codec *codec, unsigned int dai,
		    unsigned int band, u16 *coefs)
{
    unsigned int bq_reg;
    unsigned int i;

    BUG_ON(band > 1);
    BUG_ON(dai > 1);

	/* Load the base register address */
	bq_reg = dai ? M9809X_17E_DAI2_BQ_BASE : M9809X_174_DAI1_BQ_BASE;

	/* Add the band address offset, note adjustment for word address */
	bq_reg += band * (M9809X_COEFS_PER_BAND << 1);

	/* Step through the registers and coefs */
	for (i = 0; i < M9809X_COEFS_PER_BAND; i++) {
		max9809X_hw_write(codec, bq_reg++, M9809X_BYTE1(coefs[i]));
		max9809X_hw_write(codec, bq_reg++, M9809X_BYTE0(coefs[i]));
	}
}

static const char * max9809X_fltr_mode[] = { "Voice", "Music" };
static const struct soc_enum max9809X_dai1_filter_mode_enum[] = {
	SOC_ENUM_SINGLE(M9809X_02E_DAI1_FILTERS, 7, 2, max9809X_fltr_mode),
};
static const struct soc_enum max9809X_dai2_filter_mode_enum[] = {
	SOC_ENUM_SINGLE(M9809X_038_DAI2_FILTERS, 7, 2, max9809X_fltr_mode),
};

static const char * max9809X_extmic_text[] = { "None", "MIC1", "MIC2" };

static const struct soc_enum max9809X_extmic_enum =
	SOC_ENUM_SINGLE(M9809X_087_CFG_MIC, 0, 3, max9809X_extmic_text);

static const char * max9809X_linein_text[] = { "INA", "INB" };

static const struct soc_enum max9809X_linein_enum =
	SOC_ENUM_SINGLE(M9809X_086_CFG_LINE, 6, 2, max9809X_linein_text);

static const struct snd_kcontrol_new max9809X_linein_mux =
	SOC_DAPM_ENUM("Linein Input Mux", max9809X_linein_enum);

static const char * max9809X_line_mode_text[] = {
	"Stereo", "Differential"};

static const struct soc_enum max9809X_linein_mode_enum =
	SOC_ENUM_SINGLE(M9809X_086_CFG_LINE, 7, 2, max9809X_line_mode_text);

static const struct soc_enum max9809X_lineout_mode_enum =
	SOC_ENUM_SINGLE(M9809X_086_CFG_LINE, 4, 2, max9809X_line_mode_text);

#if defined(DEVICE_MAX98096)
static const char * max9809X_digmic_src_mode[] = {
	"Enhanced Filtering Disabled", "Enhanced Filtering Enabled"};

static const struct soc_enum max9809X_digmic_src_mode_enum =
	SOC_ENUM_SINGLE(M9809X_086_CFG_LINE, 0, 2, max9809X_digmic_src_mode);
#endif

static const char * max9809X_adcdai_text[] = { "None", "DAI1", "DAI2", "DAI3" };

static const struct soc_enum max9809X_adcdai_enum =
	SOC_ENUM_SINGLE(M9809X_045_CFG_DSP, 6, 4, max9809X_adcdai_text);

#if defined(DEVICE_MAX98095)
static const char * max9809X_dai_fltr[] = {
	"Off", "Elliptical-HPF-16k", "Butterworth-HPF-16k",
	"Elliptical-HPF-8k", "Butterworth-HPF-8k", "Butterworth-HPF-Fs/240"
};
static const struct soc_enum max9809X_dai1_dac_filter_enum[] = {
	SOC_ENUM_SINGLE(M9809X_02E_DAI1_FILTERS, 0, 6, max9809X_dai_fltr),
};
static const struct soc_enum max9809X_dai2_dac_filter_enum[] = {
	SOC_ENUM_SINGLE(M9809X_038_DAI2_FILTERS, 0, 6, max9809X_dai_fltr),
};
static const struct soc_enum max9809X_dai3_dac_filter_enum[] = {
	SOC_ENUM_SINGLE(M9809X_042_DAI3_FILTERS, 0, 6, max9809X_dai_fltr),
};
#elif defined(DEVICE_MAX98096)
static const char * max9809X_dai_fltr[] = {
	"Off", "Elliptical-HPF-16k", "Butterworth-HPF-16k",
	"Elliptical-HPF-8k", "Butterworth-HPF-8k", "Butterworth-HPF-Fs/240",
	"DC-Blocker-HPF-5Hz"
};
static const struct soc_enum max9809X_dai1_dac_filter_enum[] = {
	SOC_ENUM_SINGLE(M9809X_02E_DAI1_FILTERS, 0, 7, max9809X_dai_fltr),
};
static const struct soc_enum max9809X_dai2_dac_filter_enum[] = {
	SOC_ENUM_SINGLE(M9809X_038_DAI2_FILTERS, 0, 7, max9809X_dai_fltr),
};
static const struct soc_enum max9809X_dai3_dac_filter_enum[] = {
	SOC_ENUM_SINGLE(M9809X_042_DAI3_FILTERS, 0, 7, max9809X_dai_fltr),
};
#else
#error "DEVICE TYPE NOT DEFINED"
#endif

/* AX48 - Revision A and B both have revision ID value of 0x40, while revision C is 0x42 */
/* 00b = rev A */
/* 00b = rev B */
/* 10b = rev C */
static const char * max9809X_chiprev_text[] = { "B", "C" };
static const struct soc_enum max9809X_chiprev_enum =
	SOC_ENUM_SINGLE(M9809X_0FF_REV_ID, 1, 2, max9809X_chiprev_text);

static int max9809X_mic1pre_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max9809X_priv *max9809X = snd_soc_codec_get_drvdata(codec);
	unsigned int sel = ucontrol->value.integer.value[0];

	max9809X->mic1pre = sel;
	snd_soc_update_bits(codec, M9809X_05F_LVL_MIC1, M9809X_MICPRE_MASK,
		(1+sel)<<M9809X_MICPRE_SHIFT);

	return 0;
}

static int max9809X_mic1pre_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max9809X_priv *max9809X = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = max9809X->mic1pre;
	return 0;
}

static int max9809X_mic2pre_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max9809X_priv *max9809X = snd_soc_codec_get_drvdata(codec);
	unsigned int sel = ucontrol->value.integer.value[0];

	max9809X->mic2pre = sel;
	snd_soc_update_bits(codec, M9809X_060_LVL_MIC2, M9809X_MICPRE_MASK,
		(1+sel)<<M9809X_MICPRE_SHIFT);

	return 0;
}

static int max9809X_mic2pre_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max9809X_priv *max9809X = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = max9809X->mic2pre;
	return 0;
}

static const unsigned int max9809X_micboost_tlv[] = {
	TLV_DB_RANGE_HEAD(2),
	0, 1, TLV_DB_SCALE_ITEM(0, 2000, 0),
	2, 2, TLV_DB_SCALE_ITEM(3000, 0, 0),
};

static const DECLARE_TLV_DB_SCALE(max9809X_mic_tlv, 0, 100, 0);
static const DECLARE_TLV_DB_SCALE(max9809X_adc_tlv, -1200, 100, 0);
static const DECLARE_TLV_DB_SCALE(max9809X_adcboost_tlv, 0, 600, 0);

static const unsigned int max9809X_hp_tlv[] = {
	TLV_DB_RANGE_HEAD(5),
	0, 6, TLV_DB_SCALE_ITEM(-6700, 400, 0),
	7, 14, TLV_DB_SCALE_ITEM(-4000, 300, 0),
	15, 21, TLV_DB_SCALE_ITEM(-1700, 200, 0),
	22, 27, TLV_DB_SCALE_ITEM(-400, 100, 0),
	28, 31, TLV_DB_SCALE_ITEM(150, 50, 0),
};

static const unsigned int max9809X_spk_tlv[] = {
	TLV_DB_RANGE_HEAD(4),
	0, 10, TLV_DB_SCALE_ITEM(-5900, 400, 0),
	11, 18, TLV_DB_SCALE_ITEM(-1700, 200, 0),
	19, 27, TLV_DB_SCALE_ITEM(-200, 100, 0),
	28, 39, TLV_DB_SCALE_ITEM(650, 50, 0),
};

static const unsigned int max9809X_rcv_lout_tlv[] = {
	TLV_DB_RANGE_HEAD(5),
	0, 6, TLV_DB_SCALE_ITEM(-6200, 400, 0),
	7, 14, TLV_DB_SCALE_ITEM(-3500, 300, 0),
	15, 21, TLV_DB_SCALE_ITEM(-1200, 200, 0),
	22, 27, TLV_DB_SCALE_ITEM(100, 100, 0),
	28, 31, TLV_DB_SCALE_ITEM(650, 50, 0),
};

static const unsigned int max9809X_lin_tlv[] = {
	TLV_DB_RANGE_HEAD(3),
	0, 2, TLV_DB_SCALE_ITEM(-600, 300, 0),
	3, 3, TLV_DB_SCALE_ITEM(300, 1100, 0),
	4, 5, TLV_DB_SCALE_ITEM(1400, 600, 0),
};

static const unsigned int max9809X_ply_tlv[] = {
	TLV_DB_RANGE_HEAD(1),
	0, 15, TLV_DB_SCALE_ITEM(0, 200, 0),
};

static const struct snd_kcontrol_new max9809X_snd_controls[] = {

	SOC_DOUBLE_R_TLV("Headphone Volume", M9809X_064_LVL_HP_L,
		M9809X_065_LVL_HP_R, 0, 31, 0, max9809X_hp_tlv),

	SOC_DOUBLE_R_TLV("Speaker Volume", M9809X_067_LVL_SPK_L,
		M9809X_068_LVL_SPK_R, 0, 39, 0, max9809X_spk_tlv),

	SOC_SINGLE_TLV("Receiver Volume", M9809X_066_LVL_RCV,
		0, 31, 0, max9809X_rcv_lout_tlv),

	SOC_SINGLE_TLV("Playback Volume", M9809X_058_LVL_DAI1_PLAY,
		0, 15, 0, max9809X_ply_tlv),

	SOC_DOUBLE_R_TLV("Lineout Volume", M9809X_062_LVL_LINEOUT1,
		M9809X_063_LVL_LINEOUT2, 0, 31, 0, max9809X_rcv_lout_tlv),

	SOC_DOUBLE_R("Headphone Switch", M9809X_064_LVL_HP_L,
		M9809X_065_LVL_HP_R, 7, 1, 1),

	SOC_DOUBLE_R("Speaker Switch", M9809X_067_LVL_SPK_L,
		M9809X_068_LVL_SPK_R, 7, 1, 1),

	SOC_SINGLE("Receiver Switch", M9809X_066_LVL_RCV, 7, 1, 1),

	SOC_DOUBLE_R("Lineout Switch", M9809X_062_LVL_LINEOUT1,
		M9809X_063_LVL_LINEOUT2, 7, 1, 1),

	SOC_SINGLE_TLV("MIC1 Volume", M9809X_05F_LVL_MIC1, 0, 20, 1,
		max9809X_mic_tlv),

	SOC_SINGLE_TLV("MIC2 Volume", M9809X_060_LVL_MIC2, 0, 20, 1,
		max9809X_mic_tlv),

	SOC_SINGLE_EXT_TLV("MIC1 Boost Volume",
			M9809X_05F_LVL_MIC1, 5, 2, 0,
			max9809X_mic1pre_get, max9809X_mic1pre_set,
			max9809X_micboost_tlv),
	SOC_SINGLE_EXT_TLV("MIC2 Boost Volume",
			M9809X_060_LVL_MIC2, 5, 2, 0,
			max9809X_mic2pre_get, max9809X_mic2pre_set,
			max9809X_micboost_tlv),

	SOC_SINGLE_TLV("Linein Volume", M9809X_061_LVL_LINEIN, 0, 5, 1,
		max9809X_lin_tlv),

	SOC_SINGLE_TLV("ADCL Volume", M9809X_05D_LVL_ADC_L, 0, 15, 1,
		max9809X_adc_tlv),
	SOC_SINGLE_TLV("ADCR Volume", M9809X_05E_LVL_ADC_R, 0, 15, 1,
		max9809X_adc_tlv),

	SOC_SINGLE_TLV("ADCL Boost Volume", M9809X_05D_LVL_ADC_L, 4, 3, 0,
		max9809X_adcboost_tlv),
	SOC_SINGLE_TLV("ADCR Boost Volume", M9809X_05E_LVL_ADC_R, 4, 3, 0,
		max9809X_adcboost_tlv),

	SOC_SINGLE("EQ1 Switch", M9809X_088_CFG_LEVEL, 0, 1, 0),
	SOC_SINGLE("EQ2 Switch", M9809X_088_CFG_LEVEL, 1, 1, 0),

	SOC_SINGLE("Biquad1 Switch", M9809X_088_CFG_LEVEL, 2, 1, 0),
	SOC_SINGLE("Biquad2 Switch", M9809X_088_CFG_LEVEL, 3, 1, 0),

	SOC_SINGLE("ADCSRC Switch", M9809X_045_CFG_DSP, 3, 1, 0),

	SOC_SINGLE("SRC1ADC Switch",  M9809X_02F_DAI1_LVL1, 4, 1, 0),
	SOC_SINGLE("SRC1DAI1 Switch", M9809X_02F_DAI1_LVL1, 0, 1, 0),
	SOC_SINGLE("SRC1DAI2 Switch", M9809X_030_DAI1_LVL2, 4, 1, 0),
	SOC_SINGLE("SRC1DAI3 Switch", M9809X_030_DAI1_LVL2, 0, 1, 0),

	SOC_SINGLE("SRC2ADC Switch",  M9809X_039_DAI2_LVL1, 4, 1, 0),
	SOC_SINGLE("SRC2DAI1 Switch", M9809X_039_DAI2_LVL1, 0, 1, 0),
	SOC_SINGLE("SRC2DAI2 Switch", M9809X_03A_DAI2_LVL2, 4, 1, 0),
	SOC_SINGLE("SRC2DAI3 Switch", M9809X_03A_DAI2_LVL2, 0, 1, 0),

	SOC_SINGLE("SRC3ADC Switch",  M9809X_043_DAI3_LVL1, 4, 1, 0),
	SOC_SINGLE("SRC3DAI1 Switch", M9809X_043_DAI3_LVL1, 0, 1, 0),
	SOC_SINGLE("SRC3DAI2 Switch", M9809X_044_DAI3_LVL2, 4, 1, 0),
	SOC_SINGLE("SRC3DAI3 Switch", M9809X_044_DAI3_LVL2, 0, 1, 0),

	SOC_ENUM("ADCDAI Switch", max9809X_adcdai_enum),
	SOC_ENUM("DAI1 Filter Mode", max9809X_dai1_filter_mode_enum),
	SOC_ENUM("DAI2 Filter Mode", max9809X_dai2_filter_mode_enum),
	SOC_ENUM("DAI1 DAC Filter", max9809X_dai1_dac_filter_enum),
	SOC_ENUM("DAI2 DAC Filter", max9809X_dai2_dac_filter_enum),
	SOC_ENUM("DAI3 DAC Filter", max9809X_dai3_dac_filter_enum),

	SOC_ENUM("Linein Mode", max9809X_linein_mode_enum),
	SOC_ENUM("Lineout Mode", max9809X_lineout_mode_enum),

	SOC_ENUM("External MIC", max9809X_extmic_enum),
	SOC_ENUM("REV Switch", max9809X_chiprev_enum),	
};

/* Left speaker mixer switch */
static const struct snd_kcontrol_new max9809X_left_speaker_mixer_controls[] = {
	SOC_DAPM_SINGLE("Left DAC1 Switch", M9809X_050_MIX_SPK_LEFT, 0, 1, 0),
	SOC_DAPM_SINGLE("Right DAC1 Switch", M9809X_050_MIX_SPK_LEFT, 6, 1, 0),
	SOC_DAPM_SINGLE("Mono DAC2 Switch", M9809X_050_MIX_SPK_LEFT, 3, 1, 0),
	SOC_DAPM_SINGLE("Mono DAC3 Switch", M9809X_050_MIX_SPK_LEFT, 3, 1, 0),
	SOC_DAPM_SINGLE("MIC1 Switch", M9809X_050_MIX_SPK_LEFT, 4, 1, 0),
	SOC_DAPM_SINGLE("MIC2 Switch", M9809X_050_MIX_SPK_LEFT, 5, 1, 0),
	SOC_DAPM_SINGLE("IN1 Switch", M9809X_050_MIX_SPK_LEFT, 1, 1, 0),
	SOC_DAPM_SINGLE("IN2 Switch", M9809X_050_MIX_SPK_LEFT, 2, 1, 0),
};

/* Right speaker mixer switch */
static const struct snd_kcontrol_new max9809X_right_speaker_mixer_controls[] = {
	SOC_DAPM_SINGLE("Left DAC1 Switch", M9809X_051_MIX_SPK_RIGHT, 6, 1, 0),
	SOC_DAPM_SINGLE("Right DAC1 Switch", M9809X_051_MIX_SPK_RIGHT, 0, 1, 0),
	SOC_DAPM_SINGLE("Mono DAC2 Switch", M9809X_051_MIX_SPK_RIGHT, 3, 1, 0),
	SOC_DAPM_SINGLE("Mono DAC3 Switch", M9809X_051_MIX_SPK_RIGHT, 3, 1, 0),
	SOC_DAPM_SINGLE("MIC1 Switch", M9809X_051_MIX_SPK_RIGHT, 5, 1, 0),
	SOC_DAPM_SINGLE("MIC2 Switch", M9809X_051_MIX_SPK_RIGHT, 4, 1, 0),
	SOC_DAPM_SINGLE("IN1 Switch", M9809X_051_MIX_SPK_RIGHT, 1, 1, 0),
	SOC_DAPM_SINGLE("IN2 Switch", M9809X_051_MIX_SPK_RIGHT, 2, 1, 0),
};

/* Left headphone mixer switch */
static const struct snd_kcontrol_new max9809X_left_hp_mixer_controls[] = {
	SOC_DAPM_SINGLE("Left DAC1 Switch", M9809X_04C_MIX_HP_LEFT, 0, 1, 0),
	SOC_DAPM_SINGLE("Right DAC1 Switch", M9809X_04C_MIX_HP_LEFT, 5, 1, 0),
	SOC_DAPM_SINGLE("MIC1 Switch", M9809X_04C_MIX_HP_LEFT, 3, 1, 0),
	SOC_DAPM_SINGLE("MIC2 Switch", M9809X_04C_MIX_HP_LEFT, 4, 1, 0),
	SOC_DAPM_SINGLE("IN1 Switch", M9809X_04C_MIX_HP_LEFT, 1, 1, 0),
	SOC_DAPM_SINGLE("IN2 Switch", M9809X_04C_MIX_HP_LEFT, 2, 1, 0),
};

/* Right headphone mixer switch */
static const struct snd_kcontrol_new max9809X_right_hp_mixer_controls[] = {
	SOC_DAPM_SINGLE("Left DAC1 Switch", M9809X_04D_MIX_HP_RIGHT, 5, 1, 0),
	SOC_DAPM_SINGLE("Right DAC1 Switch", M9809X_04D_MIX_HP_RIGHT, 0, 1, 0),
	SOC_DAPM_SINGLE("MIC1 Switch", M9809X_04D_MIX_HP_RIGHT, 3, 1, 0),
	SOC_DAPM_SINGLE("MIC2 Switch", M9809X_04D_MIX_HP_RIGHT, 4, 1, 0),
	SOC_DAPM_SINGLE("IN1 Switch", M9809X_04D_MIX_HP_RIGHT, 1, 1, 0),
	SOC_DAPM_SINGLE("IN2 Switch", M9809X_04D_MIX_HP_RIGHT, 2, 1, 0),
};

/* Receiver earpiece mixer switch */
static const struct snd_kcontrol_new max9809X_mono_rcv_mixer_controls[] = {
	SOC_DAPM_SINGLE("Left DAC1 Switch", M9809X_04F_MIX_RCV, 0, 1, 0),
	SOC_DAPM_SINGLE("Right DAC1 Switch", M9809X_04F_MIX_RCV, 5, 1, 0),
	SOC_DAPM_SINGLE("MIC1 Switch", M9809X_04F_MIX_RCV, 3, 1, 0),
	SOC_DAPM_SINGLE("MIC2 Switch", M9809X_04F_MIX_RCV, 4, 1, 0),
	SOC_DAPM_SINGLE("IN1 Switch", M9809X_04F_MIX_RCV, 1, 1, 0),
	SOC_DAPM_SINGLE("IN2 Switch", M9809X_04F_MIX_RCV, 2, 1, 0),
};

/* Left lineout mixer switch */
static const struct snd_kcontrol_new max9809X_left_lineout_mixer_controls[] = {
	SOC_DAPM_SINGLE("Left DAC1 Switch", M9809X_053_MIX_LINEOUT1, 5, 1, 0),
	SOC_DAPM_SINGLE("Right DAC1 Switch", M9809X_053_MIX_LINEOUT1, 0, 1, 0),
	SOC_DAPM_SINGLE("MIC1 Switch", M9809X_053_MIX_LINEOUT1, 3, 1, 0),
	SOC_DAPM_SINGLE("MIC2 Switch", M9809X_053_MIX_LINEOUT1, 4, 1, 0),
	SOC_DAPM_SINGLE("IN1 Switch", M9809X_053_MIX_LINEOUT1, 1, 1, 0),
	SOC_DAPM_SINGLE("IN2 Switch", M9809X_053_MIX_LINEOUT1, 2, 1, 0),
};

/* Right lineout mixer switch */
static const struct snd_kcontrol_new max9809X_right_lineout_mixer_controls[] = {
	SOC_DAPM_SINGLE("Left DAC1 Switch", M9809X_054_MIX_LINEOUT2, 0, 1, 0),
	SOC_DAPM_SINGLE("Right DAC1 Switch", M9809X_054_MIX_LINEOUT2, 5, 1, 0),
	SOC_DAPM_SINGLE("MIC1 Switch", M9809X_054_MIX_LINEOUT2, 3, 1, 0),
	SOC_DAPM_SINGLE("MIC2 Switch", M9809X_054_MIX_LINEOUT2, 4, 1, 0),
	SOC_DAPM_SINGLE("IN1 Switch", M9809X_054_MIX_LINEOUT2, 1, 1, 0),
	SOC_DAPM_SINGLE("IN2 Switch", M9809X_054_MIX_LINEOUT2, 2, 1, 0),
};

/* Left ADC mixer switch */
static const struct snd_kcontrol_new max9809X_left_ADC_mixer_controls[] = {
	SOC_DAPM_SINGLE("MIC1 Switch", M9809X_04A_MIX_ADC_LEFT, 7, 1, 0),
	SOC_DAPM_SINGLE("MIC2 Switch", M9809X_04A_MIX_ADC_LEFT, 6, 1, 0),
	SOC_DAPM_SINGLE("IN1 Switch", M9809X_04A_MIX_ADC_LEFT, 3, 1, 0),
	SOC_DAPM_SINGLE("IN2 Switch", M9809X_04A_MIX_ADC_LEFT, 2, 1, 0),
};

/* Right ADC mixer switch */
static const struct snd_kcontrol_new max9809X_right_ADC_mixer_controls[] = {
	SOC_DAPM_SINGLE("MIC1 Switch", M9809X_04B_MIX_ADC_RIGHT, 7, 1, 0),
	SOC_DAPM_SINGLE("MIC2 Switch", M9809X_04B_MIX_ADC_RIGHT, 6, 1, 0),
	SOC_DAPM_SINGLE("IN1 Switch", M9809X_04B_MIX_ADC_RIGHT, 3, 1, 0),
	SOC_DAPM_SINGLE("IN2 Switch", M9809X_04B_MIX_ADC_RIGHT, 2, 1, 0),
};

static int max9809X_hp_event(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	u16 status;
	unsigned int hplvol, hprvol;
	/* Powering down headphone gracefully */
	switch (event) {
	case SND_SOC_DAPM_PRE_PMD:
		max9809X_hw_write(codec, M9809X_064_LVL_HP_L, 0);
		max9809X_hw_write(codec, M9809X_065_LVL_HP_R, 0);
		msleep(3);
		status = snd_soc_read(codec, M9809X_091_PWR_EN_OUT);
		if ((status & M9809X_HPEN) == M9809X_HPEN) {
			max9809X_hw_write(codec, M9809X_091_PWR_EN_OUT,
				(status & ~M9809X_HPEN));
		}
		break;

	case SND_SOC_DAPM_POST_PMD:
		hplvol = snd_soc_read(codec, M9809X_064_LVL_HP_L);
		hprvol = snd_soc_read(codec, M9809X_065_LVL_HP_R);
		max9809X_hw_write(codec, M9809X_064_LVL_HP_L, hplvol);
		max9809X_hw_write(codec, M9809X_065_LVL_HP_R, hprvol);
		break;
		
	case SND_SOC_DAPM_PRE_PMU:
		max9809X_hw_write(codec, M9809X_064_LVL_HP_L, M9809X_HP_MUTE);
		max9809X_hw_write(codec, M9809X_065_LVL_HP_R, M9809X_HP_MUTE);
		break;

	case SND_SOC_DAPM_POST_PMU:
		hplvol = snd_soc_read(codec, M9809X_064_LVL_HP_L);
		hprvol = snd_soc_read(codec, M9809X_065_LVL_HP_R);
		max9809X_hw_write(codec, M9809X_064_LVL_HP_L, hplvol);
		max9809X_hw_write(codec, M9809X_065_LVL_HP_R, hprvol);
		break;

    default:
		return -EINVAL;
    }
    return 0;
}

static int max9809X_mic_event(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct max9809X_priv *max9809X = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if (w->reg == M9809X_05F_LVL_MIC1) {
			snd_soc_update_bits(codec, w->reg, M9809X_MICPRE_MASK,
				(1+max9809X->mic1pre)<<M9809X_MICPRE_SHIFT);
		} else {
			snd_soc_update_bits(codec, w->reg, M9809X_MICPRE_MASK,
				(1+max9809X->mic2pre)<<M9809X_MICPRE_SHIFT);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, w->reg, M9809X_MICPRE_MASK, 0);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/*
 * The line inputs are stereo inputs with the left and right
 * channels sharing a common PGA power control signal.
 */
static int max9809X_line_pga(struct snd_soc_dapm_widget *w,
			     int event, u8 channel)
{
	struct snd_soc_codec *codec = w->codec;
	struct max9809X_priv *max9809X = snd_soc_codec_get_drvdata(codec);
	u8 *state;

	BUG_ON(!((channel == 1) || (channel == 2)));

	state = &max9809X->lin_state;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		*state |= channel;
		snd_soc_update_bits(codec, w->reg,
				(1 << w->shift), (1 << w->shift));
		break;
	case SND_SOC_DAPM_POST_PMD:
		*state &= ~channel;
		if (*state == 0) {
		snd_soc_update_bits(codec, w->reg,
				(1 << w->shift), 0);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int max9809X_pga_in1_event(struct snd_soc_dapm_widget *w,
				   struct snd_kcontrol *k, int event)
{
	return max9809X_line_pga(w, event, 1);
}

static int max9809X_pga_in2_event(struct snd_soc_dapm_widget *w,
				   struct snd_kcontrol *k, int event)
{
	return max9809X_line_pga(w, event, 2);
}

/*
 * The stereo line out mixer outputs to two stereo line outs.
 * The 2nd pair has a separate set of enables.
 */
static int max9809X_lineout_event(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, w->reg,
				(1 << (w->shift+2)), (1 << (w->shift+2)));
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, w->reg,
				(1 << (w->shift+2)), 0);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct snd_soc_dapm_widget max9809X_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_OUT("DAI1IN", "HiFi Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("DAI2IN", "Voice Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("DAI3IN", "BT Capture", 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_AIF_IN("DAI1OUT", "HiFi Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("DAI2OUT", "Voice Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("DAI3OUT", "BT Playback", 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_ADC("ADCL", NULL, M9809X_090_PWR_EN_IN, 0, 0),
	SND_SOC_DAPM_ADC("ADCR", NULL, M9809X_090_PWR_EN_IN, 1, 0),

	SND_SOC_DAPM_DAC("DACL1", NULL,
		M9809X_091_PWR_EN_OUT, 0, 0),
	SND_SOC_DAPM_DAC("DACR1", NULL,
		M9809X_091_PWR_EN_OUT, 1, 0),
	SND_SOC_DAPM_DAC("DACM2", NULL,
		M9809X_091_PWR_EN_OUT, 2, 0),
	SND_SOC_DAPM_DAC("DACM3", NULL,
		M9809X_091_PWR_EN_OUT, 2, 0),	

	SND_SOC_DAPM_PGA_E("HP Left Out", M9809X_091_PWR_EN_OUT,
		6, 0, NULL, 0, max9809X_hp_event, 
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD|SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_E("HP Right Out", M9809X_091_PWR_EN_OUT,
		7, 0, NULL, 0, max9809X_hp_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD|SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_PGA("SPK Left Out", M9809X_091_PWR_EN_OUT,
		4, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPK Right Out", M9809X_091_PWR_EN_OUT,
		5, 0, NULL, 0),

	SND_SOC_DAPM_PGA("RCV Mono Out", M9809X_091_PWR_EN_OUT,
		3, 0, NULL, 0),

#ifdef USE_LINEOUT_1_2
	SND_SOC_DAPM_PGA_E("LINE Left Out", M9809X_092_PWR_EN_OUT,
		0, 0, NULL, 0, max9809X_lineout_event, SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_E("LINE Right Out", M9809X_092_PWR_EN_OUT,
		1, 0, NULL, 0, max9809X_lineout_event, SND_SOC_DAPM_PRE_PMD),
#else /* use LINEOUT_3_4 */
	SND_SOC_DAPM_PGA_E("LINE Left Out", M9809X_092_PWR_EN_OUT,
		2, 0, NULL, 0, max9809X_lineout_event, SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_E("LINE Right Out", M9809X_092_PWR_EN_OUT,
		3, 0, NULL, 0, max9809X_lineout_event, SND_SOC_DAPM_PRE_PMD),
#endif

	SND_SOC_DAPM_MUX("Linein Mux", SND_SOC_NOPM, 0, 0,
		&max9809X_linein_mux),

	SND_SOC_DAPM_MIXER("Left Headphone Mixer", SND_SOC_NOPM, 0, 0,
		&max9809X_left_hp_mixer_controls[0],
		ARRAY_SIZE(max9809X_left_hp_mixer_controls)),

	SND_SOC_DAPM_MIXER("Right Headphone Mixer", SND_SOC_NOPM, 0, 0,
		&max9809X_right_hp_mixer_controls[0],
		ARRAY_SIZE(max9809X_right_hp_mixer_controls)),

	SND_SOC_DAPM_MIXER("Left Speaker Mixer", SND_SOC_NOPM, 0, 0,
		&max9809X_left_speaker_mixer_controls[0],
		ARRAY_SIZE(max9809X_left_speaker_mixer_controls)),

	SND_SOC_DAPM_MIXER("Right Speaker Mixer", SND_SOC_NOPM, 0, 0,
		&max9809X_right_speaker_mixer_controls[0],
		ARRAY_SIZE(max9809X_right_speaker_mixer_controls)),

	SND_SOC_DAPM_MIXER("Receiver Mixer", SND_SOC_NOPM, 0, 0,
		&max9809X_mono_rcv_mixer_controls[0],
		ARRAY_SIZE(max9809X_mono_rcv_mixer_controls)),

	SND_SOC_DAPM_MIXER("Left Lineout Mixer", SND_SOC_NOPM, 0, 0,
		&max9809X_left_lineout_mixer_controls[0],
		ARRAY_SIZE(max9809X_left_lineout_mixer_controls)),

	SND_SOC_DAPM_MIXER("Right Lineout Mixer", SND_SOC_NOPM, 0, 0,
		&max9809X_right_lineout_mixer_controls[0],
		ARRAY_SIZE(max9809X_right_lineout_mixer_controls)),

	SND_SOC_DAPM_MIXER("Left ADC Mixer", SND_SOC_NOPM, 0, 0,
		&max9809X_left_ADC_mixer_controls[0],
		ARRAY_SIZE(max9809X_left_ADC_mixer_controls)),

	SND_SOC_DAPM_MIXER("Right ADC Mixer", SND_SOC_NOPM, 0, 0,
		&max9809X_right_ADC_mixer_controls[0],
		ARRAY_SIZE(max9809X_right_ADC_mixer_controls)),

	SND_SOC_DAPM_PGA_E("MIC1 Input", M9809X_05F_LVL_MIC1,
		5, 0, NULL, 0, max9809X_mic_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_E("MIC2 Input", M9809X_060_LVL_MIC2,
		5, 0, NULL, 0, max9809X_mic_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_E("IN1 Input", M9809X_090_PWR_EN_IN,
		7, 0, NULL, 0, max9809X_pga_in1_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_E("IN2 Input", M9809X_090_PWR_EN_IN,
		7, 0, NULL, 0, max9809X_pga_in2_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_OUTPUT("HPL"),
	SND_SOC_DAPM_OUTPUT("HPR"),
	SND_SOC_DAPM_OUTPUT("SPKL"),
	SND_SOC_DAPM_OUTPUT("SPKR"),
	SND_SOC_DAPM_OUTPUT("RCV"),
	SND_SOC_DAPM_OUTPUT("OUT1"),
	SND_SOC_DAPM_OUTPUT("OUT2"),
	SND_SOC_DAPM_OUTPUT("OUT3"),
	SND_SOC_DAPM_OUTPUT("OUT4"),
	SND_SOC_DAPM_OUTPUT("BT Voice"),

	SND_SOC_DAPM_INPUT("MIC1"),
	SND_SOC_DAPM_INPUT("MIC2"),
	SND_SOC_DAPM_INPUT("INA1"),
	SND_SOC_DAPM_INPUT("INA2"),
	SND_SOC_DAPM_INPUT("INB1"),
	SND_SOC_DAPM_INPUT("INB2"),
};

static const struct snd_soc_dapm_route max9809X_audio_map[] = {
	/* virtual bt voice channel */
	{"BT Voice", NULL, "DACL1"},

	/* Left headphone output mixer */
	{"Left Headphone Mixer", "Left DAC1 Switch", "DACL1"},
	{"Left Headphone Mixer", "Right DAC1 Switch", "DACR1"},
	{"Left Headphone Mixer", "MIC1 Switch", "MIC1 Input"},
	{"Left Headphone Mixer", "MIC2 Switch", "MIC2 Input"},
	{"Left Headphone Mixer", "IN1 Switch", "IN1 Input"},
	{"Left Headphone Mixer", "IN2 Switch", "IN2 Input"},

	/* Right headphone output mixer */
	{"Right Headphone Mixer", "Left DAC1 Switch", "DACL1"},
	{"Right Headphone Mixer", "Right DAC1 Switch", "DACR1"},
	{"Right Headphone Mixer", "MIC1 Switch", "MIC1 Input"},
	{"Right Headphone Mixer", "MIC2 Switch", "MIC2 Input"},
	{"Right Headphone Mixer", "IN1 Switch", "IN1 Input"},
	{"Right Headphone Mixer", "IN2 Switch", "IN2 Input"},

	/* Left speaker output mixer */
	{"Left Speaker Mixer", "Left DAC1 Switch", "DACL1"},
	{"Left Speaker Mixer", "Right DAC1 Switch", "DACR1"},
	{"Left Speaker Mixer", "Mono DAC2 Switch", "DACM2"},
	{"Left Speaker Mixer", "Mono DAC3 Switch", "DACM3"},
	{"Left Speaker Mixer", "MIC1 Switch", "MIC1 Input"},
	{"Left Speaker Mixer", "MIC2 Switch", "MIC2 Input"},
	{"Left Speaker Mixer", "IN1 Switch", "IN1 Input"},
	{"Left Speaker Mixer", "IN2 Switch", "IN2 Input"},

	/* Right speaker output mixer */
	{"Right Speaker Mixer", "Left DAC1 Switch", "DACL1"},
	{"Right Speaker Mixer", "Right DAC1 Switch", "DACR1"},
	{"Right Speaker Mixer", "Mono DAC2 Switch", "DACM2"},
	{"Right Speaker Mixer", "Mono DAC3 Switch", "DACM3"},
	{"Right Speaker Mixer", "MIC1 Switch", "MIC1 Input"},
	{"Right Speaker Mixer", "MIC2 Switch", "MIC2 Input"},
	{"Right Speaker Mixer", "IN1 Switch", "IN1 Input"},
	{"Right Speaker Mixer", "IN2 Switch", "IN2 Input"},

	/* Earpiece/Receiver output mixer */
	{"Receiver Mixer", "Left DAC1 Switch", "DACL1"},
	{"Receiver Mixer", "Right DAC1 Switch", "DACR1"},
	{"Receiver Mixer", "MIC1 Switch", "MIC1 Input"},
	{"Receiver Mixer", "MIC2 Switch", "MIC2 Input"},
	{"Receiver Mixer", "IN1 Switch", "IN1 Input"},
	{"Receiver Mixer", "IN2 Switch", "IN2 Input"},

	/* Left Lineout output mixer */
	{"Left Lineout Mixer", "Left DAC1 Switch", "DACL1"},
	{"Left Lineout Mixer", "Right DAC1 Switch", "DACR1"},
	{"Left Lineout Mixer", "MIC1 Switch", "MIC1 Input"},
	{"Left Lineout Mixer", "MIC2 Switch", "MIC2 Input"},
	{"Left Lineout Mixer", "IN1 Switch", "IN1 Input"},
	{"Left Lineout Mixer", "IN2 Switch", "IN2 Input"},

	/* Right lineout output mixer */
	{"Right Lineout Mixer", "Left DAC1 Switch", "DACL1"},
	{"Right Lineout Mixer", "Right DAC1 Switch", "DACR1"},
	{"Right Lineout Mixer", "MIC1 Switch", "MIC1 Input"},
	{"Right Lineout Mixer", "MIC2 Switch", "MIC2 Input"},
	{"Right Lineout Mixer", "IN1 Switch", "IN1 Input"},
	{"Right Lineout Mixer", "IN2 Switch", "IN2 Input"},

	{"HP Left Out", NULL, "Left Headphone Mixer"},
	{"HP Right Out", NULL, "Right Headphone Mixer"},
	{"SPK Left Out", NULL, "Left Speaker Mixer"},
	{"SPK Right Out", NULL, "Right Speaker Mixer"},
	{"RCV Mono Out", NULL, "Receiver Mixer"},
	{"LINE Left Out", NULL, "Left Lineout Mixer"},
	{"LINE Right Out", NULL, "Right Lineout Mixer"},

	{"HPL", NULL, "HP Left Out"},
	{"HPR", NULL, "HP Right Out"},
	{"SPKL", NULL, "SPK Left Out"},
	{"SPKR", NULL, "SPK Right Out"},
	{"RCV", NULL, "RCV Mono Out"},
	{"OUT1", NULL, "LINE Left Out"},
	{"OUT2", NULL, "LINE Right Out"},
	{"OUT3", NULL, "LINE Left Out"},
	{"OUT4", NULL, "LINE Right Out"},

	/* Left ADC input mixer */
	{"Left ADC Mixer", "MIC1 Switch", "MIC1 Input"},
	{"Left ADC Mixer", "MIC2 Switch", "MIC2 Input"},
	{"Left ADC Mixer", "IN1 Switch", "IN1 Input"},
	{"Left ADC Mixer", "IN2 Switch", "IN2 Input"},

	/* Right ADC input mixer */
	{"Right ADC Mixer", "MIC1 Switch", "MIC1 Input"},
	{"Right ADC Mixer", "MIC2 Switch", "MIC2 Input"},
	{"Right ADC Mixer", "IN1 Switch", "IN1 Input"},
	{"Right ADC Mixer", "IN2 Switch", "IN2 Input"},

	/* Inputs */
	{"ADCL", NULL, "Left ADC Mixer"},
	{"ADCR", NULL, "Right ADC Mixer"},

	{"IN1 Input", NULL, "INA1"},
	{"IN2 Input", NULL, "INA2"},

	{"MIC1 Input", NULL, "MIC1"},
	{"MIC2 Input", NULL, "MIC2"},

	//ZTE: add for DAPM
	{"DAI1IN", NULL, "ADCL"},
	{"DAI2IN", NULL, "ADCL"},

	{"DAI1IN", NULL, "ADCR"},
	{"DAI2IN", NULL, "ADCR"},

	{"DACL1", NULL, "DAI1OUT"},	
	{"DACL1", NULL, "DAI2OUT"},
	{"DACL1", NULL, "DAI3OUT"},

	{"DACR1", NULL, "DAI1OUT"},
	{"DACR1", NULL, "DAI2OUT"},

	{"DACM2", NULL, "DAI2OUT"},
	{"DACM3", NULL, "DAI2OUT"},

};

static int max9809X_add_widgets(struct snd_soc_codec *codec)
{
	snd_soc_add_codec_controls(codec, max9809X_snd_controls,
			     ARRAY_SIZE(max9809X_snd_controls));
	return 0;
}

/* codec mclk clock divider coefficients */
static const struct {
	u32 rate;
	u8  sr;
} rate_table[] = {
	{8000,  0x01}, //ZTE: by hn for voice_call clock
	{11025, 0x02},
	{16000, 0x03},
	{22050, 0x04},
	{24000, 0x05},
	{32000, 0x06},
	{44100, 0x07},
	{48000, 0x08},
	{88200, 0x09},
	{96000, 0x0A},
};

static int rate_value(int rate, u8 *value)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(rate_table); i++) {
		if (rate_table[i].rate >= rate) {
			*value = rate_table[i].sr;
			return 0;
		}
	}
	*value = rate_table[0].sr;
	return -EINVAL;
}

static int max9809X_dai1_hw_params(struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *params,
				   struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct max9809X_priv *max9809X = snd_soc_codec_get_drvdata(codec);
	struct max9809X_cdata *cdata;
	unsigned long long ni;
	unsigned int rate;
	u8 regval;

	cdata = &max9809X->dai[0];

	rate = params_rate(params);
	dev_info(codec->dev,"%s\n", __FUNCTION__);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S32_LE:
	case SNDRV_PCM_FORMAT_S16_LE:
		snd_soc_update_bits(codec, M9809X_02A_DAI1_FORMAT,
				M9809X_DAI_WS, 0);
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		snd_soc_update_bits(codec, M9809X_02A_DAI1_FORMAT,
				M9809X_DAI_WS, M9809X_DAI_WS);
		break;
	default:
		return -EINVAL;
	}

	if (rate_value(rate, &regval))
		return -EINVAL;

	snd_soc_update_bits(codec, M9809X_027_DAI1_CLKMODE,
			M9809X_CLKMODE_MASK, regval);
	cdata->rate = rate;

	/* Configure NI when operating as master */
	if (snd_soc_read(codec, M9809X_02A_DAI1_FORMAT) & M9809X_DAI_MAS) {
		if (max9809X->sysclk == 0) {
			dev_err(codec->dev, "Invalid system clock frequency\n");
			return -EINVAL;
		}
		ni = 65536ULL * (rate < 50000 ? 96ULL : 48ULL) *
				(unsigned long long int)rate + (max9809X->sysclk >> 1);
		do_div(ni, (unsigned long long int)max9809X->sysclk);
		snd_soc_write(codec, M9809X_028_DAI1_CLKCFG_HI,(ni >> 8) & 0x7F);
		snd_soc_write(codec, M9809X_029_DAI1_CLKCFG_LO,ni & 0xFF);
	}

	/* Update sample rate mode */
	if (rate < 50000)
		snd_soc_update_bits(codec, M9809X_02E_DAI1_FILTERS,
				M9809X_DAI_DHF, 0);
	else
		snd_soc_update_bits(codec, M9809X_02E_DAI1_FILTERS,
				M9809X_DAI_DHF, M9809X_DAI_DHF);

	//ZTE: DAI1 filter type is music, reduce hp noise
	snd_soc_update_bits(codec, M9809X_02E_DAI1_FILTERS,
			M9809X_DAI_FILTER_TYPE, M9809X_DAI_FILTER_TYPE);

	return 0;
}

static int max9809X_dai2_hw_params(struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *params,
				   struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct max9809X_priv *max9809X = snd_soc_codec_get_drvdata(codec);
	struct max9809X_cdata *cdata;
	unsigned long long ni;
	unsigned int rate;
	u8 regval;

	cdata = &max9809X->dai[1];

	rate = params_rate(params);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		snd_soc_update_bits(codec, M9809X_034_DAI2_FORMAT,
			M9809X_DAI_WS, 0);
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		snd_soc_update_bits(codec, M9809X_034_DAI2_FORMAT,
			M9809X_DAI_WS, M9809X_DAI_WS);
		break;
	default:
		return -EINVAL;
	}

	if (rate_value(rate, &regval))
        	return -EINVAL;

	snd_soc_update_bits(codec, M9809X_031_DAI2_CLKMODE,
			M9809X_CLKMODE_MASK, regval);
	cdata->rate = rate;

	/* Configure NI when operating as master */
	//ZTE: don't care about master or slave 
//	if (snd_soc_read(codec, M9809X_034_DAI2_FORMAT) & M9809X_DAI_MAS) 
       {
		if (max9809X->sysclk == 0) {
			dev_err(codec->dev, "Invalid system clock frequency\n");
			return -EINVAL;
		}
		ni = 65536ULL * (rate < 50000 ? 96ULL : 48ULL) *
				(unsigned long long int)rate + (max9809X->sysclk >> 1);
		do_div(ni, (unsigned long long int)max9809X->sysclk);
		snd_soc_write(codec, M9809X_032_DAI2_CLKCFG_HI,(ni >> 8) & 0x7F);
		snd_soc_write(codec, M9809X_033_DAI2_CLKCFG_LO,ni & 0xFF);
	}

	/* Update sample rate mode */
	if (rate < 50000)
		snd_soc_update_bits(codec, M9809X_038_DAI2_FILTERS,
			M9809X_DAI_DHF, 0);
	else
		snd_soc_update_bits(codec, M9809X_038_DAI2_FILTERS,
			M9809X_DAI_DHF, M9809X_DAI_DHF);

    return 0;
}

static int max9809X_dai3_hw_params(struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *params,
				   struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct max9809X_priv *max9809X = snd_soc_codec_get_drvdata(codec);
	struct max9809X_cdata *cdata;
	unsigned long long ni;
	unsigned int rate;
	u8 regval;

	cdata = &max9809X->dai[2];
	rate = params_rate(params);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		snd_soc_update_bits(codec, M9809X_03E_DAI3_FORMAT,
			M9809X_DAI_WS, 0);
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		snd_soc_update_bits(codec, M9809X_03E_DAI3_FORMAT,
			M9809X_DAI_WS, M9809X_DAI_WS);
		break;
	default:
		return -EINVAL;
	}

	if (rate_value(rate, &regval))
		return -EINVAL;

	snd_soc_update_bits(codec, M9809X_03B_DAI3_CLKMODE,
			M9809X_CLKMODE_MASK, regval);
	cdata->rate = rate;

	/* Configure NI when operating as master */
	if (snd_soc_read(codec, M9809X_03E_DAI3_FORMAT) & M9809X_DAI_MAS) {
		if (max9809X->sysclk == 0) {
			dev_err(codec->dev, "Invalid system clock frequency\n");
			return -EINVAL;
		}
		ni = 65536ULL * (rate < 50000 ? 96ULL : 48ULL) * 
				(unsigned long long int)rate + (max9809X->sysclk >> 1);
		do_div(ni, (unsigned long long int)max9809X->sysclk);
		snd_soc_write(codec, M9809X_03C_DAI3_CLKCFG_HI,(ni >> 8) & 0x7F);
		snd_soc_write(codec, M9809X_03D_DAI3_CLKCFG_LO,ni & 0xFF);

		snd_soc_write(codec, M9809X_03F_DAI3_CLOCK,
				M9809X_DAI_BSEL_DIV8);		
	}

	/* Update sample rate mode */
	if (rate < 50000)
		snd_soc_update_bits(codec, M9809X_042_DAI3_FILTERS,
				M9809X_DAI_DHF, 0);
	else
		snd_soc_update_bits(codec, M9809X_042_DAI3_FILTERS,
				M9809X_DAI_DHF, M9809X_DAI_DHF);

	return 0;
}

static int max9809X_dai_set_sysclk(struct snd_soc_dai *dai,
				   int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	struct max9809X_priv *max9809X = snd_soc_codec_get_drvdata(codec);
	dev_info(codec->dev,"[codec] max9809X_dai_set_sysclk begin\n");
	/* Requested clock frequency is already setup */
	if (freq == max9809X->sysclk)
		return 0;

	/* Setup clocks for slave mode, and using the PLL
	 * PSCLK = 0x01 (when master clk is 10MHz to 20MHz)
	 *         0x02 (when master clk is 20MHz to 40MHz)..
	 *         0x03 (when master clk is 40MHz to 60MHz)..
	 */
	if ((freq >= 10000000) && (freq < 20000000)) {
		snd_soc_write(codec, M9809X_026_SYS_CLK, 0x10);
	} else if ((freq >= 20000000) && (freq < 40000000)) {
		snd_soc_write(codec, M9809X_026_SYS_CLK, 0x20);
	} else if ((freq >= 40000000) && (freq < 60000000)) {
		snd_soc_write(codec, M9809X_026_SYS_CLK, 0x30);
	} else {
		dev_err(codec->dev, "Invalid master clock frequency\n");
		return -EINVAL;
	}

	dev_dbg(dai->dev, "Clock source is %d at %uHz\n", clk_id, freq);
	dev_info(codec->dev,"[codec] max9809X_dai_set_sysclk end\n");
	max9809X->sysclk = freq;

	{	/*Add flexsound control enums*/
		int ret = 0;
		dev_info(codec->dev,"[codec]start to initialize DSP\n");
		
		ret = dsp_flexsound_init(codec);
		if (ret < 0) {
			dev_err(codec->dev, "Failed to initialize DSP: %d\n", ret);
		}	
	}
	
	return 0;
}
/*
 * Common set tdm routine for all 3 channels
 */
static int max9809X_set_tdm_slot(struct snd_soc_codec *codec, 
				struct max9809X_cdata *cdata, 
				int tdm_reg, 
				unsigned int tx_mask,
				unsigned int rx_mask,
				int slots,
				int slot_width)
{
	const int reg_offs = M9809X_02D_DAI1_TDM - M9809X_02A_DAI1_FORMAT;
#ifdef DEBUG
    	printk(KERN_ERR 
           		 "%s: codec=%p num_dai=%d reg=0x%x tx-0x%x slots=%d slw=%d\n",
			__func__, codec, codec->num_dai, tdm_reg, tx_mask, slots, slot_width);
#endif
	dev_info(codec->dev,"[codec] max9809X_set_tdm_slot begin\n");

	/* tx_mask is data for desired M9809X_02D_DAIx_TDM register
	 * save slot info for bclk calculations.*/
	snd_soc_write(codec, tdm_reg, tx_mask);

	 /* turn on or off the chip tdm mode - depending on slots */
	snd_soc_update_bits(codec, 
				tdm_reg - reg_offs,
				M9809X_DAI_TDM,
				slots ? M9809X_DAI_TDM : 0);
	dev_info(codec->dev,"[codec] max9809X_set_tdm_slot end\n");
	return 0;
}

static int max9809X_dai1_set_tdm_slot(struct snd_soc_dai *codec_dai, unsigned int tx_mask, 
				      unsigned int rx_mask, int slots, int slot_width)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct max9809X_priv *max9809X = snd_soc_codec_get_drvdata(codec);
	struct max9809X_cdata *cdata = &max9809X->dai[0];

#ifdef DEBUG
	printk(KERN_ERR "%s: codec=%p num_dai=%d id=%d\n",
		__func__, codec, codec->num_dai, codec_dai->id);
#endif
	return max9809X_set_tdm_slot(codec, cdata, M9809X_02D_DAI1_TDM,
					tx_mask, rx_mask, slots, slot_width);
}

static int max9809X_dai2_set_tdm_slot(struct snd_soc_dai *codec_dai, unsigned int tx_mask,
				      unsigned int rx_mask, int slots, int slot_width)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct max9809X_priv *max9809X = snd_soc_codec_get_drvdata(codec);
	struct max9809X_cdata *cdata = &max9809X->dai[1];

#ifdef DEBUG
    	printk(KERN_ERR "%s: codec=%p num_dai=%d id=%d\n",
            	__func__, codec, codec->num_dai, codec_dai->id);
#endif
	return max9809X_set_tdm_slot(codec, cdata, M9809X_037_DAI2_TDM,
					tx_mask, rx_mask, slots, slot_width);
}

static int max9809X_dai3_set_tdm_slot(struct snd_soc_dai *codec_dai, unsigned int tx_mask,
				      unsigned int rx_mask, int slots, int slot_width)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct max9809X_priv *max9809X = snd_soc_codec_get_drvdata(codec);
	struct max9809X_cdata *cdata = &max9809X->dai[2];

#ifdef DEBUG
	printk(KERN_ERR "%s: codec=%p num_dai=%d id=%d\n",
			__func__, codec, codec->num_dai, codec_dai->id);
#endif
	return max9809X_set_tdm_slot(codec, cdata, M9809X_041_DAI3_TDM,
					tx_mask, rx_mask, slots, slot_width);
}

static int max9809X_dai1_set_fmt(struct snd_soc_dai *codec_dai,
				 unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct max9809X_priv *max9809X = snd_soc_codec_get_drvdata(codec);
	struct max9809X_cdata *cdata;
	u8 regval = 0;

	cdata = &max9809X->dai[0];
	dev_info(codec->dev,"%s\n", __FUNCTION__);
	if (fmt != cdata->fmt) {
        	cdata->fmt = fmt;
		switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
		case SND_SOC_DAIFMT_CBS_CFS:
			/* Slave mode PLL */
			snd_soc_write(codec, M9809X_028_DAI1_CLKCFG_HI,
				0x80);
			snd_soc_write(codec, M9809X_029_DAI1_CLKCFG_LO,
				0x00);
			break;
		case SND_SOC_DAIFMT_CBM_CFM:
			/* Set to master mode */
			regval |= M9809X_DAI_MAS;
			break;
		case SND_SOC_DAIFMT_CBS_CFM:
		case SND_SOC_DAIFMT_CBM_CFS:
		default:
			dev_err(codec->dev, "Clock mode unsupported");
			return -EINVAL;
		}

		switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
		case SND_SOC_DAIFMT_I2S:
			regval |= M9809X_DAI_DLY;
			break;
		case SND_SOC_DAIFMT_LEFT_J:
		case SND_SOC_DAIFMT_DSP_A:
			break;
		default:
			return -EINVAL;
		}

		switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
		case SND_SOC_DAIFMT_NB_NF:
			break;
		case SND_SOC_DAIFMT_NB_IF:
			regval |= M9809X_DAI_WCI;
			break;
		case SND_SOC_DAIFMT_IB_NF:
			regval |= M9809X_DAI_BCI;
			break;
		case SND_SOC_DAIFMT_IB_IF:
			regval |= M9809X_DAI_BCI|M9809X_DAI_WCI;
			break;
		default:
			return -EINVAL;
		}

		snd_soc_update_bits(codec, M9809X_02A_DAI1_FORMAT,
			M9809X_DAI_MAS | M9809X_DAI_DLY | M9809X_DAI_BCI |
			M9809X_DAI_WCI, regval);

		snd_soc_write(codec, M9809X_02B_DAI1_CLOCK, M9809X_DAI_BSEL64);
	}
        //ZTE: ADC
	snd_soc_update_bits(codec, M9809X_048_MIX_DAC_LR,
		M9809X_DAI1L_TO_DACL|M9809X_DAI1R_TO_DACR, M9809X_DAI1L_TO_DACL|M9809X_DAI1R_TO_DACR);
       
	snd_soc_update_bits(codec, M9809X_097_PWR_SYS, M9809X_PWRSV, M9809X_PWRSV);
	dev_info(codec->dev,"[codec] max9809X_dai1_set_fmt end\n");
	return 0;
}

static int max9809X_dai2_set_fmt(struct snd_soc_dai *codec_dai,
				 unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct max9809X_priv *max9809X = snd_soc_codec_get_drvdata(codec);
	struct max9809X_cdata *cdata;
	u8 regval = 0;
	dev_info(codec->dev,"%s\n",__FUNCTION__);
	cdata = &max9809X->dai[1];
	// Master Mode for P945T30 dai is TDM
	regval = M9809X_DAI_MAS |M9809X_DAI_TDM;
	snd_soc_update_bits(codec, M9809X_034_DAI2_FORMAT,
			M9809X_DAI_MAS | M9809X_DAI_DLY | M9809X_DAI_BCI |
			M9809X_DAI_WCI | M9809X_DAI_TDM, regval);
	// BCLK is 64*8k
	snd_soc_write(codec, M9809X_035_DAI2_CLOCK, M9809X_DAI_BSEL64);

	snd_soc_update_bits(codec, M9809X_048_MIX_DAC_LR, M9809X_DAI2M_TO_DACL, M9809X_DAI2M_TO_DACL);

	snd_soc_update_bits(codec, M9809X_097_PWR_SYS, M9809X_PWRSV, M9809X_PWRSV);
	//ZTE voice noise
	snd_soc_update_bits(codec, M9809X_096_PWR_DAC_CK, M9809X_FITHEN2, 0x00);

	return 0;
}
static int max9809X_dai3_set_fmt(struct snd_soc_dai *codec_dai,
				 unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct max9809X_priv *max9809X = snd_soc_codec_get_drvdata(codec);
	struct max9809X_cdata *cdata;
	u8 regval = 0;

	cdata = &max9809X->dai[2];
	regval = M9809X_DAI_BCI |M9809X_DAI_TDM | M9809X_DAI_MAS;
		
	snd_soc_update_bits(codec, M9809X_03E_DAI3_FORMAT,
		M9809X_DAI_MAS | M9809X_DAI_DLY | M9809X_DAI_BCI |
		M9809X_DAI_WCI, regval);
		
	snd_soc_update_bits(codec, M9809X_041_DAI3_TDM,0x10,0x10);

	snd_soc_update_bits(codec, M9809X_090_PWR_EN_IN,M9809X_ADLEN,M9809X_ADLEN);
	return 0;
}

static int max9809X_dai1_digital_mute(struct snd_soc_dai *codec_dai, int mute)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	int reg;

	reg = mute ? M9809X_DAI_MUTE : 0;
	snd_soc_update_bits(codec, M9809X_058_LVL_DAI1_PLAY,
			M9809X_DAI_MUTE_MASK, reg);
	return 0;
}

static int max9809X_dai2_digital_mute(struct snd_soc_dai *codec_dai, int mute)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	int reg;

	reg = mute ? M9809X_DAI_MUTE : 0;
	snd_soc_update_bits(codec, M9809X_05A_LVL_DAI2_PLAY,
			M9809X_DAI_MUTE_MASK, reg);
	return 0;
}

static int max9809X_dai3_digital_mute(struct snd_soc_dai *codec_dai, int mute)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	int reg;

	reg = mute ? M9809X_DAI_MUTE : 0;
	snd_soc_update_bits(codec, M9809X_05C_LVL_DAI3_PLAY,
			M9809X_DAI_MUTE_MASK, reg);
	return 0;
}

static int max9809X_set_bias_level(struct snd_soc_codec *codec,
				   enum snd_soc_bias_level level)
{
	int ret;

	switch (level) {
	case SND_SOC_BIAS_ON:
		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		if (codec->dapm.bias_level == SND_SOC_BIAS_OFF) {
			ret = snd_soc_cache_sync(codec);
			if (ret != 0) {
				dev_err(codec->dev, "Failed to sync cache: %d\n", ret);
				return ret;
			}
		}
		//ZTE: modified for hp detect 
		snd_soc_update_bits(codec, M9809X_090_PWR_EN_IN,
				M9809X_MB1EN, M9809X_MB1EN);
		break;
	case SND_SOC_BIAS_OFF:
		snd_soc_update_bits(codec, M9809X_090_PWR_EN_IN,
				M9809X_MB1EN, 0);
		codec->cache_sync = 1;
		break;
	}
	codec->dapm.bias_level = level;

	return 0;
}

#define MAX9809X_RATES SNDRV_PCM_RATE_8000_96000
#define MAX9809X_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE)

static struct snd_soc_dai_ops max9809X_dai1_ops = {
	.set_sysclk = max9809X_dai_set_sysclk,
	.set_fmt = max9809X_dai1_set_fmt,
	.set_tdm_slot = max9809X_dai1_set_tdm_slot,
	.hw_params = max9809X_dai1_hw_params,
	.digital_mute = max9809X_dai1_digital_mute,
};

static struct snd_soc_dai_ops max9809X_dai2_ops = {
	.set_sysclk = max9809X_dai_set_sysclk,
	.set_fmt = max9809X_dai2_set_fmt,
	.set_tdm_slot = max9809X_dai2_set_tdm_slot,
	.hw_params = max9809X_dai2_hw_params,
	.digital_mute = max9809X_dai2_digital_mute,
};

static struct snd_soc_dai_ops max9809X_dai3_ops = {
	.set_sysclk = max9809X_dai_set_sysclk,
	.set_fmt = max9809X_dai3_set_fmt,
	.set_tdm_slot = max9809X_dai3_set_tdm_slot,
	.hw_params = max9809X_dai3_hw_params,
	.digital_mute = max9809X_dai3_digital_mute,
};

static struct snd_soc_dai_driver max9809X_dai[] = {
{
	.name = "max9809X-hifi",
        .id = 0,
        .playback = {
            .stream_name = "HiFi Playback",
            .channels_min = 1,
            .channels_max = 2,
		.rates = MAX9809X_RATES,
		.formats = MAX9809X_FORMATS,
        },
        .capture = {
            .stream_name = "HiFi Capture",
            .channels_min = 1,
            .channels_max = 2,
		.rates = MAX9809X_RATES,
		.formats = MAX9809X_FORMATS,
        },
	.ops = &max9809X_dai1_ops,
    },
    {
	.name = "max9809X Voice",
        .id = 1,
        .playback = {
            .stream_name    = "Voice Playback",
            .channels_min   = 1,
            .channels_max   = 1,
            .rates          = SNDRV_PCM_RATE_8000,
            .formats        = SNDRV_PCM_FMTBIT_S16_LE,
        },
        .capture = {
            .stream_name    = "Voice Capture",
            .channels_min   = 1,
            .channels_max   = 1,
            .rates          = SNDRV_PCM_RATE_8000,
            .formats        = SNDRV_PCM_FMTBIT_S16_LE,
        },
	.ops = &max9809X_dai2_ops,
    },
    {
	.name = "max9809X BT",
        .playback = {
            .stream_name = "BT Playback",
            .channels_min = 1,
            .channels_max = 1,
            .rates = SNDRV_PCM_RATE_8000,
            .formats = SNDRV_PCM_FMTBIT_S16_LE,
        },
        .capture = {
            .stream_name    = "BT Capture",
            .channels_min   = 1,
            .channels_max   = 1,
            .rates          = SNDRV_PCM_RATE_8000,
            .formats        = SNDRV_PCM_FMTBIT_S16_LE,
        },
	.ops = &max9809X_dai3_ops,
    },
};

static int max9809X_get_eq_channel(const char *name)
{
    if (strcmp(name, "EQ1 Mode") == 0)
        return 0;
    if (strcmp(name, "EQ2 Mode") == 0)
        return 1;
    return -EINVAL;
}

//#define DEBUG_DUMP
#ifdef DEBUG_DUMP
void reg_print(struct snd_soc_codec *codec, char * str, int regfirst, int reglast)
{
    int i;
    volatile uint8_t buf[8];
    int regno;

    printk(KERN_ERR "%s", str);

    regno = regfirst;
    while (regno<=reglast) {
        for (i=0; i<8; i++)
            buf[i] = codec->hw_read(codec, regno+i);

        printk(KERN_ERR "reg[%02X]= %02X %02X %02X %02X %02X %02X %02X %02X ",
                regno, buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7]);

        regno = regno + 8;
    }
}
#endif


static int max9809X_put_eq_enum(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max9809X_priv *max9809X = snd_soc_codec_get_drvdata(codec);
	struct max9809X_pdata *pdata = max9809X->pdata;
	int channel = max9809X_get_eq_channel(kcontrol->id.name);
	struct max9809X_cdata *cdata;
	int sel = ucontrol->value.integer.value[0];
	struct max9809X_eq_cfg *coef_set;
	int fs, best, best_val, i;
	int regmask, regsave;

	BUG_ON(channel > 1);

    if (!pdata || !max9809X->eq_textcnt)
        return 0;

    if (sel >= pdata->eq_cfgcnt)
        return -EINVAL;

    cdata = &max9809X->dai[channel];
    cdata->eq_sel = sel;
    fs = cdata->rate;

	/* Find the selected configuration with nearest sample rate */
	best = 0;
	best_val = INT_MAX;
	for (i = 0; i < pdata->eq_cfgcnt; i++) {
		if (strcmp(pdata->eq_cfg[i].name, max9809X->eq_texts[sel]) == 0 &&
			abs(pdata->eq_cfg[i].rate - fs) < best_val) {
			best = i;
			best_val = abs(pdata->eq_cfg[i].rate - fs);
		}
	}

    dev_dbg(codec->dev, "Selected %s/%dHz for %dHz sample rate\n",
    pdata->eq_cfg[best].name,
    pdata->eq_cfg[best].rate, fs);

    coef_set = &pdata->eq_cfg[best];

	regmask = (channel == 0) ? M9809X_EQ1EN : M9809X_EQ2EN;

	/* Disable filter while configuring, and save current on/off state */
	regsave = snd_soc_read(codec, M9809X_088_CFG_LEVEL);
	snd_soc_update_bits(codec, M9809X_088_CFG_LEVEL, regmask, 0);

	mutex_lock(&codec->mutex);
	snd_soc_update_bits(codec, M9809X_00F_HOST_CFG, M9809X_SEG, M9809X_SEG);
	m9809X_eq_band(codec, channel, 0, coef_set->band1);
	m9809X_eq_band(codec, channel, 1, coef_set->band2);
	m9809X_eq_band(codec, channel, 2, coef_set->band3);
	m9809X_eq_band(codec, channel, 3, coef_set->band4);
	m9809X_eq_band(codec, channel, 4, coef_set->band5);
	snd_soc_update_bits(codec, M9809X_00F_HOST_CFG, M9809X_SEG, 0);
	mutex_unlock(&codec->mutex);

	/* Restore the original on/off state */
	snd_soc_update_bits(codec, M9809X_088_CFG_LEVEL, regmask, regsave);

#ifdef DEBUG_DUMP
    if (channel==0) {
        reg_print(codec, "Seg/Xtensa\n", 0x0F, 0x11);
        reg_print(codec, "FIFOS\n", 0x1E, 0x23);
        reg_print(codec, "DAI1\n", 0x27, 0x30);
        reg_print(codec, "DAI2\n", 0x31, 0x3A);
        reg_print(codec, "ADC DAI\n", 0x45, 0x46);
        reg_print(codec, "Mixers\n", 0x48, 0x52);
        reg_print(codec, "Levels\n", 0x5D, 0x6A);
        reg_print(codec, "Power\n", 0x90, 0x97);
    }
#endif

    return 0;
}

static int max9809X_get_eq_enum(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max9809X_priv *max9809X = snd_soc_codec_get_drvdata(codec);
	int channel = max9809X_get_eq_channel(kcontrol->id.name);
	struct max9809X_cdata *cdata;

	cdata = &max9809X->dai[channel];
	ucontrol->value.enumerated.item[0] = cdata->eq_sel;

	return 0;
}

static void max9809X_handle_eq_pdata(struct snd_soc_codec *codec)
{
	struct max9809X_priv *max9809X = snd_soc_codec_get_drvdata(codec);
	struct max9809X_pdata *pdata = max9809X->pdata;
	struct max9809X_eq_cfg *cfg;
	unsigned int cfgcnt;
	int i, j;
	const char **t;
	int ret;

	struct snd_kcontrol_new controls[] = {
		SOC_ENUM_EXT("EQ1 Mode",
			max9809X->eq_enum,
			max9809X_get_eq_enum,
			max9809X_put_eq_enum),
		SOC_ENUM_EXT("EQ2 Mode",
			max9809X->eq_enum,
			max9809X_get_eq_enum,
			max9809X_put_eq_enum),
	};

	cfg = pdata->eq_cfg;
	cfgcnt = pdata->eq_cfgcnt;

	/* Setup an array of texts for the equalizer enum.
	 * This is based on Mark Brown's equalizer driver code.
	 */
	max9809X->eq_textcnt = 0;
	max9809X->eq_texts = NULL;
	for (i = 0; i < cfgcnt; i++) {
		for (j = 0; j < max9809X->eq_textcnt; j++) {
			if (strcmp(cfg[i].name, max9809X->eq_texts[j]) == 0)
				break;
		}

		if (j != max9809X->eq_textcnt)
			continue;

		/* Expand the array */
		t = krealloc(max9809X->eq_texts,
			     sizeof(char *) * (max9809X->eq_textcnt + 1),
			     GFP_KERNEL);
		if (t == NULL)
			continue;

		/* Store the new entry */
		t[max9809X->eq_textcnt] = cfg[i].name;
		max9809X->eq_textcnt++;
		max9809X->eq_texts = t;
	}

	/* Now point the soc_enum to .texts array items */
	max9809X->eq_enum.texts = max9809X->eq_texts;
	max9809X->eq_enum.max = max9809X->eq_textcnt;

    ret = snd_soc_add_codec_controls(codec, controls, ARRAY_SIZE(controls));
    if (ret != 0)
        dev_err(codec->dev, "Failed to add EQ control: %d\n", ret);
}

static const char *bq_mode_name[] = {"Biquad1 Mode", "Biquad2 Mode"};
static int max9809X_get_bq_channel(struct snd_soc_codec *codec,
				   const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(bq_mode_name); i++)
		if (strcmp(name, bq_mode_name[i]) == 0)
			return i;

	/* Shouldn't happen */
	dev_err(codec->dev, "Bad biquad channel name '%s'\n", name);
	return -EINVAL;
}

static int max9809X_put_bq_enum(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max9809X_priv *max9809X = snd_soc_codec_get_drvdata(codec);
	struct max9809X_pdata *pdata = max9809X->pdata;
    int channel = max9809X_get_bq_channel(codec, kcontrol->id.name);
	struct max9809X_cdata *cdata;
	int sel = ucontrol->value.integer.value[0];
	struct max9809X_biquad_cfg *coef_set;
	int fs, best, best_val, i;
	int regmask, regsave;

	BUG_ON(channel > 1);

	cdata = &max9809X->dai[channel];

    if (sel >= pdata->bq_cfgcnt)
        return -EINVAL;

    cdata->bq_sel = sel;

	if (!pdata || !max9809X->bq_textcnt)
		return 0;

    fs = cdata->rate;

    /* Find the selected configuration with nearest sample rate */
    best = 0;
    best_val = INT_MAX;
    for (i = 0; i < pdata->bq_cfgcnt; i++) {
		if (strcmp(pdata->bq_cfg[i].name, max9809X->bq_texts[sel]) == 0 &&
                abs(pdata->bq_cfg[i].rate - fs) < best_val) {
            best = i;
            best_val = abs(pdata->bq_cfg[i].rate - fs);
        }
    }

    dev_dbg(codec->dev, "Selected %s/%dHz for %dHz sample rate\n",
    pdata->bq_cfg[best].name,
    pdata->bq_cfg[best].rate, fs);

    coef_set = &pdata->bq_cfg[best];

	regmask = (channel == 0) ? M9809X_BQ1EN : M9809X_BQ2EN;

	/* Disable filter while configuring, and save current on/off state */
	regsave = snd_soc_read(codec, M9809X_088_CFG_LEVEL);
	snd_soc_update_bits(codec, M9809X_088_CFG_LEVEL, regmask, 0);

	mutex_lock(&codec->mutex);
	snd_soc_update_bits(codec, M9809X_00F_HOST_CFG, M9809X_SEG, M9809X_SEG);
	m9809X_biquad_band(codec, channel, 0, coef_set->band1);
	m9809X_biquad_band(codec, channel, 1, coef_set->band2);
	snd_soc_update_bits(codec, M9809X_00F_HOST_CFG, M9809X_SEG, 0);
	mutex_unlock(&codec->mutex);

	/* Restore the original on/off state */
	snd_soc_update_bits(codec, M9809X_088_CFG_LEVEL, regmask, regsave);
	return 0;
}

static int max9809X_get_bq_enum(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max9809X_priv *max9809X = snd_soc_codec_get_drvdata(codec);
	int channel = max9809X_get_bq_channel(codec, kcontrol->id.name);
	struct max9809X_cdata *cdata;

	cdata = &max9809X->dai[channel];
	ucontrol->value.enumerated.item[0] = cdata->bq_sel;

    return 0;
}

static void max9809X_handle_bq_pdata(struct snd_soc_codec *codec)
{
	struct max9809X_priv *max9809X = snd_soc_codec_get_drvdata(codec);
	struct max9809X_pdata *pdata = max9809X->pdata;
	struct max9809X_biquad_cfg *cfg;
	unsigned int cfgcnt;
	int i, j;
	const char **t;
	int ret;

	struct snd_kcontrol_new controls[] = {
		SOC_ENUM_EXT((char *)bq_mode_name[0],
			max9809X->bq_enum,
			max9809X_get_bq_enum,
			max9809X_put_bq_enum),
		SOC_ENUM_EXT((char *)bq_mode_name[1],
			max9809X->bq_enum,
			max9809X_get_bq_enum,
			max9809X_put_bq_enum),
	};

	cfg = pdata->bq_cfg;
	cfgcnt = pdata->bq_cfgcnt;

	/* Setup an array of texts for the biquad enum.
	 * This is based on Mark Brown's equalizer driver code.
	 */
	max9809X->bq_textcnt = 0;
	max9809X->bq_texts = NULL;
	for (i = 0; i < cfgcnt; i++) {
		for (j = 0; j < max9809X->bq_textcnt; j++) {
			if (strcmp(cfg[i].name, max9809X->bq_texts[j]) == 0)
				break;
		}

		if (j != max9809X->bq_textcnt)
			continue;

		/* Expand the array */
		t = krealloc(max9809X->bq_texts,
			     sizeof(char *) * (max9809X->bq_textcnt + 1),
			     GFP_KERNEL);
		if (t == NULL)
			continue;

		/* Store the new entry */
		t[max9809X->bq_textcnt] = cfg[i].name;
		max9809X->bq_textcnt++;
		max9809X->bq_texts = t;
	}

	/* Now point the soc_enum to .texts array items */
	max9809X->bq_enum.texts = max9809X->bq_texts;
	max9809X->bq_enum.max = max9809X->bq_textcnt;

    ret = snd_soc_add_codec_controls(codec, controls, ARRAY_SIZE(controls));
	if (ret != 0)
        	dev_err(codec->dev, "Failed to add Biquad control: %d\n", ret);
}

static void max9809X_handle_pdata(struct snd_soc_codec *codec)
{
	struct max9809X_priv *max9809X = snd_soc_codec_get_drvdata(codec);
	struct max9809X_pdata *pdata = max9809X->pdata;
	u8 regval = 0;

	if (!pdata) {
		dev_dbg(codec->dev, "No platform data\n");
		return;
	}

	/* Configure mic for analog/digital mic mode */
	if (pdata->digmic_left_mode)
		regval |= M9809X_DIGMIC_L;

	if (pdata->digmic_right_mode)
		regval |= M9809X_DIGMIC_R;

	snd_soc_write(codec, M9809X_087_CFG_MIC, regval);

	/* Configure equalizers */
	if (pdata->eq_cfgcnt)
		max9809X_handle_eq_pdata(codec);

	/* Configure bi-quad filters */
	if (pdata->bq_cfgcnt)
		max9809X_handle_bq_pdata(codec);
}

#ifdef CONFIG_SWITCH
/* These values are copied from WiredAccessoryObserver */
enum headset_state {
    BIT_NO_HEADSET = 0,
    BIT_HEADSET = (1 << 0),
    BIT_HEADSET_NO_MIC = (1 << 1),
    BIT_PRESS_BUTTON = (1<<7),
    BIT_ADC_STATE_MASK = 0x7F,
};
#endif

#ifdef CONFIG_PM
extern  int read_hard_version_gpio(void);

static int max9809X_suspend(struct snd_soc_codec *codec)
{
	struct max9809X_priv *max9809X = snd_soc_codec_get_drvdata(codec);
	int dc_gpio_val = 0;

	dev_info(codec->dev, "+%s\n", __func__);

      disable_irq(max9809X->irq);
	max9809X_set_bias_level(codec, SND_SOC_BIAS_OFF);

	snd_soc_write(codec, M9809X_025_XCLKCFG, 0x00);
	/* reset the DSP firmware download flag for new download during power up */ 
	/* Low power for max9809X for wangyunan Begin */
	snd_soc_update_bits(codec,M9809X_026_SYS_CLK,0x10,0x00);
	snd_soc_update_bits(codec,M9809X_02C_DAI1_IOCFG,0x40,0x00);
	snd_soc_update_bits(codec,M9809X_036_DAI2_IOCFG,0x80,0x00);
	snd_soc_update_bits(codec,M9809X_040_DAI3_IOCFG,0xC0,0x00);
	snd_soc_update_bits(codec,M9809X_096_PWR_DAC_CK,0x3F,0x00);

        //ZTE: the bit affects the hook detect
        if (BIT_HEADSET != state_hp){
            dev_info(codec->dev, "+CODEC SHUTDOWN+\n");
	      snd_soc_update_bits(codec, M9809X_097_PWR_SYS, M9809X_SHDNRUN|M9809X_PWRSV, 0);

            dc_gpio_val = read_hard_version_gpio();
	     if(dc_gpio_val == 0x02)
	     {
	          gpio_set_value(TEGRA_GPIO_SPK_DC_EN_PIN, 0);

	     }
        }
	/* Low power for max9809X for wangyunan end */
	max9809X->cur_dsp = -1;
	dev_info(codec->dev, "-%s\n", __func__);
	return 0;
}

static int max9809X_resume(struct snd_soc_codec *codec)
{   
	struct max9809X_priv *max9809X = snd_soc_codec_get_drvdata(codec);
	int dc_gpio_val = 0;

	dev_info(codec->dev, "+%s\n", __func__);

      dc_gpio_val = read_hard_version_gpio();
	if(dc_gpio_val == 0x02)
	{
		gpio_set_value(TEGRA_GPIO_SPK_DC_EN_PIN, 1);
	}

	max9809X_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	snd_soc_write(codec, M9809X_025_XCLKCFG, 0x5F);
	/* Resume */
	snd_soc_update_bits(codec,M9809X_026_SYS_CLK,0x10,0x10);
	snd_soc_update_bits(codec,M9809X_02C_DAI1_IOCFG,0x40,0x40);
	snd_soc_update_bits(codec,M9809X_036_DAI2_IOCFG,0x80,0x80);
	snd_soc_update_bits(codec,M9809X_040_DAI3_IOCFG,0xC0,0xC0);

	snd_soc_update_bits(codec,M9809X_096_PWR_DAC_CK,0x3F,0x3F);
	snd_soc_update_bits(codec,M9809X_097_PWR_SYS, M9809X_SHDNRUN|M9809X_PWRSV, M9809X_SHDNRUN|M9809X_PWRSV);
	enable_irq(max9809X->irq);
	dev_info(codec->dev, "-%s\n", __func__);
    	return 0;
}
#else
#define max9809X_suspend NULL
#define max9809X_resume NULL
#endif

static int max9809X_reset(struct snd_soc_codec *codec)
{
    	int i, ret;
	/* Gracefully reset the DSP core and the codec hardware
	 * in a proper sequence */
	ret = snd_soc_write(codec, M9809X_00F_HOST_CFG, 0);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to reset DSP: %d\n", ret);
		return ret;
	}

	ret = snd_soc_write(codec, M9809X_097_PWR_SYS, 0);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to reset codec: %d\n", ret);
		return ret;
	}

	/* Reset to hardware default for registers, as there is not
	 * a soft reset hardware control register */
	for (i = M9809X_010_HOST_INT_CFG; i < M9809X_REG_MAX_CACHED; i++) {
		ret = snd_soc_write(codec, i, max9809X_reg_def[i]);
		if (ret < 0) {
			dev_err(codec->dev, "Failed to reset: %d\n", ret);
			return ret;
		}
	}

    return ret;
}

//ZTE: hp detection
#ifdef CONFIG_SWITCH
static int state_flag = 0;
static irqreturn_t tegra_max9809X_mic_irq(int irq, void *data)
{
    struct snd_soc_codec *codec = data;
    struct max9809X_priv *max9809X = snd_soc_codec_get_drvdata(codec);

    int gpio_val,reg_val = 0;
    int time=0;

    max9809X_mic_time = jiffies /HZ;
    time= abs (max9809X_mic_time -max9809X_hp_time);
    dev_info(codec->dev,"[hp]tegra_max9809X_mic_irq mic_time :%ld,time=%d\n",max9809X_mic_time,time);
    mutex_lock(&codec->mutex);
    reg_val =snd_soc_read(codec, M9809X_007_JACK_AUTO_STS);
    reg_val = snd_soc_read(codec, M9809X_00A_ACC_ADC_STS);
    mutex_unlock(&codec->mutex);
  
    // Add delay for stable the irq of hook
    msleep(20);
    gpio_val = gpio_get_value(TEGRA_GPIO_HP_DET);
    if ((time >= 2)&&(state_hp == BIT_HEADSET)&&(gpio_val == 0)){
        if(reg_val & BIT_PRESS_BUTTON){
            state_flag = 1;
            dev_info(codec->dev,"[hp]    button press input_report_key KEY_MEDIA 1\n");
            input_report_key(max9809X->idev, KEY_MEDIA, 1);
            input_sync(max9809X->idev);
        }else{
            dev_info(codec->dev,"[hp]    button release state_flag = %d\n",state_flag);
            if(state_flag ==  1){
                state_flag = 0;
                input_report_key(max9809X->idev, KEY_MEDIA, 0);
                dev_info(codec->dev,"[hp]    button releases input_report_key KEY_MEDIA 0\n");
                input_sync(max9809X->idev);
            }else{
                dev_info(codec->dev,"[hp]    button press and releases\n");
                input_report_key(max9809X->idev, KEY_MEDIA, 1);
                msleep(50);
                input_report_key(max9809X->idev, KEY_MEDIA, 0);
                input_sync(max9809X->idev);
            }
        }
    }            
    return IRQ_HANDLED;
}

#endif

static int max9809X_probe(struct snd_soc_codec *codec)
{
	struct max9809X_priv *max9809X = snd_soc_codec_get_drvdata(codec);
	struct max9809X_cdata *cdata;
	int ret = 0;
	max9809X_codec = codec;

	dev_info(codec->dev,"[codec]  max9809X_probe \n");
	codec->cache_sync = 1;

	if (SND_SOC_SPI == max9809X->control_type){
		codec->write = max9809X_write;
		codec->read = max9809X_read;
		codec->control_data = container_of(codec->dev,
					struct spi_device,dev);
		codec->hw_write = (hw_write_t)max9809X_do_spi_write;
		codec->hw_read = max9809X_do_spi_read;
	}else{//ZTE: the other bus format
		return -1;
	}
	/* reset the codec, the DSP core, and disable all interrupts */
	max9809X_reset(codec);

	if (SND_SOC_SPI == max9809X->control_type)
	{
	snd_soc_write(codec,0x46,0x03);
	}

	/* initialize private data */
	max9809X->sysclk = (unsigned)-1;
	max9809X->eq_textcnt = 0;
	max9809X->bq_textcnt = 0;
	max9809X->fs_textcnt = 0;
	max9809X->fs_sel = 0;
	max9809X->cur_dsp = -1;

	cdata = &max9809X->dai[0];
	cdata->rate = (unsigned)-1;
	cdata->fmt  = (unsigned)-1;
	cdata->eq_sel = 0;
	cdata->bq_sel = 0;

	cdata = &max9809X->dai[1];
	cdata->rate = (unsigned)-1;
	cdata->fmt  = (unsigned)-1;
	cdata->eq_sel = 0;
	cdata->bq_sel = 0;

	cdata = &max9809X->dai[2];
	cdata->rate = (unsigned)-1;
	cdata->fmt  = (unsigned)-1;
	cdata->eq_sel = 0;
	cdata->bq_sel = 0;

	max9809X->lin_state = 0;
	max9809X->mic1pre = 0;
	max9809X->mic2pre = 0;

	snd_soc_write(codec, M9809X_097_PWR_SYS, M9809X_PWRSV);
	snd_soc_write(codec, M9809X_048_MIX_DAC_LR,
	M9809X_DAI1L_TO_DACL|M9809X_DAI1R_TO_DACR|
	M9809X_DAI2M_TO_DACL|M9809X_DAI2M_TO_DACR);

	snd_soc_write(codec, M9809X_092_PWR_EN_OUT, M9809X_SPK_SPREADSPECTRUM);
	snd_soc_write(codec, M9809X_045_CFG_DSP, M9809X_DSPNORMAL);
	snd_soc_write(codec, M9809X_04E_CFG_HP, M9809X_HPNORMAL);
	snd_soc_write(codec, M9809X_02C_DAI1_IOCFG,
	M9809X_S1NORMAL|M9809X_SDATA);

	snd_soc_write(codec, M9809X_036_DAI2_IOCFG,
	M9809X_S2NORMAL|M9809X_SDATA);

	snd_soc_write(codec, M9809X_040_DAI3_IOCFG,
	M9809X_S3NORMAL|M9809X_SDATA);

	snd_soc_write(codec, M9809X_08D_JACK_KEY_THRESH,
	0x3c);

	snd_soc_write(codec, M9809X_046_DAC_CTRL1, M9809X_CHOPCLKNORMAL);
	/* set MICBIAS2 to 2.8V */
	snd_soc_update_bits(codec, M9809X_093_BIAS_CTRL, M9809X_VMICBIAS1_MASK|M9809X_VMICBIAS2_MASK,
	M9809X_VMICBIAS2_2_8V);
	//ZTE: set ADC volume
	snd_soc_update_bits(codec, M9809X_05D_LVL_ADC_L, M9809X_ADC_ATT, M9809X_ADC_ATT_VOLUME);
	snd_soc_update_bits(codec, M9809X_05E_LVL_ADC_R, M9809X_ADC_ATT, M9809X_ADC_ATT_VOLUME);

	snd_soc_update_bits(codec, M9809X_088_CFG_LEVEL,M9809X_VSEN,M9809X_VSEN);
	snd_soc_update_bits(codec, M9809X_088_CFG_LEVEL,M9809X_VS2EN,M9809X_VS2EN);

	max9809X_handle_pdata(codec);

	/* take the codec out of the shut down */
	snd_soc_update_bits(codec, M9809X_097_PWR_SYS, M9809X_SHDNRUN,M9809X_SHDNRUN);
	/*set the codec low power */
	snd_soc_update_bits(codec, M9809X_097_PWR_SYS, M9809X_PERFMODE,M9809X_PERFMODE);

	/*Initial Key Press Enable for AP HP detect*/
	snd_soc_update_bits(codec, M9809X_089_JACK_DET_AUTO, M9809X_KEYEN, M9809X_KEYEN);  

#ifdef CONFIG_SWITCH
	if (max9809X->irq) {
		ret= request_threaded_irq(max9809X->irq, NULL, tegra_max9809X_mic_irq,
				IRQF_TRIGGER_FALLING, "HOOK DETECT", codec);
		if (ret < 0){
			dev_info(codec->dev,"max9809X_hook_init fail!ret = %d\n",ret);
		}
		max9809X->idev = kzalloc(sizeof (struct input_dev), GFP_KERNEL);
		max9809X->idev = input_allocate_device();

		input_set_capability(max9809X->idev, EV_KEY, KEY_MEDIA);
		ret= input_register_device(max9809X->idev);
		if (ret < 0){
			pr_err("[audio]max9809X input_register_device fail\n");
		}
		ret = enable_irq_wake(max9809X->irq);
		if (ret) {
			pr_err("Could NOT set up mic_det gpio pins for wakeup the AP.\n");
		}
		device_init_wakeup(&(max9809X->idev->dev), 1);
	}       
#endif
	max9809X_add_widgets(codec);
	dsp_fs_setup(codec);

	/* initialize registers cache to hardware default */
	max9809X_set_bias_level(codec, SND_SOC_BIAS_STANDBY);	
	dev_info(codec->dev,"[codec]  max9809X_probe OK!\n");

	return ret;
}

static int max9809X_remove(struct snd_soc_codec *codec)
{
	struct max9809X_priv *max9809X = snd_soc_codec_get_drvdata(codec);

	max9809X_set_bias_level(codec, SND_SOC_BIAS_OFF);
	kfree(max9809X->bq_texts);
	kfree(max9809X->eq_texts);

	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_max9809X = {
	.probe   = max9809X_probe,
	.remove  = max9809X_remove,
	.suspend = max9809X_suspend,
	.resume  = max9809X_resume,
	.set_bias_level = max9809X_set_bias_level,
	.reg_cache_size = ARRAY_SIZE(max9809X_reg_def),
	.reg_word_size = sizeof(u8),
	.reg_cache_default = max9809X_reg_def,
	.readable_register = max9809X_readable,
	.volatile_register = max9809X_volatile_register,
	.dapm_widgets	  = max9809X_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(max9809X_dapm_widgets),
	.dapm_routes     = max9809X_audio_map,
	.num_dapm_routes = ARRAY_SIZE(max9809X_audio_map),
};

/* ---------------------------------------------------------------------
* SPI device portion of driver: probe and release routines and SPI
*                               driver registration
*  Jelphi.zhang added 5/22/2012
* SPI tools begin
-----------------------------------------------------------------------*/
static struct cdev max_cdev;
static void * control;

//#define DEBUG_SPI_TOOLS
#define MAXSPI_MAX_DEVICES 1

static int maxspi_open(struct inode *inode, struct file *filep)
{
	return 0;
}

static ssize_t maxspi_read(struct file *filep, char __user *buf,
			size_t count, loff_t *ppos)
{
	struct snd_soc_codec * codec;
	unsigned int data = 0;
	char  dbuf[200];
	char reg;
	int ret;
	int i;
#ifdef DEBUG_SPI_TOOLS
	dev_info(codec->dev,"maxspi_read in count = %d\n",count);
#endif
	ret = copy_from_user (&reg, buf, 1);
	if (ret)
		return -EFAULT;

	codec = max9809X_codec;
	codec->control_data = control;

	for (i = 0; i < count; i++) {
		data = max9809X_do_spi_read (codec, (unsigned int) reg);
#ifdef DEBUG_SPI_TOOLS
		pr_info( "%s: data 0x%02X, byte %d of %d\n", __func__, data, i, count );
#endif
		dbuf[i] = (char) data;
		if (reg != 0)
			reg++;
	}
	ret = copy_to_user (buf, dbuf, count);
	if (ret)
		return (ssize_t) ret;

	return (ssize_t) count;
}

static ssize_t maxspi_write(struct file *filep, const char __user *buf,
			size_t count, loff_t *ppos)
{
	char  dbuf[200]  ;
	int ret;

	ret = copy_from_user (dbuf, buf, count);
#ifdef DEBUG_SPI_TOOLS
	dev_info(codec->dev,"maxspi_write ret = %d count =%d\n",ret,count);
#endif
	if (ret)
		return -EFAULT;

	ret = max9809X_do_spi_write (control, dbuf, count);
#ifdef DEBUG_SPI_TOOLS
	dev_info(codec->dev,"maxspi_write dbuf[0] = %x dbuf[1] = %x,count = %d, ret=%d\n",dbuf[0],dbuf[1],count,ret);
#endif
	if (ret)
		return (ssize_t) ret;

	return (ssize_t) count;
}

static const struct file_operations maxspi_fops = {
    .owner		= THIS_MODULE,
    .open		= maxspi_open,
    .release	= NULL,
    .read		= maxspi_read,
    .write		= maxspi_write,
    .mmap		= NULL,
    .poll		= NULL,
    .fasync		= NULL,
    .llseek		= NULL,
};

static int maxspi_init (struct spi_device *spi)
{
	static const char name[] = "maxspi";
	dev_t dev = 0;
	int result;
	struct device  *spidev;
	struct class *maxspinod_class;
	control = (void *) spi;

	result = alloc_chrdev_region (&dev, 0, MAXSPI_MAX_DEVICES, name);
	if (result) {
		printk (KERN_ERR "alloc_chrdev_region FAILED\n");
		return result;
	}

	cdev_init(&max_cdev, &maxspi_fops);
	max_cdev.owner = THIS_MODULE;
	max_cdev.ops   = &maxspi_fops;
	result = cdev_add(&max_cdev, dev, MAXSPI_MAX_DEVICES);
	if (result) {
		printk (KERN_ERR "cdev_add FAILED\n");
		unregister_chrdev_region(dev, MAXSPI_MAX_DEVICES);
		return result;
	}

	maxspinod_class = class_create(THIS_MODULE, "maxspi");
	spidev=device_create(maxspinod_class, NULL, MKDEV(MAJOR(dev), 0), NULL, "maxspi");
	if (IS_ERR(spidev)) {
		printk(KERN_ERR "Unable to create device  errno = %ld\n", PTR_ERR(spidev));
		spidev= NULL;
	}
	return 0;
}

/* ---------------------------------------------------------------------
* SPI tools end
-----------------------------------------------------------------------*/

static int __devinit max9809X_spi_probe(struct spi_device *spi)
{
	struct max9809X_priv *max9809X;
	int ret;

	pr_info("%s: max_speed_hz %d, chip_select %d, mode %d, bits_per_word %d\n",
			__func__,
			spi->max_speed_hz,
			spi->chip_select,
			spi->mode,
			spi->bits_per_word);

	dev_info(&spi->dev, "[codec]  max9809X_spi_probe in\n");
	max9809X = kzalloc(sizeof(struct max9809X_priv), GFP_KERNEL);
	if (max9809X == NULL)
		return -ENOMEM;

	spi_set_drvdata(spi, max9809X);
	max9809X->control_data = spi;
	max9809X->control_type = SND_SOC_SPI;
	max9809X->pdata = spi->dev.platform_data;
	//ZTE: add for codec irq
	max9809X->irq = spi->irq;

	ret = snd_soc_register_codec(&spi->dev,
	  		&soc_codec_dev_max9809X, &max9809X_dai, ARRAY_SIZE(max9809X_dai));
	if (ret < 0){
		dev_err(&spi->dev, "snd_soc_register_codec fail ret=%x\n", ret);
		kfree(max9809X);
	}else{
		maxspi_init (spi);
		dev_info(&spi->dev, "[codec]  max9809X_spi_probe OK!\n");
	}
	return ret;
}

static int __devexit max9809X_spi_remove(struct spi_device *spi)               
{
	snd_soc_unregister_codec(&spi->dev);                                 
	kfree(spi_get_drvdata(spi));                                         
	return 0;
}

static const struct spi_device_id max9809X_spi_id[] = {
	{ "max9809X", MAX9809X },
	{ }
};

static struct spi_driver max9809X_spi_driver = {                               
         .driver = {                                                          
                 .name   = "max9809X-codec",                                    
                 .owner  = THIS_MODULE,                                       
         },                                                                   
         .probe          = max9809X_spi_probe,                                  
         .remove         = __devexit_p(max9809X_spi_remove), 
         .id_table = max9809X_spi_id,
 };   

static int __init max9809X_init(void)
{
	int ret;
	printk("[codec] max9809X_init\n");
	ret = spi_register_driver(&max9809X_spi_driver);
	if (ret != 0)
		pr_err("Failed to register max9809X SPI driver: %d\n", ret);

	return ret;
}
module_init(max9809X_init);

static void __exit max9809X_exit(void)
{
	spi_unregister_driver (&max9809X_spi_driver);
}
module_exit(max9809X_exit);

MODULE_DESCRIPTION("ALSA SoC MAX9809X driver");
MODULE_AUTHOR("Peter Hsiang");
MODULE_LICENSE("GPL");

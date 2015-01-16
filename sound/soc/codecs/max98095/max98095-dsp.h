/*
 * max98095-dsp.h
 *
 * Copyright 2011 Maxim Integrated Products
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _MAX98095_DSP_H
#define _MAX98095_DSP_H

#define DSP_XFER_MAX         128 /* 512 */

/* DSP communication low level protocol constants */
#define MID_ESCAPE              0xdb
#define MID_ESCAPE_REPLACEMENT  0x80
#define MID_ACK                 0x33
#define MID_LEN_MASK            0x7f
#define MID_TOGGLE_MASK         0x80

/* DSP communication commands and requests */
#define MID_GET_ID              0xff
#define MID_DOWNLOAD            0x00
#define MID_SET_PARAMETER       0x01
#define MID_GET_PARAMETER       0x02
#define MID_SET_CONFIGURATION   0x03
#define MID_GET_CONFIGURATION   0x04

int max98095_flexsound_init(struct snd_soc_codec *codec);

#endif /* _MAX98095_DSP_H */


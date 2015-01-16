/*
 * max9809X-dsp.h
 *
 * Copyright 2012 Maxim Integrated Products
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _MAX9809X_DSP_H
#define _MAX9809X_DSP_H
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

#define DSP_CACHE_FIRMWARE
#define BANK0                      0
#define BANK1             M9809X_SEG

/* maximum length of a firmware's kcontrol name */
#define MAX_NAME_LEN             (64)
#define MAX_EXT_LEN               (5)

typedef struct dsp_fw_entry {
	unsigned char         name[MAX_NAME_LEN];
	unsigned int          size;
	int                   file_num;
	void                * fsav;
	struct dsp_fw_entry * next;
} dsp_fw__entry_t;

struct dsp_fw {
	unsigned int count;
	struct dsp_fw_entry * dsp_fw_list;
};

struct __attribute__((packed)) header {
	char version[5];
	char date[11];
	char time[9];
	unsigned int kcontrol_name_size;
	unsigned int kcontrol_name_offset;
	unsigned int regs_bank0_count;
	unsigned int regs_bank0_offset;
	unsigned int regs_bank1_count;
	unsigned int regs_bank1_offset;
	unsigned int fw_images_count;
	unsigned int fw_images_offset;
	unsigned int params_count;
	unsigned int params_offset;
	unsigned int checksum_size;
	unsigned int checksum_offset;
	unsigned int raw_data_size;
	unsigned int raw_data_offset;
};

struct __attribute__((packed)) fw_images {
	unsigned int fw_image_size;
	unsigned int fw_image_offset;
};

struct __attribute__((packed)) params {
	unsigned int params_size;
	unsigned int module_id;
	unsigned int data[];
};

struct __attribute__((packed)) config_reg_data {
	unsigned char reg;
	unsigned char value;
};

/* FW versions supported */
#define FW_VERSION1			"0001"

/* Access macros for version 0001 binary FSAVs */
#define REGS_BANK0_COUNT(p) (unsigned int)(p->regs_bank0_count/sizeof(struct config_reg_data))
#define REGS_BANK0_DATA(p) (struct config_reg_data *)((unsigned)p+p->regs_bank0_offset)
#define REGS_BANK1_COUNT(p) (unsigned int)(p->regs_bank1_count/sizeof(struct config_reg_data))
#define REGS_BANK1_DATA(p) (struct config_reg_data *)((unsigned)p+p->regs_bank1_offset)
#define FW_IMAGES(p) (struct fw_images *)((unsigned)p+p->fw_images_offset)
#define FW_IMAGE(p,f) (char *)((unsigned)p+f->fw_image_offset)
//#define DSP_CONFIG(p) (char *)((unsigned)p+p->configuration_offset)
#define DSP_PARAMS_COUNT(p) (unsigned int)(p->params_count)
#define DSP_PARAMS(p) (struct params *)((unsigned)p+p->params_offset)
#define MODULE_PARAMS_SIZE(p) (unsigned int)(p->params_size)
#define MODULE_PARAMS_COUNT(p) (unsigned int)((p->params_size-sizeof(unsigned int)*2)/sizeof(unsigned int))
#define MODULE_PARAMS_MODULE_ID(p) (unsigned int)(p->module_id)

int dsp_flexsound_init(struct snd_soc_codec *codec);

#endif /* _MAX9809X_DSP_H */


/*
 * max9809X.h -- MAX98095/6 ALSA SoC Audio driver
 *
 * Copyright 2011-2012 Maxim Integrated Products
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _MAX9809X_H
#define _MAX9809X_H

/*
 * Driver version
 */
#define MAX9809X_VERSION                "0.01.50"
/*

 * MAX98095/6 Registers Definition
 */

#define M9809X_000_HOST_DATA                0x00
#define M9809X_001_HOST_INT_STS             0x01
#define M9809X_002_HOST_RSP_STS             0x02
#define M9809X_003_HOST_CMD_STS             0x03
#define M9809X_004_CODEC_STS                0x04
#define M9809X_005_DAI1_ALC_STS             0x05
#define M9809X_006_DAI2_ALC_STS             0x06
#define M9809X_007_JACK_AUTO_STS            0x07
#define M9809X_008_JACK_MANUAL_STS          0x08
#define M9809X_009_JACK_VBAT_STS            0x09
#define M9809X_00A_ACC_ADC_STS              0x0A
#define M9809X_00B_MIC_NG_AGC_STS           0x0B
#define M9809X_00C_SPK_L_VOLT_STS           0x0C
#define M9809X_00D_SPK_R_VOLT_STS           0x0D
#define M9809X_00E_TEMP_SENSOR_STS          0x0E
#define M9809X_00F_HOST_CFG                 0x0F
#define M9809X_010_HOST_INT_CFG             0x10
#define M9809X_011_HOST_INT_EN              0x11
#define M9809X_012_CODEC_INT_EN             0x12
#define M9809X_013_JACK_INT_EN              0x13
#define M9809X_014_JACK_INT_EN              0x14
#define M9809X_015_DEC                      0x15
#define M9809X_016_RESERVED                 0x16
#define M9809X_017_RESERVED                 0x17
#define M9809X_018_KEYCODE3                 0x18
#define M9809X_019_KEYCODE2                 0x19
#define M9809X_01A_KEYCODE1                 0x1A
#define M9809X_01B_KEYCODE0                 0x1B
#define M9809X_01C_OEMCODE1                 0x1C
#define M9809X_01D_OEMCODE0                 0x1D
#define M9809X_01E_XCFG1                    0x1E
#define M9809X_01F_XCFG2                    0x1F
#define M9809X_020_XCFG3                    0x20
#define M9809X_021_XCFG4                    0x21
#define M9809X_022_XCFG5                    0x22
#define M9809X_023_XCFG6                    0x23
#define M9809X_024_XGPIO                    0x24
#define M9809X_025_XCLKCFG                  0x25
#define M9809X_026_SYS_CLK                  0x26
#define M9809X_027_DAI1_CLKMODE             0x27
#define M9809X_028_DAI1_CLKCFG_HI           0x28
#define M9809X_029_DAI1_CLKCFG_LO           0x29
#define M9809X_02A_DAI1_FORMAT              0x2A
#define M9809X_02B_DAI1_CLOCK               0x2B
#define M9809X_02C_DAI1_IOCFG               0x2C
#define M9809X_02D_DAI1_TDM                 0x2D
#define M9809X_02E_DAI1_FILTERS             0x2E
#define M9809X_02F_DAI1_LVL1                0x2F
#define M9809X_030_DAI1_LVL2                0x30
#define M9809X_031_DAI2_CLKMODE             0x31
#define M9809X_032_DAI2_CLKCFG_HI           0x32
#define M9809X_033_DAI2_CLKCFG_LO           0x33
#define M9809X_034_DAI2_FORMAT              0x34
#define M9809X_035_DAI2_CLOCK               0x35
#define M9809X_036_DAI2_IOCFG               0x36
#define M9809X_037_DAI2_TDM                 0x37
#define M9809X_038_DAI2_FILTERS             0x38
#define M9809X_039_DAI2_LVL1                0x39
#define M9809X_03A_DAI2_LVL2                0x3A
#define M9809X_03B_DAI3_CLKMODE             0x3B
#define M9809X_03C_DAI3_CLKCFG_HI           0x3C
#define M9809X_03D_DAI3_CLKCFG_LO           0x3D
#define M9809X_03E_DAI3_FORMAT              0x3E
#define M9809X_03F_DAI3_CLOCK               0x3F
#define M9809X_040_DAI3_IOCFG               0x40
#define M9809X_041_DAI3_TDM                 0x41
#define M9809X_042_DAI3_FILTERS             0x42
#define M9809X_043_DAI3_LVL1                0x43
#define M9809X_044_DAI3_LVL2                0x44
#define M9809X_045_CFG_DSP                  0x45
#define M9809X_046_DAC_CTRL1                0x46
#define M9809X_047_DAC_CTRL2                0x47
#define M9809X_048_MIX_DAC_LR               0x48
#define M9809X_049_MIX_DAC_M                0x49
#define M9809X_04A_MIX_ADC_LEFT             0x4A
#define M9809X_04B_MIX_ADC_RIGHT            0x4B
#define M9809X_04C_MIX_HP_LEFT              0x4C
#define M9809X_04D_MIX_HP_RIGHT             0x4D
#define M9809X_04E_CFG_HP                   0x4E
#define M9809X_04F_MIX_RCV                  0x4F
#define M9809X_050_MIX_SPK_LEFT             0x50
#define M9809X_051_MIX_SPK_RIGHT            0x51
#define M9809X_052_MIX_SPK_CFG              0x52
#define M9809X_053_MIX_LINEOUT1             0x53
#define M9809X_054_MIX_LINEOUT2             0x54
#define M9809X_055_MIX_LINEOUT_CFG          0x55
#define M9809X_056_LVL_SIDETONE_DAI12       0x56
#define M9809X_057_LVL_SIDETONE_DAI3        0x57
#define M9809X_058_LVL_DAI1_PLAY            0x58
#define M9809X_059_LVL_DAI1_EQ              0x59
#define M9809X_05A_LVL_DAI2_PLAY            0x5A
#define M9809X_05B_LVL_DAI2_EQ              0x5B
#define M9809X_05C_LVL_DAI3_PLAY            0x5C
#define M9809X_05D_LVL_ADC_L                0x5D
#define M9809X_05E_LVL_ADC_R                0x5E
#define M9809X_05F_LVL_MIC1                 0x5F
#define M9809X_060_LVL_MIC2                 0x60
#define M9809X_061_LVL_LINEIN               0x61
#define M9809X_062_LVL_LINEOUT1             0x62
#define M9809X_063_LVL_LINEOUT2             0x63
#define M9809X_064_LVL_HP_L                 0x64
#define M9809X_065_LVL_HP_R                 0x65
#define M9809X_066_LVL_RCV                  0x66
#define M9809X_067_LVL_SPK_L                0x67
#define M9809X_068_LVL_SPK_R                0x68
#define M9809X_069_MICAGC_CFG               0x69
#define M9809X_06A_MICAGC_THRESH            0x6A
#define M9809X_06B_SPK_NOISEGATE            0x6B
#define M9809X_06C_DAI1_ALC1_TIME           0x6C
#define M9809X_06D_DAI1_ALC1_COMP           0x6D
#define M9809X_06E_DAI1_ALC1_EXPN           0x6E
#define M9809X_06F_DAI1_ALC1_GAIN           0x6F
#define M9809X_070_DAI1_ALC2_TIME           0x70
#define M9809X_071_DAI1_ALC2_COMP           0x71
#define M9809X_072_DAI1_ALC2_EXPN           0x72
#define M9809X_073_DAI1_ALC2_GAIN           0x73
#define M9809X_074_DAI1_ALC3_TIME           0x74
#define M9809X_075_DAI1_ALC3_COMP           0x75
#define M9809X_076_DAI1_ALC3_EXPN           0x76
#define M9809X_077_DAI1_ALC3_GAIN           0x77
#define M9809X_078_DAI2_ALC1_TIME           0x78
#define M9809X_079_DAI2_ALC1_COMP           0x79
#define M9809X_07A_DAI2_ALC1_EXPN           0x7A
#define M9809X_07B_DAI2_ALC1_GAIN           0x7B
#define M9809X_07C_DAI2_ALC2_TIME           0x7C
#define M9809X_07D_DAI2_ALC2_COMP           0x7D
#define M9809X_07E_DAI2_ALC2_EXPN           0x7E
#define M9809X_07F_DAI2_ALC2_GAIN           0x7F
#define M9809X_080_DAI2_ALC3_TIME           0x80
#define M9809X_081_DAI2_ALC3_COMP           0x81
#define M9809X_082_DAI2_ALC3_EXPN           0x82
#define M9809X_083_DAI2_ALC3_GAIN           0x83
#define M9809X_084_HP_NOISE_GATE            0x84
#define M9809X_085_AUX_ADC                  0x85
#define M9809X_086_CFG_LINE                 0x86
#define M9809X_087_CFG_MIC                  0x87
#define M9809X_088_CFG_LEVEL                0x88
#define M9809X_089_JACK_DET_AUTO            0x89
#define M9809X_08A_JACK_DET_MANUAL          0x8A
#define M9809X_08B_JACK_KEYSCAN_DBC         0x8B
#define M9809X_08C_JACK_KEYSCAN_DLY         0x8C
#define M9809X_08D_JACK_KEY_THRESH          0x8D
#define M9809X_08E_JACK_DC_SLEW             0x8E
#define M9809X_08F_JACK_TEST_CFG            0x8F
#define M9809X_090_PWR_EN_IN                0x90
#define M9809X_091_PWR_EN_OUT               0x91
#define M9809X_092_PWR_EN_OUT               0x92
#define M9809X_093_BIAS_CTRL                0x93
#define M9809X_094_PWR_DAC_21               0x94
#define M9809X_095_PWR_DAC_03               0x95
#define M9809X_096_PWR_DAC_CK               0x96
#define M9809X_097_PWR_SYS                  0x97

#define M9809X_0FF_REV_ID                   0xFF

#define M9809X_REG_CNT                      (0xFF+1)
#define M9809X_REG_MAX_CACHED               0X97

/* MAX98095/6 Registers Bit Fields */

/* M98095_007_JACK_AUTO_STS */
       #define M9809X_DDONE              (1<<7)
       #define M9809X_HP_IN              (1<<6)
       #define M9809X_MIC_IN            (1<<3)
	#define M9809X_ILO_IN            (1<<5)
	#define M9809X_IVID_IN           (1<<4)
	#define M9809X_MCSW              (1<<1)
/* M9809X_00F_HOST_CFG */
	#define M9809X_SEG                      (1<<0)
	#define M9809X_XTEN                     (1<<1)
	#define M9809X_MDLLEN                   (1<<2)
	#define M9809X_XTCLKEN                  (1<<3)
	#define M9809X_DCRYPTEN                 (1<<4)

/* M9809X_013_JACK_INT_EN */
       #define M9809X_IDDONE                  (1<<7)
	#define M9809X_IMCSW                   (1<<1)
	#define M9809X_IKEYDET                    (1<<0)
/* M9809X_027_DAI1_CLKMODE, M9809X_031_DAI2_CLKMODE, M9809X_03B_DAI3_CLKMODE */
	#define M9809X_CLKMODE_MASK             0xFF

/* M9809X_02A_DAI1_FORMAT, M9809X_034_DAI2_FORMAT, M9809X_03E_DAI3_FORMAT */
	#define M9809X_DAI_MAS                  (1<<7)
	#define M9809X_DAI_WCI                  (1<<6)
	#define M9809X_DAI_BCI                  (1<<5)
	#define M9809X_DAI_DLY                  (1<<4)
	#define M9809X_DAI_TDM                  (1<<2)
	#define M9809X_DAI_FSW                  (1<<1)
	#define M9809X_DAI_WS                   (1<<0)

/* M9809X_02B_DAI1_CLOCK, M9809X_035_DAI2_CLOCK, M9809X_03F_DAI3_CLOCK */
	#define M9809X_DAI_BSEL64               (1<<0)
	#define M9809X_DAI_DOSR_DIV2            (0<<5)
	#define M9809X_DAI_DOSR_DIV4            (1<<5)
	#define M9809X_DAI_BSEL_DIV8         (1<<3)

/* M9809X_02C_DAI1_IOCFG, M9809X_036_DAI2_IOCFG, M9809X_040_DAI3_IOCFG */
	#define M9809X_S1NORMAL                 (1<<6)
	#define M9809X_S2NORMAL                 (2<<6)
	#define M9809X_S3NORMAL                 (3<<6)
	#define M9809X_SDATA                    (3<<0)

/* M9809X_02E_DAI1_FILTERS, M9809X_038_DAI2_FILTERS, M9809X_042_DAI3_FILTERS */
	#define M9809X_DAI_DHF                  (1<<3)
       #define M9809X_DAI_FILTER_TYPE   (1<<7)
/* M9809X_045_DSP_CFG */
//ZTE: modify for hook detect
//	#define M9809X_DSPNORMAL                (5<<4)
        #define M9809X_DSPNORMAL                (1<<6)
	#define M9809X_JKSWTYP                     (1<<5)
	#define M9809X_ADCDAI_MASK              (3<<6)
	#define M9809X_ADCDAI_SHIFT             6

/* M9809X_046_DAC_CTRL1 */
	#define M9809X_CHOPCLKNORMAL            (3<<0)

/* M9809X_048_MIX_DAC_LR */
	#define M9809X_DAI1L_TO_DACR            (1<<7)
	#define M9809X_DAI1R_TO_DACR            (1<<6)
	#define M9809X_DAI2M_TO_DACR            (1<<5)
	#define M9809X_DAI3M_TO_DACR            (1<<4)
	#define M9809X_DAI1L_TO_DACL            (1<<3)
	#define M9809X_DAI1R_TO_DACL            (1<<2)
	#define M9809X_DAI2M_TO_DACL            (1<<1)
	#define M9809X_DAI3M_TO_DACL            (1<<0)

/* M9809X_049_MIX_DAC_M */
	#define M9809X_DAI1L_TO_DACM            (1<<3)
	#define M9809X_DAI1R_TO_DACM            (1<<2)
	#define M9809X_DAI2M_TO_DACM            (1<<1)
	#define M9809X_DAI3M_TO_DACM            (1<<0)

/* M9809X_04E_MIX_HP_CFG */
	#define M9809X_HPNORMAL                 (3<<4)

/* M9809X_058_LVL_DAI1_PLAY, M9809X_05A_LVL_DAI2_PLAY, M9809X_05C_LVL_DAI3_PLAY */
	#define M9809X_DAI_MUTE                 (1<<7)
	#define M9809X_DAI_MUTE_MASK            (1<<7)

/* M98095_05D_LVL_ADC_L/M98095_05E_LVL_ADC_R */
	#define M9809X_ADC_ATT        (0x0F)

/* M9809X_05F_LVL_MIC1, M9809X_060_LVL_MIC2 */
	#define M9809X_MICPRE_MASK              (3<<5)
	#define M9809X_MICPRE_SHIFT             5

/* M9809X_064_LVL_HP_L, M9809X_065_LVL_HP_R */
	#define M9809X_HP_MUTE                  (1<<7)

/* M9809X_066_LVL_RCV */
	#define M9809X_REC_MUTE                 (1<<7)

/* M9809X_067_LVL_SPK_L, M9809X_068_LVL_SPK_R */
	#define M9809X_SP_MUTE                  (1<<7)

/* M9809X_087_CFG_MIC */
	#define M9809X_MICSEL_MASK              (3<<0)
	#define M9809X_DIGMIC_L                 (1<<2)
	#define M9809X_DIGMIC_R                 (1<<3)
	#define M9809X_DIGMIC2L                 (1<<4)
	#define M9809X_DIGMIC2R                 (1<<5)

/* M9809X_088_CFG_LEVEL */
	#define M9809X_VS2EN                     (1<<7)	
	#define M9809X_VSEN                     (1<<6)
	#define M9809X_ZDEN                     (1<<5)
	#define M9809X_BQ2EN                    (1<<3)
	#define M9809X_BQ1EN                    (1<<2)
	#define M9809X_EQ2EN                    (1<<1)
	#define M9809X_EQ1EN                    (1<<0)

/* M9809X_090_PWR_EN_IN */
	#define M9809X_INEN                     (1<<7)
	#define M9809X_MB2EN                    (1<<3)
	#define M9809X_MB1EN                    (1<<2)
	#define M9809X_MBEN                     (3<<2)
	#define M9809X_ADREN                    (1<<1)
	#define M9809X_ADLEN                    (1<<0)

/* M9809X_091_PWR_EN_OUT */
	#define M9809X_HPREN                    (1<<7)
	#define M9809X_HPLEN                    (1<<6)
	#define M9809X_HPEN                     ((1<<7)|(1<<6))
	#define M9809X_SPREN                    (1<<5)
	#define M9809X_SPLEN                    (1<<4)
	#define M9809X_RECEN                    (1<<3)
	#define M9809X_DAREN                    (1<<1)
	#define M9809X_DALEN                    (1<<0)
	#define M9809X_DAEN                     (M9809X_DAREN | M9809X_DALEN)

/* M9809X_092_PWR_EN_OUT */
	#define M9809X_SPK_FIXEDSPECTRUM        (0<<4)
	#define M9809X_SPK_SPREADSPECTRUM       (1<<4)

/* M9809X_093_BIAS_CTRL */
	#define M9809X_VMICBIAS1_MASK           (3<<0)
	#define M9809X_VMICBIAS2_MASK           (3<<2)
	#define M9809X_VMICBIAS1_2_2V           (0<<0)
	#define M9809X_VMICBIAS2_2_2V           (0<<2)
	#define M9809X_VMICBIAS1_2_4V           (1<<0)
	#define M9809X_VMICBIAS2_2_4V           (1<<2)
	#define M9809X_VMICBIAS1_2_6V           (2<<0)
	#define M9809X_VMICBIAS2_2_6V           (2<<2)
	#define M9809X_VMICBIAS1_2_8V           (3<<0)
	#define M9809X_VMICBIAS2_2_8V           (3<<2)

/* M9809X_096_DAC_POWER */
     #define M9809X_FITHEN2                   (1<<4)

/* M9809X_097_PWR_SYS */
	#define M9809X_SHDNRUN                  (1<<7)
	#define M9809X_CODECSHDN                (1<<6)
	#define M9809X_PERFMODE                 (1<<3)
	#define M9809X_HPPLYBACK                (1<<2)
	#define M9809X_PWRSV8K                  (1<<1)
	#define M9809X_PWRSV                    (1<<0)

#define M9809X_COEFS_PER_BAND            5

#define M9809X_BYTE1(w) ((w >> 8) & 0xff)
#define M9809X_BYTE0(w) (w & 0xff)

/* Equalizer filter coefficients */
#define M9809X_110_DAI1_EQ_BASE             0x10
#define M9809X_142_DAI2_EQ_BASE             0x42

/* Biquad filter coefficients */
#define M9809X_174_DAI1_BQ_BASE             0x74
#define M9809X_17E_DAI2_BQ_BASE             0x7E

/* start detecting jacksns */
#define M9809X_JDEN                                (1<<7)
#define M9809X_KEYEN				(1<<3)
#define M9809X_PIN5EN                            (1<<2) 

/* Silicon revision number */
#define M9809X_REVAB                        0x40
#define M9809X_REVB                         0x40
#define M9809X_REVC                         0x42

//ZTE: fixed VOLUME
#define M9809X_ADC_ATT_VOLUME 0x03
/***************************************************************
 * Silicon revision build selection:
 * For version A enter: M9809X_REVA
 * For version B enter: M9809X_REVB
 ***************************************************************/
#define M9809X_SILICON_VERSION	M9809X_REVC

/***************************************************************
 * As of April 2011, kernel-next (which is closest to 2.6.39)
 * has a version code value of 2.6.38 in version.h
 * One can override it here with an explicit version number
 ***************************************************************/
#include <linux/version.h>

extern int max9809X_mic_detect(struct snd_soc_codec *codec, struct snd_soc_jack *jack,
		      int det, int shrt);

enum max9809X_type {
	MAX9809X,
	MAX98095,
	MAX98096
};

struct max9809X_cdata {
	unsigned int rate;
	unsigned int fmt;
	int eq_sel;
	int bq_sel;
};

struct max9809X_priv {
	enum snd_soc_control_type control_type;
	enum max9809X_type devtype;
	void *control_data;
	struct max9809X_pdata *pdata;
	unsigned int sysclk;
	struct max9809X_cdata dai[3];
	const char **eq_texts;
	const char **bq_texts;
	const char **fs_texts;
	struct soc_enum eq_enum;
	struct soc_enum bq_enum;
	struct soc_enum fs_enum;
	int eq_textcnt;
	int bq_textcnt;
	int fs_textcnt;
	u8 lin_state;
	unsigned int mic1pre;
	unsigned int mic2pre;
	unsigned int fs_sel;
	int cur_dsp;
	int irq;
	struct input_dev *idev;
	void * dsp_fw;
};

extern int zte_get_board_id(void);
#endif

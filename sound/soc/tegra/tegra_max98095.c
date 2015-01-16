/*
 * tegra_max98095.c - Tegra machine ASoC driver for boards using MAX98095 codec.
 *
 * Author: Ravindra Lokhande <rlokhande@nvidia.com>
 * Copyright (C) 2012 - NVIDIA, Inc.
 *
 * Based on version from Sumit Bhattacharya <sumitb@nvidia.com>
 *
 * Based on code copyright/by:
 *
 * Copyright (c) 2010-12, NVIDIA CORPORATION. All rights reserved.
 *
 * Copyright 2007 Wolfson Microelectronics PLC.
 * Author: Graeme Gregory
 *         graeme.gregory@wolfsonmicro.com or linux@wolfsonmicro.com
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <asm/mach-types.h>

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#ifdef CONFIG_SWITCH
#include <linux/switch.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#endif

#include <mach/tegra_asoc_pdata.h>

#include <sound/core.h>
#include <sound/jack.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "../codecs/max98095/max98095.h"

#include "tegra_pcm.h"
#include "tegra_asoc_utils.h"
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
#include "tegra30_ahub.h"
#include "tegra30_i2s.h"
#include "tegra30_dam.h"
#endif

#define DRV_NAME "tegra-snd-max98095"

#define GPIO_SPKR_EN    BIT(0)
#define GPIO_HP_MUTE    BIT(1)
#define GPIO_INT_MIC_EN BIT(2)
#define GPIO_EXT_MIC_EN BIT(3)
#define GPIO_HP_DET     BIT(4)
#define GPIO_HP_MIC_DET     BIT(5)

#define DEBUG_MAX98095

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
const char *tegra_max98095_i2s_dai_name[TEGRA30_NR_I2S_IFC] = {
	"tegra30-i2s.0",
	"tegra30-i2s.1",
	"tegra30-i2s.2",
	"tegra30-i2s.3",
	"tegra30-i2s.4",
};
#endif

struct tegra_max98095 {
	struct tegra_asoc_utils_data util_data;
	struct tegra_asoc_platform_data *pdata;
	int gpio_requested;
	bool init_done;
	int is_call_mode;
	int is_device_bt;
	int fm_mode;
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	struct codec_config codec_info[NUM_I2S_DEVICES];
#endif
	enum snd_soc_bias_level bias_level;
	struct snd_soc_card *pcard;
};
static struct wake_lock hp_wake_lock;
static bool is_in_call_state = false;
bool in_call_state(void)
{
    return is_in_call_state;
}
EXPORT_SYMBOL(in_call_state);

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
static int tegra_max98095_set_dam_cif(int dam_ifc, int srate,
			int channels, int bit_size)
{
	tegra30_dam_set_samplerate(dam_ifc, TEGRA30_DAM_CHOUT,
				srate);
	tegra30_dam_set_samplerate(dam_ifc, TEGRA30_DAM_CHIN1,
				srate);
	tegra30_dam_set_acif(dam_ifc, TEGRA30_DAM_CHIN1,
		channels, bit_size, channels,
				bit_size);
	tegra30_dam_set_acif(dam_ifc, TEGRA30_DAM_CHOUT,
		channels, bit_size, channels,
				bit_size);

	return 0;
}
#endif

static int tegra_max98095_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_card *card = codec->card;
	struct tegra_max98095 *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	struct tegra30_i2s *i2s = snd_soc_dai_get_drvdata(cpu_dai);
#endif
	unsigned int srate, mclk, sample_size, i2s_daifmt;
	int err, rate;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		sample_size = 16;
		break;
	default:
		return -EINVAL;
	}

	srate = params_rate(params);
	switch (srate) {
	case 8000:
	case 16000:
	case 24000:
	case 32000:
	case 48000:
	case 64000:
	case 96000:
		mclk = 12288000;
		break;
	case 11025:
	case 22050:
	case 44100:
	case 88200:
		mclk = 11289600;
		break;
	default:
		mclk = 12000000;
		break;
	}

	i2s_daifmt = SND_SOC_DAIFMT_NB_NF;
	i2s_daifmt |= pdata->i2s_param[HIFI_CODEC].is_i2s_master ?
			SND_SOC_DAIFMT_CBS_CFS : SND_SOC_DAIFMT_CBM_CFM;

	switch (pdata->i2s_param[HIFI_CODEC].i2s_mode) {
		case TEGRA_DAIFMT_I2S :
			i2s_daifmt |= SND_SOC_DAIFMT_I2S;
			break;
		case TEGRA_DAIFMT_DSP_A :
			i2s_daifmt |= SND_SOC_DAIFMT_DSP_A;
			break;
		case TEGRA_DAIFMT_DSP_B :
			i2s_daifmt |= SND_SOC_DAIFMT_DSP_B;
			break;
		case TEGRA_DAIFMT_LEFT_J :
			i2s_daifmt |= SND_SOC_DAIFMT_LEFT_J;
			break;
		case TEGRA_DAIFMT_RIGHT_J :
			i2s_daifmt |= SND_SOC_DAIFMT_RIGHT_J;
			break;
		default :
			dev_err(card->dev, "Can't configure i2s format\n");
			return -EINVAL;
	}
#ifdef DEBUG_MAX98095
    printk("tegra_max98095_hw_params   sample_size = %d mclk = %d  i2s_daifmt = %d\n",sample_size,mclk,i2s_daifmt);
#endif
	err = tegra_asoc_utils_set_rate(&machine->util_data, srate, mclk);
	if (err < 0) {
		if (!(machine->util_data.set_mclk % mclk))
			mclk = machine->util_data.set_mclk;
		else {
			dev_err(card->dev, "Can't configure clocks\n");
			return err;
		}
	}

	tegra_asoc_utils_lock_clk_rate(&machine->util_data, 1);

	rate = clk_get_rate(machine->util_data.clk_cdev1);
#ifdef DEBUG_MAX98095
    printk("tegra_max98095_hw_params in  rate = %d \n",rate);
#endif
	err = snd_soc_dai_set_fmt(codec_dai, i2s_daifmt);
	if (err < 0) {
		dev_err(card->dev, "codec_dai fmt not set\n");
		return err;
	}

	err = snd_soc_dai_set_fmt(cpu_dai, i2s_daifmt);
	if (err < 0) {
		dev_err(card->dev, "cpu_dai fmt not set\n");
		return err;
	}

	err = snd_soc_dai_set_sysclk(codec_dai, 0, rate, SND_SOC_CLOCK_IN);
	if (err < 0) {
		dev_err(card->dev, "codec_dai clock not set\n");
		return err;
	}

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		tegra_max98095_set_dam_cif(i2s->dam_ifc, srate,
				params_channels(params), sample_size);
#endif

	return 0;
}


static int tegra_voice_call_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params)
{
    struct snd_soc_pcm_runtime *rtd = substream->private_data;
    struct snd_soc_dai *codec_dai = rtd->codec_dai;
    struct snd_soc_codec *codec = rtd->codec;
    struct snd_soc_card *card = codec->card;
    struct tegra_max98095 *machine = snd_soc_card_get_drvdata(card);
    int srate, mclk;
    int err;
    
    srate = params_rate(params);
    switch (srate) {
        case 8000:
        case 16000:
        case 24000:
        case 32000:
        case 48000:
        case 64000:
        case 96000:
            mclk = 12288000;
            break;
        case 11025:
        case 22050:
        case 44100:
        case 88200:
            mclk = 11289600;
            break; 
        default:
            mclk = 12000000;
            break;
    }

    err = tegra_asoc_utils_set_rate(&machine->util_data, srate, mclk);
    if (err < 0) {
        if (!(machine->util_data.set_mclk % mclk))
            mclk = machine->util_data.set_mclk;
        else {
            dev_err(card->dev, "Can't configure clocks\n");
            return err;
        }
    }
    
#ifdef DEBUG_MAX98095
    printk("tegra_voice_call_hw_params in  mclk = %d \n",mclk);
#endif
    tegra_asoc_utils_lock_clk_rate(&machine->util_data, 1);

    err = snd_soc_dai_set_fmt(codec_dai,
                SND_SOC_DAIFMT_I2S |
                SND_SOC_DAIFMT_NB_NF |
                SND_SOC_DAIFMT_CBS_CFS);
    if (err < 0) {
        dev_err(card->dev, "codec_dai fmt not set\n");
        return err;
    }

    err = snd_soc_dai_set_sysclk(codec_dai, 0, mclk,
                SND_SOC_CLOCK_IN);
    if (err < 0) {
        dev_err(card->dev, "codec_dai clock not set\n");
        return err;
    }

    return 0;
}

static void tegra_voice_call_shutdown(struct snd_pcm_substream *substream)
{
}

static int tegra_bt_voice_call_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params)
{
    struct snd_soc_pcm_runtime *rtd = substream->private_data;
    struct snd_soc_codec *codec = rtd->codec;
    struct snd_soc_card *card = codec->card;
    struct snd_soc_dai *codec_dai = rtd->codec_dai;
    struct tegra_max98095 *machine = snd_soc_card_get_drvdata(card);

    int err, srate, mclk, min_mclk;

    srate = params_rate(params);
    switch (srate) {
        case 8000:
        case 16000:
        case 24000:
        case 32000:
        case 48000:
        case 64000:
        case 96000:
            mclk = 12288000;
        break;
        case 11025:
        case 22050:
        case 44100:
        case 88200:
            mclk = 11289600;
            break;
        default:
            mclk = 12000000;
            break;
    }
    min_mclk = 64 * srate;
    
#ifdef DEBUG_MAX98095
    printk("tegra_bt_voice_call_hw_params in  min_mclk = %d \n",min_mclk);
#endif
    err = tegra_asoc_utils_set_rate(&machine->util_data, srate, mclk);
    if (err < 0) {
    printk("[bt_voice]	tegra_bt_voice_call_hw_params	err = %d",err);
    if (!(machine->util_data.set_mclk % min_mclk))
    mclk = machine->util_data.set_mclk;
    else {
    dev_err(card->dev, "Can't configure clocks\n");
    return err;
    }
    }

    tegra_asoc_utils_lock_clk_rate(&machine->util_data, 1);

    err = snd_soc_dai_set_fmt(codec_dai,
                SND_SOC_DAIFMT_I2S |
                SND_SOC_DAIFMT_NB_NF |
                SND_SOC_DAIFMT_CBS_CFS);
    if (err < 0) {
        dev_err(card->dev, "codec_dai fmt not set\n");
        return err;
    }

    return 0;
}

static void tegra_bt_voice_call_shutdown(struct snd_pcm_substream *substream)
{
}


static int tegra_spdif_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct tegra_max98095 *machine = snd_soc_card_get_drvdata(card);
	unsigned int srate, mclk, min_mclk;
	int err;

	srate = params_rate(params);
	switch (srate) {
	case 11025:
	case 22050:
	case 44100:
	case 88200:
		mclk = 11289600;
		break;
	case 8000:
	case 16000:
	case 32000:
	case 48000:
	case 64000:
	case 96000:
		mclk = 12288000;
		break;
	default:
		return -EINVAL;
	}
	min_mclk = 128 * srate;

	err = tegra_asoc_utils_set_rate(&machine->util_data, srate, mclk);
	if (err < 0) {
		if (!(machine->util_data.set_mclk % min_mclk))
			mclk = machine->util_data.set_mclk;
		else {
			dev_err(card->dev, "Can't configure clocks\n");
			return err;
		}
	}

	tegra_asoc_utils_lock_clk_rate(&machine->util_data, 1);

	return 0;
}

static int tegra_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tegra_max98095 *machine = snd_soc_card_get_drvdata(rtd->card);

	tegra_asoc_utils_lock_clk_rate(&machine->util_data, 0);

	return 0;
}

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
static int tegra_max98095_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct tegra30_i2s *i2s = snd_soc_dai_get_drvdata(cpu_dai);

	if ((substream->stream != SNDRV_PCM_STREAM_PLAYBACK) ||
		!(i2s->is_dam_used))
		return 0;

	/*dam configuration*/
	if (!i2s->dam_ch_refcount)
		i2s->dam_ifc = tegra30_dam_allocate_controller();

	tegra30_dam_allocate_channel(i2s->dam_ifc, TEGRA30_DAM_CHIN1);
	i2s->dam_ch_refcount++;
	tegra30_dam_enable_clock(i2s->dam_ifc);
	tegra30_dam_set_gain(i2s->dam_ifc, TEGRA30_DAM_CHIN1, 0x1000);

	tegra30_ahub_set_rx_cif_source(TEGRA30_AHUB_RXCIF_DAM0_RX1 +
			(i2s->dam_ifc*2), i2s->txcif);

	/*
	*make the dam tx to i2s rx connection if this is the only client
	*using i2s for playback
	*/
	if (i2s->playback_ref_count == 1)
		tegra30_ahub_set_rx_cif_source(
			TEGRA30_AHUB_RXCIF_I2S0_RX0 + i2s->id,
			TEGRA30_AHUB_TXCIF_DAM0_TX0 + i2s->dam_ifc);

	/* enable the dam*/
	tegra30_dam_enable(i2s->dam_ifc, TEGRA30_DAM_ENABLE,
			TEGRA30_DAM_CHIN1);

	return 0;
}

static void tegra_max98095_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct tegra30_i2s *i2s = snd_soc_dai_get_drvdata(cpu_dai);

	if ((substream->stream != SNDRV_PCM_STREAM_PLAYBACK) ||
		!(i2s->is_dam_used))
		return;

	/* disable the dam*/
	tegra30_dam_enable(i2s->dam_ifc, TEGRA30_DAM_DISABLE,
			TEGRA30_DAM_CHIN1);

	/* disconnect the ahub connections*/
	tegra30_ahub_unset_rx_cif_source(TEGRA30_AHUB_RXCIF_DAM0_RX1 +
				(i2s->dam_ifc*2));

	/* disable the dam and free the controller */
	tegra30_dam_disable_clock(i2s->dam_ifc);
	tegra30_dam_free_channel(i2s->dam_ifc, TEGRA30_DAM_CHIN1);
	i2s->dam_ch_refcount--;
	if (!i2s->dam_ch_refcount)
		tegra30_dam_free_controller(i2s->dam_ifc);

	return;
}
#endif

static struct snd_soc_ops tegra_max98095_ops = {
	.hw_params = tegra_max98095_hw_params,
	.hw_free = tegra_hw_free,
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	.startup = tegra_max98095_startup,
	.shutdown = tegra_max98095_shutdown,
#endif
};

static struct snd_soc_ops tegra_voice_call_ops = {
    .hw_params = tegra_voice_call_hw_params,
    .shutdown = tegra_voice_call_shutdown,
    .hw_free = tegra_hw_free,
};

static struct snd_soc_ops tegra_bt_voice_call_ops = {
    .hw_params = tegra_bt_voice_call_hw_params,
    .shutdown = tegra_bt_voice_call_shutdown,
    .hw_free = tegra_hw_free,
};

static struct snd_soc_ops tegra_spdif_ops = {
	.hw_params = tegra_spdif_hw_params,
	.hw_free = tegra_hw_free,
};

#ifdef CONFIG_SWITCH
//static struct snd_soc_jack tegra_max98095_hp_jack;

/* These values are copied from Android WiredAccessoryObserver */

#define     HP_PLUG_IN  0
#define     HP_PLUG_OUT  1    
#define     BIT_NO_HEADSET  0
#define     BIT_HEADSET  (1 << 0)
#define     BIT_HEADSET_NO_MIC  (1 << 1)

#define     STABLE_TIME 3
#define     max98095_DELAY 800

long max98095_hp_time=0;
static int gpio_val_old = HP_PLUG_OUT;
int state_hp = 0;

static struct switch_dev tegra_max98095_headset_switch = {
	.name = "h2w",
};

static irqreturn_t tegra_max98095_jack_notifier(int irq, void *data)
{
    struct snd_soc_codec *codec = data;
    struct snd_soc_card *card = codec->card;
    struct tegra_max98095 *machine = snd_soc_card_get_drvdata(card);
    struct tegra_asoc_platform_data *pdata = machine->pdata;
    int gpio_val = HP_PLUG_OUT;
    int codec_state =0;
    int i ;

    gpio_val = gpio_get_value(pdata->gpio_hp_det);
    wake_lock_timeout(&hp_wake_lock, 2 * HZ);
   
    if(gpio_val == gpio_val_old){
#ifdef DEBUG_MAX98095
        printk("[hp] Headphoe states not change\n");
#endif
        return NOTIFY_OK;
    }else{
        if(gpio_val == HP_PLUG_OUT){
            state_hp = BIT_NO_HEADSET; 
            gpio_val_old = HP_PLUG_OUT;
			
            max98095_hp_time = jiffies /HZ;	

            snd_soc_update_bits(codec, M98095_013_JACK_INT_EN, M98095_IMCSW|M98095_IKEYDET, 0); 
            snd_soc_update_bits(codec, M98095_089_JACK_DET_AUTO, M98095_JDEN, 0);  
            snd_soc_update_bits(codec, M98095_090_PWR_EN_IN,M98095_MB2EN, 0);
#ifdef DEBUG_MAX98095
            printk("[hp] No headphone here\n");
#endif
            printk("[hp] No headphone here\n");
        }else{
            snd_soc_update_bits(codec, M98095_090_PWR_EN_IN,M98095_MB2EN, M98095_MB2EN);
            max98095_hp_time = jiffies /HZ;	

            msleep(max98095_DELAY);
            /*Stable the state of 0x07 Register*/
            for(i=0;i<STABLE_TIME;i++){
                snd_soc_update_bits(codec, M98095_089_JACK_DET_AUTO, M98095_JDEN, 0);  	
                snd_soc_update_bits(codec, M98095_089_JACK_DET_AUTO, M98095_JDEN, M98095_JDEN);   
                msleep(10);
                codec_state = snd_soc_read(codec, M98095_007_JACK_AUTO_STS);   
	}
             snd_soc_update_bits(codec, M98095_089_JACK_DET_AUTO, M98095_JDEN, 0);  	

            if(codec_state == 0xC0){
                state_hp = BIT_HEADSET_NO_MIC; 
                snd_soc_write(codec, M98095_013_JACK_INT_EN, 0); 
#ifdef DEBUG_MAX98095
                printk("[hp] Headphone without Mic\n");
#endif
            }else if(codec_state == 0xC8){
                state_hp = BIT_HEADSET; 
                snd_soc_update_bits(codec, M98095_013_JACK_INT_EN, M98095_IMCSW|M98095_IKEYDET, M98095_IMCSW|M98095_IKEYDET); 
#ifdef DEBUG_MAX98095
                printk("[hp] Headphone with Mic\n");
#endif
            }else{
                state_hp = BIT_HEADSET_NO_MIC;
                snd_soc_write(codec, M98095_013_JACK_INT_EN, 0); 
#ifdef DEBUG_MAX98095     
                printk("[hp] Headphone state Error \n");
#endif
            }
            gpio_val_old = HP_PLUG_IN;
        }        
            max98095_hp_time = jiffies /HZ;	
    }
    switch_set_state(&tegra_max98095_headset_switch, state_hp);
	return NOTIFY_OK;
}

#endif

static const struct snd_soc_dapm_widget tegra_max98095_dapm_widgets[] = {
    SND_SOC_DAPM_HP("Headphone Jack", NULL),
    SND_SOC_DAPM_SPK("Earpiece", NULL),
    SND_SOC_DAPM_SPK("Int Spk", NULL),
    SND_SOC_DAPM_MIC("Mainmic", NULL),
    SND_SOC_DAPM_MIC("Slavemic", NULL),
    SND_SOC_DAPM_MIC("LineIn", NULL),
    SND_SOC_DAPM_OUTPUT("Virtual BT"),
};

static const struct snd_soc_dapm_route max98095_audio_map[] = {
    /* Headphone connected to HPL and HPR */
    {"Headphone Jack", NULL, "HPL"},
    {"Headphone Jack", NULL, "HPR"},
    /*Speaker*/
    {"Int Spk", NULL, "SPKL"},
    /*Receiver*/
    {"Earpiece", NULL, "RCV"},
    /*main mic*/
    {"MIC1", NULL, "Mainmic"},
    /*Slave mic*/
    {"MIC2", NULL, "Slavemic"},
    /* FM in */
    {"INA1", NULL, "LineIn"},
    {"INA2", NULL, "LineIn"},
    {"Virtual BT", NULL, "BT Voice"},
};

static const struct snd_kcontrol_new tegra_max98095_controls[] = {
    SOC_DAPM_PIN_SWITCH("Headphone Jack"),
    SOC_DAPM_PIN_SWITCH("Earpiece"),
    SOC_DAPM_PIN_SWITCH("Int Spk"),
    SOC_DAPM_PIN_SWITCH("Mainmic"),
    SOC_DAPM_PIN_SWITCH("Slavemic"),
    SOC_DAPM_PIN_SWITCH("LineIn"),
    SOC_DAPM_PIN_SWITCH("Virtual BT"),
};

static int tegra_max98095_call_mode_info(struct snd_kcontrol *kcontrol,
            struct snd_ctl_elem_info *uinfo)
	{
    uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
    uinfo->count = 1;
    uinfo->value.integer.min = 0;
    uinfo->value.integer.max = 1;
    return 0;
}

static int tegra_max98095_call_mode_get(struct snd_kcontrol *kcontrol,
        struct snd_ctl_elem_value *ucontrol)
{
    struct tegra_max98095 *machine = snd_kcontrol_chip(kcontrol);

    ucontrol->value.integer.value[0] = machine->is_call_mode;

    return 0;
}

static int tegra_max98095_call_mode_put(struct snd_kcontrol *kcontrol,
        struct snd_ctl_elem_value *ucontrol)
{
    struct tegra_max98095 *machine = snd_kcontrol_chip(kcontrol);
    int is_call_mode_new = ucontrol->value.integer.value[0];
    unsigned int i;

    if (machine->is_call_mode == is_call_mode_new)
		return 0;

    if (is_call_mode_new) {
        for(i=0; i<machine->pcard->num_links; i++)
            machine->pcard->dai_link[i].ignore_suspend = 1;
        is_in_call_state = true;
    } else {
        for(i=0; i<machine->pcard->num_links; i++)
            machine->pcard->dai_link[i].ignore_suspend = 0;
        is_in_call_state = false;
    }
    machine->is_call_mode = is_call_mode_new;
    return 1;
}

struct snd_kcontrol_new tegra_max98095_call_mode_control = {
    .access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
    .iface = SNDRV_CTL_ELEM_IFACE_MIXER,
    .name = "Call Mode Switch",
    .private_value = 0xffff,
    .info = tegra_max98095_call_mode_info,
    .get = tegra_max98095_call_mode_get,
    .put = tegra_max98095_call_mode_put
};
static int tegra_max98095_fm_mode_info(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_info *uinfo)
{
    uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
    uinfo->count = 1;
    uinfo->value.integer.min = 0;
    uinfo->value.integer.max = 1;
	return 0;
}

static int tegra_max98095_fm_mode_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
    struct tegra_max98095 *machine = snd_kcontrol_chip(kcontrol);

    ucontrol->value.integer.value[0] = machine->fm_mode;

    return 0;
}

static int tegra_max98095_fm_mode_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
    struct tegra_max98095 *machine = snd_kcontrol_chip(kcontrol);
    int fm_mode = ucontrol->value.integer.value[0];
    unsigned int i;

    if (fm_mode != machine->fm_mode) {
        if (fm_mode) {
            machine->fm_mode = 1;
            for (i = 0; i < machine->pcard->num_links; i++)
                machine->pcard->dai_link[i].ignore_suspend = 1;
        } else {
            machine->fm_mode = 0;
            for (i = 0; i < machine->pcard->num_links; i++)
                machine->pcard->dai_link[i].ignore_suspend = 0;
        }
    }
    return 0;
}

struct snd_kcontrol_new tegra_max98095_fm_mode_control = {
    .access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
    .iface = SNDRV_CTL_ELEM_IFACE_MIXER,
    .name = "FM Mode Switch",
    .private_value = 0xffff,
    .info = tegra_max98095_fm_mode_info,
    .get = tegra_max98095_fm_mode_get,
    .put = tegra_max98095_fm_mode_put
};


static int tegra_max98095_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_card *card = codec->card;
	struct tegra_max98095 *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	struct tegra30_i2s *i2s = snd_soc_dai_get_drvdata(rtd->cpu_dai);
#endif
	int ret;

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	if (machine->codec_info[BASEBAND].i2s_id != -1)
		i2s->is_dam_used = true;
#endif

	if (machine->init_done)
		return 0;

	machine->init_done = true;
	machine->pcard = card;
	if (gpio_is_valid(pdata->gpio_spkr_en)) {
		ret = gpio_request(pdata->gpio_spkr_en, "spkr_en");
		if (ret) {
			dev_err(card->dev, "cannot get spkr_en gpio\n");
			return ret;
		}
		machine->gpio_requested |= GPIO_SPKR_EN;

		gpio_direction_output(pdata->gpio_spkr_en, 0);
	}

	if (gpio_is_valid(pdata->gpio_hp_mute)) {
		ret = gpio_request(pdata->gpio_hp_mute, "hp_mute");
		if (ret) {
			dev_err(card->dev, "cannot get hp_mute gpio\n");
			return ret;
		}
		machine->gpio_requested |= GPIO_HP_MUTE;

		gpio_direction_output(pdata->gpio_hp_mute, 0);
	}

	if (gpio_is_valid(pdata->gpio_int_mic_en)) {
		ret = gpio_request(pdata->gpio_int_mic_en, "int_mic_en");
		if (ret) {
			dev_err(card->dev, "cannot get int_mic_en gpio\n");
			return ret;
		}
		machine->gpio_requested |= GPIO_INT_MIC_EN;

		/* Disable int mic; enable signal is active-high */
		gpio_direction_output(pdata->gpio_int_mic_en, 0);
	}

	if (gpio_is_valid(pdata->gpio_ext_mic_en)) {
		ret = gpio_request(pdata->gpio_ext_mic_en, "ext_mic_en");
		if (ret) {
			dev_err(card->dev, "cannot get ext_mic_en gpio\n");
			return ret;
		}
		machine->gpio_requested |= GPIO_EXT_MIC_EN;

		/* Enable ext mic; enable signal is active-low */
		gpio_direction_output(pdata->gpio_ext_mic_en, 0);
	}

	ret = snd_soc_add_card_controls(card, tegra_max98095_controls,
				   ARRAY_SIZE(tegra_max98095_controls));
	if (ret < 0)
		return ret;

	snd_soc_dapm_new_controls(dapm, tegra_max98095_dapm_widgets,
			ARRAY_SIZE(tegra_max98095_dapm_widgets));

	snd_soc_dapm_add_routes(dapm, max98095_audio_map,
			ARRAY_SIZE(max98095_audio_map));

	/* Add call mode switch control */
	ret = snd_ctl_add(codec->card->snd_card,
                snd_ctl_new1(&tegra_max98095_call_mode_control,
                machine));
	if (ret < 0)
        	return ret;
    	/* Add FM mode switch control */
    	ret = snd_ctl_add(codec->card->snd_card,
                snd_ctl_new1(&tegra_max98095_fm_mode_control,
                machine));
	if (ret < 0)
		return ret;

	ret = tegra_asoc_utils_register_ctls(&machine->util_data);
	if (ret < 0)
		return ret;

#ifdef CONFIG_SWITCH
    if (gpio_is_valid(pdata->gpio_hp_det)) { 
#ifdef DEBUG_MAX98095   
        printk("gpio_hp_det_is_valid  pdata->gpio_hp_det = %d\n",pdata->gpio_hp_det);
#endif

        gpio_request((pdata->gpio_hp_det), "headphone detect");	
        gpio_direction_input(pdata->gpio_hp_det);
        ret= request_threaded_irq(gpio_to_irq(pdata->gpio_hp_det), NULL, tegra_max98095_jack_notifier,
                        IRQF_TRIGGER_RISING |IRQF_TRIGGER_FALLING, "Headphone Detect", codec);
        if (ret < 0){
            printk("max98095_headphone detect_init fail!ret = %d\n",ret);
        }
        machine->gpio_requested |= GPIO_HP_DET;
        ret = enable_irq_wake(gpio_to_irq(pdata->gpio_hp_det));
        if (ret) {
           pr_err("Could NOT set up hp_det gpio pins for wakeup the AP.\n");
        }

    }
#endif     
	snd_soc_dapm_sync(dapm);

	return 0;
}

static struct snd_soc_dai_link tegra_max98095_spi_dai[] = {
	{
		.name = "MAX98095",
		.stream_name = "MAX98095 HIFI",
        .codec_name = "spi1.1",
		.platform_name = "tegra-pcm-audio",
        .cpu_dai_name = "tegra30-i2s.0",
        .codec_dai_name = "max98095-hifi",
		.init = tegra_max98095_init,
		.ops = &tegra_max98095_ops,
	},
	{
        .name = "VOICE CALL",
        .stream_name = "VOICE CALL PCM",
        .codec_name = "spi1.1",
        .platform_name = "tegra-pcm-audio",
        .cpu_dai_name = "dit-hifi",
        .codec_dai_name = "max98095 Voice",
        .ops = &tegra_voice_call_ops,
    },
    {
        .name = "BT VOICE CALL",
        .stream_name = "BT VOICE CALL PCM",
        .codec_name = "spi1.1",
        .platform_name = "tegra-pcm-audio",
        .cpu_dai_name = "dit-hifi",
        .codec_dai_name = "max98095 BT",
        .ops = &tegra_bt_voice_call_ops,
    },
    {
		.name = "SPDIF",
		.stream_name = "SPDIF PCM",
		.codec_name = "spdif-dit.0",
		.platform_name = "tegra-pcm-audio",
		.cpu_dai_name = "tegra30-spdif",
		.codec_dai_name = "dit-hifi",
		.ops = &tegra_spdif_ops,
	},
};

static int tegra30_soc_set_bias_level(struct snd_soc_card *card,
					struct snd_soc_dapm_context *dapm,
					enum snd_soc_bias_level level)
{
	struct tegra_max98095 *machine = snd_soc_card_get_drvdata(card);

	if (machine->bias_level == SND_SOC_BIAS_OFF &&
		level != SND_SOC_BIAS_OFF)
		tegra_asoc_utils_clk_enable(&machine->util_data);

	machine->bias_level = level;

	return 0;
}

static int tegra30_soc_set_bias_level_post(struct snd_soc_card *card,
					struct snd_soc_dapm_context *dapm,
					enum snd_soc_bias_level level)
{
	struct tegra_max98095 *machine = snd_soc_card_get_drvdata(card);

	if (level == SND_SOC_BIAS_OFF)
		tegra_asoc_utils_clk_disable(&machine->util_data);

	return 0 ;
}

static struct snd_soc_card snd_soc_tegra_spi_max98095 = {
	.name = "tegra-max98095",
	.owner = THIS_MODULE,
	.dai_link = tegra_max98095_spi_dai,
	.num_links = ARRAY_SIZE(tegra_max98095_spi_dai),
	.set_bias_level = tegra30_soc_set_bias_level,
	.set_bias_level_post = tegra30_soc_set_bias_level_post,
};

static __devinit int tegra_max98095_driver_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_tegra_spi_max98095;
	struct tegra_max98095 *machine;
	struct tegra_asoc_platform_data *pdata;
	int ret;
#ifdef DEBUG_MAX98095
	printk("[codec]  tegra_max98095_driver_probe in\n");
#endif

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev, "No platform data supplied\n");
		return -EINVAL;
	}

	machine = kzalloc(sizeof(struct tegra_max98095), GFP_KERNEL);
	if (!machine) {
		dev_err(&pdev->dev, "Can't allocate tegra_max98095 struct\n");
		return -ENOMEM;
	}

	machine->pdata = pdata;

	ret = tegra_asoc_utils_init(&machine->util_data, &pdev->dev, card);
	if (ret)
		goto err_free_machine;

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, machine);

#ifdef CONFIG_SWITCH
	/* Add h2w switch class support */
	ret = tegra_asoc_switch_register(&tegra_max98095_headset_switch);
	if (ret < 0) {
		dev_err(&pdev->dev, "not able to register switch device\n");
		goto err_fini_utils;
	}
#endif

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err_switch_unregister;
	}

	if (!card->instantiated) {
		ret = -ENODEV;
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
				ret);
		goto err_unregister_card;
	}

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	ret = tegra_asoc_utils_set_parent(&machine->util_data,
				pdata->i2s_param[HIFI_CODEC].is_i2s_master);
	if (ret) {
		dev_err(&pdev->dev, "tegra_asoc_utils_set_parent failed (%d)\n",
			ret);
		goto err_switch_unregister;
	}
#endif

	return 0;

err_unregister_card:
	snd_soc_unregister_card(card);
err_switch_unregister:
#ifdef CONFIG_SWITCH
	tegra_asoc_switch_unregister(&tegra_max98095_headset_switch);
#endif
err_fini_utils:
	tegra_asoc_utils_fini(&machine->util_data);
err_free_machine:
	kfree(machine);
	return ret;
}

static int __devexit tegra_max98095_driver_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct tegra_max98095 *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;

	snd_soc_unregister_card(card);

#ifdef CONFIG_SWITCH
	tegra_asoc_switch_unregister(&tegra_max98095_headset_switch);
#endif

	tegra_asoc_utils_fini(&machine->util_data);

    	if (machine->gpio_requested & GPIO_HP_DET)
        	gpio_free(pdata->gpio_hp_det);  
	if (machine->gpio_requested & GPIO_EXT_MIC_EN)
		gpio_free(pdata->gpio_ext_mic_en);
	if (machine->gpio_requested & GPIO_INT_MIC_EN)
		gpio_free(pdata->gpio_int_mic_en);
	if (machine->gpio_requested & GPIO_HP_MUTE)
		gpio_free(pdata->gpio_hp_mute);
	if (machine->gpio_requested & GPIO_SPKR_EN)
		gpio_free(pdata->gpio_spkr_en);

	kfree(machine);

	return 0;
}

static struct platform_driver tegra_max98095_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
	},
	.probe = tegra_max98095_driver_probe,
	.remove = __devexit_p(tegra_max98095_driver_remove),
};

static int __init tegra_max98095_modinit(void)
{
    wake_lock_init(&hp_wake_lock, WAKE_LOCK_SUSPEND, "hp_wake");

    return platform_driver_register(&tegra_max98095_driver);
}
module_init(tegra_max98095_modinit);

static void __exit tegra_max98095_modexit(void)
{
    wake_lock_destroy(&hp_wake_lock);
	platform_driver_unregister(&tegra_max98095_driver);
}
module_exit(tegra_max98095_modexit);

MODULE_AUTHOR("Ravindra Lokhande <rlokhande@nvidia.com>");
MODULE_DESCRIPTION("Tegra+MAX98095 machine ASoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);

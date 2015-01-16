/*
 * max98095/6 codec DSP firmware download and control interface functions
 * Copyright 2011-2012 Maxim Integrated Products
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <linux/firmware.h>
#include "max9809X.h"
////////////////////////////////////////////////////// prototypes
static int dsp_load_fsav(struct snd_soc_codec *codec, struct dsp_fw_entry * p);
//static int free_firmware(struct dsp_fw * dsp_fw);
void * dsp_enumerate_firmware(struct snd_soc_codec *codec);

//#define DSP_SUPPORT_CONFIG_DOWNLOAD
//#define DSP_ENCRYPTION
//#define DSP_DBGPRINT
//#define DSP_DBGDUMP


extern int state_hp;

#ifdef DSP_DBGPRINT
  #define DBG_PRINT(...) pr_info(__VA_ARGS__)
#else
  #define DBG_PRINT(...) ;
#endif

#ifdef DSP_DBGDUMP
  #define DBG_DUMP(...) dsp_hex_dump(__VA_ARGS__)
#else
  #define DBG_DUMP(...) ;
#endif

#ifdef DSP_DBGDUMP
static void dsp_hex_dump(uint8_t *ptr, int bytelen)
{
	int i;
	int first_time = 1;

	for (i = 0; i < bytelen; i++) {
		if (i % 8 == 0 || first_time) {
			pr_info("\n0x%04x:", i);
			first_time = 0;
		}
		pr_info(" 0x%02x", ptr[i] & 0xff);
	}
	pr_info("\n");
}
#endif

static char * predefined[] = {"RELOAD_FSAVS", "OFF"};
#define NUM_PREDEFINED (sizeof(predefined) / sizeof(char *))
#define MODENUM		5
//static char *voice_call_mode[MODENUM ]={"00-RCV2", "01-RCV1", "02-SPK", "03-WHS", "04-WHS_MIC"};  
static char *voice_call_mode[MODENUM ]={"RCV2", "RCV1", "SPK", "WHS", "WHS_MIC"};  

#undef DUMP_FSAV_HEADER
#ifdef DUMP_FSAV_HEADER
static void pr_header (struct header * header);
#endif

/*
 * add code to support DSP FIFO operation with SPI interface //jelphi.zhang
 */
static int dsp_xfer_read(struct snd_soc_codec *codec, int reg, uint8_t *buf,
						int avail_len, int max_to_read)
{
	int len = min(avail_len, max_to_read);
	int i = 0;
	do{
		buf[i] = codec->hw_read(codec, reg);
		i++;
	} while (i < len);

	return len;
}

/* Read data from the DSP based on number of available bytes to read */
#define MSEC_TIMEOUT 60
#define MSEC_PER_PAUSE 1
static int dsp_fifo_read(struct snd_soc_codec *codec, uint8_t *buf, int read_len)
{
	int ret;
	int len;
	int indx = 0;
	int delay = MSEC_TIMEOUT / MSEC_PER_PAUSE;

	while (indx < read_len) {
		/* check number of bytes (0..8) available to read */
		len = snd_soc_read(codec, M9809X_002_HOST_RSP_STS);
		if (len < 0)
			return len;

		DBG_PRINT("Avail %d bytes reading %d bytes ind=%d\n", len, read_len, indx);

		if (len) {
			ret = dsp_xfer_read(codec, M9809X_000_HOST_DATA,
					&buf[indx], len, read_len - indx);
			if (ret < 0)
				return ret;

			DBG_PRINT("reg=0x00 indx=%d len=%d ret=0x%x\n", indx, read_len, ret);
			indx += ret; /* update collected amount */
			/* restart timeout */
			delay = MSEC_TIMEOUT / MSEC_PER_PAUSE;
		}

		else {
                        if (SND_SOC_I2C == codec->control_type){
                            mdelay(MSEC_PER_PAUSE);
                        }
			if (!(delay--)) {
				dev_err(codec->dev, "%s: read_len %d, indx %d\n", __func__, read_len, indx);
				return -7;
			}
		}
	}
	return 0;
}

/*
*add codes to support dsp fifo operation with spi interface //jelphi.zhang
*/
static int dsp_xfer_write(struct snd_soc_codec *codec, int i2c_reg,
			uint8_t *buf, int avail_len, int max_to_write)
{
	void *control = codec->control_data;
	int ret;
	int len = min(avail_len, max_to_write);
	u8 reg=i2c_reg;
	u8 *xfer=(u8 *)NULL;

	if(len<2)
		return 0;

	xfer=kzalloc((len+1),GFP_KERNEL);
	if (xfer == NULL)
		return -ENOMEM;

	/* get the first two bytes ready */
	xfer[0]=reg;
	memcpy(&xfer[1], buf, len);
	ret = codec->hw_write(control, xfer, (len+1));

	if (ret != len + 1) {
		DBG_PRINT("spi:hw_writer() returned %d\n", ret);
		kfree(xfer);
		return -8;
	}

	kfree(xfer);
	return len;
}

/* Write data to the DSP through the FIFO */
static int dsp_fifo_write(struct snd_soc_codec *codec, uint8_t *buf, int write_len)
{
	int ret;
	int avail;
	int index = 0;
	int delay = MSEC_TIMEOUT / MSEC_PER_PAUSE;
	
	do {
		/* check number of bytes free in FIFO available to write (up to 64) */
		avail = snd_soc_read(codec, M9809X_003_HOST_CMD_STS);
		if (avail < 0)
			return avail; /* return read error status */
		if (avail) {
			ret = dsp_xfer_write(codec, M9809X_000_HOST_DATA, &buf[index], avail, write_len - index);
			if (ret < 0)
				return ret;

			index += ret;
			delay = MSEC_TIMEOUT / MSEC_PER_PAUSE; /* restart timeout */
		}

		else {
			mdelay(MSEC_PER_PAUSE);
			if (!(delay--)) {
				dev_err(codec->dev, "%s: avail %d, write_len %d, delay %d\n", __func__, avail, write_len, delay);
				return -7;
			}
		}
	} while (index < write_len);

	return 0;
}

/* Read len bytes from FIFO into buffer. Returns <0 on error */
static int dsp_fifo_block_read(struct snd_soc_codec *codec, uint8_t *buf, int len)
{
	int ret = 0;
	int offs = 0;
	int xferlen;

	while (len) {
		xferlen = min(len, I2C_SMBUS_BLOCK_MAX);
		ret = dsp_fifo_read(codec, &buf[offs], xferlen);
		if (ret < 0) {
			dev_err(codec->dev, "FIFO read error %d\n", ret);
			return ret;
		}

		DBG_DUMP(&buf[offs], xferlen);
		len  -= xferlen;
		offs += xferlen;
	}
	return ret;
}

/* Write len bytes from buf. Returns <0 on error */
int dsp_fifo_block_write(struct snd_soc_codec *codec, uint8_t *buf, int len)
{
	int ret = 0;

	ret = dsp_fifo_write(codec, buf, len);

	return ret;
}

/*
 * read flush the response queue hardware FIFO
 * return <0 if error, else 0
 */

#if 0
static int dsp_fifo_read_flush(struct snd_soc_codec *codec)
{
	int cmd;
	int ret;
	uint8_t buf[8];

	do {
		/* check number of bytes (0..8) available to read */
		cmd = snd_soc_read(codec, M9809X_002_HOST_RSP_STS);
		ret = cmd;

		if (cmd > 0) {
			ret = dsp_fifo_block_read(codec, buf, cmd);
			if (ret < 0) {
				dev_err(codec->dev, "error %d purging input buffer\n", ret);
				break;
			}
		}
	} while (cmd > 0);

	return ret;
}
#endif

int dsp_read_command_status(struct snd_soc_codec *codec)
{
	int ret;
	uint8_t status[4];

	ret = dsp_fifo_block_read(codec, status, 4);
	DBG_PRINT("Command status=0x%x 0x%x 0x%x 0x%x\n", status[0], status[1], status[2], status[3]);
	if (ret >= 0) {
		ret = status[2] + (status[3] << 8);
		ret = (ret == 0xffff) ? -1 : ret;
	}

	return ret;
}

/*
 * Send a dsp command, and read back response.
 * buf is the source and destination buffer for commands.
 */
int dsp_command(struct snd_soc_codec *codec, uint8_t *buf, int txlen, int rxlen)
{
	int ret;

	DBG_PRINT("%s sending %d bytes waiting for %d bytes\n", __func__, txlen, rxlen);
	DBG_DUMP(buf, txlen);

	mutex_lock(&codec->mutex);
	ret = dsp_fifo_block_write(codec, buf, txlen);
	if (ret < 0) {
		dev_err(codec->dev, "%s: error %d writing data\n", __func__, ret);
		goto done;
	}

	ret = dsp_fifo_block_read(codec, buf, rxlen);
//	if (ret < 0)
//		dev_err(codec->dev, "%s: error %d reading data\n", __func__, ret);

done:
	mutex_unlock(&codec->mutex);
	return ret;
}

/*
 * Issue a get ROM ID (version) command, where id is module number, or 0 for romid
 * returns 16 bit id number of <0 if error accessing id
 */
int dsp_get_rom_id(struct snd_soc_codec *codec, int id)
{
	int ret;
	uint8_t buf[] = { MID_GET_ID, id, 0, 0 };

	ret = dsp_command(codec, buf, sizeof(buf), sizeof(buf));
	if (ret < 0)
		ret = -1;
	else
		ret = buf[2] | (buf[3] << 8);

	return ret;
}

/*
 * Issue a set param command, where
 * id is module number
 * len is number of unsigned int's in a unsigned int array of params
 * returns negative status if failed
 */
#define PARAM_STEP_SIZE  15

int dsp_set_param(struct snd_soc_codec *codec, int id, int len, unsigned int *in_params)
{
	uint8_t buf[PARAM_STEP_SIZE*sizeof(unsigned int)+4];

	buf[0] = MID_SET_PARAMETER; /* get param */
	buf[1] = id;                /* module id (middleware) */
	buf[2] = len;               /* length, low */
	buf[3] = 0x0;               /* length, high */

	if (len > PARAM_STEP_SIZE) {
		pr_err("too many params %d\n", len);
		return -1;
	}

	memcpy(&buf[4], in_params, len * 4);
	DBG_PRINT ("param[0] 0x%08X, param[1] 0x%08X, param[2] 0x%08X, param[3] 0x%08X\n",
				((unsigned int *) buf)[0],
				((unsigned int *) buf)[1],
				((unsigned int *) buf)[2],
				((unsigned int *) buf)[3]);

	return dsp_command(codec, buf, 4 + (len * 4), 4);
}

#if defined(DSP_ENCRYPTION) || defined(DSP_SUPPORT_CONFIG_DOWNLOAD)
/*
 * load the optional DSP configuration
 */
static int dsp_mem_download(struct snd_soc_codec *codec, uint8_t *bptr, int len, int chkstatus)
{
	int ret;

	mutex_lock(&codec->mutex);

	ret = dsp_fifo_block_write(codec, bptr, len);
	if (ret >= 0 && chkstatus)
		ret = dsp_read_command_status(codec);

	mutex_unlock(&codec->mutex);

	return ret;
}
#endif

#ifdef DSP_ENCRYPTION

#define PREAMBLE_LENGTH 12
#define POSTAMBLE_LENGTH 9
#define KEY_LENGTH 6
static int dsp_send_preamble(struct snd_soc_codec *codec, int cfg_no, uint8_t *data)
{
	int i;

	for (i = 0; i < KEY_LENGTH; i++) {
		snd_soc_write(codec, M9809X_018_KEYCODE3 + i,
			fw_registers[offset_reg(cfg_no) + M9809X_018_KEYCODE3 + i - fw_reglist_first]);
	}

	for (i = 0; i < PREAMBLE_LENGTH; i++)
		snd_soc_write(codec, M9809X_015_DEC, data[i]);

	return 0;
}

static int dsp_send_postamble(struct snd_soc_codec *codec, uint8_t *data)
{
	int i;

	for (i = 0; i < POSTAMBLE_LENGTH; i++)
		snd_soc_write(codec, M9809X_015_DEC, data[i]);

	return 0;
}

static int dsp_load_encrypted(struct snd_soc_codec *codec, int cfg_no, int filesize, char * firmware)
{
	int ret;

	/* handle encrypted download */
	snd_soc_update_bits(codec, M9809X_00F_HOST_CFG, M9809X_DCRYPTEN, M9809X_DCRYPTEN);

	ret = dsp_send_preamble(codec, cfg_no, firmware);
	if (ret < 0)
		return ret;

	DBG_DUMP(&fw_binary[myfile + PREAMBLE_LENGTH], filesize - POSTAMBLE_LENGTH - PREAMBLE_LENGTH);

	ret = dsp_mem_download(codec, firmware+PREAMBLE_LENGTH,
			filesize - POSTAMBLE_LENGTH - PREAMBLE_LENGTH, 0);
	if (ret < 0)
		return ret;

	ret = dsp_send_postamble(codec, firmware + filesize - PREAMBLE_LENGTH);
	if (ret < 0)
		return ret;

	snd_soc_update_bits(codec, M9809X_00F_HOST_CFG, M9809X_DCRYPTEN, 0);
	ret = dsp_read_command_status(codec);
	return ret;
}
#endif

static int dsp_module_parameters (struct snd_soc_codec *codec, unsigned int pmid,
		unsigned int count, unsigned int data[])
{
	int stepcnt;   /* number of parameters in this packet to send */
	int ret = 0;
	int i;

	i = 0;
	while (i < count) {
 		stepcnt = (count - i);
 		if (stepcnt > PARAM_STEP_SIZE)
 			stepcnt = PARAM_STEP_SIZE;

 		ret = dsp_set_param(codec, pmid, stepcnt, &data[i]);
		if (ret < 0)
			goto done;

 		i += stepcnt;
 	}

done:
	return ret;
}

static int dsp_download_parameters (struct snd_soc_codec *codec, struct header * header)
{
	struct params * p = DSP_PARAMS(header);
	int ret = 0;
	int i;

//	pr_info("%s: DSP params count %d, module params count %d\n", __func__, DSP_PARAMS_COUNT(header), MODULE_PARAMS_COUNT(p));

	for (i = 0; i < DSP_PARAMS_COUNT(header); i++) {
		ret = dsp_module_parameters(codec, MODULE_PARAMS_MODULE_ID(p),
					MODULE_PARAMS_COUNT(p), p->data);
		if (ret < 0)
			goto done;

		p = (struct params *)((unsigned)p + MODULE_PARAMS_SIZE(p));
	}

done:
	return ret;
}

static int dsp_AEC_off(struct snd_soc_codec *codec, unsigned int pmid)
{
    unsigned int param = (16 << 24) | 0;
	return dsp_set_param(codec, pmid, 1, &param);
}

int write_config_regs(struct snd_soc_codec *codec,
			int count, struct config_reg_data * reg, unsigned int bank)
{
	int regsave = snd_soc_read(codec, M9809X_00F_HOST_CFG);
	int ret;
	int i;

//	pr_info("%s: regsave 0x%02X, bank 0x%02X\n", __func__, regsave, bank);

	mutex_lock(&codec->mutex);
	snd_soc_update_bits(codec, M9809X_00F_HOST_CFG, M9809X_SEG, bank);

	if (bank == BANK1){
		for (i = 0; i < count; i++) {
	//		pr_info("%s: reg 0x%02X, value 0x%02X\n", __func__, reg->reg, reg->value);
			ret = max9809X_hw_write(codec, reg->reg, reg->value);
			if (ret < 0) {
				dev_err (codec->dev, "configuration register write failed\n");
				return -1;
			}
			reg++;
		}
	}
	else if (bank == BANK0)
	{
		for (i = 0; i < count; i++) {
			switch (reg->reg){
			case M9809X_010_HOST_INT_CFG:
			case M9809X_011_HOST_INT_EN:
			/*Enable HW FIFOs and setup DSP clock */
			case M9809X_025_XCLKCFG:
				ret = snd_soc_write(codec, reg->reg, reg->value);
				if (ret < 0) {
					dev_err (codec->dev, "configuration register write failed reg 0x%02x\n", reg->reg);
				}
				break;
				
			default:
				break;
			}
			reg++;
		}	
	}

	snd_soc_update_bits(codec, M9809X_00F_HOST_CFG, M9809X_SEG, regsave);
	mutex_unlock(&codec->mutex);

	return 0;
}

/* Write regs (0x1E..0x23) needed to turn on FIFOs 	*/
static int dsp_fifo_enable(struct snd_soc_codec *codec, struct header *header)
{
	struct config_reg_data * reg;
	int ret = 0;
        int i = 0;

	reg = REGS_BANK0_DATA(header) + (M9809X_01E_XCFG1 - M9809X_00F_HOST_CFG);
        for (i = 0; i < 6; i++) {
                   ret = snd_soc_write(codec, reg->reg, reg->value);
                   if (ret < 0) break;
 
                   reg++;
         }

	return ret;
}

/* set data FIFO to bypass DSP while we download */
static int dsp_fifo_disable(struct snd_soc_codec *codec)
{
	int ret = 0;
	int i;

	for (i = 0; i < 6; i++) {
		ret = snd_soc_write(codec, 0x1E + i, 0);
		if (ret < 0)
			break;
	}

	return ret;
}

static int dsp_download_firmware(struct snd_soc_codec *codec, struct header * header)
{
	struct fw_images * fw_images = FW_IMAGES(header);
	int fw_image_count = header->fw_images_count;
	char * firmware;
	int filesize;
	int filetype;
	int ret = 0;
	int i;

	for (i = 0; i < fw_image_count; i++) {
		firmware = FW_IMAGE(header, fw_images);
		filetype = firmware[0] + firmware[1] * 256;
		filesize = fw_images->fw_image_size;
		fw_images++;

		#define FLEXSOUND_ENCRYPTED	0x2118
		#define FLEXSOUND_DOWNLOAD	0
		#define FLEXSOUND_CONFIG	0x003

		switch (filetype) {
#ifdef DSP_ENCRYPTION
		case FLEXSOUND_ENCRYPTED:
			ret = dsp_load_encrypted(codec, cfg_no, filesize, firmware);
			break;
#endif
		case FLEXSOUND_DOWNLOAD:
		case FLEXSOUND_CONFIG:

			//dev_info(codec->dev, "%s: image %d, firmware %p, size %d\n", __func__, i, firmware, filesize);

			mutex_lock(&codec->mutex);
			ret = dsp_fifo_block_write(codec, firmware, filesize);
			dev_info(codec->dev, "^^^^^^^^^^^^^^^^^%s 1: image %d, firmware %p, size %d\n", __func__, i, firmware, filesize);
		
			if (ret >= 0)
				ret = dsp_read_command_status(codec);
			mutex_unlock(&codec->mutex);
			//dev_info(codec->dev,"%s 2: image %d, firmware %p, size %d\n", __func__, i, firmware, filesize);
			
			break;

		default:
			ret = -44; /* unknown binary */
			break;
		}
		if (ret < 0)
			break;
	}

	return ret;
}

/*
 * Install a new firmware image, send it a system configuration and
 * then send it all the required configs and params to get it running.
 */
int dsp_fw_download(struct snd_soc_codec *codec, int cfg_no)
{
	struct max9809X_priv *max9809X = snd_soc_codec_get_drvdata(codec);
	struct dsp_fw_entry * p;
	struct header * header;
	int ret = 0;
	int i;
	volatile int status;
	int r48, r49, r97, temp;
	//int r13 = 0;

	if (max9809X->dsp_fw == NULL) {
		ret = -ENXIO;
		goto done;
	}

	if (cfg_no > ((struct dsp_fw *)(max9809X->dsp_fw))->count) {
		ret = -EINVAL;
		goto done;
	}

	p = ((struct dsp_fw *)(max9809X->dsp_fw))->dsp_fw_list;
	for (i = 0; i < cfg_no; i++)
		p = p->next;

	if (strcmp(p->name, "RELOAD_FSAVS") == 0) {
		dev_info(codec->dev, "RELOAD_FSAVS not implemented\n");
		ret = -EINVAL;
		goto done;
	}

	//NM May23 Do this regardless of code download or not
	if ((ret = dsp_fifo_disable(codec)) < 0) {
		dev_info(codec->dev, "dsp_fifo_disable returned %d\n", ret);
		goto done;
	}
	dev_info(codec->dev, "DSP download ::%s\n", p->name);

	/* If the config is 'OFF' then write 0x00 to register 0x0F and leave */
	if (strcmp(p->name, "OFF") == 0) {
		snd_soc_update_bits(codec, M9809X_013_JACK_INT_EN, M9809X_IMCSW|M9809X_IKEYDET, 0);
		snd_soc_update_bits(codec, M9809X_097_PWR_SYS, M9809X_CODECSHDN,M9809X_CODECSHDN); 
		snd_soc_update_bits(codec, M9809X_097_PWR_SYS, M9809X_CODECSHDN,0);
		if(state_hp ==1){
			snd_soc_read(codec,M9809X_007_JACK_AUTO_STS);
			snd_soc_update_bits(codec, M9809X_013_JACK_INT_EN, M9809X_IMCSW|M9809X_IKEYDET, M9809X_IMCSW|M9809X_IKEYDET); 
		}      
		max9809X->cur_dsp = cfg_no;
		snd_soc_write (codec, M9809X_00F_HOST_CFG, 0x00);
		return 0;	
	}
	dev_info(codec->dev, "cfg_no = %d max9809X->cur_dsp=%d\n",cfg_no,max9809X->cur_dsp);
	if (p->fsav == NULL)
		if (dsp_load_fsav(codec, p) < 0) {
			dev_err (codec->dev, "error loading FSAV %d\n", cfg_no);
			ret = -1;
			goto done;
		}
		
	header = (struct header *) p->fsav;
	snd_soc_read(codec,M9809X_007_JACK_AUTO_STS);
	snd_soc_read(codec,M9809X_00A_ACC_ADC_STS);
        //r13 = snd_soc_read(codec,M9809X_013_JACK_INT_EN);
	snd_soc_update_bits(codec, M9809X_013_JACK_INT_EN, M9809X_IMCSW|M9809X_IKEYDET, 0);
	if(cfg_no!=max9809X->cur_dsp){
		max9809X->cur_dsp = cfg_no;
		snd_soc_update_bits(codec, M9809X_097_PWR_SYS, M9809X_SHDNRUN, 0);
		snd_soc_update_bits(codec, M9809X_097_PWR_SYS, M9809X_SHDNRUN, M9809X_SHDNRUN);

		/* clear any xsts bits by reading status reg, and reset the DSP for download */
		status = snd_soc_read(codec, M9809X_001_HOST_INT_STS);
		snd_soc_update_bits(codec, M9809X_00F_HOST_CFG,
				M9809X_XTCLKEN | M9809X_MDLLEN | M9809X_XTEN | M9809X_SEG, 0);

		/* load configuration registers */
		ret = write_config_regs(codec, REGS_BANK1_COUNT(header), REGS_BANK1_DATA(header), BANK1);
		ret = write_config_regs(codec, REGS_BANK0_COUNT(header), REGS_BANK0_DATA(header), BANK0);
		if (ret < 0) {
			dev_err (codec->dev, "write_config_regs returned %d\n", ret);
			goto done;
		}

		/* see if any firmware in this FSAV to download to the DSP */
		if (header->fw_images_count == 0) {
			ret = 0;
			goto done;
		}

		/* turn on the dsp one bit at a time, with XTEN enable bit last, separate from clocking */
		snd_soc_update_bits(codec, M9809X_00F_HOST_CFG, M9809X_MDLLEN, M9809X_MDLLEN);
		snd_soc_update_bits(codec, M9809X_00F_HOST_CFG, M9809X_XTCLKEN, M9809X_XTCLKEN);
		snd_soc_update_bits(codec, M9809X_00F_HOST_CFG, M9809X_XTEN, M9809X_XTEN);

		/* verify reg 1 xsts[7:6] bits are 1s */
		status = snd_soc_read(codec, M9809X_001_HOST_INT_STS);
		if ((status & 0xC0) != 0xC0) {
			/* reset dsp core again - toggling xten is sufficient */
			snd_soc_update_bits(codec, M9809X_00F_HOST_CFG, M9809X_XTEN, 0);
			DBG_PRINT(KERN_ERR "Toggle XTEN. INT_STS status: 0x%02X\n", status);
			/* wait at least 50us */
			msleep(1);
			snd_soc_update_bits(codec, M9809X_00F_HOST_CFG, M9809X_XTEN, M9809X_XTEN);
		}

		/* NM May 23 Get ROM id and if it fails try once to reboot DSP */
		if ((status = dsp_get_rom_id(codec, 0)) < 0) {
			/* reset dsp core again - toggling xten is sufficient */
			snd_soc_update_bits(codec, M9809X_00F_HOST_CFG, M9809X_XTEN, 0);
			dev_info(codec->dev, "Toggle XTEN. INT_STS status: 0x%02X\n", status);
			/* wait at least 50us */
			mdelay(1);
			snd_soc_update_bits(codec, M9809X_00F_HOST_CFG, M9809X_XTEN, M9809X_XTEN);

			/* try to read rom ID again after resetting the DSP */
			if ((status = dsp_get_rom_id(codec, 0)) < 0) {
				dev_err(codec->dev, "get_rom_id error %d\n", status);
				ret = -1;
				goto done;
			}
		}

		/* download DSP firmware */
		ret = dsp_download_firmware(codec, header);
		if (ret < 0) {
			dev_err(codec->dev, "download firmware error %d\n", ret);
			goto done;
		}

#if defined(DSP_SUPPORT_CONFIG_DOWNLOAD)
		/* download the optional config */
		if (header->configuration_size) {
			ret = dsp_mem_download(codec, DSP_CONFIG(header), header->configuration_size, 1);
			if (ret < 0) {
				dev_err(codec->dev, "download config error %d\n", ret);
				goto done;
			}
		}
#endif
	}

	
	// NM May23  codec shutdown before params download
	mutex_lock(&codec->mutex);
	r97 = snd_soc_read(codec, M9809X_097_PWR_SYS);
	temp = r97 | M9809X_CODECSHDN;
	snd_soc_write(codec, M9809X_097_PWR_SYS, temp);
	mutex_unlock(&codec->mutex);

	//NM May23 FIFO reset
	/* write the FIFO enable configuration for this firmware image */
	ret = dsp_fifo_enable(codec, header);
	if (ret < 0) {
		dev_err(codec->dev, "%s: dsp_fifo_enable returned %d\n", __func__, ret);
		goto done;
	}

	/* Ensure AEC is off before enabling it again when the parameters are downloaded */
	ret = dsp_AEC_off(codec, (DSP_PARAMS(header))->module_id);
	if (ret < 0) {
		dev_err(codec->dev, "%s: dsp_AEC_off returned %d\n", __func__, ret);
	}

	/* download the parameters */
	ret = dsp_download_parameters(codec, header);
	if (ret < 0) {
		dev_err(codec->dev, "%s: dsp_download_parameters returned %d\n", __func__, ret);
	}

	mutex_lock(&codec->mutex);
	//NM May23 turn codec back on - next line
	snd_soc_write(codec, M9809X_097_PWR_SYS, r97);
	r48 = snd_soc_read(codec, M9809X_048_MIX_DAC_LR);
	r49 = snd_soc_read(codec, M9809X_049_MIX_DAC_M);
	max9809X_hw_write(codec, M9809X_048_MIX_DAC_LR, 0);
	max9809X_hw_write(codec, M9809X_049_MIX_DAC_M, 0);
	max9809X_hw_write(codec, M9809X_048_MIX_DAC_LR, r48);
	max9809X_hw_write(codec, M9809X_049_MIX_DAC_M, r49);
	mutex_unlock(&codec->mutex);

done:
#if ! defined(DSP_CACHE_FIRMWARE)
	if (p->fsav) {
		free (p->fsav);
		p->fsav = NULL;
	}
#endif

	if (ret == 0) {
		dev_err(codec->dev, "FSAV %s applied\n", p->name);
	}
	
	if(state_hp ==1){
		snd_soc_read(codec,M9809X_007_JACK_AUTO_STS);
		snd_soc_update_bits(codec, M9809X_013_JACK_INT_EN, M9809X_IMCSW|M9809X_IKEYDET, M9809X_IMCSW|M9809X_IKEYDET); 
	}      
	snd_soc_update_bits(codec, M9809X_089_JACK_DET_AUTO, 0x80, 0);

	return ret;
}

static void dsp_reset_clks(struct snd_soc_codec *codec)
{
	snd_soc_write(codec, M9809X_025_XCLKCFG, 0x5F);      //XTCLK=DLLCK     //0x0E);//XTCLK = (PCLK/2)*6, XTclk=DLLCLK*14/16
	snd_soc_write(codec, M9809X_026_SYS_CLK, 0x10);	     //PSCLK, MCLKSEL
	snd_soc_write(codec, M9809X_010_HOST_INT_CFG, 0xC0); //Host Int Config
	snd_soc_write(codec, M9809X_011_HOST_INT_EN, 0x06);  //Host Int Enable
	snd_soc_write(codec, M9809X_00F_HOST_CFG, 0x0C);     //XTCLKEN, MDLLEN
	snd_soc_write(codec, M9809X_00F_HOST_CFG, 0x0E);     //+XTEN
}

static int dsp_put_fs_enum(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max9809X_priv *max9809X = snd_soc_codec_get_drvdata(codec);
	int sel = ucontrol->value.integer.value[0];
	int ret,i;
	struct dsp_fw_entry * p;
	static int download_count = 0;

	if (!max9809X->fs_textcnt)
		return 0;

	max9809X->fs_sel = sel;
	dev_info(codec->dev,"DSP Download ++++++++++++++++++\n ");
	ret = dsp_fw_download(codec, sel);
	dev_info(codec->dev,"DSP Download ----------------------\n ");

	/*
	 * if the DSP download failed with a read or write error then re-download
	 * the firmware
	 */
	if ((ret == -7) || (ret == -8)) {
		download_count++;
		dev_err(codec->dev, "%s: re-download fw, cfg %d, count %d\n", __func__, sel, download_count);
		snd_soc_write(codec, M9809X_025_XCLKCFG, 0x5F );
		snd_soc_write(codec, M9809X_026_SYS_CLK, 0x10);	     //PSCLK, MCLKSEL
		snd_soc_write(codec, M9809X_011_HOST_INT_EN, 0x00);  //Host Int Enable
		snd_soc_write(codec, M9809X_010_HOST_INT_CFG, 0x00); //Host Int Config
		snd_soc_write(codec, M9809X_010_HOST_INT_CFG, 0xC0); //Host Int Config
		snd_soc_write(codec, M9809X_011_HOST_INT_EN, 0x06);  //Host Int Enable
		snd_soc_write(codec, M9809X_00F_HOST_CFG, 0x0C);     //XTCLKEN, MDLLEN
		snd_soc_write(codec, M9809X_00F_HOST_CFG, 0x0E);     //+XTEN
		max9809X->cur_dsp = -1;
		ret = dsp_fw_download(codec, sel);
	}

	if (ret < 0) {
		dev_err(codec->dev, "%s: start DSP error %d\n", __func__, ret);
		return -EINVAL;
	}
	
	p = ((struct dsp_fw *)(max9809X->dsp_fw))->dsp_fw_list;
	for (i = 0; i < sel; i++)
		p = p->next;
	
	if ((strcmp(p->name, "RCV1") == 0) || (strcmp(p->name, "RCV2") == 0)) {
                snd_soc_update_bits(codec, M9809X_013_JACK_INT_EN, M9809X_IMCSW|M9809X_IKEYDET, 0);
		snd_soc_write(codec, M9809X_020_XCFG3, 0x22);
		snd_soc_write(codec, M9809X_022_XCFG5, 0x22);
		snd_soc_write(codec, M9809X_023_XCFG6, 0x02);
		snd_soc_update_bits(codec, M9809X_097_PWR_SYS, M9809X_CODECSHDN,M9809X_CODECSHDN); 
		mdelay(2);
		snd_soc_write(codec, M9809X_020_XCFG3, 0x33);
		snd_soc_write(codec, M9809X_022_XCFG5, 0x33);
		snd_soc_write(codec, M9809X_023_XCFG6, 0x03);
		snd_soc_update_bits(codec, M9809X_097_PWR_SYS, M9809X_CODECSHDN,0);
		mdelay(2);
                if(state_hp ==1){
                    snd_soc_read(codec,M9809X_007_JACK_AUTO_STS);
                    snd_soc_update_bits(codec, M9809X_013_JACK_INT_EN, M9809X_IMCSW|M9809X_IKEYDET, M9809X_IMCSW|M9809X_IKEYDET); 
                }      
	}
	return ret;
}

static int dsp_get_fs_enum(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max9809X_priv *max9809X = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = max9809X->fs_sel;
	return 0;
}

void dsp_fs_setup(struct snd_soc_codec *codec)
{
	struct max9809X_priv *max9809X = snd_soc_codec_get_drvdata(codec);
	int i, ret;

	struct snd_kcontrol_new controls[] = {
		SOC_ENUM_EXT("Flexsound",
			max9809X->fs_enum,
			dsp_get_fs_enum,
			dsp_put_fs_enum)
	};

	max9809X->fs_textcnt = NUM_PREDEFINED+MODENUM;
	max9809X->fs_texts = kmalloc(sizeof(char *) * max9809X->fs_textcnt, GFP_KERNEL);
	if (max9809X->fs_texts == NULL) {
		dev_err(codec->dev, "failed to allocate memory for fs_texts\n");
		return;
	}
	for (i = 0; i < max9809X->fs_textcnt; i++) {
		if(i<NUM_PREDEFINED)
			max9809X->fs_texts[i] = predefined[i];
		else
			max9809X->fs_texts[i] = voice_call_mode[i-NUM_PREDEFINED];
	}

	max9809X->fs_enum.texts = max9809X->fs_texts;
	max9809X->fs_enum.max = max9809X->fs_textcnt;

	if ((ret = snd_soc_add_codec_controls(codec, controls, ARRAY_SIZE(controls))) != 0)
		dev_err(codec->dev, "failed to add Flexsound control\n");

	return;
}

#ifdef DUMP_FSAV_HEADER
static void pr_header (struct header * header)
{
	struct fw_images * fw_images = FW_IMAGES(header);
	struct params * params = DSP_PARAMS(header);
	int i;

	pr_info("version              %4s \n", header->version);
	pr_info("date                 %10s \n", header->date);
	pr_info("time                 %8s \n", header->time);
	pr_info("name length          %d \n", header->kcontrol_name_size);
	pr_info("name offset        0x%08X \n", header->kcontrol_name_offset);
	pr_info("name                 %s \n", (char *) header+header->kcontrol_name_offset);
	pr_info("regs_bank0_count     %d \n", REGS_BANK0_COUNT(header));
	pr_info("regs_bank0_offset    %d \n", header->regs_bank0_offset);
	pr_info("regs_bank1_count     %d \n", REGS_BANK1_COUNT(header));
	pr_info("regs_bank1_offset    %d \n", header->regs_bank1_offset);
	pr_info("fw image count       %d \n", header->fw_images_count);
	pr_info("fw images offset   0x%08x \n", header->fw_images_offset);
	for (i = 0; i < header->fw_images_count; i++ ) {
		pr_info("  image %2d size          %8d \n", i, fw_images->fw_image_size);
		pr_info("  image %2d offset      0x%08X \n", i, fw_images->fw_image_offset);
		fw_images++;
	}
	pr_info("params count         %d \n", header->params_count);
	pr_info("params offset      0x%08x \n", header->params_offset);
	for (i = 0; i < header->params_count; i++ ) {
		pr_info("  params %2d size         %8d \n", i, MODULE_PARAMS_COUNT(params));
		pr_info("  params %2d module ID  0x%08X \n", i, MODULE_PARAMS_MODULE_ID(params));
		pr_info("  params %2d data[0]    0x%08X \n", i, params->data[0]);
		params = (struct params *)((unsigned)params + MODULE_PARAMS_SIZE(params));
	}
	pr_info("checksum size        %d \n", header->checksum_size);
	pr_info("checksum offset    0x%08X \n", header->checksum_offset);
	pr_info("raw data size        %d \n", header->raw_data_size);
	pr_info("raw data offset    0x%08X \n", header->raw_data_offset);
}
#endif



static int dsp_request_fw(char* file_name, const struct firmware ** fw,
						void * control_data,int file_index)
{
	char fname[MAX_NAME_LEN];
	int res = 0;

#if defined(DEVICE_MAX98095)
	snprintf(fname, MAX_NAME_LEN, "maxim/%02d-%s.%s", file_index, file_name,"f095");
	//snprintf(fname, MAX_NAME_LEN, "maxim/%s.%s", file_name,"f095");
#elif defined(DEVICE_MAX98096)
	snprintf(fname, MAX_NAME_LEN, "maxim/%02d-%s.%s", file_index, file_name,"f096");
#else
#error "DEVICE TYPE NOT DEFINED"
#endif
	pr_info("%s: requesting firmware file '%s'\n", __func__, fname);
	res = request_firmware(fw, fname,  control_data);

	return res;
}

static int dsp_cache_fw(struct header * header, void ** p, unsigned int size)
{
	*p = kmalloc(size, GFP_KERNEL);
	if (p == NULL)
		return -1;

	memcpy(*p, header, size);

	return 0;
}

static struct dsp_fw_entry * create_fsav_node(const struct firmware * fw)
{
	struct header * header = (struct header *) fw->data;
	struct dsp_fw_entry * p;

#ifdef DUMP_FSAV_HEADER
	pr_header( header );
#endif

	if (strcmp(header->version, FW_VERSION1) != 0)
		return NULL;

	p = kmalloc(sizeof(*p), GFP_KERNEL);
	if (p == NULL)
		return NULL;

	snprintf (p->name, MAX_NAME_LEN, (char *) header+header->kcontrol_name_offset);
	p->size = fw->size;
#if defined(DSP_CACHE_FIRMWARE)
	if (dsp_cache_fw(header, &p->fsav, p->size) < 0)
		return NULL;
#else
	p->fsav = NULL;
#endif

	return p;
}

static struct dsp_fw_entry * create_predefined_fsav_node(int index)
{
	struct dsp_fw_entry * p;

	p = kmalloc(sizeof(*p), GFP_KERNEL);
	if (p != NULL) {
		snprintf(p->name, MAX_NAME_LEN, predefined[index]);
		p->file_num = 0;
		p->size = 0;
		p->fsav = NULL;
	}

	return p;
}

void * dsp_enumerate_firmware(struct snd_soc_codec *codec)
{
	const struct firmware * fw = NULL;
	struct dsp_fw_entry * p;
	struct dsp_fw_entry * fw_list_tail;
	struct dsp_fw_entry * fw_list_head;
	struct dsp_fw * dsp_fw;
	int result;
	int i;

	if ((dsp_fw = kmalloc(sizeof(*dsp_fw), GFP_KERNEL)) == NULL) {
		dev_err(codec->dev, "error allocating DSP firmware memory");
		return NULL;
	}
	fw_list_head = kmalloc(sizeof(*fw_list_head), GFP_KERNEL);
	if (fw_list_head == NULL)
		return NULL;

	fw_list_tail = kmalloc(sizeof(*fw_list_tail), GFP_KERNEL);
	if (fw_list_tail == NULL)
		return NULL;

	dsp_fw->dsp_fw_list = NULL;
	dsp_fw->count = 0;
	i = 0;
	do {
		result = dsp_request_fw(voice_call_mode[i], &fw, codec->control_data,i);
		dev_info(codec->dev," dsp_request_fw FSAV result:%d\n", result);
		if (result == 0) {
			p = create_fsav_node(fw);
			if (p == NULL) {
				dev_err(codec->dev, "failed to create FSAV node %d\n", i);
				continue;
			}
			p->file_num = i;
			if(!dsp_fw->dsp_fw_list){
				dsp_fw->dsp_fw_list = p;
				fw_list_tail = p;
			}else{
				fw_list_tail->next = p;
				fw_list_tail = p;
			}
			release_firmware(fw);
			dsp_fw->count++;
		}
		i++;
	} while (i < MODENUM);

	for (i = 0; i < NUM_PREDEFINED; i++) {
		p = create_predefined_fsav_node(i);
		if (p == NULL) {
			dev_err(codec->dev, "create_predefined_fsav_node failed\n");
			break;
		}

		if(!i){
			fw_list_tail = p;
			fw_list_head =p;
		}else{
			fw_list_tail->next = p;
			fw_list_tail = p;
		}
		if(i==(NUM_PREDEFINED-1)){
			fw_list_tail->next = dsp_fw->dsp_fw_list;
			dsp_fw->dsp_fw_list = fw_list_head;
		}
		dsp_fw->count++;
	}

	dev_info(codec->dev, "%d FSAVs enumerated\n", dsp_fw->count);
	return dsp_fw;
}

static int dsp_load_fsav(struct snd_soc_codec *codec, struct dsp_fw_entry *p)
{
	const struct firmware * fw = NULL;
	int result;

	result = dsp_request_fw(p->name, &fw, codec->control_data,p->file_num);

	if (result == 0) {
		result = dsp_cache_fw((struct header *) fw->data, &p->fsav, p->size);
		release_firmware(fw);
	}

	return result;
}

/* Initialize the Flexsound DSP firmware */
int dsp_flexsound_init(struct snd_soc_codec *codec)
{
	struct max9809X_priv *max9809X = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	if(max9809X->dsp_fw)
		return 0;

	/* turn on the codec and enable the clocks to the DSP */
	snd_soc_update_bits(codec, M9809X_097_PWR_SYS, M9809X_SHDNRUN, M9809X_SHDNRUN);
	dsp_reset_clks(codec);

	max9809X->dsp_fw = dsp_enumerate_firmware(codec);
	//dsp_fs_setup(codec);

	max9809X->cur_dsp = -1;
	ret = dsp_fw_download(codec, 1);
	if (ret < 0)
		dev_err(codec->dev, "Initial DSP download error %d\n", ret);

	return ret;
}

/*
 * drivers/misc/sensors/light/tmd271x.c
 *
 * Copyright (C) 2010-2012 ZTE Corporation
 *
 * Author:
 *	Poyuan Lu <lu.poyuan@zte.com.cn>
 *	John Koshi - Surya Software
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

//caution: the cpu and device must be little endian memory

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/suspend.h>
#include <asm/delay.h>
#include <asm/errno.h>
#include <asm/uaccess.h>

#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/wakelock.h>

#include <linux/miscdevice.h>

#include <linux/i2c/tmd2771x.h>


/*register table*/
// register offsets
#define TMD2771X_REG_ENABLE    0x00
#define TMD2771X_REG_ATIME     0X01
#define TMD2771X_REG_PTIME     0x02
#define TMD2771X_REG_WTIME     0x03
#define TMD2771X_REG_AILTL     0X04
#define TMD2771X_REG_AILTH     0X05
#define TMD2771X_REG_AIHTL     0X06
#define TMD2771X_REG_AIHTH     0X07
#define TMD2771X_REG_PILTL     0X08
#define TMD2771X_REG_PILTH     0X09
#define TMD2771X_REG_PIHTL     0X0A
#define TMD2771X_REG_PIHTH     0X0B
#define TMD2771X_REG_PERS      0x0C
#define TMD2771X_REG_CONFIG    0x0D
#define TMD2771X_REG_PPCOUNT   0x0E
#define TMD2771X_REG_CONTROL   0x0F
#define TMD2771X_REG_ID        0x12
#define TMD2771X_REG_STATUS    0x13
#define TMD2771X_REG_C0DATA    0x14
#define TMD2771X_REG_C0DATAH   0x15
#define TMD2771X_REG_C1DATA    0x16
#define TMD2771X_REG_C1DATAH   0x17
#define TMD2771X_REG_PDATA     0x18
#define TMD2771X_REG_PDATAH     0x19

// cmd reg masks
#define TMD2771X_CMD_REG             0X80
#define TMD2771X_CMD_SNG_RW          0x00
#define TMD2771X_CMD_BUST_RW         0x20
#define TMD2771X_CMD_SPL_FN          0x60
#define TMD2771X_CMD_PROX_INTCLR     0X05
#define TMD2771X_CMD_ALS_INTCLR      0X06
#define TMD2771X_CMD_PROXALS_INTCLR  0X07

// cntrl reg masks
#define TMD2771X_ENBL_PROX_INT       0X20
#define TMD2771X_ENBL_ALS_INT        0X10
#define TMD2771X_ENBL_WAIT_TMR       0X08
#define TMD2771X_ENBL_PROX_DET       0X04
#define TMD2771X_ENBL_ADC_ENBL       0x02
#define TMD2771X_ENBL_PWRON          0x01

#define TMD2771X_PROX_ON 0x2F

// status reg masks
#define TMD2771X_STATUS_ADCVALID     0x01
#define TMD2771X_STATUS_PRXVALID     0x02
#define TMD2771X_STATUS_ADCINTR      0x10
#define TMD2771X_STATUS_PRXINTR      0x20

// Device ID
#define TMD2771X_DEVID_TMD27711      0x20
#define TMD2771X_DEVID_TMD27713      0x29

#define TAOS_MAX_LUX                    10000
#define I2C_MAX_TRANSBYTES 128

#define TMD2771X_BIT_ISSET(x, mask) (((x) & (mask)) == (mask))

#define TAOS_IOCTL_MAGIC        	0xCF
#define TAOS_IOCTL_ALS_ON       	_IO(TAOS_IOCTL_MAGIC, 1)
#define TAOS_IOCTL_ALS_OFF      	_IO(TAOS_IOCTL_MAGIC, 2)
#define TAOS_IOCTL_PROX_ON		_IO(TAOS_IOCTL_MAGIC, 3)
#define TAOS_IOCTL_PROX_OFF	_IO(TAOS_IOCTL_MAGIC, 4)

struct tmd2771x_data {
	struct i2c_client *client;

	//interrupt
	struct work_struct work;
	struct wake_lock irq_wake_lock;

	//input device
	struct input_dev *input_dev;

	char config_IsInitialized;
	int shutdown_complete;
};

struct lux_data {
    u16 ir;
    u16 ratio;
    u16 clear;;
};

static struct als_config g_als_config = {
	.enabled = 0,
	.int_pers = 0x20,
	.adc_time = 0xED, //2  50ms
	.gain = ALS_GAIN_1X,
	.scale_factor = 1,
	.threshold_hi = 3000,
	.threshold_lo = 10,
	.scale_down = 1,
	.scale_up = 1,
};

static struct prox_config g_prox_config = {
	.enabled = 0,
	.int_pers = 0x00,
	.adc_time = 0xFF, //2  2.72ms
	.pulse_count = 4,
	.led_drive = PROX_LED_DRIVE_25MA,
	.channel = PROX_CHANNEL_1,
	.threshold_hi = 360,
	.threshold_lo = 320,
	.thresh_gain_hi = 200,
	.thresh_gain_lo = 170,
};

static struct wait_config g_wait_config = {
	.wait_time = 0xFF, //2 2.72ms
	.wlong = 0x00,
};

/*
In order to reject 50/60-HZ ripple strongly present in fluorescent lighting,
the integration time needs to be programmed in multiples of 10/8.3ms or half
cycle time. Both frequencies can be rejected with a programmed value of 50ms
(ATIME=0xED) or multiples of 50ms(ie. 100, 150, 200, 400, 700).
ATIME = 256-integration time/2.72ms

for n*50ms integration time
ATIME = 256 -integration time/50 *18

we recommand the integration time is range from 50ms to 650ms
*/

static unsigned int integration_time=250;
static unsigned char als_gain_table[] = {1, 8, 16, 120};

struct lux_data tmd2771x_lux_data[] = {
    { 9830,  8320,  15360 },
    { 12452, 10554, 22797 },
    { 14746, 6234,  11430 },
    { 17695, 3968,  6400  },
    { 0,     0,     0     }
};

char *leddrive_strs[] = {
	"100 mA",
	"50 mA",
	"25 mA",
	"12.5 mA"
};

char *proxchannels_strs[] = {
	"Reserved",
	"Channel 0",
	"Channel 1",
	"BOTH"
};

static atomic_t irq_enabled;
static struct mutex g_control_mutex;
static int taos_debugfs_mode = 0;
static int prox_thresh_reset = 0;
static int prox_distance=0;
static int interruptCounter=0;
static struct workqueue_struct *irq_workqueue;
static struct i2c_client *tmd2771x_clientp = NULL;

static atomic_t suspend_state;
static DECLARE_WAIT_QUEUE_HEAD(tmd2771x_wait);

extern bool in_call_state(void);

static int i2c_write(unsigned int len, unsigned char *data)
{
	struct i2c_msg msg;
	int res;
	struct tmd2771x_data *datap =NULL;

	if (NULL == data)
		return -EINVAL;

	if (NULL == tmd2771x_clientp)
		return -EIO;
	datap =  i2c_get_clientdata(tmd2771x_clientp);

	if (datap && datap->shutdown_complete){
		return -ENODEV;
	}
	msg.addr = tmd2771x_clientp->addr;
	msg.flags = 0;	/* write */
	msg.buf = data;
	msg.len = len;

	res = i2c_transfer(tmd2771x_clientp->adapter, &msg, 1);
	if (res < 1)
		return -1;
	else
		return 0;
}

static int i2c_read(unsigned char reg,
		    unsigned int len, unsigned char *data)
{
	struct i2c_msg msgs[2];
	int res;
	struct tmd2771x_data *datap =NULL;   

	if (NULL == data)
		return -EINVAL;

	if (NULL == tmd2771x_clientp)
		return -EIO;

	datap =  i2c_get_clientdata(tmd2771x_clientp);
	if (datap && datap->shutdown_complete){
		return -ENODEV;
	}

	msgs[0].addr = tmd2771x_clientp->addr;
	msgs[0].flags = 0;	/* write */
	msgs[0].buf = &reg;
	msgs[0].len = 1;

	msgs[1].addr = tmd2771x_clientp->addr;
	msgs[1].flags = I2C_M_RD; /* read */
	msgs[1].buf = data;
	msgs[1].len = len;

	res = i2c_transfer(tmd2771x_clientp->adapter, msgs, 2);
	if (res < 2)
		return -1;
	else
		return 0;
}

static int i2c_write_register(unsigned char reg, unsigned char value)
{
	unsigned char data[2];

	data[0] = (reg |TMD2771X_CMD_REG | TMD2771X_CMD_SNG_RW);
	data[1] = value;
	return i2c_write(2, data);
}

static int i2c_read_register(unsigned char reg, unsigned char *value)
{
	return i2c_read((reg |TMD2771X_CMD_REG | TMD2771X_CMD_SNG_RW), 1, value);
}

static int i2c_write_block(unsigned char reg,
		    unsigned int len, unsigned char *data)
{
	unsigned char msg_buf[I2C_MAX_TRANSBYTES];

	if(len > (I2C_MAX_TRANSBYTES-1))
		return -1;

	msg_buf[0] = (reg |TMD2771X_CMD_REG | TMD2771X_CMD_BUST_RW);
	memcpy(msg_buf+1, data, len);

	return i2c_write(len+1, msg_buf);
}

static int i2c_read_block(unsigned char reg,
		    unsigned int len, unsigned char *data)
{
	return i2c_read((reg |TMD2771X_CMD_REG | TMD2771X_CMD_BUST_RW), len, data);
}

static int tmd2771x_sfcommand(unsigned char command)
{
	struct i2c_msg msg;
	int res;
	unsigned char data =  command | TMD2771X_CMD_REG | TMD2771X_CMD_SPL_FN;

	if (NULL == tmd2771x_clientp)
		return -EIO;

	msg.addr = tmd2771x_clientp->addr;
	msg.flags = 0;	/* write */
	msg.buf = &data;
	msg.len = 1;

	res = i2c_transfer(tmd2771x_clientp->adapter, &msg, 1);
	if (res < 1)
		return -1;
	else
		return 0;
}

#if 0
static void printk_regs(const char *header,
					int start_addr, int nums)
{
	unsigned char regs[4];
	int reg_item;
	int ret;

	if(nums >4)
		return;

	printk("%s:\n", header);
	if(nums == 1){
		ret = i2c_read_register(start_addr, regs);
		if(ret >= 0)
			printk(" Reg 0x%02x Value 0x%02x\n", start_addr, regs[0]);
	}else{
		ret = i2c_read_block(start_addr, nums, regs);
		if(ret >=0){
			for(reg_item = 0; reg_item < nums; reg_item++){
				printk(" Reg 0x%02x Value 0x%02x\n", start_addr+reg_item, regs[reg_item]);
			}
		}
	}
}
#endif

static int tmd2771x_quick_configure(void)
{
	int ret;
	unsigned char reg_val;
	struct tmd2771x_data *datap;

	datap = i2c_get_clientdata(tmd2771x_clientp);

	/*ALS interrupt clear*/
	ret = tmd2771x_sfcommand(TMD2771X_CMD_PROXALS_INTCLR);
	if(ret < 0){
		printk(KERN_ERR "%s:failed at line %d\n", __func__, __LINE__);
		ret = -1;
		goto failed;
	}

	//disable device before the register config
	ret = i2c_write_register(TMD2771X_REG_ENABLE, 0x00);
	if(ret < 0){
		printk(KERN_ERR "%s:failed at line %d\n", __func__, __LINE__);
		ret = -2;
		goto failed;
	}

        /*Register setting*/
	//time setting
	ret = i2c_write_register(TMD2771X_REG_ATIME, 0xFF);
	if(ret < 0){
		printk(KERN_ERR "%s:failed at line %d\n", __func__, __LINE__);
		ret = -3;
		goto failed;
	}

	ret = i2c_write_register(TMD2771X_REG_PTIME, g_prox_config.adc_time);
	if(ret < 0){
		printk(KERN_ERR "%s:failed at line %d\n", __func__, __LINE__);
		ret = -4;
		goto failed;
	}

	ret = i2c_write_register(TMD2771X_REG_WTIME, g_wait_config.wait_time);
	if(ret < 0){
		printk(KERN_ERR "%s:failed at line %d\n", __func__, __LINE__);
		ret = -5;
		goto failed;
	}

	ret = i2c_write_register(TMD2771X_REG_CONFIG, g_wait_config.wlong);
	if(ret < 0){
		printk(KERN_ERR "%s:failed at line %d\n", __func__, __LINE__);
		ret = -6;
		goto failed;
	}

	//interrupt filter setting
	reg_val = g_prox_config.int_pers |g_als_config.int_pers;
	ret = i2c_write_register(TMD2771X_REG_PERS, reg_val);
	if(ret < 0){
		printk(KERN_ERR "%s:failed at line %d\n", __func__, __LINE__);
		ret = -7;
		goto failed;
	}

	//led pulse count
	ret = i2c_write_register(TMD2771X_REG_PPCOUNT, g_prox_config.pulse_count);
	if(ret < 0){
		printk(KERN_ERR "%s:failed at line %d\n", __func__, __LINE__);
		ret = -8;
		goto failed;
	}

	//led drive and als gain
	reg_val = g_prox_config.led_drive | g_prox_config.channel | g_als_config.gain;
	ret = i2c_write_register(TMD2771X_REG_CONTROL, reg_val);
	if(ret < 0){
		printk(KERN_ERR "%s:failed at line %d\n", __func__, __LINE__);
		ret = -9;
		goto failed;
	}

	datap->config_IsInitialized = 1;
	ret = 0;
failed:
	return ret;
}

static int tmd2771x_configure(void)
{
	int ret;
	unsigned char reg_val;
	struct tmd2771x_data *datap;

	datap = i2c_get_clientdata(tmd2771x_clientp);

	/*ALS interrupt clear*/
	ret = tmd2771x_sfcommand(TMD2771X_CMD_PROXALS_INTCLR);
	if(ret < 0){
		printk(KERN_ERR "%s:failed at line %d\n", __func__, __LINE__);
		ret = -1;
		goto failed;
	}

	//disable device before the register config
	ret = i2c_write_register(TMD2771X_REG_ENABLE, 0x00);
	if(ret < 0){
		printk(KERN_ERR "%s:failed at line %d\n", __func__, __LINE__);
		ret = -2;
		goto failed;
	}

        /*Register setting*/
	//time setting
	ret = i2c_write_register(TMD2771X_REG_ATIME, g_als_config.adc_time);
	if(ret < 0){
		printk(KERN_ERR "%s:failed at line %d\n", __func__, __LINE__);
		ret = -3;
		goto failed;
	}

	ret = i2c_write_register(TMD2771X_REG_PTIME, g_prox_config.adc_time);
	if(ret < 0){
		printk(KERN_ERR "%s:failed at line %d\n", __func__, __LINE__);
		ret = -4;
		goto failed;
	}

	ret = i2c_write_register(TMD2771X_REG_WTIME, g_wait_config.wait_time);
	if(ret < 0){
		printk(KERN_ERR "%s:failed at line %d\n", __func__, __LINE__);
		ret = -5;
		goto failed;
	}

	ret = i2c_write_register(TMD2771X_REG_CONFIG, g_wait_config.wlong);
	if(ret < 0){
		printk(KERN_ERR "%s:failed at line %d\n", __func__, __LINE__);
		ret = -6;
		goto failed;
	}

	//interrupt filter setting
	reg_val = g_prox_config.int_pers |g_als_config.int_pers;
	ret = i2c_write_register(TMD2771X_REG_PERS, reg_val);
	if(ret < 0){
		printk(KERN_ERR "%s:failed at line %d\n", __func__, __LINE__);
		ret = -7;
		goto failed;
	}

	//led pulse count
	ret = i2c_write_register(TMD2771X_REG_PPCOUNT, g_prox_config.pulse_count);
	if(ret < 0){
		printk(KERN_ERR "%s:failed at line %d\n", __func__, __LINE__);
		ret = -8;
		goto failed;
	}

	//led drive and als gain
	reg_val = g_prox_config.led_drive | g_prox_config.channel | g_als_config.gain;
	ret = i2c_write_register(TMD2771X_REG_CONTROL, reg_val);
	if(ret < 0){
		printk(KERN_ERR "%s:failed at line %d\n", __func__, __LINE__);
		ret = -9;
		goto failed;
	}

	datap->config_IsInitialized = 1;
	ret = 0;
failed:
	return ret;
}

static int tmd2771x_prox_threshupdate(unsigned short chdata[3], int *state)
{

	int ret;
	unsigned char thres_buf[4];

	prox_distance = chdata[2];

        printk("+tmd2771x_prox_threshupdate: chdata[2]=0x%x, lo=0x%x, hi=0x%x + \n",chdata[2] ,g_prox_config.threshold_lo, g_prox_config.threshold_hi);
	//justify the near or far state and update the threshold register
	if (chdata[2] < g_prox_config.threshold_lo) { /*far state*/
		//report the distance is higher than the threshold
		if(state)
			*state = 5;
		//set the higher threshold to check the near state
		thres_buf[0] = 0x0;
		thres_buf[1] = 0x0;
		thres_buf[2] = g_prox_config.threshold_hi & 0xFF;
		thres_buf[3] = g_prox_config.threshold_hi >> 8;
	} else if (chdata[2] > g_prox_config.threshold_hi ){ /*near state*/
		//check whether in strong visible light evironment
		if (chdata[1] > ((g_als_config.saturation*80)/100)){
			if(state)
				*state = 5;
			return 0;
		}
		//report the distance is lower than the threshold
		if(state)
			*state = 0;
		//set the lower threshold to check the far state
		thres_buf[0] = g_prox_config.threshold_lo & 0xFF;
		thres_buf[1] = g_prox_config.threshold_lo >> 8;
		thres_buf[2] = 0xff;
		thres_buf[3] = 0xff;
	}
        else
	{
		printk("%s:threshold between hi and lo, threshold_lo=%d, threshold_hi=%d\n", __func__, g_prox_config.threshold_lo, g_prox_config.threshold_hi);
		return 0;
	}
	//update the prox threshhold register
	ret = i2c_write_block(TMD2771X_REG_PILTL, 4 , thres_buf);
	if(ret < 0){
		printk(KERN_ERR "%s:failed at line %d\n", __func__, __LINE__);
		return -1;
	}

	return 0;
}

static int tmd2771x_als_threshupdate(unsigned short ch0)
{
	int ret;
	u16 als_threshold[2];

	//set als threshold when ch0 data has been changed by 20%
	//hi threshold
	als_threshold[1] = (12*ch0)/10;
	//low threshold
	als_threshold[0] = (8*ch0)/10;

	g_als_config.threshold_hi = als_threshold[1];
	g_als_config.threshold_lo = als_threshold[0];

	/*update the als threshold*/
	ret = i2c_write_block(TMD2771X_REG_AILTL, 4 , (unsigned char *)als_threshold);
	if(ret < 0){
		printk(KERN_ERR "%s:failed at line %d\n", __func__, __LINE__);
		return -1;
	}

	return 0;
}

static unsigned int tmd2771x_als_cal(unsigned short ch0, unsigned short ch1)
{
	unsigned int raw_clear = 0;/*visible*/
	unsigned int raw_ir = 0; /*infrared*/
	unsigned int raw_lux = 0;
	unsigned int lux=0;
	unsigned int ratio = 0;
	unsigned int saturation;
	unsigned char dev_gain = 0;
	struct lux_data *p;

	raw_clear = ch0 ;//* g_als_config.scale_factor *  11;
	raw_ir    = ch1 ;//* g_als_config.scale_factor * 3;

	if(raw_ir > raw_clear) {
		raw_lux = raw_ir;
		raw_ir = raw_clear;
		raw_clear = raw_lux;
	}

	saturation = 300 * integration_time;
	if(saturation>65535)
		saturation = 65535;

	if(raw_clear >= saturation)
		return (TAOS_MAX_LUX);

	if(raw_ir >= saturation)
		return (TAOS_MAX_LUX);

	if(raw_clear == 0)
		return 1;

	raw_clear = ch0 * g_als_config.scale_factor *  11;
	raw_ir    = ch1 * g_als_config.scale_factor * 3;

	if(raw_ir > raw_clear) {
		raw_lux = raw_ir;
		raw_ir = raw_clear;
		raw_clear = raw_lux;
	}

	ratio = (raw_ir<<15)/raw_clear;
	for (p = tmd2771x_lux_data; p->ratio && p->ratio < ratio; p++);
	if(!p->ratio) {
		return (TAOS_MAX_LUX);
	}

	dev_gain = als_gain_table[g_als_config.gain & 0x03];
	raw_clear = ((raw_clear*400 + (dev_gain>>1))/dev_gain + (integration_time>>1))/integration_time;
	raw_ir = ((raw_ir*400 +(dev_gain>>1))/dev_gain + (integration_time>>1))/integration_time;

	lux = ((raw_clear*(p->clear)) - (raw_ir*(p->ir)));
	lux = (lux + 32000)/64000;

	if(g_als_config.scale_down != 0)
		lux = lux * g_als_config.scale_up / g_als_config.scale_down;

	if(lux > TAOS_MAX_LUX) {
		lux = TAOS_MAX_LUX;
	}
	return(lux);
}

static void tmd2771x_enabledisable(void){
	struct tmd2771x_data *datap;
	unsigned char enable_reg = 0;
	u16 thres_buf[2];

	datap = i2c_get_clientdata(tmd2771x_clientp);

	cancel_work_sync(&datap->work);

	printk("%s:%d-%d\n", __func__, g_prox_config.enabled, g_als_config.enabled);

	//control the sensor active state
	if(g_prox_config.enabled ||g_als_config.enabled){
		if (atomic_cmpxchg(&irq_enabled, 1, 0) == 1) {
			disable_irq(tmd2771x_clientp->irq);
		}

		if(!datap->config_IsInitialized)
			tmd2771x_configure();
		else{
			tmd2771x_sfcommand(TMD2771X_CMD_PROXALS_INTCLR);
			i2c_write_register(TMD2771X_REG_ENABLE, 0x00);
		}

		if(g_prox_config.enabled){
			enable_reg |= (TMD2771X_ENBL_PROX_DET | TMD2771X_ENBL_PROX_INT | \
							TMD2771X_ENBL_WAIT_TMR);
			/*prox*/
			thres_buf[0] = g_prox_config.threshold_lo;
			thres_buf[1] = g_prox_config.threshold_hi;
			i2c_write_block(TMD2771X_REG_PILTL, 4 , (unsigned char *)thres_buf);
		}

		if(g_als_config.enabled){
			enable_reg |= TMD2771X_ENBL_ALS_INT;
			/*als:
				the threshold recommend the value to make sure any value can trigger
				the interrupt*/
			thres_buf[0] = 10000;
			thres_buf[1] = 10000;
			i2c_write_block(TMD2771X_REG_AILTL, 4 , (unsigned char *)thres_buf);
		}

		/*enable the sensor*/
		enable_reg |= (TMD2771X_ENBL_PWRON | TMD2771X_ENBL_ADC_ENBL);
		i2c_write_register(TMD2771X_REG_ENABLE, enable_reg);

		if (atomic_cmpxchg(&irq_enabled, 0, 1) == 0) {
			enable_irq(tmd2771x_clientp->irq);
		}
	}else{
		// avoiding same events are filtered by input model, for solving keyboard light problem
		input_report_abs(datap->input_dev, ABS_MISC, 10001);
		if (atomic_cmpxchg(&irq_enabled, 1, 0) == 1) {
			disable_irq(tmd2771x_clientp->irq);
		}
		i2c_write_register(TMD2771X_REG_ENABLE, 0x00);
		datap->config_IsInitialized = 0;
	}
}

static void tmd2771x_update_control(void){
	int sensors = 0;

	sensors = (g_als_config.enabled << 1) + g_prox_config.enabled;

	switch(sensors){
		case 0x00:
			i2c_write_register(TMD2771X_REG_ENABLE, 0x00);//als off, prox off
			break;
		case 0x01:
			i2c_write_register(TMD2771X_REG_ENABLE, 0x2F);//als off, prox on
			break;
		case 0x02:
			i2c_write_register(TMD2771X_REG_ENABLE, 0x13);//als on, prox off
			break;
		case 0x03:
			i2c_write_register(TMD2771X_REG_ENABLE, 0x3F);//als on, prox on
			break;
	}
}
int g_tmd2771x_lux = -1;
static void tmd2771x_als_handle(unsigned short chdata[3])
{

	int ret;
	int lux_val;

	struct tmd2771x_data *datap;

	datap = i2c_get_clientdata(tmd2771x_clientp);

	/*update the als threshold*/
	ret = tmd2771x_als_threshupdate(chdata[0]);
	if(!ret){
		//report the light lux value
		lux_val = tmd2771x_als_cal(chdata[0], chdata[1]);
		g_tmd2771x_lux = lux_val;
		input_report_abs(datap->input_dev, ABS_MISC, lux_val);
	}
}

static void tmd2771x_prox_handle(unsigned short chdata[3])
{
	int ret;
	int state = 0;
	struct tmd2771x_data *datap;

	//update the prox threshhold
	ret = tmd2771x_prox_threshupdate(chdata, &state);
	if(!ret){
		//report the prox state: near or far
		datap = i2c_get_clientdata(tmd2771x_clientp);
		printk("tmd2771x: report prox data =%d\n", state);
		input_report_abs(datap->input_dev, ABS_DISTANCE, state);
	}
}

static void tmd2771x_int_handle(void)
{
	int ret;
	unsigned char status;
	unsigned char enable_reg;
	unsigned short channel_data[3];

	//just whether the device is power
	//just whether the als adc is enabled
	ret = i2c_read_register(TMD2771X_REG_ENABLE, &enable_reg);
	if(ret < 0){
		printk(KERN_ERR "%s:failed at line %d\n", __func__, __LINE__);
		goto clear_int;
	}

	if(!TMD2771X_BIT_ISSET(enable_reg, (TMD2771X_ENBL_ADC_ENBL | TMD2771X_ENBL_PWRON))){
		printk(KERN_ERR "%s:failed at line %d\n", __func__, __LINE__);
		goto clear_int;
	}

	//just whether the als data is available
	ret = i2c_read_register(TMD2771X_REG_STATUS, &status);
	if(ret < 0){
		printk(KERN_ERR "%s:failed at line %d\n", __func__, __LINE__);
		goto clear_int;
	}

	if(!TMD2771X_BIT_ISSET(status, TMD2771X_STATUS_ADCVALID)){
		printk(KERN_ERR "%s:failed at line %d\n", __func__, __LINE__);
		goto clear_int;
	}

	//get the als and prox channel data
	ret = i2c_read_block(TMD2771X_REG_C0DATA, 6, (unsigned char *)channel_data);
	if(ret < 0){
		printk(KERN_ERR "%s:failed at line %d\n", __func__, __LINE__);
		goto clear_int;
	}

	//prox has the high priority
	if(TMD2771X_BIT_ISSET(status, TMD2771X_STATUS_PRXINTR | TMD2771X_STATUS_PRXVALID)){
		if(g_prox_config.enabled){
			ret = tmd2771x_sfcommand(TMD2771X_CMD_PROX_INTCLR);
			if(ret == 0){
				tmd2771x_prox_handle(channel_data);
			}else{
				printk(KERN_ERR "%s:failed at line %d\n", __func__, __LINE__);
			}
		}
	}

	if(TMD2771X_BIT_ISSET(status, TMD2771X_STATUS_ADCINTR)){
		if(g_als_config.enabled){
			ret = tmd2771x_sfcommand(TMD2771X_CMD_ALS_INTCLR);
			if(ret == 0){
				tmd2771x_als_handle(channel_data);
			}else{
				printk(KERN_ERR "%s:failed at line %d\n", __func__, __LINE__);
			}
		}
	}

	return;
clear_int:
	ret = tmd2771x_sfcommand(TMD2771X_CMD_PROXALS_INTCLR);
	if(ret < 0){
		printk(KERN_ERR "%s:failed at line %d\n", __func__, __LINE__);
	}
}

static void tmd2771x_work_func(struct work_struct * work)
{
	struct tmd2771x_data *datap;

	//printk("+tmd2771x_work_func: enter+\n");
	datap = i2c_get_clientdata(tmd2771x_clientp);

	wake_lock(&datap->irq_wake_lock);

	//wait for all devices resume
	if(atomic_read(&suspend_state))
		printk("Interrupt occured in the suspend state");

	wait_event_timeout(tmd2771x_wait, atomic_read(&suspend_state) == 0, HZ/2);
	//wait_event(tmd2771x_wait, atomic_read(&suspend_state) == 0);

	tmd2771x_int_handle();

	/*we must use the input_sync for report one data available*/
	input_sync(datap->input_dev);

	wake_unlock(&datap->irq_wake_lock);

	//reenable the interrupt
	if ((g_prox_config.enabled ||g_als_config.enabled) && \
				(atomic_cmpxchg(&irq_enabled, 0, 1) == 0))
	{
		enable_irq(tmd2771x_clientp->irq);
	}
}

static irqreturn_t tmd2771x_irq_handler(int irq, void *dev_id)
{
	struct tmd2771x_data *datap;

	interruptCounter+=1;

	datap = i2c_get_clientdata(tmd2771x_clientp);

	/*In the interrupt handler must use the disable_irq_nosync to disbale the interrupt*/
	if (atomic_cmpxchg(&irq_enabled, 1, 0) == 1) {
		disable_irq_nosync(tmd2771x_clientp->irq);
	}

	wake_up(&tmd2771x_wait);
	queue_work(irq_workqueue, &datap->work);


	return IRQ_HANDLED;
}

/*TMD2771X misc driver*/
static int tmd2771x_misc_open(struct inode *inode, struct file *file)
{
	int ret = -1;
	unsigned char enable_reg = 0;
	struct tmd2771x_data *datap = i2c_get_clientdata(tmd2771x_clientp);

	int readcount;
	unsigned int prox_sum = 0;
	unsigned int prox_mean = 0;
	unsigned int prox_max = 0;
	unsigned int prox_diff = 0;
	unsigned short prox_threshold_hi = 0;
	unsigned short prox_threshold_lo = 0;

	ret = nonseekable_open(inode, file);
	if (ret)
		return ret;

	//calculate the prox threshold
	ret = tmd2771x_quick_configure();
	if(ret){
		goto failed;
	}

	//open the prox sensor
	enable_reg |= (TMD2771X_ENBL_PWRON | TMD2771X_ENBL_ADC_ENBL | \
					TMD2771X_ENBL_PROX_DET);

	ret = i2c_write_register(TMD2771X_REG_ENABLE, enable_reg);
	if(ret){
		goto failed;
	}

	/*get the prox data when nothing cover the glass
		it take 100ms time for data ready*/
	for(readcount = 0; readcount < 20; readcount++){
		unsigned char chdata[6];
		unsigned short proxdata = 0;
		unsigned short cleardata = 0;

		mdelay(13);
		ret = i2c_read_block(TMD2771X_REG_C0DATA, 6, chdata);
		if(ret){
			goto failed;
		}

		/*check whether the device is in the strong sun light
		    ortherwise we can't get the correct prox data      */
		cleardata = chdata[1];
		cleardata <<= 8;
		cleardata |= chdata[0];
		if (cleardata > ((g_als_config.saturation*80)/100))
				goto failed;

		//the prox data without no covery
		proxdata = chdata[5];
		proxdata <<= 8;
		proxdata |= chdata[4];

		if(prox_max < proxdata)
			prox_max = proxdata;
		prox_sum += proxdata;
	}

	prox_mean = prox_sum/20;

        if(prox_mean > (g_prox_config.saturation*80/100))
		goto failed;

	//we set the threshold:prox_mean+(1.7~2)*(prox_max - prox_mean)
        prox_diff = prox_max - prox_mean ;
        if( prox_diff < 50)
            prox_diff = 50;

        prox_threshold_hi = (((prox_diff * g_prox_config.thresh_gain_hi) + 50)/100) + prox_mean;
        prox_threshold_lo = (((prox_diff * g_prox_config.thresh_gain_lo) + 50)/100) + prox_mean;

        if(prox_threshold_hi > (g_prox_config.saturation*80/100))
		goto failed;

        g_prox_config.threshold_hi = g_prox_config.thresh_offset + prox_threshold_hi;
        g_prox_config.threshold_lo = g_prox_config.thresh_offset + prox_threshold_lo;

failed:
	tmd2771x_configure(); /* recover als atime reg =0xA5 config */
	i2c_write_register(TMD2771X_REG_ENABLE, 0x00);
	g_als_config.enabled = 0;
	g_prox_config.enabled = 0;

	datap->config_IsInitialized = 0;
	return 0;
}

static int tmd2771x_misc_release(struct inode *inode, struct file *file)
{
	struct tmd2771x_data *datap = i2c_get_clientdata(tmd2771x_clientp);

	i2c_write_register(TMD2771X_REG_ENABLE, 0x00);
	datap->config_IsInitialized = 0;

	return 0;
}

static long tmd2771x_misc_ioctl(struct file *file,
                                   unsigned int cmd, unsigned long arg)
{
	//update the sensor enable state
	switch (cmd) {
		case TAOS_IOCTL_ALS_ON:
			if(g_als_config.enabled)
				return 0;
			g_als_config.enabled = 1;
			break;
		case TAOS_IOCTL_ALS_OFF:
			if(!g_als_config.enabled)
				return 0;
			g_als_config.enabled = 0;
			break;
		case TAOS_IOCTL_PROX_ON:
			if(g_prox_config.enabled)
				return 0;
			g_prox_config.enabled = 1;
			break;
		case TAOS_IOCTL_PROX_OFF:
			if(!g_prox_config.enabled)
				return 0;
			g_prox_config.enabled = 0;
			break;
	}

	tmd2771x_enabledisable();

	return 0;
}

static const struct file_operations tmd2771x_misc_fops = {
	.owner = THIS_MODULE,
	.open = tmd2771x_misc_open,
	.release = tmd2771x_misc_release,
	.unlocked_ioctl = tmd2771x_misc_ioctl,
};

static struct miscdevice tmd2771x_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "taos",
	.fops = &tmd2771x_misc_fops,
};

static ssize_t attr_set_pulsecount(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	/*because the pulse count register is 8-bit register,
		so the largest the number is 255*/
	if(val > 255)
		val = 255;

	mutex_lock(&g_control_mutex);
	g_prox_config.pulse_count = (unsigned char)val;

	i2c_write_register(TMD2771X_REG_PPCOUNT, g_prox_config.pulse_count);
	mutex_unlock(&g_control_mutex);

	return size;
}

static ssize_t attr_get_pulsecount(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	return sprintf(buf, "prox pulse count:%d\n", g_prox_config.pulse_count);
}

static ssize_t attr_set_leddrive(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long val;
	unsigned char reg_val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	/*the led driver has 4 different configuration*/
	if(val > 3)
		return -EINVAL;

	mutex_lock(&g_control_mutex);
	g_prox_config.led_drive =(( (unsigned char)val) << 6) & PROX_LED_DRIVE_MASK;

	reg_val = g_prox_config.led_drive | g_prox_config.channel | g_als_config.gain;
	i2c_write_register(TMD2771X_REG_CONTROL, reg_val);
	mutex_unlock(&g_control_mutex);

	return size;
}

static ssize_t attr_get_leddrive(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	ssize_t ret;
	int leddrive_index = 0;

	switch(g_prox_config.led_drive){
		case PROX_LED_DRIVE_100MA:
			leddrive_index = 0;
			break;
		case PROX_LED_DRIVE_50MA:
			leddrive_index = 1;
			break;
		case PROX_LED_DRIVE_25MA:
			leddrive_index = 2;
			break;
		case PROX_LED_DRIVE_12P5MA:
			leddrive_index = 3;
			break;
	}

	ret = sprintf(buf, "Led drive:%s\n", leddrive_strs[leddrive_index]);
	return ret;
}

static ssize_t attr_set_proxenable(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	mutex_lock(&g_control_mutex);
	if(val){
		g_prox_config.enabled = 1;
	}else{
		g_prox_config.enabled = 0;
	}

	tmd2771x_enabledisable();
	mutex_unlock(&g_control_mutex);
	return size;
}

static ssize_t attr_get_proxenable(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	return sprintf(buf, "%s\n", g_prox_config.enabled? "on": "off");
}


static ssize_t attr_set_alsenable(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	mutex_lock(&g_control_mutex);
	if(val){
		g_als_config.enabled = 1;
	}else{
		g_als_config.enabled = 0;
	}

	tmd2771x_enabledisable();
	mutex_unlock(&g_control_mutex);

	return size;
}

static ssize_t attr_get_alsenable(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	return sprintf(buf, "%s\n", g_als_config.enabled? "on": "off");
}

static ssize_t attr_set_debugfs_mode(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	mutex_lock(&g_control_mutex);
	if(val){
		taos_debugfs_mode = 1;
	}else{
		taos_debugfs_mode = 0;
	}
	mutex_unlock(&g_control_mutex);

	return size;
}

static ssize_t attr_get_debugfs_mode(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	return sprintf(buf, "%s\n", taos_debugfs_mode? "Prox calculate mode": "Register and Configs mode");
}
static ssize_t attr_set_thresh_gain_hi(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	if(val < 100 || val > 3000)
		return -EINVAL;

	if(val <= g_prox_config.thresh_gain_lo)
		return -EINVAL;

	mutex_lock(&g_control_mutex);
	g_prox_config.thresh_gain_hi = (unsigned short)val;
	prox_thresh_reset = 1;
	mutex_unlock(&g_control_mutex);

	return size;
}

static ssize_t attr_get_thresh_gain_hi(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	return sprintf(buf, "%d\n", g_prox_config.thresh_gain_hi);
}

static ssize_t attr_set_thresh_gain_lo(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	if(val < 100 || val > 3000)
		return -EINVAL;

	if(val >=  g_prox_config.thresh_gain_hi)
		return -EINVAL;

	mutex_lock(&g_control_mutex);
	g_prox_config.thresh_gain_lo = (unsigned short)val;
	prox_thresh_reset = 1;
	mutex_unlock(&g_control_mutex);

	return size;
}

static ssize_t attr_get_thresh_gain_lo(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	return sprintf(buf, "%d\n", g_prox_config.thresh_gain_lo);
}

static ssize_t attr_set_thresh_offset(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	if(val < 0 || val > 3000)
		return -EINVAL;

	mutex_lock(&g_control_mutex);
	g_prox_config.thresh_offset = (unsigned short)val;
	prox_thresh_reset = 1;
	mutex_unlock(&g_control_mutex);

	return size;
}

static ssize_t attr_get_thresh_offset(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	return sprintf(buf, "%d\n", g_prox_config.thresh_offset);
}

static ssize_t attr_set_scale_up(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	mutex_lock(&g_control_mutex);
	g_als_config.scale_up = (unsigned short)val;
	mutex_unlock(&g_control_mutex);

	return size;
}

static ssize_t attr_get_scale_up(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	return sprintf(buf, "%d\n", g_als_config.scale_up);
}

static ssize_t attr_set_scale_down(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	if(val == 0)
		return -EINVAL;

	mutex_lock(&g_control_mutex);
	g_als_config.scale_down = (unsigned short)val;
	mutex_unlock(&g_control_mutex);

	return size;
}

static ssize_t attr_get_scale_down(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	return sprintf(buf, "%d\n", g_als_config.scale_down);
}

static ssize_t attr_set_als_gain(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long val;
	unsigned char reg_val;
	int i;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	for(i = 0; i < 4; i++){
		if(val == als_gain_table[i])
			break;
	}

	if(i == 4)
		return -EINVAL;

	mutex_lock(&g_control_mutex);
	g_als_config.gain = i;

	reg_val = g_prox_config.led_drive | g_prox_config.channel | g_als_config.gain;
	i2c_write_register(TMD2771X_REG_CONTROL, reg_val);
	mutex_unlock(&g_control_mutex);

	return size;
}

static ssize_t attr_get_als_gain(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	return sprintf(buf, "als channel gain:%dX\n", \
		als_gain_table[(g_als_config.gain & ALS_GAIN_MASK)]);
}

static ssize_t attr_set_alsreport(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long val;
	struct tmd2771x_data *datap;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	datap = i2c_get_clientdata(tmd2771x_clientp);
	input_report_abs(datap->input_dev, ABS_MISC, val);
	input_sync(datap->input_dev);

	return size;
}

static ssize_t attr_set_proxreport(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long val;
	struct tmd2771x_data *datap;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	datap = i2c_get_clientdata(tmd2771x_clientp);
	input_report_abs(datap->input_dev, ABS_DISTANCE, val);
	input_sync(datap->input_dev);

	return size;
}

static struct device_attribute attributes[] = {
	__ATTR(als_gain, 0600, attr_get_als_gain, attr_set_als_gain),
	__ATTR(pulsecount, 0600, attr_get_pulsecount, attr_set_pulsecount),
	__ATTR(leddrive, 0600, attr_get_leddrive, attr_set_leddrive),
	__ATTR(prox_enable, 0600, attr_get_proxenable, attr_set_proxenable),
	__ATTR(als_enable, 0600, attr_get_alsenable, attr_set_alsenable),
	__ATTR(debugfs_mode, 0600, attr_get_debugfs_mode, attr_set_debugfs_mode),
	__ATTR(tgh, 0600, attr_get_thresh_gain_hi, attr_set_thresh_gain_hi),
	__ATTR(tgl, 0600, attr_get_thresh_gain_lo, attr_set_thresh_gain_lo),
	__ATTR(tof, 0600, attr_get_thresh_offset, attr_set_thresh_offset),
	__ATTR(scale_up, 0600, attr_get_scale_up, attr_set_scale_up),
	__ATTR(scale_down, 0600, attr_get_scale_down, attr_set_scale_down),
	__ATTR(alsreport, 0200, NULL, attr_set_alsreport),
	__ATTR(proxreport, 0200, NULL, attr_set_proxreport),
};

static int create_sysfs_interfaces(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		if (device_create_file(dev, attributes + i))
			goto error;
	return 0;

error:
	for ( ; i >= 0; i--)
		device_remove_file(dev, attributes + i);
	dev_err(dev, "%s:Unable to create interface\n", __func__);
	return -1;
}

static int remove_sysfs_interfaces(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		device_remove_file(dev, attributes + i);
	return 0;
}

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#include <linux/seq_file.h>
static void print_regs(const char *header, struct seq_file *s,
		int start_addr, int nums)
{
	unsigned char regs[4];
	int reg_item;
	int ret;

	if(nums >4)
		return;

	seq_printf(s, "%s:\n", header);
	if(nums == 1){
		ret = i2c_read_register(start_addr, regs);
		if(ret >= 0)
			seq_printf(s, " Reg 0x%02x Value 0x%02x\n", start_addr, regs[0]);
	}else{
		ret = i2c_read_block(start_addr, nums, regs);
		if(ret >=0){
			for(reg_item = 0; reg_item < nums; reg_item++){
				seq_printf(s, " Reg 0x%02x Value 0x%02x\n", start_addr+reg_item, regs[reg_item]);
			}
		}
	}
}

static void dbg_reg_config_show(struct seq_file *s)
{
	unsigned short chdata[3];
	int ret;

	seq_printf(s, "TMD2771x Registers\n");

	print_regs("ENABLE", s, 0x00, 1);
	print_regs("ATIME", s, 0x01, 1);
	print_regs("PTIME", s, 0x02, 1);
	print_regs("WTIME", s, 0x03, 1);
	print_regs("ALS threshold regs", s, 0x04, 4);
	print_regs("PROX threshold regs", s, 0x08, 4);
	print_regs("PERS", s, 0x0C, 1);
	print_regs("CONFIG", s, 0x0D, 1);
	print_regs("PPCOUNT", s, 0x0E, 1);
	print_regs("CONTROL", s, 0x0F, 1);
	print_regs("ID", s, 0x12, 1);
	print_regs("STATUS", s, 0x13, 1);
	//print_regs("ALS channel regs", s, 0x14, 4);
	//print_regs("PROX channel regs", s, 0x18, 2);
	ret = i2c_read_block(0x14, 6, (unsigned char *)chdata);
	if(ret >=0){
		seq_printf(s, "Channel data:\n");
		seq_printf(s, " Ch0=%d, Ch1=%d\n", chdata[0], chdata[1]);
		seq_printf(s, " Prox=%d\n", chdata[2]);
	}

	seq_printf(s, "\n");
	seq_printf(s, "TMD2771x config variables\n");

	seq_printf(s, "ALS configs:\n");
	seq_printf(s, " enabled:%d\n", g_als_config.enabled);
	seq_printf(s, " adc_time:0x%02x(%d.%d ms)\n", g_als_config.adc_time, \
					(int)(256 - g_als_config.adc_time) * 272 / 100, \
					(int)(256 - g_als_config.adc_time) * 272 % 100);
	if(g_als_config.gain<4)
		seq_printf(s, " gain:0x%02x(%dX)\n", g_als_config.gain, \
						als_gain_table[g_als_config.gain]);
	seq_printf(s, " int_pers:0x%02x(%d)\n", g_als_config.int_pers, g_als_config.int_pers >> 4);
	seq_printf(s, " saturation:%d\n", g_als_config.saturation);
	seq_printf(s, " scale_factor:%d\n", g_als_config.scale_factor);
	seq_printf(s, " threshold_hi:%d\n", g_als_config.threshold_hi);
	seq_printf(s, " threshold_lo:%d\n", g_als_config.threshold_lo);

	seq_printf(s, "PROX configs:\n");
	seq_printf(s, " enabled:%d\n", g_prox_config.enabled);
	seq_printf(s, " adc_time:0x%02x(%d.%d ms)\n", g_prox_config.adc_time, \
					(int)(256 - g_prox_config.adc_time) *  272 / 100, \
					(int)(256 - g_prox_config.adc_time) *  272 % 100);
	seq_printf(s, " channel:0x%02x(%s)\n", g_prox_config.channel, \
						proxchannels_strs[(g_prox_config.channel & PROX_CHANNEL_MASK)>>4]);
	seq_printf(s, " int_pers:0x%02x(%d)\n", g_prox_config.int_pers, \
		(g_prox_config.int_pers < 4)?g_prox_config.int_pers:((g_prox_config.int_pers-3)*5));
	seq_printf(s, " led_drive:0x%02x(%s)\n", g_prox_config.led_drive, \
					leddrive_strs[(g_prox_config.led_drive >> 6)]);
	seq_printf(s, " pulse_count:%d\n", g_prox_config.pulse_count);
	seq_printf(s, " saturation:%d\n", g_prox_config.saturation);
	seq_printf(s, " thresh_offset:%d\n", g_prox_config.thresh_offset);
	seq_printf(s, " threshold_hi:%d\n", g_prox_config.threshold_hi);
	seq_printf(s, " threshold_lo:%d\n", g_prox_config.threshold_lo);
	seq_printf(s, " distance:%d\n", prox_distance);

	seq_printf(s, "Interrupt enable state:%d\n", atomic_read(&irq_enabled));
	seq_printf(s, "Interrupt counter:%d\n", interruptCounter);
}

static void dbg_prox_cal_show(struct seq_file *s)
{
	int ret;
	unsigned char enable_reg;
	int readcount;
	unsigned int prox_sum = 0;
	unsigned int prox_mean = 0;
	unsigned int prox_max = 0;
	unsigned short prox_threshold_hi = 0;
	unsigned short prox_threshold_lo = 0;

	struct tmd2771x_data *datap = i2c_get_clientdata(tmd2771x_clientp);

	//disable the irq
	if (atomic_cmpxchg(&irq_enabled, 1, 0) == 1) {
		disable_irq(tmd2771x_clientp->irq);
	}

	//calculate the prox threshold
	ret = tmd2771x_configure();
	if(ret){
		seq_printf(s, "configure register failed(%d)\n", ret);
		goto failed;
	}

	//open the prox sensor
	enable_reg = (TMD2771X_ENBL_PWRON | TMD2771X_ENBL_ADC_ENBL | \
					TMD2771X_ENBL_PROX_DET);

	ret = i2c_write_register(TMD2771X_REG_ENABLE, enable_reg);
	if(ret){
		seq_printf(s, "write ENABLE register failed\n");
		goto failed;
	}

	/*get the prox data when nothing cover the glass
		it take 100ms time for data ready*/
	for(readcount = 0; readcount < 20; readcount++){
		unsigned char chdata[6];
		unsigned short proxdata = 0;
		unsigned short cleardata = 0;

		mdelay(220);
		ret = i2c_read_block(TMD2771X_REG_C0DATA, 6, chdata);
		if(ret){
			seq_printf(s, "Read channel data register failed\n");
			goto failed;
		}

		/*check whether the device is in the strong sun light
		    ortherwise we can't get the correct prox data      */
		cleardata = chdata[1];
		cleardata <<= 8;
		cleardata |= chdata[0];
		if (cleardata > ((g_als_config.saturation*80)/100)){
			seq_printf(s, "Clear data exceed the 80%% of als saturation\n");
			goto failed;
		}

		//the prox data without no covery
		proxdata = chdata[5];
		proxdata <<= 8;
		proxdata |= chdata[4];

		seq_printf(s, "index %02d: prox data=%d\n", readcount, proxdata);

		if(prox_max < proxdata)
			prox_max = proxdata;
		prox_sum += proxdata;
	}

	prox_mean = prox_sum/20;

	seq_printf(s, "prox_mean=%d prox_max=%d diff=%d\n", prox_mean, prox_max, \
														(prox_max-prox_mean));
        if(prox_mean > (g_prox_config.saturation*80/100)) {
		seq_printf(s, "prox_mean(%d) exceed the 80%% of prox saturation(%d)\n", \
					prox_mean, g_prox_config.saturation);
		goto failed;
        }

	//we set the threshold:(default)prox_mean+(1.7~2)*(prox_max - prox_mean)
        prox_threshold_hi = ((((prox_max - prox_mean) * g_prox_config.thresh_gain_hi) + 50)/100) + prox_mean;
        prox_threshold_lo = ((((prox_max - prox_mean) * g_prox_config.thresh_gain_lo) + 50)/100) + prox_mean;

        seq_printf(s, "prox_threshold_hi=%d prox_threshold_lo=%d\n", \
							prox_threshold_hi, prox_threshold_lo);

        if(prox_threshold_hi > (g_prox_config.saturation*80/100)){
		seq_printf(s, "prox_threshold_hi(%d) exceed the 80%% of prox saturation(%d)\n", \
					prox_threshold_hi, g_prox_config.saturation);
        }
	 /*after set he thres_gain_hi/lo variables we should update the prox threshold*/
	 if(prox_thresh_reset){
		g_prox_config.threshold_hi =  g_prox_config.thresh_offset + prox_threshold_hi;
		g_prox_config.threshold_lo =  g_prox_config.thresh_offset + prox_threshold_lo;
		prox_thresh_reset = 0;
	 }
failed:
	datap->config_IsInitialized = 0;
	tmd2771x_enabledisable();
}

static int dbg_taos_show(struct seq_file *s, void *unused)
{
	if(taos_debugfs_mode){
		dbg_prox_cal_show(s);
	}else{
		dbg_reg_config_show(s);
	}

	return 0;
}

static int dbg_taos_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_taos_show, inode->i_private);
}

static const struct file_operations debug_fops = {
	.open		= dbg_taos_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void __init tmd2771x_debuginit(void)
{
	(void)debugfs_create_file("tmd2771x", S_IRUGO, NULL,
			NULL, &debug_fops);
}
#else
static void __init tmd2771x_debuginit(void)
{
	return;
}
#endif

#ifdef CONFIG_PM
static int tmd2771x_pm_notify(struct notifier_block *nb,
			unsigned long event, void *nouse)
{
	struct tmd2771x_data *datap;

	printk("%s(%ld):%d-%d\n", __func__, event, g_prox_config.enabled, g_als_config.enabled);

	datap = i2c_get_clientdata(tmd2771x_clientp);

	cancel_work_sync(&datap->work);
	if(in_call_state()) {
		switch (event) {
			case PM_SUSPEND_PREPARE:
				i2c_write_register(TMD2771X_REG_ENABLE, TMD2771X_PROX_ON);
				atomic_set(&suspend_state, 1);
				wake_up(&tmd2771x_wait);
				break;
			case PM_POST_SUSPEND:
				tmd2771x_update_control();
				atomic_set(&suspend_state, 0);
				wake_up(&tmd2771x_wait);
				break;
			default:
				printk("%s:wrong pm event\n", __func__);
		}

		return NOTIFY_OK;
	}

	switch (event) {
		case PM_SUSPEND_PREPARE:
			if (atomic_cmpxchg(&irq_enabled, 1, 0) == 1) {
				disable_irq(tmd2771x_clientp->irq);
			}
			i2c_write_register(TMD2771X_REG_ENABLE, 0x00);
			break;
		case PM_POST_SUSPEND:
			tmd2771x_enabledisable();
			break;
		default:
			printk("%s:wrong pm event\n", __func__);
	}

	return NOTIFY_OK;
}

static struct notifier_block tmd2771x_notify = {
	.notifier_call = tmd2771x_pm_notify,
};
#endif
/*TMD2771X I2C driver*/
static int tmd2771x_probe(struct i2c_client *clientp, const struct i2c_device_id *dev_id)
{
	int ret = 0;
	int count = 0;
	unsigned char chip_id;
	unsigned int als_maxcount;
	unsigned int prox_maxcount;
	struct tmd2771x_platform_data *pdata;
	struct tmd2771x_data *datap;

	tmd2771x_clientp = clientp;

	//i2c function check
	if (!i2c_check_functionality(clientp->adapter, I2C_FUNC_I2C)) {
		ret = -1;
		goto failed1;
	}

	//get the platform data
	pdata =  (struct tmd2771x_platform_data *) clientp->dev.platform_data;
	if (!pdata) {
		printk(KERN_ERR "%s|%d: Missing platform data\n", __func__, __LINE__);
		ret = -2;
		goto failed1;
	}

	/*power on the device
	 we change the power on operation to the regulater init stage for the power stable
	 and save the driver init time. we reenable the sensor power for increase the
	 regulater enable use_count.*/
	if(pdata->power_control)
		pdata->power_control(1);

	//device check
	count = 3;
	while(count>0)
	{
	ret = i2c_read_register(TMD2771X_REG_ID, &chip_id);
		if((ret == 0) &&(chip_id == TMD2771X_DEVID_TMD27713))
		{
			printk(KERN_INFO "%s|%d: ID check successful!\n", __func__, __LINE__);
			break;
		}
		count--;
		printk("%s: i2c_read_register ID check failed retry count=%d \n", __func__, (3-count));
		mdelay(200);
	}
	if((ret < 0) ||(chip_id != TMD2771X_DEVID_TMD27713)){

		printk(KERN_ERR "%s|%d: ID check failed\n", __func__, __LINE__);
		ret = -3;
		goto failed1;
	}

	//disable the sensor
	i2c_write_register(TMD2771X_REG_ENABLE, 0x00);

	mutex_init(&g_control_mutex);

	//module config
	if(pdata->als_config){
		g_als_config.gain = pdata->als_config->gain;
		g_als_config.threshold_lo = pdata->als_config->threshold_lo;
		g_als_config.threshold_hi = pdata->als_config->threshold_hi;
	}

	if(pdata->prox_config){
		g_prox_config.pulse_count = pdata->prox_config->pulse_count;
		g_prox_config.led_drive = pdata->prox_config->led_drive;
		g_prox_config.threshold_lo = pdata->prox_config->threshold_lo;
		g_prox_config.threshold_hi = pdata->prox_config->threshold_hi;
		g_prox_config.thresh_offset = pdata->prox_config->thresh_offset;
		g_prox_config.thresh_gain_hi = pdata->prox_config->thresh_gain_hi;
		g_prox_config.thresh_gain_lo= pdata->prox_config->thresh_gain_lo;
	}

	//initalize the sensor config data
	if(integration_time < 50)
		integration_time = 50;
	if(integration_time > 650)
		integration_time = 650;
	integration_time = (integration_time + 25)/50 *50;

	g_als_config.adc_time =256 - (integration_time / 50 * 18 + 1);

	//calculate the saturation
	als_maxcount = (256 - g_als_config.adc_time) * 1024;
	if(als_maxcount > 65535)
		als_maxcount = 65535;
	g_als_config.saturation = als_maxcount;

	prox_maxcount =(256 - g_prox_config.adc_time) * 1024;
	if(prox_maxcount > 65535)
		prox_maxcount = 65535;
	g_prox_config.saturation = prox_maxcount;

	//initialize the driver data
	//allocate the memory for tmd2771x_data
	datap = kmalloc(sizeof(struct tmd2771x_data), GFP_KERNEL);
	if (!datap) {
		printk(KERN_ERR "%s|%d: kmalloc for struct tmd2771x_data failed\n", __func__, __LINE__);
		ret = -4;
		goto failed1;
	}

	i2c_set_clientdata(clientp, datap);
	datap->client = clientp;

	//configure the interrupt
	wake_lock_init(&datap->irq_wake_lock, WAKE_LOCK_SUSPEND, "tmd2771x");
	irq_workqueue = create_singlethread_workqueue("tmd2771x");
	INIT_WORK(&(datap->work),tmd2771x_work_func);
	datap->shutdown_complete =0;

	/*we must ensure the int pin level is high before request_irq is called,
	otherwise the soft lockup may occur in the request_irq*/
	ret = tmd2771x_sfcommand(TMD2771X_CMD_PROXALS_INTCLR);
	if(ret < 0){
		printk(KERN_ERR "%s|%d: clear the interrupt failed\n", __func__, __LINE__);
		ret = -5;
		goto failed2;
	}

	//config the irq pin as gpio input mode
	if(pdata->irq_gpio_setup){
		pdata->irq_gpio_setup();
	}

	ret = request_irq(clientp->irq, tmd2771x_irq_handler, IRQ_TYPE_LEVEL_LOW, "taos_irq", datap);
	if (ret != 0) {
		printk(KERN_ERR "%s|%d: register interrupt failed\n", __func__, __LINE__);
		ret = -6;
		goto failed2;
	}
	disable_irq_nosync(clientp->irq);

	ret = enable_irq_wake(tmd2771x_clientp->irq);
	if (ret < 0)
		printk("TMD2771x wake-up event registration"
					"failed with eroor: %d\n", ret);

	atomic_set(&irq_enabled, 0);
	atomic_set(&suspend_state, 0);

	//register the tmd2771x miscdevice
	ret = misc_register(&tmd2771x_misc_device);
	if (ret) {
		printk(KERN_ERR "%s|%d: register miscdevice failed\n", __func__, __LINE__);
		ret = -7;
		goto failed3;
	}

	//initialize the input device
	datap->input_dev = input_allocate_device();
	if (datap->input_dev == NULL) {
		printk(KERN_ERR "%s|%d: input_allocate_device failed\n", __func__, __LINE__);
		ret = -8;
		goto failed4;
	}

	datap->input_dev->name = "taos";
	datap->input_dev->id.bustype = BUS_I2C;
	datap->input_dev->dev.parent = &clientp->dev;

	set_bit(EV_ABS,datap->input_dev->evbit);
	input_set_capability(datap->input_dev,EV_ABS,ABS_MISC);
	input_set_capability(datap->input_dev,EV_ABS,ABS_DISTANCE);
	input_set_abs_params(datap->input_dev, ABS_MISC, INT_MIN, INT_MAX, 0, 0);
	input_set_abs_params(datap->input_dev, ABS_DISTANCE, INT_MIN, INT_MAX, 0, 0);
	ret = input_register_device(datap->input_dev);
	if( 0 != ret )
	{
		printk(KERN_ERR "%s|%d: input_register_device failed\n", __func__, __LINE__);
		ret = -9;
		goto failed4;
	}

	input_set_drvdata(datap->input_dev, datap);

	ret = create_sysfs_interfaces(&clientp->dev);
	if( 0 != ret )
	{
		printk(KERN_ERR "%s|%d: create_sysfs_interfaces failed\n", __func__, __LINE__);
		ret = -10;
		goto failed5;
	}

	tmd2771x_debuginit();

#ifdef CONFIG_PM
	/* Register PM notifier. */
	register_pm_notifier(&tmd2771x_notify);
#endif

	return 0;

failed5:
	input_unregister_device(datap->input_dev);
failed4:
	misc_deregister(&tmd2771x_misc_device);
failed3:
	free_irq(clientp->irq, datap);
failed2:
	kfree(datap);
failed1:
	return ret;
}

static int tmd2771x_remove(struct i2c_client *clientp)
{
	struct tmd2771x_data *datap = i2c_get_clientdata(tmd2771x_clientp);
#ifdef CONFIG_PM
	/* Register PM notifier. */
	unregister_pm_notifier(&tmd2771x_notify);
#endif
	disable_irq_wake(clientp->irq);
	disable_irq(clientp->irq);
	free_irq(clientp->irq, datap);

	remove_sysfs_interfaces(&clientp->dev);
	input_unregister_device(datap->input_dev);
	misc_deregister(&tmd2771x_misc_device);
	kfree(datap);

	return 0;
}

static int tmd2771x_suspend(struct device *dev)
{
#ifdef CONFIG_TMD2771_SUSPEND_POWEROFF
	struct i2c_client *clientp = to_i2c_client(dev);
	struct tmd2771x_platform_data *pdata;
	struct tmd2771x_data *datap;
#endif

	printk("%s:%d-%d\n", __func__, g_prox_config.enabled, g_als_config.enabled);

	if(in_call_state()) {
		return 0;
	}

#ifdef CONFIG_TMD2771_SUSPEND_POWEROFF
	pdata = (struct tmd2771x_platform_data *) clientp->dev.platform_data;
	datap = i2c_get_clientdata(tmd2771x_clientp);

	if(pdata->power_control)
		pdata->power_control(0);

	datap->config_IsInitialized = 0;
#endif
	return 0;
}

static int tmd2771x_resume(struct device *dev)
{
#ifdef CONFIG_TMD2771_SUSPEND_POWEROFF
	struct i2c_client *clientp = to_i2c_client(dev);
	struct tmd2771x_platform_data *pdata;
#endif

	printk("%s:%d-%d\n", __func__, g_prox_config.enabled, g_als_config.enabled);

	if(in_call_state()) {
		return 0;
	}

#ifdef CONFIG_TMD2771_SUSPEND_POWEROFF
	pdata = (struct tmd2771x_platform_data *) clientp->dev.platform_data;

	if(pdata->power_control)
		pdata->power_control(1);
#endif

	return 0;
}

static void tmd2771x_shutdown(struct i2c_client *client)
{
	struct tmd2771x_data *datap = i2c_get_clientdata(client);

	mutex_lock(&g_control_mutex);
	datap->shutdown_complete = 1;
	mutex_unlock(&g_control_mutex);
}

static const struct i2c_device_id tmd2771x_id[] = {
	{ "tmd2771x", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tmd2771x_id);

static struct dev_pm_ops tmd2771x_pm = {
	.suspend = tmd2771x_suspend,
	.resume = tmd2771x_resume,
};

static struct i2c_driver tmd2771x_driver = {
	.driver = {
		.name	= "tmd2771x",
		.owner	= THIS_MODULE,
		.pm = &tmd2771x_pm,
	},
	.probe	= tmd2771x_probe,
	.remove	= __devexit_p(tmd2771x_remove),
	.shutdown = tmd2771x_shutdown,
	.id_table = tmd2771x_id,
};

static int __init tmd2771x_init(void)
{
	return i2c_add_driver(&tmd2771x_driver);
}

static void __exit tmd2771x_exit(void)
{
	i2c_del_driver(&tmd2771x_driver);
}


module_init(tmd2771x_init);
module_exit(tmd2771x_exit);

MODULE_AUTHOR("Poyuan Lu - ZTE Corporation");
MODULE_DESCRIPTION("TAOS ambient light and proximity sensor driver");
MODULE_LICENSE("GPL");

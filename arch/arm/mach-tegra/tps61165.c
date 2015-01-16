/*****************************************************************************
*Copyright(C) 2012-2020,ZTE Corporation
*
*  Module Name £ºtps61165
*  File Name   £ºtps61165.c
*  Description £ºcontrol backlight on IC tps61165 in 1-wire mode
*  Author      £ºtong.weili
*  Version     £ºV0.0.1
*  Data        £º2013-02-17
*  Others      £º
*  Revision Details1£º
*     Modify Data£º
*     Version£º
*     Author£º
*     Modification£º
*  Revision Details2£º
*****************************************************************************/
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include "gpio-names.h"

static int debug = 0;
module_param(debug, int, 0600);
static spinlock_t s_tps61165_lock;
static bool s_tps61165_is_inited = true;
static bool s_bNeedSetBacklight = false;
static int tps61165_init(void);
static int tps61165_write_bit(u8 b);
static int tps61165_write_byte(u8 bytedata);
static int tps61165_config_ES_timing(void);
static int tps61165_shutdown(void);
int tps61165_set_backlight(int brightness);
void tps61165_pin_init(void);

#define DEFAULT_BRIGHTNESS (132) 
#define TPS61165_CTRL_PIN		TEGRA_GPIO_PH1 

#define TPS61165_DEVICE_ADDR  (0x72)
#define BIT_DELAY_UNIT (3)
#define LOGIC_FACTOR (3)
#define CONDITION_DELAY (3)
#define ES_DETECT_DELAY  (200) 
#define ES_DETECT_TIME  (300) 
#define ES_TIMING_WINDOW  (1000)
#define TPS61165_DELAY(n) udelay(n)

static int tps61165_write_bit(u8 b)
{
    if(1 == b)
    {
        gpio_set_value(TPS61165_CTRL_PIN, 0);
        TPS61165_DELAY(BIT_DELAY_UNIT);
        gpio_set_value(TPS61165_CTRL_PIN, 1);
        TPS61165_DELAY(LOGIC_FACTOR*BIT_DELAY_UNIT);
        //gpio_set_value(TPS61165_CTRL_PIN, 0);
    }
    else if(0 == b)
    {
        gpio_set_value(TPS61165_CTRL_PIN, 0);
        TPS61165_DELAY(LOGIC_FACTOR*BIT_DELAY_UNIT);
        gpio_set_value(TPS61165_CTRL_PIN, 1);
        TPS61165_DELAY(BIT_DELAY_UNIT);
        //gpio_set_value(TPS61165_CTRL_PIN, 0);
    }
    else
    {
        printk("[tong]:tps61165_write_bit: error param!\n");
        return -1;
    }
    return 0;
}

static int tps61165_write_byte(u8 bytedata)
{
    u8 bit_cnt = 8;
    u8 val = bytedata;
    int ret = 0;
    unsigned long flags;
    
    spin_lock_irqsave(&s_tps61165_lock, flags);
    gpio_set_value(TPS61165_CTRL_PIN, 1);
    TPS61165_DELAY(CONDITION_DELAY); //Start condition,  at least 2us

    bit_cnt = 8;
		
    while(bit_cnt)
    {
        bit_cnt--;
        if((val >> bit_cnt) & 1)
        {          
            ret = tps61165_write_bit(1);
        }
        else
        {
            ret = tps61165_write_bit(0);
        } 

        if(ret)
        {
            printk("[tong]:tps61165_write_byte:failed!\n");
            spin_unlock_irqrestore(&s_tps61165_lock, flags);
            return ret;
        }     
    }

    gpio_set_value(TPS61165_CTRL_PIN, 0);
    TPS61165_DELAY(CONDITION_DELAY); //EOS condition, at least 2us
    gpio_set_value(TPS61165_CTRL_PIN, 1);
    
    spin_unlock_irqrestore(&s_tps61165_lock, flags);
    return 0;
}

static int tps61165_config_ES_timing(void)
{
    unsigned long flags;

    if(debug)
    {
        printk("[tong]:tps61165_config_ES_timing\n");
    }
    
    spin_lock_irqsave(&s_tps61165_lock, flags); 
    
    gpio_set_value(TPS61165_CTRL_PIN, 1);  //start ES Timing Window
    TPS61165_DELAY(ES_DETECT_DELAY); //at least 100us

    gpio_set_value(TPS61165_CTRL_PIN, 0);
    TPS61165_DELAY(ES_DETECT_TIME); //at least 260us

    gpio_set_value(TPS61165_CTRL_PIN, 1);
    TPS61165_DELAY(ES_TIMING_WINDOW - ES_DETECT_DELAY - ES_DETECT_TIME);

    spin_unlock_irqrestore(&s_tps61165_lock, flags);
    return 0;
}

static int tps61165_shutdown(void)
{
    if(debug)
    {
        printk("[tong]:tps61165_shutdown\n");
    }
    
    gpio_set_value(TPS61165_CTRL_PIN, 0);
    mdelay(3); //enter shutdown mode, at least 2.5ms
    return 0;
}

static int tps61165_init(void)
{
    tps61165_shutdown();
    tps61165_config_ES_timing();
    return 0;
}

int tps61165_set_backlight(int brightness)
{
    u8 tps61165_level;
    static u8 old_level = -1;
 
    if(DEFAULT_BRIGHTNESS == brightness)
    {
        if(!s_bNeedSetBacklight)
        {
            s_bNeedSetBacklight = true;
            return 0;
        }      
    }
    
    tps61165_level = (brightness & 0xFF) >> 3;/*convert level 0~255  to  0~31*/

    if(old_level == tps61165_level)
    {
        //printk("[tong]:tps61165_set_backlight: the same level as before, nothing done!level=%d\n", tps61165_level);
        return 0;
    }

    if(debug)
    {
        printk("[tong]:tps61165_set_backlight: brightness=%d, tps61165_level=%d\n", brightness, tps61165_level);
    }

    if(tps61165_level)
    {
        if(!s_tps61165_is_inited)
        {
            tps61165_init();
            s_tps61165_is_inited = true;
        }      
    }
    else
    {
        tps61165_shutdown();
        old_level = tps61165_level;
        s_tps61165_is_inited = false;
        return 0;
    }

    tps61165_write_byte(TPS61165_DEVICE_ADDR);
    tps61165_write_byte(tps61165_level);

    old_level = tps61165_level;
    return 0;    
}

void tps61165_pin_init(void)
{
      //tegra_gpio_enable(TPS61165_CTRL_PIN);  
      gpio_request(TPS61165_CTRL_PIN,  "lcd_backlight");
      gpio_direction_output(TPS61165_CTRL_PIN, 1);
      s_tps61165_is_inited = false;
      spin_lock_init(&s_tps61165_lock);
}
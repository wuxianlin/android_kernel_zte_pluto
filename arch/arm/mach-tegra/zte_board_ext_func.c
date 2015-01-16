/********************************************************************
added by pengtao for zte ext functions
***********************************************************************/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include "board.h"
#include <asm/uaccess.h>


//static int sdw_fmstat=0xFF;
extern unsigned int g_zte_board_id;

int zte_get_board_id(void);
#ifdef CONFIG_ZTE_PROP_BRIDGE
extern int prop_add( char *devname, char *item, char *value);
#endif

static ssize_t zte_read_hver(char *page, char **start, off_t off,
		int count, int *eof, void *data)
{
	int board_id = -1;
	int len = 0;	
	
    board_id = zte_get_board_id();
    printk("zte_read_hver board_id=%d \n",board_id);
    switch (board_id)
    {
        case 0:
            snprintf(page ,5,"tz1A");
            len = 5;
            break;

        case 1:
            snprintf(page ,5,"tdeA");
            len = 5;
            break;            

        case 2:
            snprintf(page ,5,"tdeA");
	      len = 5;
            break;            

        case 3:
            snprintf(page ,5,"tdeA");
            len = 5;
            break;            
            
        default:
            snprintf(page ,4,"err");
            len = 4;
            break;
    }	
    return len;
}

static ssize_t zte_read_hver_material(char *page, char **start, off_t off,
		int count, int *eof, void *data)
{
    int board_id = -1;
    int len = 0;
    board_id = zte_get_board_id();
    printk("zte_read_hver_material board_id=%d \n",board_id);
    switch (board_id)
    {
        case 0:
            snprintf(page ,4,"ABB");
            len = 4;
            break;
        case 1:
            snprintf(page ,4,"ADB");
            len = 4;
            break;
        case 2:
            snprintf(page ,4,"AEB");
	      len = 4;
            break;
        case 3:
            snprintf(page ,4,"AFB");
            len = 4;
            break; 
        default:
            snprintf(page ,4,"err");
            len = 4;
            break;
    }	
    return len;
}

#if 0
static ssize_t zte_read_fmstate(char *page, char **start, off_t off,
		int count, int *eof, void *data)
{

    if(0==sdw_fmstat)
             snprintf(page ,4,"off");
   else if(1==sdw_fmstat)
             snprintf(page ,4," on");
    else
             snprintf(page ,4,"err");
    return 4;
}

static ssize_t zte_set_fmstate(struct file *filp,
				       const char *buff, size_t len,
				       loff_t * off)
{
    char state[4];

    if (len > 3)
		len = 3;

    if (copy_from_user(state, buff, len))
		return -EFAULT;

    if ('0'==state[0])
        sdw_fmstat = 0;
    else if('1'==state[0])
        sdw_fmstat = 1;
    else 
        sdw_fmstat = 0xFF;
        return len;
}
#endif
static void zte_creat_hver_proc_file(void)
{
	struct proc_dir_entry *prop_proc_file =
		create_proc_entry("driver/hardwareVersion", 0444, NULL);

	if (prop_proc_file) {
		prop_proc_file->read_proc = zte_read_hver;
		prop_proc_file->write_proc = NULL;
	}
}

static void zte_creat_hver_material_proc_file(void)
{
	struct proc_dir_entry *prop_proc_file_material =
        create_proc_entry("driver/hardwareVersionMaterial", 0444, NULL);

	if (prop_proc_file_material) {
		prop_proc_file_material->read_proc = zte_read_hver_material;
		prop_proc_file_material->write_proc = NULL;
	}
}

#ifdef CONFIG_ZTE_PROP_BRIDGE
static void zte_creat_engineer_prop(void)
{
    prop_add("10","Vibtator_on","echo 2000 > /sys/class/timed_output/vibrator/enable");
    prop_add("11","Vibtator_off","echo 0 > /sys/class/timed_output/vibrator/enable");
    prop_add("20","Keypad_on","echo 160 > /sys/class/leds/keybl/brightness");
    prop_add("21","Keypad_off", "echo 0 > /sys/class/leds/keybl/brightness");
    prop_add("30","Flash_on", "echo 1 > /proc/driver/adp1650");
    prop_add("31","Flash_off", "echo 0 > /proc/driver/adp1650");
    prop_add("40","Ledred", "echo 0x00ff0000 > /sys/class/leds/notifications/color");
    prop_add("41","Ledred_delay", "echo 1 > /sys/class/leds/notifications/delay_on");
    prop_add("42","Ledred_on", "echo 0 > /sys/class/leds/notifications/blink");
    prop_add("43","Ledred_off", "echo 0x00000000 > /sys/class/leds/notifications/color");
    prop_add("50","Ledgreen", "echo 0x0000ff00 > /sys/class/leds/notifications/color");
    prop_add("51","Ledgreen_delay", "echo 1 > /sys/class/leds/notifications/delay_on");
    prop_add("52","Ledgreen_on", "echo 0 > /sys/class/leds/notifications/blink");
    prop_add("53","Ledgreen_off", "echo 0x00000000 > /sys/class/leds/notifications/color");
    prop_add("60","Charge_on", "echo 1 > /proc/driver/set_charger_on_off");
    prop_add("61","Charger_off", "echo 0 > /proc/driver/set_charger_on_off");
    prop_add("70","High_current_on", "echo 1 > /proc/driver/set_high_current");
    prop_add("71","High_current_off", "echo 0 > /proc/driver/set_high_current");
    prop_add("80","Pmu_reg_dump","cat /proc/driver/rtc_status");
}
#endif

#if 0
int zte_get_fmstate(void)
{
    return sdw_fmstat;
}


static void zte_creat_fm_state_proc_file(void)
{
	struct proc_dir_entry *prop_proc_file =
		create_proc_entry("driver/fmstate", 0666, NULL);

	if (prop_proc_file) {
		prop_proc_file->read_proc = zte_read_fmstate;
		prop_proc_file->write_proc = zte_set_fmstate;
	}
}
#endif

int  zte_hver_proc_init(void)
{
    zte_creat_hver_proc_file();
    zte_creat_hver_material_proc_file();
#ifdef CONFIG_ZTE_PROP_BRIDGE
    zte_creat_engineer_prop();
#endif
    return 0;
}

#if 0
int __init zte_fm_state_proc_init(void)
{
    zte_creat_fm_state_proc_file();
    return 0;
}
#endif

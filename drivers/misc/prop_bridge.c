/*
 * Driver for ZTE prop bridge
 *
 * Copyright (C) 2011 ZTE Company, 
 *	Cui jian <cui.jian1@zte.com.cn>
 *
 */
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/file.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>

#define DRIVER_VERSION "v0.1"
#define DRIVER_AUTHOR "Cui jian <cui.jian1@zte.com.cn>"
#define PROP_MAX_LEN 40
#define PROP_ITEM_MAX_LEN 64
#define PROPERTY_PROC  "driver/prop"

#define DEBUG 0

#ifdef DEBUG
#define DBGMSG(fmt, args ...)    printk(KERN_INFO "PROP(%s)DBG"DRIVER_VERSION":" fmt "\n" ,__func__, ## args)
#else
#define DBGMSG(fmt, args ...)     
#endif

#define ERRMSG(fmt, args ...)    printk(KERN_ERR "PROP(%s)ERR"DRIVER_VERSION":" fmt "\n" ,__func__, ## args)


struct prop_item_node{
	char *dev;
	char *item;
	char *value;
};

static struct prop_item_node prop_item_list[PROP_MAX_LEN];

int prop_add( char *devname, char *item, char *value){
	int i;
	int len_dev;
	int len_item;	
	int len_value;	
	
	DBGMSG("Enter");

/*
	if (!dev){
		ERRMSG("dev null %s %s", item, value);
		return -EINVAL;
	}
*/
	
	DBGMSG("Dev[%s] item[%s] value[%s]", devname, item, value);
	
	len_dev = strlen(devname);
	len_item = strlen(item);
	len_value = strlen(value);

	if (len_dev > PROP_ITEM_MAX_LEN - 1
		|| len_item > PROP_ITEM_MAX_LEN - 1
		|| len_value> PROP_ITEM_MAX_LEN - 1){
		ERRMSG("len exceed %d %d %d", len_dev, len_item, len_value);
		return -EINVAL;
	}
	DBGMSG("begin to add an item");
	i = 0;
	while(i < PROP_MAX_LEN){
		if (!prop_item_list[i].dev){
			DBGMSG("use free node index %d", i);
			prop_item_list[i].dev = (char *)kmalloc( len_dev + 1, GFP_KERNEL);
			memset( prop_item_list[i].dev, 0, len_dev + 1);
			prop_item_list[i].item = (char *)kmalloc(len_item + 1, GFP_KERNEL);
			memset( prop_item_list[i].item, 0, len_item + 1);
			prop_item_list[i].value = (char *)kmalloc(len_value + 1, GFP_KERNEL);
			memset( prop_item_list[i].value , 0, len_value + 1);
	        memcpy( prop_item_list[i].dev, devname, len_dev);
	        memcpy( prop_item_list[i].item, item, len_item);
	        memcpy( prop_item_list[i].value, value, len_value);
			break;
		}
		i++;
	}
	
	DBGMSG("item num is %d,name is %s, item is %s, value is %s",i,prop_item_list[i].dev,prop_item_list[i].item, prop_item_list[i].value);
	
	return 0;	
}
EXPORT_SYMBOL_GPL(prop_add);

static ssize_t prop_read_proc(char *page, char **start, off_t off,
		int count, int *eof, void *data)
{
	int i = 0;
	int len = 0;
    
	DBGMSG("Enter");
	
	while(i < PROP_MAX_LEN){ 
		if (prop_item_list[i].dev){
			sprintf(page + len, "[%s]", prop_item_list[i].dev);
			len += strlen(prop_item_list[i].dev) + 2;
			sprintf(page + len, "%s:", prop_item_list[i].item);
			len += strlen(prop_item_list[i].item) + 1;		
			sprintf(page + len, "%s\r\n", prop_item_list[i].value);
			len += strlen(prop_item_list[i].value) + 2;	
		}
		i++;
	}
	return len + 1;
}

static void create_prop_proc_file(void)
{
	struct proc_dir_entry *prop_proc_file =
		create_proc_entry(PROPERTY_PROC, 0444, NULL);

	if (prop_proc_file) {
		prop_proc_file->read_proc = prop_read_proc;
		prop_proc_file->write_proc = NULL;
	}
}

static int __init prop_bridge_init(void)
{
	DBGMSG("prop_bridge_init Enter");
//	memset( prop_item_list, 0, sizeof(prop_item_list));
	create_prop_proc_file();
	DBGMSG("Exit");
	return 0;
}

static void __exit prop_bridge_exit(void)
{
    remove_proc_entry(PROPERTY_PROC,NULL);
	DBGMSG("Enter");
	return;
}

MODULE_VERSION(DRIVER_VERSION);
MODULE_AUTHOR(DRIVER_AUTHOR);

module_init(prop_bridge_init);
module_exit(prop_bridge_exit);

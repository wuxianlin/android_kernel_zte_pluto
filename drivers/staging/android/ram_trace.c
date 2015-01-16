/* drivers/android/ram_trace.c
 *
 * Copyright (C) 2007-2008 Google, Inc.
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

#include <linux/console.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/debugfs.h>

#include "../../../kernel/trace/trace.h"

struct ram_trace_buffer {
	uint32_t    sig;
	uint32_t    start;
	uint32_t    size;
	uint8_t     data[0];
};

#define RAM_TRACE_SIG (0x43474244) /* DBGC */

static char *ram_trace_old_log;
static size_t ram_trace_old_log_size;

static struct ram_trace_buffer *ram_trace_buffer;
static size_t ram_trace_buffer_size;

static void notrace
ram_trace_write(const void *s, unsigned int count)
{
	int rem;
	struct ram_trace_buffer *buffer = ram_trace_buffer;
	static DEFINE_SPINLOCK(ram_lock);
	unsigned long flags;

	spin_lock_irqsave(&ram_lock, flags);
	if (unlikely(count > ram_trace_buffer_size)) {
		s += count - ram_trace_buffer_size;
		count = ram_trace_buffer_size;
	}
	rem = ram_trace_buffer_size - buffer->start;
	if (rem < count) {
		memcpy(buffer->data + buffer->start, s, rem);
		s += rem;
		count -= rem;
		buffer->start = 0;
		buffer->size = ram_trace_buffer_size;
	}
	memcpy(buffer->data + buffer->start, s, count);

	buffer->start += count;
	if (buffer->size < ram_trace_buffer_size)
		buffer->size += count;
	spin_unlock_irqrestore(&ram_lock, flags);
}

static void
ram_trace_save_old(struct ram_trace_buffer *buffer, char *dest)
{
	size_t old_log_size = buffer->size;
	size_t bootinfo_size = 0;
	size_t total_size = old_log_size;
	char *ptr;

	total_size += bootinfo_size;

	if (dest == NULL) {
		dest = kmalloc(total_size, GFP_KERNEL);
		if (dest == NULL) {
			printk(KERN_ERR
			       "ram_trace: failed to allocate buffer\n");
			return;
		}
	}

	ram_trace_old_log = dest;
	ram_trace_old_log_size = total_size;
	memcpy(ram_trace_old_log,
	       &buffer->data[buffer->start], buffer->size - buffer->start);
	memcpy(ram_trace_old_log + buffer->size - buffer->start,
	       &buffer->data[0], buffer->start);
	ptr = ram_trace_old_log + old_log_size;
}

static int ram_trace_init(struct ram_trace_buffer *buffer,
				   size_t buffer_size, char *old_buf)
{
	ram_trace_buffer = buffer;
	ram_trace_buffer_size =
		buffer_size - sizeof(struct ram_trace_buffer);

	if (ram_trace_buffer_size > buffer_size) {
		pr_err("ram_trace: buffer %p, invalid size %zu, "
		       "datasize %zu\n", buffer, buffer_size,
		       ram_trace_buffer_size);
		return 0;
	}

	if (buffer->sig == RAM_TRACE_SIG) {
		if (buffer->size > ram_trace_buffer_size
		    || buffer->start > buffer->size)
			printk(KERN_INFO "ram_trace: found existing invalid "
			       "buffer, size %d, start %d\n",
			       buffer->size, buffer->start);
		else {
			printk(KERN_INFO "ram_trace: found existing buffer, "
			       "size %d, start %d\n",
			       buffer->size, buffer->start);
			if (buffer->size > 0)
				ram_trace_save_old(buffer, old_buf);
		}
	} else {
		printk(KERN_INFO "ram_trace: no valid data in buffer "
		       "(sig = 0x%08x)\n", buffer->sig);
	}

	buffer->sig = RAM_TRACE_SIG;
	buffer->start = 0;
	buffer->size = 0;

	return 0;
}

struct ram_trace_record {
	unsigned long ip;
	unsigned long parent_ip;
};

#define REC_SIZE sizeof(struct ram_trace_record)

static int ram_trace_enabled;

static struct trace_array *ram_trace_array;

static struct ftrace_ops trace_ops;

static void ram_trace_call(unsigned long ip, unsigned long parent_ip);

static struct ftrace_ops trace_ops __read_mostly = {
	.func = ram_trace_call,
	.flags = FTRACE_OPS_FL_GLOBAL,
};

static int ram_tracer_init(struct trace_array *tr)
{
	ram_trace_array = tr;
	tr->cpu = get_cpu();
	put_cpu();
	
	tracing_start_cmdline_record();
	
	ram_trace_enabled = 0;
	smp_wmb();

	register_ftrace_function(&trace_ops);

	smp_wmb();
	ram_trace_enabled = 1;

	return 0;
}

static void ram_trace_reset(struct trace_array *tr)
{
	ram_trace_enabled = 0;
	smp_wmb();

	unregister_ftrace_function(&trace_ops);

	tracing_stop_cmdline_record();
}

static void ram_trace_start(struct trace_array *tr)
{
	tracing_reset_online_cpus(tr);
}

static void ram_trace_call(unsigned long ip, unsigned long parent_ip)
{
	struct trace_array *tr = ram_trace_array;
	struct trace_array_cpu *data;
	long disabled;
	struct ram_trace_record rec;
	unsigned long flags;
	int cpu;
	
	smp_rmb();
	if (unlikely(!ram_trace_enabled))
		return;

	if (unlikely(oops_in_progress))
		return;

	/*
	 * Need to use raw, since this must be called before the
	 * recursive protection is performed.
	 */
	local_irq_save(flags);
	cpu = raw_smp_processor_id();
	data = tr->data[cpu];
	disabled = atomic_inc_return(&data->disabled);

	if (likely(disabled == 1)) {
		rec.ip = ip;
		rec.parent_ip = parent_ip;
		rec.ip |= cpu;
		ram_trace_write(&rec, sizeof(rec));
	}

	atomic_dec(&data->disabled);
	local_irq_restore(flags);
}

static struct tracer ram_tracer __read_mostly = {
	.name		= "ram",
	.init		= ram_tracer_init,
	.reset		= ram_trace_reset,
	.start		= ram_trace_start,
	.wait_pipe	= poll_wait_pipe,
};

static int ram_trace_driver_probe(struct platform_device *pdev)
{
	struct resource *res = pdev->resource;
	size_t start;
	size_t buffer_size;
	void *buffer;
	int ret;

	if (res == NULL || pdev->num_resources != 1 ||
	    !(res->flags & IORESOURCE_MEM)) {
		printk(KERN_ERR "ram_trace: invalid resource, %p %d flags "
		       "%lx\n", res, pdev->num_resources, res ? res->flags : 0);
		return -ENXIO;
	}
	buffer_size = res->end - res->start + 1;
	start = res->start;
	printk(KERN_INFO "ram_trace: got buffer at %zx, size %zx\n",
	       start, buffer_size);
	buffer = ioremap(res->start, buffer_size);
	if (buffer == NULL) {
		printk(KERN_ERR "ram_trace: failed to map memory\n");
		return -ENOMEM;
	}
	printk(KERN_INFO "ram_trace: buffer mapped at %p\n", buffer);

	ram_trace_init(buffer, buffer_size, NULL/* allocate */);
	
	ret = register_tracer(&ram_tracer);
	if (ret) {
		pr_err("ram_trace: failed to register tracer");
	}
	
	return ret;
}

static struct platform_driver ram_trace_driver = {
	.probe = ram_trace_driver_probe,
	.driver		= {
		.name	= "ram_trace",
	},
};

static int __init ram_trace_module_init(void)
{
	int err;
	err = platform_driver_register(&ram_trace_driver);
	return err;
}
device_initcall(ram_trace_module_init);

struct ram_trace_seq_data {
	const void *ptr;
	size_t off;
	size_t size;
};

void *ram_trace_seq_start(struct seq_file *s, loff_t *pos)
{
	struct ram_trace_seq_data *data;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return NULL;

	data->ptr = ram_trace_old_log;
	data->size = ram_trace_old_log_size;
	data->off = data->size % REC_SIZE;

	data->off += *pos * REC_SIZE;

	if (data->off + REC_SIZE > data->size) {
		kfree(data);
		return NULL;
	}

	return data;

}
void ram_trace_seq_stop(struct seq_file *s, void *v)
{
	kfree(v);
}

void *ram_trace_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	struct ram_trace_seq_data *data = v;

	data->off += REC_SIZE;

	if (data->off + REC_SIZE > data->size)
		return NULL;

	(*pos)++;

	return data;
}

int ram_trace_seq_show(struct seq_file *s, void *v)
{
	struct ram_trace_seq_data *data = v;
	struct ram_trace_record *rec;

	rec = (struct ram_trace_record *)(data->ptr + data->off);

	seq_printf(s, "%ld %08lx  %08lx  %pf <- %pF\n",
		rec->ip & 3, rec->ip, rec->parent_ip,
		(void *)rec->ip, (void *)rec->parent_ip);

	return 0;
}

static const struct seq_operations ram_trace_seq_ops = {
	.start = ram_trace_seq_start,
	.next = ram_trace_seq_next,
	.stop = ram_trace_seq_stop,
	.show = ram_trace_seq_show,
};

static int ram_trace_old_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &ram_trace_seq_ops);
}

static const struct file_operations ram_trace_old_fops = {
	.open		= ram_trace_old_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int __init ram_trace_late_init(void)
{
	struct dentry *d;

	if (ram_trace_old_log == NULL)
		return 0;
	
	d = debugfs_create_file("ram_trace", S_IRUGO, NULL,
		NULL, &ram_trace_old_fops);
	if (IS_ERR_OR_NULL(d))
		pr_err("ram_trace: failed to create old file\n");
	
	return 0;
}
late_initcall(ram_trace_late_init);

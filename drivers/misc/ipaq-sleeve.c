/*
*
* Driver for iPAQ H3xxx Extension Packs
*
* Copyright 2000 Compaq Computer Corporation.
*
* This program is free software; you can redistribute it and/or modify 
* it under the terms of the GNU General Public License as published by 
* the Free Software Foundation; either version 2 of the License, or 
* (at your option) any later version. 
* 
* COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
* AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
* FITNESS FOR ANY PARTICULAR PURPOSE.
*
* Author: Jamey Hicks.
*
*/

#include <linux/module.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/sysctl.h>
#include <linux/pm.h>
#include <linux/delay.h>

#include <asm/mach-types.h>
#include <asm/arch/hardware.h>
#include <asm/arch/irqs.h>
#include <linux/serial_reg.h>
#include <asm/hardware/ipaq-ops.h>
#include <asm/ipaq-sleeve.h>

#ifdef CONFIG_SLEEVE_DEBUG
int g_sleeve_debug = CONFIG_SLEEVE_DEBUG_VERBOSE;
EXPORT_SYMBOL(g_sleeve_debug);
#endif /* CONFIG_SLEEVE_DEBUG */

static struct ipaq_sleeve_ops *sleeve_ops;
static void *sleeve_dev;



void 
ipaq_sleeve_write_egpio(int nr, int state)
{
	sleeve_ops->control_egpio(sleeve_dev, nr, state);
}
EXPORT_SYMBOL(ipaq_sleeve_write_egpio);

int
ipaq_sleeve_read_egpio(int nr)
{
	return sleeve_ops->read_egpio(sleeve_dev, nr);
}
EXPORT_SYMBOL(ipaq_sleeve_read_egpio);

int
ipaq_sleeve_egpio_irq(int nr)
{
	return sleeve_ops->egpio_irq(sleeve_dev, nr);
}
EXPORT_SYMBOL(ipaq_sleeve_egpio_irq);

/*******************************************************************************/

#ifndef FALSE
#define FALSE	(0)
#endif
#ifndef TRUE
#define TRUE	(!FALSE)
#endif

/*******************************************************************************/

struct proc_dir_entry *proc_sleeve_dir = NULL;

static int ipaq_sleeve_proc_read_eeprom(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	char *p = page;
	int len = 0;
	int i;
	char mark = 0;
	int offset = off;
	int ret;

	sleeve_ops->spi_read(sleeve_dev, 0, &mark, 1);
	sleeve_ops->spi_read(sleeve_dev, 1, (char*)&len, 4);
#if 0
	len += 5; /* include the mark byte */
	if (mark != 0xaa || len == 0xFFFFFFFF)
		return 0;
#else
	len++;	  /* Add one for the byte before? */
	if (len == -1)
		len = 256;
#endif

	if (len < (off+count)) {
		count = len-off;
		/* *eof = 1; */
	}
	for (i = 0; i < count; i+=8) {
		sleeve_ops->spi_read(sleeve_dev, offset, p, 8);
		p += 8;
		offset += 8;
	}
	*start = page + off;
	ret = count;

	return ret;
}

static int ipaq_sleeve_proc_read_device(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	char *p = page;
	int len = 0;
	struct ipaq_sleeve_device *sdev = (struct ipaq_sleeve_device *) data;

	sleeve_ops->spi_read(sleeve_dev, 6, (unsigned char *)&sdev->vendor, 2);
	sleeve_ops->spi_read(sleeve_dev, 8, (unsigned char *)&sdev->device, 2);
	p += sprintf(p, "vendor=0x%04x\n", sdev->vendor);
	p += sprintf(p, "device=0x%04x\n", sdev->device);

	len = (p - page) - off;
	*start = page + off;
	return len;
}

struct sleeve_feature_entry {
	unsigned int flag;
	char *name;
};

static struct sleeve_feature_entry ipaq_sleeve_feature_entries[] = {
	{ SLEEVE_HAS_BLUETOOTH,		"Bluetooth" },
	{ SLEEVE_HAS_GPS,		"GPS" },
	{ SLEEVE_HAS_GPRS ,		"GPRS" },
	{ SLEEVE_HAS_CAMERA ,		"Camera" },
	{ SLEEVE_HAS_ACCELEROMETER ,	"Accelerometer" },
	{ SLEEVE_HAS_FLASH ,		"Flash" },
	{ SLEEVE_HAS_BARCODE_READER ,	"Barcode Reader" },
	{ SLEEVE_HAS_802_11B ,		"802.11b" },
	{ SLEEVE_HAS_EBAT ,		"EBAT discharging" },
	{ SLEEVE_HAS_VGA_OUT ,		"VGA output" },
	{ SLEEVE_HAS_VIDEO_OUT ,	"Video output" },
	{ SLEEVE_HAS_VIDEO_IN ,		"Video input" },
	{ SLEEVE_HAS_IRDA ,		"IrDA" },
	{ SLEEVE_HAS_AUDIO ,		"Audio" },
	{ SLEEVE_HAS_FIXED_BATTERY ,	"Fixed battery" },
	{ SLEEVE_HAS_REMOVABLE_BATTERY ,"Removable battery" },
};

static int ipaq_sleeve_proc_read_features(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	char *p = page;
	int len = 0;
	int i;
	struct ipaq_sleeve_device *sdev = (struct ipaq_sleeve_device *) data;

	if (sdev->dev.driver) {
	  unsigned int f = to_ipaq_sleeve_driver(sdev->dev.driver)->features;
		if ( f & SLEEVE_HAS_CF_MASK )
			p += sprintf(p, "CF slots=%d\n", SLEEVE_GET_CF(f));
		if ( f & SLEEVE_HAS_PCMCIA_MASK )
			p += sprintf(p, "PCMCIA slots=%d\n", SLEEVE_GET_PCMCIA(f));
		
		for ( i = 0 ; i < ARRAY_SIZE(ipaq_sleeve_feature_entries) ; i++ )
			if ( f & ipaq_sleeve_feature_entries[i].flag )
				p += sprintf(p, "%s\n", ipaq_sleeve_feature_entries[i].name );
	}

	len = (p - page) - off;
	*start = page + off;
	return len;
}

int ipaq_sleeve_proc_attach_device(struct ipaq_sleeve_device *dev)
{
	dev->procent = NULL;
	return 0;
}

int ipaq_sleeve_proc_detach_device(struct ipaq_sleeve_device *dev)
{
	struct proc_dir_entry *e;

	if ((e = dev->procent) != NULL) {
#if 0 
		if (atomic_read(e->count) != 0)
			return -EBUSY;
#endif
		remove_proc_entry(e->name, proc_sleeve_dir);
		dev->procent = NULL;
	}
	return 0;
}

/***********************************************************************************/

static LIST_HEAD(sleeve_drivers);

const struct ipaq_sleeve_device_id *
ipaq_sleeve_match_device(const struct ipaq_sleeve_device_id *ids, struct ipaq_sleeve_device *dev)
{
	while (ids->vendor) {
		if ((ids->vendor == SLEEVE_ANY_ID || ids->vendor == dev->vendor) &&
		    (ids->device == SLEEVE_ANY_ID || ids->device == dev->device))
			return ids;
		ids++;
	}
	return NULL;
}

static struct ipaq_sleeve_device active_sleeve;

static void ipaq_sleeve_insert(void) 
{
	SLEEVE_DEBUG(1, "\n");

	ipaq_sleeve_set_egpio(IPAQ_EGPIO_OPT_NVRAM_ON);
	mdelay(250);

	if ( active_sleeve.vendor || active_sleeve.device ) {
		device_unregister (&active_sleeve.dev);
		SLEEVE_DEBUG(0,"*** Error - sleeve insert without sleeve remove ***\n");
	}

	sleeve_ops->spi_read(sleeve_dev, 6, (char*)&active_sleeve.vendor, 2);
	sleeve_ops->spi_read(sleeve_dev, 8, (char*)&active_sleeve.device, 2);

	SLEEVE_DEBUG(1, "vendorid=%#x devid=%#x\n", active_sleeve.vendor, active_sleeve.device);
			
#ifdef CONFIG_PROC_FS
	ipaq_sleeve_proc_attach_device(&active_sleeve);
#endif

	snprintf(active_sleeve.dev.bus_id, BUS_ID_SIZE, "%04x:%04x",
		 active_sleeve.vendor, active_sleeve.device);

	if (device_register (&active_sleeve.dev) != 0) {
		printk(KERN_ERR "Could not register sleeve device\n");
	};
}

static void ipaq_sleeve_eject(void) 
{
	if (active_sleeve.vendor == 0 && active_sleeve.device == 0)
		return;		/* No sleeve was inserted */

	device_unregister (&active_sleeve.dev);

	SLEEVE_DEBUG(1,"active_sleeve.driver=%p sleeve removed\n", active_sleeve.dev.driver);
	if (active_sleeve.dev.driver && active_sleeve.dev.driver->remove)
		active_sleeve.dev.driver->remove(&active_sleeve.dev);

	memset(&active_sleeve, 0, sizeof(struct ipaq_sleeve_device));

	ipaq_sleeve_clear_egpio (IPAQ_EGPIO_OPT_NVRAM_ON);
}

#if 0
static void ipaq_sleeve_suspend(void)
{
	SLEEVE_DEBUG(1,"active_sleeve.driver=%p sleeve suspended\n", active_sleeve.driver);
	if (active_sleeve.driver && active_sleeve.driver->suspend)
		active_sleeve.driver->suspend(&active_sleeve, SUSPEND_POWER_DOWN);

	ipaq_sleeve_clear_egpio(IPAQ_EGPIO_OPT_NVRAM_ON);
}

static void ipaq_sleeve_resume(void)
{
	int opt_det;
	SLEEVE_DEBUG(1,"active_sleeve.driver=%p sleeve resumed\n", active_sleeve.driver);

	sleeve_ops->get_option_detect(sleeve_dev, &opt_det);
	if (opt_det) {
		ipaq_sleeve_set_egpio(IPAQ_EGPIO_OPT_NVRAM_ON);
		if (active_sleeve.driver && active_sleeve.driver->resume) 
			active_sleeve.driver->resume(&active_sleeve, RESUME_ENABLE);
	}
	else {
		if (active_sleeve.driver && active_sleeve.driver->remove)
			active_sleeve.driver->remove(&active_sleeve);
		memset(&active_sleeve, 0, sizeof(struct ipaq_sleeve_device));
	}
}
#endif

int ipaq_current_sleeve( void )
{
	return IPAQ_SLEEVE_ID( active_sleeve.vendor, active_sleeve.device );
}

EXPORT_SYMBOL(ipaq_current_sleeve);

int ipaq_sleeve_bus_match(struct device * dev, struct device_driver * drv)
{
	struct ipaq_sleeve_device *sleeve_device = to_ipaq_sleeve_device(dev);
	struct ipaq_sleeve_driver *sleeve_driver = to_ipaq_sleeve_driver(drv);
	const struct ipaq_sleeve_device_id *id_table = sleeve_driver->id_table;
	const struct ipaq_sleeve_device_id *id =
		ipaq_sleeve_match_device(id_table, sleeve_device);
	if (id)
		return 1;
	else
		return 0;
}

int ipaq_sleeve_bus_hotplug(struct device *dev, char **envp, 
			    int num_envp, char *buffer, int buffer_size)
{
	return 0;
}

int ipaq_sleeve_bus_suspend(struct device * dev, pm_message_t state)
{
	struct ipaq_sleeve_device *sleeve_device = to_ipaq_sleeve_device(dev);
	return sleeve_device->dev.driver->suspend(&sleeve_device->dev, state);
}

int ipaq_sleeve_bus_resume(struct device * dev)
{
	struct ipaq_sleeve_device *sleeve_device = to_ipaq_sleeve_device(dev);
	return sleeve_device->dev.driver->resume(&sleeve_device->dev);
}

struct bus_type ipaq_sleeve_bus_type = {
	.name = "ipaq-sleeve",
	.match = ipaq_sleeve_bus_match,
	.suspend = ipaq_sleeve_bus_suspend,
	.resume  = ipaq_sleeve_bus_resume
};

/*******************************************************************************/

/**
 * ipaq_sleeve_register_driver: Registers a driver for an iPAQ extension pack.
 * @drv: the sleeve driver
 */
int ipaq_sleeve_driver_register(struct ipaq_sleeve_driver *drv)
{
	drv->driver.bus = &ipaq_sleeve_bus_type;
	return driver_register(&drv->driver);
}

/**
 * ipaq_sleeve_unregister_driver: Unregisters a driver for an iPAQ extension pack.
 * @drv: the sleeve driver
 */
void ipaq_sleeve_driver_unregister(struct ipaq_sleeve_driver *drv)
{
	driver_unregister(&drv->driver);
}

EXPORT_SYMBOL(ipaq_sleeve_driver_register);
EXPORT_SYMBOL(ipaq_sleeve_driver_unregister);

/*******************************************************************************/

static void ipaq_sleeve_task_handler(struct work_struct *x)
{
	int opt_det;
	/* debounce */
	mdelay(100);
	sleeve_ops->get_option_detect(sleeve_dev, &opt_det);

	printk("option_detect=%d\n", opt_det);

	if (opt_det)
		ipaq_sleeve_insert();
	else
		ipaq_sleeve_eject();
}

DECLARE_WORK(ipaq_sleeve_task, ipaq_sleeve_task_handler);

irqreturn_t
ipaq_sleeve_presence_interrupt (int irq, void *dev_id)
{
	schedule_work (&ipaq_sleeve_task);
	return IRQ_HANDLED;
}
EXPORT_SYMBOL(ipaq_sleeve_presence_interrupt);

/***********************************************************************************/

int ipaq_sleeve_register(struct ipaq_sleeve_ops *ops, void *devp)
{
	if (sleeve_ops)
		return -EBUSY;

	sleeve_ops = ops;
	sleeve_dev = devp;
	
	ipaq_sleeve_clear_egpio(IPAQ_EGPIO_OPT_RESET);
	ipaq_sleeve_clear_egpio(IPAQ_EGPIO_CARD_RESET);
	ipaq_sleeve_clear_egpio(IPAQ_EGPIO_OPT_ON);
	ipaq_sleeve_clear_egpio(IPAQ_EGPIO_OPT_NVRAM_ON);

	memset(&active_sleeve, 0, sizeof(struct ipaq_sleeve_device));
	active_sleeve.dev.bus = &ipaq_sleeve_bus_type;

	schedule_work(&ipaq_sleeve_task);
	SLEEVE_DEBUG(2,"init_module successful\n");

	return 0;
}

int ipaq_sleeve_unregister(void)
{
	if (!sleeve_ops)
		return -ENODEV;

	ipaq_sleeve_eject ();

	flush_scheduled_work(); /* make sure all tasks have run */

	/* Sanity check */
	ipaq_sleeve_clear_egpio(IPAQ_EGPIO_OPT_RESET);
	ipaq_sleeve_clear_egpio(IPAQ_EGPIO_CARD_RESET);
	ipaq_sleeve_clear_egpio(IPAQ_EGPIO_OPT_ON);
	ipaq_sleeve_clear_egpio(IPAQ_EGPIO_OPT_NVRAM_ON);

	sleeve_ops = NULL;

	return 0;
}

static int
ipaq_sleeve_init (void)
{
	int result;

/* Is this really neccessary? Kconfig should take care about that... */
/*	if (!machine_is_ipaq())
		return -ENODEV;*/

	result = bus_register(&ipaq_sleeve_bus_type);
	if (result) {
		printk("bus_register failed\n");
		return result;
	}

#ifdef CONFIG_PROC_FS
	proc_sleeve_dir = proc_mkdir("sleeve", proc_bus);
	if (proc_sleeve_dir) {
		create_proc_read_entry("device", 0, proc_sleeve_dir, ipaq_sleeve_proc_read_device, &active_sleeve);
		create_proc_read_entry("eeprom", 0, proc_sleeve_dir, ipaq_sleeve_proc_read_eeprom, &active_sleeve);
		create_proc_read_entry("features", 0, proc_sleeve_dir, ipaq_sleeve_proc_read_features, &active_sleeve);
	}
#endif

	return result;
}

static void
ipaq_sleeve_exit (void)
{
#ifdef CONFIG_PROC_FS
	if (proc_sleeve_dir) {
		remove_proc_entry("features", proc_sleeve_dir);
		remove_proc_entry("device", proc_sleeve_dir);
		remove_proc_entry("eeprom", proc_sleeve_dir);
		remove_proc_entry("sleeve", proc_bus);
	}
#endif

	bus_unregister(&ipaq_sleeve_bus_type);
}

EXPORT_SYMBOL(ipaq_sleeve_register);
EXPORT_SYMBOL(ipaq_sleeve_unregister);

module_init(ipaq_sleeve_init);
module_exit(ipaq_sleeve_exit);

MODULE_AUTHOR("Jamey Hicks <jamey.hicks@hp.com>");
MODULE_DESCRIPTION("Hotplug driver for Compaq iPAQ expansion packs (sleeves)");
MODULE_LICENSE("Dual BSD/GPL");

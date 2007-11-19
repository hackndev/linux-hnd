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
#include <linux/version.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/sysctl.h>
#include <linux/pm.h>

#ifdef CONFIG_DRIVERFS_FS
#include <linux/device.h>
#endif

/* SA1100 serial defines */
#include <asm/arch/hardware.h>
#include <linux/serial_reg.h>
#include <asm/arch/irqs.h>
#include <asm/arch-sa1100/h3600-sleeve.h>
#include <asm/arch-sa1100/h3600_hal.h>

#ifdef CONFIG_SLEEVE_DEBUG
int g_sleeve_debug = CONFIG_SLEEVE_DEBUG_VERBOSE;
EXPORT_SYMBOL(g_sleeve_debug);
#endif /* CONFIG_SLEEVE_DEBUG */

static void msleep(unsigned int msec)
{
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout( (msec * HZ) / 1000);
}


/*******************************************************************************/

#ifdef CONFIG_HOTPLUG

#ifndef FALSE
#define FALSE	(0)
#endif
#ifndef TRUE
#define TRUE	(!FALSE)
#endif

extern char hotplug_path[];

extern int call_usermodehelper(char *path, char **argv, char **envp);

static void run_sbin_hotplug(struct sleeve_dev *sdev, int insert)
{
	int i;
	char *argv[3], *envp[8];
	char id[64], sub_id[64], name[100];

	if (!hotplug_path[0])
		return;

	i = 0;
	argv[i++] = hotplug_path;
	argv[i++] = "sleeve";
	argv[i] = 0;

	SLEEVE_DEBUG(1,"%d * hotplug_path=%s\n", __LINE__, hotplug_path);
	i = 0;
	/* minimal command environment */
	envp[i++] = "HOME=/";
	envp[i++] = "PATH=/sbin:/bin:/usr/sbin:/usr/bin";
	
	/* other stuff we want to pass to /sbin/hotplug */
	sprintf(id,	"VENDOR_ID=%04x",   sdev->vendor);
	sprintf(sub_id, "DEVICE_ID=%04x",   sdev->device);
	if ( sdev->driver != NULL )
		sprintf(name,	"DEVICE_NAME=%s", sdev->driver->name);
	else 
		sprintf(name,	"DEVICE_NAME=Unknown");

	envp[i++] = id;
	envp[i++] = sub_id;
	envp[i++] = name;
	if (insert)
		envp[i++] = "ACTION=add";
	else
		envp[i++] = "ACTION=remove";
	envp[i] = 0;

	call_usermodehelper (argv [0], argv, envp);
}
#else
static void run_sbin_hotplug(struct sleeve_dev *sdev, int insert) { }
#endif /* CONFIG_HOTPLUG */

/*******************************************************************************/

struct proc_dir_entry *proc_sleeve_dir = NULL;

static int h3600_sleeve_proc_read_eeprom(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	char *p = page;
	int len = 0;
	int i;
	char mark = 0;
	int offset = off;
	int ret;

	h3600_spi_read(0, &mark, 1);
	h3600_spi_read(1, (char*)&len, 4);
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
		h3600_spi_read(offset, p, 8);
		p += 8;
		offset += 8;
	}
	*start = page + off;
	ret = count;

	return ret;
}

static int h3600_sleeve_proc_read_device(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	char *p = page;
	int len = 0;
	struct sleeve_dev *sdev = (struct sleeve_dev *) data;

	h3600_spi_read(6, &sdev->vendor, 2);
	h3600_spi_read(8, &sdev->device, 2);
	p += sprintf(p, "vendor=0x%04x\n", sdev->vendor);
	p += sprintf(p, "device=0x%04x\n", sdev->device);
	if (sdev->driver != NULL)
		p += sprintf(p, "driver=%s\n", sdev->driver->name);

	len = (p - page) - off;
	*start = page + off;
	return len;
}

struct sleeve_feature_entry {
	unsigned int flag;
	char *name;
};

static struct sleeve_feature_entry h3600_sleeve_feature_entries[] = {
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

static int h3600_sleeve_proc_read_features(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	char *p = page;
	int len = 0;
	int i;
	struct sleeve_dev *sdev = (struct sleeve_dev *) data;

	if (sdev->driver) {
		unsigned int f = sdev->driver->features;
		if ( f & SLEEVE_HAS_CF_MASK )
			p += sprintf(p, "CF slots=%d\n", SLEEVE_GET_CF(f));
		if ( f & SLEEVE_HAS_PCMCIA_MASK )
			p += sprintf(p, "PCMCIA slots=%d\n", SLEEVE_GET_PCMCIA(f));
		
		for ( i = 0 ; i < ARRAY_SIZE(h3600_sleeve_feature_entries) ; i++ )
			if ( f & h3600_sleeve_feature_entries[i].flag )
				p += sprintf(p, "%s\n", h3600_sleeve_feature_entries[i].name );
	}

	len = (p - page) - off;
	*start = page + off;
	return len;
}

int h3600_sleeve_proc_attach_device(struct sleeve_dev *dev)
{
	dev->procent = NULL;
	return 0;
}

int h3600_sleeve_proc_detach_device(struct sleeve_dev *dev)
{
	struct proc_dir_entry *e;

	if ((e = dev->procent) != NULL) {
#if Broken 
		if (atomic_read(e->count) != 0)
			return -EBUSY;
#endif
		remove_proc_entry(e->name, proc_sleeve_dir);
		dev->procent = NULL;
	}
	return 0;
}

/***********************************************************************************/

#ifdef CONFIG_DRIVERFS_FS
static int sleeve_iobus_scan(struct iobus *bus)
{
	printk("%s\n", __FUNCTION__);
	return 0;
}

static int sleeve_iobus_add_device(struct iobus *bus, char *devname)
{
	printk("%s: devname=%s\n", __FUNCTION__, devname);
	return 0;
}

static struct device_driver sleeve_device_driver = {
	probe: NULL,
};

static struct device sleeve_device = {
	name: "sleeve device",
	driver: &sleeve_device_driver,
};

static struct iobus_driver sleeve_iobus_driver = {
	name: "sleeve bus",
	scan: sleeve_iobus_scan,
	add_device: sleeve_iobus_add_device,
};

struct iobus h3600_sleeve_iobus = {
	self: &sleeve_device,
	bus_id: "sleeve",
	name:	"H3600 Sleeve Bus",
	driver: &sleeve_iobus_driver,
};
EXPORT_SYMBOL(h3600_sleeve_iobus);
#endif

/*******************************************************************************/

static LIST_HEAD(sleeve_drivers);

const struct sleeve_device_id *
h3600_sleeve_match_device(const struct sleeve_device_id *ids, struct sleeve_dev *dev)
{
	while (ids->vendor) {
		if ((ids->vendor == SLEEVE_ANY_ID || ids->vendor == dev->vendor) &&
		    (ids->device == SLEEVE_ANY_ID || ids->device == dev->device))
			return ids;
		ids++;
	}
	return NULL;
}

static int h3600_sleeve_announce_device(struct sleeve_driver *drv, struct sleeve_dev *dev)
{
	const struct sleeve_device_id *id = NULL;

	if (drv->id_table) {
		id = h3600_sleeve_match_device(drv->id_table, dev);
		if (!id)
			return 0;
	}

	dev->driver = drv;
	if (drv->probe(dev, id) >= 0) {
#ifdef CONFIG_DRIVERFS_FS
		strncpy(dev->dev.name, dev->driver->name, DEVICE_NAME_SIZE);
		strcpy(dev->dev.bus_id, "0");
		dev->dev.driver = drv->driver;
		dev->dev.driver_data = dev;
		device_register(&dev->dev);
#endif
		if (drv->features & SLEEVE_HAS_EBAT)
			h3600_set_ebat();

		run_sbin_hotplug(dev,TRUE);
		return 1;
	}

	return 0;
}

static struct sleeve_dev active_sleeve;


static void h3600_sleeve_insert(void) 
{
	struct list_head *item;

	SLEEVE_DEBUG(1, "\n");

	assign_ipaqsa_egpio(IPAQ_EGPIO_OPT_NVRAM_ON, 1);
	msleep(250);

	if ( active_sleeve.vendor || active_sleeve.device )
		SLEEVE_DEBUG(0,"*** Error - sleeve insert without sleeve remove ***\n");

	h3600_spi_read(6, (char*)&active_sleeve.vendor, 2);
	h3600_spi_read(8, (char*)&active_sleeve.device, 2);

	SLEEVE_DEBUG(1, "vendorid=%#x devid=%#x\n", active_sleeve.vendor, active_sleeve.device);
			
#ifdef CONFIG_PROC_FS
	h3600_sleeve_proc_attach_device(&active_sleeve);
#endif
#ifdef CONFIG_DRIVERFS_FS
	device_init_dev(&active_sleeve.dev);
	active_sleeve.dev.parent = &h3600_sleeve_iobus;
#endif

	for (item=sleeve_drivers.next; item != &sleeve_drivers; item=item->next) {
		struct sleeve_driver *drv = list_entry(item, struct sleeve_driver, node);
		if (h3600_sleeve_announce_device(drv, &active_sleeve)) {
			SLEEVE_DEBUG(1, "matched driver %s\n", drv->name);
			return;
		}
	}

	SLEEVE_DEBUG(0, "unrecognized sleeve vendor=0x%04x device=0x%04x\n", 
		     active_sleeve.vendor, active_sleeve.device);
}

static void h3600_sleeve_eject(void) 
{
	run_sbin_hotplug(&active_sleeve, FALSE);

	SLEEVE_DEBUG(1,"active_sleeve.driver=%p sleeve removed\n", active_sleeve.driver);
	if (active_sleeve.driver && active_sleeve.driver->remove)
		active_sleeve.driver->remove(&active_sleeve);

	memset(&active_sleeve, 0, sizeof(struct sleeve_dev));
	assign_ipaqsa_egpio(IPAQ_EGPIO_OPT_NVRAM_ON, 0);
}

static void h3600_sleeve_suspend(void)
{
	SLEEVE_DEBUG(1,"active_sleeve.driver=%p sleeve suspended\n", active_sleeve.driver);
	if (active_sleeve.driver && active_sleeve.driver->suspend)
		active_sleeve.driver->suspend(&active_sleeve);

	assign_ipaqsa_egpio(IPAQ_EGPIO_OPT_NVRAM_ON, 0);
}

static void h3600_sleeve_resume(void)
{
	int opt_det;
	SLEEVE_DEBUG(1,"active_sleeve.driver=%p sleeve resumed\n", active_sleeve.driver);

	h3600_get_option_detect(&opt_det);
	if (opt_det) {
		assign_ipaqsa_egpio(IPAQ_EGPIO_OPT_NVRAM_ON, 1);
		if (active_sleeve.driver && active_sleeve.driver->resume) 
			active_sleeve.driver->resume(&active_sleeve);
	}
	else {
		if (active_sleeve.driver && active_sleeve.driver->remove)
			active_sleeve.driver->remove(&active_sleeve);
		memset(&active_sleeve, 0, sizeof(struct sleeve_dev));
	}
}

int h3600_current_sleeve( void )
{
	return H3600_SLEEVE_ID( active_sleeve.vendor, active_sleeve.device );
}

EXPORT_SYMBOL(h3600_current_sleeve);

/*******************************************************************************/

/**
 * h3600_sleeve_register_driver: Registers a driver for an iPAQ extension pack.
 * @drv: the sleeve driver
 */
int h3600_sleeve_register_driver(struct sleeve_driver *drv)
{
	list_add_tail(&drv->node, &sleeve_drivers);

	if ( h3600_sleeve_announce_device( drv, &active_sleeve ) ) {
		SLEEVE_DEBUG(1,"registered a driver that matches the current sleeve %s\n", drv->name);
		return 1;
	}

	return 0;
}

/**
 * h3600_sleeve_unregister_driver: Unregisters a driver for an iPAQ extension pack.
 * @drv: the sleeve driver
 */
void h3600_sleeve_unregister_driver(struct sleeve_driver *drv)
{
	list_del(&drv->node);
	if (active_sleeve.driver == drv) {
		if (drv->remove)
			drv->remove(&active_sleeve);
		active_sleeve.driver = NULL;
	}
}

EXPORT_SYMBOL(h3600_sleeve_register_driver);
EXPORT_SYMBOL(h3600_sleeve_unregister_driver);

static int h3600_sleeve_proc_read_drivers(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	char *p = page;
	int len = 0;
	struct list_head *item;
	const struct sleeve_device_id *id;

	for (item=sleeve_drivers.next; item != &sleeve_drivers; item=item->next) {
		struct sleeve_driver *drv = list_entry(item, struct sleeve_driver, node);
		p += sprintf(p, "%35s : ", drv->name);
		if ( drv->id_table ) {
			for ( id = drv->id_table ; id->vendor ; id++ )
				p += sprintf(p, "(0x%04x 0x%04x) ", id->vendor, id->device);
			p += sprintf(p,"\n");
		}
		else {
			p += sprintf(p,"ANY\n");
		}
	}

	len = (p - page) - off;
	*start = page + off;
	return len;
}

/*******************************************************************************/

static void h3600_sleeve_task_handler(void *x)
{
	int opt_det;
	/* debounce */
	msleep(100);
	h3600_get_option_detect(&opt_det);

	if (opt_det)
		h3600_sleeve_insert();
	else
		h3600_sleeve_eject();
}

static struct tq_struct h3600_sleeve_task = {
	routine: h3600_sleeve_task_handler
};

void h3600_sleeve_interrupt( int present )
{
	schedule_task(&h3600_sleeve_task);
}

/***********************************************************************************/

static struct pm_dev *h3600_sleeve_pm_dev;
static int suspended = 0;

static int h3600_sleeve_pm_callback(struct pm_dev *pm_dev, pm_request_t req, void *data)
{
	SLEEVE_DEBUG(1, "sleeve pm callback %d\n", req);

	switch (req) {
	case PM_SUSPEND: /* Enter D1-D3 */
		h3600_sleeve_suspend();
		suspended = 1;
		break;
	case PM_RESUME:	 /* Enter D0 */
		if ( suspended ) {
			h3600_sleeve_resume();
			suspended = 0;
		}
		break;
	}
	return 0;
}

/*******************************************************************************/

static struct ctl_table sleeve_table[] = 
{
	{1, "eject", NULL, 0, 0600, NULL, (proc_handler *)&h3600_sleeve_eject},
	{2, "insert", NULL, 0, 0600, NULL, (proc_handler *)&h3600_sleeve_insert},
#ifdef CONFIG_SLEEVE_DEBUG_VERBOSE
	{3, "debug", &g_sleeve_debug, sizeof(g_sleeve_debug), 0644, NULL, &proc_dointvec },
#endif
	{0}
};
static struct ctl_table sleeve_dir_table[] = 
{
	{BUS_SLEEVE, "sleeve", NULL, 0, 0555, sleeve_table},
	{0}
};
static struct ctl_table bus_dir_table[] = 
{
	{CTL_BUS, "bus", NULL, 0, 0555, sleeve_dir_table},
	{0}
};
static struct ctl_table_header *sleeve_ctl_table_header = NULL;

static struct h3600_driver_ops hal_driver_ops = {
	option_detect : h3600_sleeve_interrupt,
};

static int h3600_sleeve_major = 0;

int __init h3600_sleeve_init_module(void)
{
	int result;

//	if (!machine_is_ipaq())
//		return -ENODEV;

	assign_ipaqsa_egpio(IPAQ_EGPIO_OPT_RESET, 0);
	assign_ipaqsa_egpio(IPAQ_EGPIO_CARD_RESET, 0);
	assign_ipaqsa_egpio(IPAQ_EGPIO_OPT_ON, 0);
	assign_ipaqsa_egpio(IPAQ_EGPIO_OPT_NVRAM_ON, 0);

	memset(&active_sleeve, 0, sizeof(struct sleeve_dev));
	result = h3600_hal_register_driver( &hal_driver_ops );
	if (result) return -ENODEV;

#ifdef CONFIG_PROC_FS
	proc_sleeve_dir = proc_mkdir("sleeve", proc_bus);
	if (proc_sleeve_dir) {
		create_proc_read_entry("device", 0, proc_sleeve_dir, h3600_sleeve_proc_read_device, &active_sleeve);
		create_proc_read_entry("drivers", 0, proc_sleeve_dir, h3600_sleeve_proc_read_drivers, &active_sleeve);
		create_proc_read_entry("eeprom", 0, proc_sleeve_dir, h3600_sleeve_proc_read_eeprom, &active_sleeve);
		create_proc_read_entry("features", 0, proc_sleeve_dir, h3600_sleeve_proc_read_features, &active_sleeve);
	}
#endif
	h3600_sleeve_pm_dev = pm_register(PM_UNKNOWN_DEV, PM_SYS_UNKNOWN, h3600_sleeve_pm_callback);
	sleeve_ctl_table_header = register_sysctl_table(bus_dir_table, 0);

#ifdef CONFIG_DRIVERFS_FS
	device_register(&sleeve_device);
	iobus_init(&h3600_sleeve_iobus);
	iobus_register(&h3600_sleeve_iobus);
#endif
	schedule_task(&h3600_sleeve_task);
	SLEEVE_DEBUG(2,"init_module successful init major= %d\n", h3600_sleeve_major);

	return result;
}

void h3600_sleeve_cleanup_module(void)
{
	flush_scheduled_tasks(); /* make sure all tasks have run */
	h3600_hal_unregister_driver( &hal_driver_ops );

	pm_unregister(h3600_sleeve_pm_dev);

#ifdef CONFIG_PROC_FS
	if (proc_sleeve_dir) {
		remove_proc_entry("features", proc_sleeve_dir);
		remove_proc_entry("device", proc_sleeve_dir);
		remove_proc_entry("drivers", proc_sleeve_dir);
		remove_proc_entry("eeprom", proc_sleeve_dir);
		remove_proc_entry("sleeve", proc_bus);
	}
#endif
	unregister_sysctl_table(sleeve_ctl_table_header);

#ifdef CONFIG_DRIVERFS_FS
	/* no unregister functions? */
#endif

	/* Sanity check */
	assign_ipaqsa_egpio(IPAQ_EGPIO_OPT_RESET, 0);
	assign_ipaqsa_egpio(IPAQ_EGPIO_CARD_RESET, 0);
	assign_ipaqsa_egpio(IPAQ_EGPIO_OPT_ON, 0);
	assign_ipaqsa_egpio(IPAQ_EGPIO_OPT_NVRAM_ON, 0);
}

module_init(h3600_sleeve_init_module);
module_exit(h3600_sleeve_cleanup_module);

MODULE_AUTHOR("Jamey Hicks <jamey.hicks@hp.com>");
MODULE_DESCRIPTION("Hotplug driver for Compaq iPAQ expanstion packs (sleeves)");
MODULE_LICENSE("Dual BSD/GPL");

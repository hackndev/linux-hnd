/*
*
* Driver for iPAQ H3xxx/h5xxx Extension Packs
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
#ifndef _INCLUDE_IPAQ_SLEEVE_H_
#define _INCLUDE_IPAQ_SLEEVE_H_

#include <linux/device.h>
#include <linux/irqreturn.h>
//#include <asm/arch-sa1100/h3600.h>	/* !!! */

enum sleeve_vendor_id {
        SLEEVE_NO_VENDOR     = 0,
        COMPAQ_VENDOR_ID     = 0x1125,
        TALON_VENDOR         = 0x0100,
        BIOCENTRIC_SOLUTIONS = 0x0101,
        HANVIT_INFO_TECH     = 0x0102,
        SYMBOL_VENDOR        = 0x0103,
        OMRON_CORPORATION    = 0x0104,
        HAGIWARA_SYSCOM_CO   = 0x0105,
        TACTEL_SLEEVE_VENDOR = 0x0107,
        NEXIAN_VENDOR        = 0x010a,
	PITECH_VENDOR        = 0x0111,
        LIFEVIEW_VENDOR      = 0x0601,
        SLEEVE_ANY_ID = -1,
};

enum sleeve_device_id {
        SLEEVE_NO_DEVICE           = 0,

        /* COMPAQ_VENDOR_ID sleeve devices */
        SINGLE_COMPACTFLASH_SLEEVE = 0xCF11,
        SINGLE_PCMCIA_SLEEVE       = 0xD7C3,
        DUAL_PCMCIA_SLEEVE         = 0x0001,
        EXTENDED_BATTERY_SLEEVE    = 0x0002,
        SINGLE_CF_PLUS_SLEEVE      = 0x0003,
        SINGLE_PCMCIA_PLUS_SLEEVE  = 0x0004,
        GPRS_EXPANSION_PACK        = 0x0010,
        BLUETOOTH_EXPANSION_PACK   = 0x0011,
        MERCURY_BACKPAQ            = 0x0100,

        /* TALON_VENDOR sleeve devices */
        NAVMAN_GPS_SLEEVE          = 0x0001,
	NAVMAN_GPS_SLEEVE_ALT      = 0x0010,

        /* Nexian sleeve devices */
        NEXIAN_DUAL_CF_SLEEVE      = 0x0001,
        NEXIAN_CAMERA_CF_SLEEVE    = 0x0003,

        /* Symbol sleeve devices */
        SYMBOL_WLAN_SCANNER_SLEEVE = 0x0011,

        /* Lifeview sleeve devices */
        LIFEVIEW_FLYJACKET_SLEEVE  = 0x8a10,

	/* PiTech sleeve devices */
	MEMPLUG_SLEEVE             = 0x0001,
	MEMPLUG_SLEEVE3            = 0x0003,

	/* TACTEL */
	TACTEL_BLUETOOTH_CF_SLEEVE = 0xbcbc

};

#define IPAQ_SLEEVE_ID(vendor,device) ((vendor)<<16 | (device))

struct ipaq_sleeve_driver;

/**
 * struct sleeve_dev
 * @driver: which driver allocated this device
 * @procent: device entry in /proc/bus/sleeve
 * @vendor:  vendor id copied from eeprom
 * @device:  device id copied from eeprom
 * @driver_data: private data for the driver
 * @dev: 
 */ 
struct ipaq_sleeve_device {
        struct device         dev;
        struct proc_dir_entry *procent;	        /* device entry in /proc/bus/sleeve */
        unsigned short	       vendor;          /* Copied from EEPROM */
        unsigned short	       device;  
};
#define to_ipaq_sleeve_device(obj) container_of(obj, struct ipaq_sleeve_device, dev)

struct ipaq_sleeve_device_id {
        unsigned int vendor, device;		/* Vendor and device ID or SLEEVE_ANY_ID */
};

#define SLEEVE_HAS_CF_MASK               0x00000003  /* 0 to 3 */
#define SLEEVE_HAS_CF(x)                        (x)
#define SLEEVE_GET_CF(x)                 ((x)&SLEEVE_HAS_CF_MASK)
#define SLEEVE_HAS_PCMCIA_MASK           0x0000000c
#define SLEEVE_HAS_PCMCIA(x)               ((x)<<2)
#define SLEEVE_GET_PCMCIA(x)             (((x)&SLEEVE_HAS_PCMCIA_MASK)>>2)

#define SLEEVE_HAS_BLUETOOTH             0x00000010
#define SLEEVE_HAS_GPS                   0x00000020
#define SLEEVE_HAS_GPRS                  0x00000040
#define SLEEVE_HAS_CAMERA                0x00000080
#define SLEEVE_HAS_ACCELEROMETER         0x00000100
#define SLEEVE_HAS_FLASH                 0x00000200
#define SLEEVE_HAS_BARCODE_READER        0x00000400
#define SLEEVE_HAS_802_11B               0x00000800
#define SLEEVE_HAS_EBAT                  0x00001000
#define SLEEVE_HAS_VGA_OUT               0x00002000
#define SLEEVE_HAS_VIDEO_OUT             0x00004000
#define SLEEVE_HAS_VIDEO_IN              0x00008000
#define SLEEVE_HAS_IRDA                  0x00010000
#define SLEEVE_HAS_AUDIO                 0x00020000

#define SLEEVE_HAS_FIXED_BATTERY         0x10000000
#define SLEEVE_HAS_REMOVABLE_BATTERY     0x20000000

struct h3600_battery;

/**
 * struct ipaq_sleeve_driver: driver for iPAQ extension pack
 * @name: name of device type
 * @id_table: table of &struct_sleeve_device_id identifying matching sleeves by vendor,device
 * @features: SLEEVE_HAS flags 
 * @driver: used by driverfs
 * @probe: called when new sleeve inserted
 * @remove: called when sleeve removed
 * @suspend: called before suspending iPAQ
 * @resume: called while resuming iPAQ
 *
 */
struct ipaq_sleeve_driver {
	struct device_driver          driver;
	const struct ipaq_sleeve_device_id *id_table;   /* NULL if wants all devices */
	unsigned int                   features;   /* SLEEVE_HAS flags */
};

#define to_ipaq_sleeve_driver(obj) container_of(obj, struct ipaq_sleeve_driver, driver)

/**
 * struct ipaq_sleeve_controller_driver: driver for the controller for iPAQ expansion sleeves
 */
struct ipaq_sleeve_controller_driver {
	/* called to indicate that sleeve has been inserted or removed */
	int  (*sleeve_detect)(struct ipaq_sleeve_device *dev, int present);
	struct device_driver *driver;
};

extern struct bus_type ipaq_sleeve_bus_type;

extern int  ipaq_current_sleeve( void );   /* returns IPAQ_SLEEVE_ID(vendor,dev) */

/**
 * ipaq_sleeve_driver_register: Registers a driver for an iPAQ extension pack.
 * @drv: the sleeve driver
 */
extern int  ipaq_sleeve_driver_register(struct ipaq_sleeve_driver *drv);

/**
 * ipaq_sleeve_driver_unregister: Unregisters a driver for an iPAQ extension pack.
 * @drv: the sleeve driver
 */
extern void ipaq_sleeve_driver_unregister(struct ipaq_sleeve_driver *drv);

extern int ipaq_sleeve_egpio_irq(int egpio);

#define ipaq_sleeve_clear_egpio(n)		\
	ipaq_sleeve_write_egpio(n, 0)
#define ipaq_sleeve_set_egpio(n)		\
	ipaq_sleeve_write_egpio(n, 1)

extern void ipaq_sleeve_write_egpio(int nr, int state);
extern int ipaq_sleeve_read_egpio(int nr);
extern int ipaq_sleeve_egpio_irq(int nr);

#ifdef CONFIG_SLEEVE_DEBUG
#ifndef CONFIG_SLEEVE_DEBUG_VERBOSE
#define CONFIG_SLEEVE_DEBUG_VERBOSE 3
#endif
extern int g_sleeve_debug;
#define SLEEVE_DEBUG(n, args...)    \
	if (n <=  g_sleeve_debug) { \
		printk(KERN_DEBUG  "%s: ", __FUNCTION__); printk(args); \
	}
#else
#define SLEEVE_DEBUG(n, args...)
#endif /* CONFIG_SLEEVE_DEBUG */

struct ipaq_sleeve_version {
	char d[3];
};

/* Functions provided to generic ipaq-sleeve code by board support */
struct ipaq_sleeve_ops {
	int (*spi_read)(void *dev, unsigned short address, unsigned char *data, unsigned short len);
	int (*spi_write)(void *dev, unsigned short address, unsigned char *data, unsigned short len);
	int (*get_option_detect)(void *dev, int *result);
	int (*get_version)(void *dev, struct ipaq_sleeve_version *);
	int (*set_ebat)(void *dev);
	int (*control_egpio)(void *dev, int egpio, int state);
	int (*read_egpio)(void *dev, int egpio);
	int (*egpio_irq)(void *dev, int egpio);
	int (*read_battery)(void *dev, unsigned char *chem, unsigned char *percent, unsigned char *flag);
};

extern int ipaq_sleeve_register(struct ipaq_sleeve_ops *ops, void *dev);
extern int ipaq_sleeve_unregister(void);
extern irqreturn_t ipaq_sleeve_presence_interrupt(int irq, void *dev_id);

#endif /* _INCLUDE_IPAQ_SLEEVE_H_ */

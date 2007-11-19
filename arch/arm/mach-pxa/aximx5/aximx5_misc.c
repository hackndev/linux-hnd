/*
 * Miscelaneous device handlers for Dell Axim X5.
 * Tracks cradle insertion/removal, provides the MediaQ
 * platform device, provides SIR support.
 *
 * Copyright © 2004 Andrew Zabolotny
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/dock-hotplug.h>
#include <linux/platform_device.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/irqs.h>
#include <asm/arch/aximx5-gpio.h>

#include <linux/mfd/mq11xx.h>
#include "../drivers/usb/gadget/mq11xx_udc.h"

#if defined CONFIG_DOCKING_HOTPLUG || \
	(defined CONFIG_AXIMX5_MISC_MODULE && defined CONFIG_DOCKING_HOTPLUG_MODULE)
#  define DOCKING_HOTPLUG
#endif

#if 0
#  define debug(s, args...) printk (KERN_INFO s, ##args)
#else
#  define debug(s, args...)
#endif
#define debug_func(s, args...) debug ("%s: " s, __FUNCTION__, ##args)

#define MQ_BASE	0x14000000

extern int driver_pxa_ac97_wm97xx;

//-------------------------------------------// USB Device Controller //-----//

int aximx5_udc_is_connected (void)
{
	int connected = !GET_AXIMX5_GPIO (CRADLE_DETECT_N);
	printk("aximx5_udc_is_connected: %d\n", connected);
	return connected;
}

void aximx5_udc_command (int cmd)
{
	switch (cmd) {
	case MQ11XX_UDC_CMD_DISCONNECT:
		printk("aximx5_udc_command: disconnect\n");
		SET_AXIMX5_GPIO_N (USB_PULL_UP, 0);
		break;
	case MQ11XX_UDC_CMD_CONNECT:
		printk("aximx5_udc_command: connect\n");
		SET_AXIMX5_GPIO_N (USB_PULL_UP, 1);
		break;
	}
}


static struct mq11xx_udc_mach_info aximx5_udc_info = {
	.udc_is_connected = &aximx5_udc_is_connected,
	.udc_command	= &aximx5_udc_command,
};

#if 0
static struct platform_device aximx5_udc = {
	.name           = "mq11xx_udc",
	.dev            = {
		.platform_data  = &aximx5_udc_info,
		.release	= aximx5_udc_release
	}
};
#endif

/* MediaQ 1132 init state */
static struct mediaq11xx_init_data aximx5_mq1132_init_data = {
	/* DC */
	.DC = {
		/* dc00 */		0x00000001,
		/* dc01 */		0x00000003,
		/* dc02 */		0x00000001,
		/* dc03 NOT SET */	0x0,
		/* dc04 */		0x00000004,
		/* dc05 */		0x00000001,
	},
	/* CC */
	.CC = {
		/* cc00 */		0x00000000,
		/* cc01 */		0x00001010,
		/* cc02 */		0x000009a0,
		/* cc03 */		0xa2202200,
		/* cc04 */		0x00000004,
	},
	/* MIU */
	.MIU = {
		/* mm00 */		0x00000001,
		/* mm01 */		0x1b676ca8,
		/* mm02 */		0x00000000,
		/* mm03 */		0x00000000,
		/* mm04 */		0x00000000,
	},
	/* GC */
	.GC = {
		/* gc00 */		0x080100c8, /* powered down */
		/* gc01 */		0x00000000,
		/* gc02 */		0x00f0011a,
		/* gc03 */		0x013f015e,
		/* gc04 */		0x011100fa,
		/* gc05 */		0x01590157,
		/* gc06 */		0x00000000,
		/* gc07 NOT SET */	0x0,
		/* gc08 */		0x00ef0000,
		/* gc09 */		0x013f0000,
		/* gc0a */		0x00000000,
		/* gc0b */		0x011700f2,
		/* gc0c */		0x00000000,
		/* gc0d */		0x000091a6,
		/* gc0e */		0x000001e0,
		/* gc0f NOT SET */	0x0,
		/* gc10 */		0x031f071f,
		/* gc11 */		0x000000ff,
		/* gc12 NOT SET */	0x0,
		/* gc13 NOT SET */	0x0,
		/* gc14 */		0x00000000,
		/* gc15 */		0x00000000,
		/* gc16 */		0x00000000,
		/* gc17 */		0x00000000,
		/* gc18 */		0x00000000,
		/* gc19 */		0x00000000,
		/* gc1a */		0x00000000,
	},
	/* FP */
	.FP = {
		/* fp00 */		0x00406120,
		/* fp01 */		0x001d5008,
		/* fp02 */		0xbffcfcff,
		/* fp03 */		0x00000000,
		/* fp04 */		0x80bd0001,
		/* fp05 */		0x89000000,
		/* fp06 */		0x80000000,
		/* fp07 */		0x00000000,
		/* fp08 */		0x3afe46fb,
		/* fp09 NOT SET */	0x0,
		/* fp0a */		0x00000000,
		/* fp0b */		0x00000000,
		/* fp0c NOT SET */	0x0,
		/* fp0d NOT SET */	0x0,
		/* fp0e NOT SET */	0x0,
		/* fp0f */		0x00005ca0,
		/* fp10 */		0x513fc706,
		/* fp11 */		0x18f8b182,
		/* fp12 */		0xff8d6644,
		/* fp13 */		0xae133357,
		/* fp14 */		0x2b90bee1,
		/* fp15 */		0xa6fee27a,
		/* fp16 */		0x5aa04ee2,
		/* fp17 */		0xb4e957f8,
		/* fp18 */		0xd2fe9cfb,
		/* fp19 */		0x94ff70fb,
		/* fp1a */		0x7bfe5efb,
		/* fp1b */		0x3ab676ad,
		/* fp1c */		0x32e2ec5c,
		/* fp1d */		0xc1309546,
		/* fp1e */		0xf6933d7a,
		/* fp1f */		0x02bfa020,
		/* fp20 */		0x0,
		/* fp21 */		0x0,
		/* fp22 */		0x0,
		/* fp23 */		0x0,
		/* fp24 */		0x0,
		/* fp25 */		0x0,
		/* fp26 */		0x0,
		/* fp27 */		0x0,
		/* fp28 */		0x0,
		/* fp29 */		0x0,
		/* fp2a */		0x0,
		/* fp2b */		0x0,
		/* fp2c */		0x0,
		/* fp2d */		0x0,
		/* fp2e */		0x0,
		/* fp2f */		0x0,
		/* fp30 */		0xc3089f9a,
		/* fp31 */		0x4488419f,
		/* fp32 */		0x4131a146,
		/* fp33 */		0xdbc01ccd,
		/* fp34 */		0x7649f9d6,
		/* fp35 */		0xa0122146,
		/* fp36 */		0x1c2ed6e7,
		/* fp37 */		0x182d52e9,
		/* fp38 */		0xb3d06f09,
		/* fp39 */		0x663a5cf4,
		/* fp3a */		0x54fb278b,
		/* fp3b */		0x07181a1b,
		/* fp3c */		0x4d201fdd,
		/* fp3d */		0x082459ef,
		/* fp3e */		0xe044b973,
		/* fp3f */		0x749c7102,
		/* fp40 */		0x0,
		/* fp41 */		0x0,
		/* fp42 */		0x0,
		/* fp43 */		0x0,
		/* fp44 */		0x0,
		/* fp45 */		0x0,
		/* fp46 */		0x0,
		/* fp47 */		0x0,
		/* fp48 */		0x0,
		/* fp49 */		0x0,
		/* fp4a */		0x0,
		/* fp4b */		0x0,
		/* fp4c */		0x0,
		/* fp4d */		0x0,
		/* fp4e */		0x0,
		/* fp4f */		0x0,
		/* fp50 */		0x0,
		/* fp51 */		0x0,
		/* fp52 */		0x0,
		/* fp53 */		0x0,
		/* fp54 */		0x0,
		/* fp55 */		0x0,
		/* fp56 */		0x0,
		/* fp57 */		0x0,
		/* fp58 */		0x0,
		/* fp59 */		0x0,
		/* fp5a */		0x0,
		/* fp5b */		0x0,
		/* fp5c */		0x0,
		/* fp5d */		0x0,
		/* fp5e */		0x0,
		/* fp5f */		0x0,
		/* fp60 */		0x0,
		/* fp61 */		0x0,
		/* fp62 */		0x0,
		/* fp63 */		0x0,
		/* fp64 */		0x0,
		/* fp65 */		0x0,
		/* fp66 */		0x0,
		/* fp67 */		0x0,
		/* fp68 */		0x0,
		/* fp69 */		0x0,
		/* fp6a */		0x0,
		/* fp6b */		0x0,
		/* fp6c */		0x0,
		/* fp6d */		0x0,
		/* fp6e */		0x0,
		/* fp6f */		0x0,
		/* fp70 */		0x6e0812af,
		/* fp71 */		0x4116e7b4,
		/* fp72 */		0x7714bcdb,
		/* fp73 */		0x0,
		/* fp74 */		0x0,
		/* fp75 */		0x0,
		/* fp76 */		0x9bb93ec3,
		/* fp77 */		0xb2021123,
	},
	/* GE */
	.GE = {
		/* ge00 NOT SET */	0x0,
		/* ge01 NOT SET */	0x0,
		/* ge02 NOT SET */	0x0,
		/* ge03 NOT SET */	0x0,
		/* ge04 NOT SET */	0x0,
		/* ge05 NOT SET */	0x0,
		/* ge06 NOT SET */	0x0,
		/* ge07 NOT SET */	0x0,
		/* ge08 NOT SET */	0x0,
		/* ge09 NOT SET */	0x0,
		/* ge0a */		0x400001e0,
		/* ge0b */		0x00000000,
	},
	/* SPI */
	.SPI = {
		/* sp00 */		0x40481189,
		/* sp01 */		0,
		/* sp02 NOT SET */	0,
		/* sp03 NOT SET */	0,
		/* sp04 */		0x004ef9ff,
		/* sp05 NOT SET */	0,
		/* sp06 NOT SET */	0,
		/* sp07 */		0xfffffff0,
		/* sp08 */		0x002ffc7d,
	},

	.udc_info = &aximx5_udc_info,
};

static struct resource mq1132_resources[] = {
	/* Synchronous memory */
	[0] = {
		.start	= MQ_BASE,
		.end	= MQ_BASE + MQ11xx_FB_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	/* Non-synchronous memory */
	[1] = {
		.start	= MQ_BASE + MQ11xx_FB_SIZE + MQ11xx_REG_SIZE,
		.end	= MQ_BASE + MQ11xx_FB_SIZE * 2 - 1,
		.flags	= IORESOURCE_MEM,
	},
	/* MediaQ registers */
	[2] = {
		.start	= MQ_BASE + MQ11xx_FB_SIZE,
		.end	= MQ_BASE + MQ11xx_FB_SIZE + MQ11xx_REG_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	/* MediaQ interrupt number */
	[3] = {
		.start	= AXIMX5_IRQ (MQ1132_INT),
		.flags	= IORESOURCE_IRQ,
	}
};

struct platform_device aximx5_mq1132 = {
	.name		= "mq11xx",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(mq1132_resources),
	.resource	= mq1132_resources,
	.dev		= {
		.platform_data = &aximx5_mq1132_init_data,
	}
};

//-----------------------------------------------// Battery latch IRQ //-----//

static irqreturn_t aximx5_battery_latch(int irq, void *dev_id)
{
	/* todo: go to sleep immediately or something like that */
	return IRQ_HANDLED;
};

static void aximx5_init_latch (void)
{
	request_irq (AXIMX5_IRQ (BATTERY_LATCH), aximx5_battery_latch,
		     0, "Battery latch", NULL);
	set_irq_type (AXIMX5_IRQ (BATTERY_LATCH), IRQT_BOTHEDGE);
}

//----------------------------------------// Cradle insertion/removal //-----//

#ifdef DOCKING_HOTPLUG

static wait_queue_head_t cradle_event;
static volatile int cradle_event_count;

static struct dock_hotplug_caps_t dockcaps = {
	UNKNOWN_DOCKING_IDENTIFIER,
	0,
	DOCKCAPS_HOT
};

static irqreturn_t aximx5_cradle (int irq, void *dev_id, struct pt_regs *regs)
{
	cradle_event_count++;
	wake_up_interruptible (&cradle_event);
	return IRQ_HANDLED;
};

static int check_cradle (void)
{
	static int old_type = -1;
	int type = AXIMX5_CONNECTOR_TYPE;

	if (GET_AXIMX5_GPIO (CRADLE_DETECT_N))
		type = -1;

	if (type == old_type)
		return 0;

	/* If we're going in a dock, find out its type */
	if (type != 7 && type != -1) {
		dockcaps.flavour = AXIMX5_CONNECTOR_IS_CRADLE (type) ?
			DOCKFLAV_CRADLE : DOCKFLAV_CABLE;
		dockcaps.capabilities &= ~(DOCKCAPS_UDC | DOCKCAPS_RS232);
	}

	if (AXIMX5_CONNECTOR_IS_SERIAL (type))
		dockcaps.capabilities |= DOCKCAPS_RS232;
	else if (AXIMX5_CONNECTOR_IS_USB (type))
		dockcaps.capabilities |= DOCKCAPS_UDC;
	else {
		/* If we get a cradle code out of range ... */
		if (type >= 0 && type < 7)
			printk (KERN_INFO "Unknown connector=%d: "
				"please mail this to anpaza@mail.ru\n", type);
		if (old_type == -1)
			return 0;
		type = -1;
	}
	old_type = type;

	/* Setup RS-232 transceiver power */
	SET_AXIMX5_GPIO (RS232_DCD, dockcaps.capabilities & DOCKCAPS_RS232);

	return dock_hotplug_event(type != -1, &dockcaps);
}

/* This thread handles cradle insertion/removal */
static int cradle_thread (void *arg)
{
	int old_count;

	/* set up thread context */
	daemonize ("kcradle");
	/* This thread is low priority */
	set_user_nice (current, 5);
	/* At startup check if we're in cradle */
	goto again;

	for (;;) {
		wait_event_interruptible (cradle_event, old_count != cradle_event_count);

		/* Wait for a steady contact */
again:		do {
			old_count = cradle_event_count;
			set_task_state (current, TASK_INTERRUPTIBLE);
			schedule_timeout(HZ/2);
		} while (old_count != cradle_event_count);

		/* Now see what we have to do ... */
		if (check_cradle () == EAGAIN)
			goto again;
	}
}

static void aximx5_init_cradle (void)
{
	init_waitqueue_head (&cradle_event);
	kernel_thread (cradle_thread, NULL, 0);
	request_irq (AXIMX5_IRQ (CRADLE_DETECT_N), &aximx5_cradle,
		     0, "Cradle", NULL);
	set_irq_type (AXIMX5_IRQ (CRADLE_DETECT_N), IRQT_BOTHEDGE);
}

#endif

static int __init aximx5_misc_init(void)
{
	platform_device_register (&aximx5_mq1132);
//	platform_device_register (&aximx5_udc);
#ifdef DOCKING_HOTPLUG
	aximx5_init_cradle ();
#endif
	aximx5_init_latch ();
	return 0;
}

module_init (aximx5_misc_init);
/* No module_exit function since this driver should be never unloaded */

MODULE_AUTHOR ("Andrew Zabolotny <zap@homelink.ru>");
MODULE_DESCRIPTION ("Miscelaneous device support for Dell Axim X5");
MODULE_LICENSE ("GPL");

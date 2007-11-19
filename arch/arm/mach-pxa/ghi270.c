/*
 * Copyright (c) 2007 SDG Systems
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/gpio_keys.h>
#include <linux/ioport.h>
#include <linux/input.h>
#include <linux/input_pda.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/miscdevice.h>
#include <linux/backlight.h>
#include <linux/corgi_bl.h>
#include <linux/mtd/partitions.h>
#include <linux/proc_fs.h>
#include <linux/irq.h>

#include <asm/mach-types.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/setup.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/audio.h>
#include <asm/arch/bitfield.h>
#include <asm/arch/pxafb.h>
#include <asm/arch/serial.h>
#include <asm/arch/ghi270.h>
#include <asm/arch/ghi270-nand.h>
#include <asm/arch/udc.h>
#include <asm/arch/mmc.h>
#include <asm/arch/pxa27x_keyboard.h>
#include <asm/arch/ohci.h>
#include <asm/arch/irda.h>
#include <asm/arch/i2c.h>
#include <asm/arch/pxa2xx_spi.h>

#include "generic.h"

static void __init ghi270_map_io(void)
{
	pxa_map_io();

	MSC0 = 0x11d036d8;
}

/* bootloader flash */
static struct platform_device ghi270_flash = {
	.name = "ghi270-flash",
	.id = -1,
};

/* NAND flash */

static struct resource ghi270_nand_resources[] = {
	[0] = {
		.start	= PXA_CS1_PHYS,
		.end	= PXA_CS1_PHYS + 7,
		.flags	= IORESOURCE_MEM,
	},
};

static struct mtd_partition duramaxh_nand_partitions[] = {
	{
		.name	    = "params",
		.offset	    = 0,
		.size	    = 32768,
	},
	{
		.name	    = "kernel",
		.offset	    = MTDPART_OFS_NXTBLK,
		.size	    = 3 * 1024 * 1024,
	},
	{
		.name	    = "rootfs",
		.offset	    = MTDPART_OFS_NXTBLK,
		.size	    = MTDPART_SIZ_FULL,
	}
};

static struct ghi270_nand_data duramaxh_nand_data = {
	.rnb_gpio	= GHI270_H_GPIO106_NAND_RNB,
	.n_partitions	= ARRAY_SIZE(duramaxh_nand_partitions),
	.partitions	= duramaxh_nand_partitions
};

static struct platform_device ghi270_nand = {
	.name		= "ghi270-nand",
	.id		= -1,
	.dev		= {
		.platform_data = &duramaxh_nand_data,
	},
	.resource	= ghi270_nand_resources,
	.num_resources	= ARRAY_SIZE(ghi270_nand_resources),
};

/* MMC */

static struct pxamci_platform_data ghi270_mci_platform_data;

static int ghi270_mci_init(struct device *dev, irq_handler_t detect_handler,
			   void *data)
{
	int err;

	pxa_gpio_mode(GPIO32_MMCCLK_MD);
	pxa_gpio_mode(GPIO92_MMCDAT0_MD);
	pxa_gpio_mode(GPIO109_MMCDAT1_MD);
	pxa_gpio_mode(GPIO110_MMCDAT2_MD);
	pxa_gpio_mode(GPIO111_MMCDAT3_MD);
	pxa_gpio_mode(GPIO112_MMCCMD_MD);

	gpio_direction_input(GHI270_GPIO36_MMC_WP);
	gpio_direction_input(GHI270_GPIO38_MMC_CD);

	ghi270_mci_platform_data.detect_delay = msecs_to_jiffies(250);

	err = request_irq(gpio_to_irq(GHI270_GPIO38_MMC_CD), detect_handler,
		IRQF_DISABLED | IRQF_TRIGGER_RISING |
		IRQF_TRIGGER_FALLING, "MMC card detect", data);
	if (err) {
		printk(KERN_ERR "ghi270_mci_init: can't get MMC CD IRQ\n");
		return -1;
	}

	return 0;
}

static int ghi270_mci_get_ro(struct device *dev)
{
	return gpio_get_value(GHI270_GPIO36_MMC_WP);
}

static void ghi270_mci_exit(struct device *dev, void *data)
{
	free_irq(gpio_to_irq(GHI270_GPIO38_MMC_CD), data);
}

static struct pxamci_platform_data ghi270_mci_platform_data = {
	.ocr_mask       = MMC_VDD_32_33|MMC_VDD_33_34,
	.init           = ghi270_mci_init,
	.get_ro         = ghi270_mci_get_ro,
	.exit           = ghi270_mci_exit,
};


/* USB client */
static int ghi270_udc_detect(void)
{
	return gpio_get_value(GHI270_GPIO90_USBC_DET) ? 0 : 1;
}

static void ghi270_udc_command(int cmd)
{
	switch (cmd) {
		case PXA2XX_UDC_CMD_DISCONNECT:
			printk(KERN_NOTICE "USB cmd disconnect\n");
			gpio_set_value(GHI270_GPIO91_USBC_ACT, 0);
			break;

		case PXA2XX_UDC_CMD_CONNECT:
			printk(KERN_NOTICE "USB cmd connect\n");
			gpio_set_value(GHI270_GPIO91_USBC_ACT, 1);
			break;
	}
}

static struct pxa2xx_udc_mach_info ghi270_udc_mach_info __initdata = {
	.udc_command		= ghi270_udc_command,
	/* XXX The following are not implemented for pxa27x_udc. */
	.udc_is_connected	= ghi270_udc_detect,
	.gpio_vbus		= GHI270_GPIO90_USBC_DET,
	.gpio_pullup		= GHI270_GPIO91_USBC_ACT,
};


/* fb/lcd */

static void ghi270_lcd_power(int on, struct fb_var_screeninfo *si)
{
	/* There is no control over the power to the LCD.  Yes, that seems
	 * rather strange, but it's the way Grayhill did it. */
}

static void ghi270_backlight_power(int on)
{
	gpio_set_value(GPIO16_PWM0, on ? 1 : 0);
}

static struct pxafb_mode_info ghi270_pxafb_modes[] = {
	{
		.pixclock       = 156000,   /* from datasheet: 156 ns = 6.39MHz */
		.bpp            = 16,
		.xres           = 240,
		.yres           = 320,
		.hsync_len      = 11,
		.vsync_len      = 3,
		.left_margin    = 19,
		.right_margin   = 10,
		.upper_margin   = 2,
		.lower_margin   = 2,
	},
};

static struct pxafb_mach_info ghi270_pxafb_mach_info = {
	.modes          = ghi270_pxafb_modes,
	.num_modes      = 1,
	.fixed_modes    = 1,
	.lccr0          = LCCR0_Act | LCCR0_Sngl | LCCR0_Color | LCCR0_LDDALT,
	.lccr3          = LCCR3_OutEnH | LCCR3_PixRsEdg | LCCR3_Acb(0xff),
	.pxafb_backlight_power = ghi270_backlight_power,
	.pxafb_lcd_power = ghi270_lcd_power,
};

static struct platform_device ghi270_audio = {
	.name           = "pxa2xx-ac97",
	.id             = -1,
};

static struct platform_device ghi270hg_barometer = {
	.name           = "duramax-bar",
	.id             = -1,
};


/* LCD backlight */

#define GHI270_MAX_BRIGHTNESS 0xff

static void ghi270_set_bl_intensity(int intensity)
{
	PWM_CTRL0   = 0x3f;
	PWM_PERVAL0 = GHI270_MAX_BRIGHTNESS - 1;
	PWM_PWDUTY0 = GHI270_MAX_BRIGHTNESS - (56 * intensity/ 256);
}

static struct corgibl_machinfo ghi270_bl_machinfo = {
	.max_intensity		= GHI270_MAX_BRIGHTNESS,
	.default_intensity	= GHI270_MAX_BRIGHTNESS,
	.set_bl_intensity	= ghi270_set_bl_intensity
};

static struct platform_device ghi270_bl = {
	.name		= "corgi-bl",
	.id		= 0,
	.dev		= {
		    .platform_data = &ghi270_bl_machinfo,
	},
};

/* keypad backlight led */

static struct platform_device ghi270_key_led = {
	.name	= "ghi270-leds",
	.id	= -1,
};

/* keypad */
static struct pxa27x_keyboard_platform_data ghi270_h_kbd_data = {
	.nr_rows = 7,
	.nr_cols = 6,
	.keycodes = {
		/*
		 * Note:  Qtopia assumes portrait mode, so we have to define
		 * the keys in protrait mode.  When Qtopia rotates the screen,
		 * it also rotates the keys, so they're then right.
		 */
		{
			/* row 0 */
			KEY_1,
			KEY_2,
			KEY_3,
			-1,
			KEY_LEFT, /* KEY_UP, */
			KEY_F1	  /* top bar */
		},
		{
			/* row 1 */
			KEY_4,
			KEY_5,
			KEY_6,
			-1,
			KEY_DOWN, /* KEY_LEFT, */
			KEY_HOME  /* windows */
		},
		{
			/* row 2 */
			KEY_7,
			KEY_8,
			KEY_9,
			-1,
			KEY_ENTER,  /* enter next to arrows */
			KEY_F3	    /* light */
		},
		{
			/* row 3 */
			KEY_ESC,
			KEY_0,
			KEY_ENTER,  /* enter next to numbers */
			-1,
			KEY_UP,     /* KEY_RIGHT, */
			KEY_F4	    /* keyboard */
		},
		{
			/* row 4 */
			-1,
			-1,
			-1,
			-1,
			-1,
			-1
		},
		{
			/* row 5 */
			-1,
			-1,
			-1,
			-1,
			-1,
			-1
		},
		{
			/* row 6 */
			-1,
			-1,
			-1,
			-1,
			KEY_RIGHT,  /* KEY_DOWN, */
			KEY_F5	    /* bottom bar */
		}
	},
	/*
	 * Note that the code which uses .gpio_modes assumes that it has
	 * nr_rows + nr_cols entries in it (see
	 * drivers/input/keyboard/pxa27x_keyboard.c).  This is a wrong
	 * assumption, but to make it happy, we duplicate entries below to get
	 * the full 13.
	 */
	.gpio_modes = {
		GPIO100_KP_MKIN0_MD,
		GPIO101_KP_MKIN1_MD,
		GPIO102_KP_MKIN2_MD,
		GPIO97_KP_MKIN3_MD,
		GPIO97_KP_MKIN3_MD,  /* Pad to fill in 4, unused */
		GPIO97_KP_MKIN3_MD,  /* Pad to fill in 5, unused */
		GPIO95_KP_MKIN6_MD,
		GPIO103_KP_MKOUT0_MD,
		GPIO104_KP_MKOUT1_MD,
		GPIO105_KP_MKOUT2_MD,
		GPIO105_KP_MKOUT2_MD,	/* GPIO106 is used for Nand RDYnBUSY */
		GPIO107_KP_MKOUT4_MD,
		GPIO108_KP_MKOUT5_MD,
	},
};

static struct pxa27x_keyboard_platform_data ghi270_hg_kbd_data = {
	.nr_rows = 2,
	.nr_cols = 8,
	.keycodes = {
		{
			/* row 0 */
			KEY_DOWN,
			-1,
			KEY_LEFT,
			-1,
			KEY_UP,
			-1,
			KEY_RIGHT,
			-1,
		},
		{
			/* row 1 */
			KEY_ENTER,
			KEY_HOME,
			KEY_F9,
			KEY_F10,
			KEY_F11,
			KEY_ESC,
			-1,
			-1,
		},
	},
	.gpio_modes = {
		GPIO100_KP_MKIN0_MD,
		GPIO101_KP_MKIN1_MD,
		GPIO103_KP_MKOUT0_MD,
		GPIO104_KP_MKOUT1_MD,
		GPIO105_KP_MKOUT2_MD,
		GPIO106_KP_MKOUT3_MD,
		GPIO107_KP_MKOUT4_MD,
		GPIO108_KP_MKOUT5_MD,
		GPIO35_KP_MKOUT6_MD,
		GPIO41_KP_MKOUT7_MD,
	},
};

static struct platform_device ghi270_kbd = {
	.name   = "pxa27x-keyboard",
	.id     = -1,
	.dev    =  {
		.platform_data  = &ghi270_h_kbd_data,
	},
};

/*
 * STUART is used for GPS
 */
static void gps_configure(int state)
{
	gpio_set_value(GHI270_HG_GPIO116_GPS_nRESET, 1);
	switch(state) {
		case PXA_UART_CFG_PRE_STARTUP:
			gpio_set_value(GHI270_HG_GPIO115_GPS_nSLEEP, 1);
			break;
		case PXA_UART_CFG_POST_SHUTDOWN:
			gpio_set_value(GHI270_HG_GPIO115_GPS_nSLEEP, 0);
			break;
	}
}
static struct platform_pxa_serial_funcs ghi270_gps_funcs = {
	.configure = gps_configure,
};

/*
 * USB Host (OHCI)
 */
static int ghi270_ohci_init(struct device *dev)
{
	/* setup Port1 GPIO pin. */
	pxa_gpio_mode(GPIO88_USBHPWR1 | GPIO_ALT_FN_1_IN);	/* USBHPWR1 */
	pxa_gpio_mode(GPIO89_USBHPEN1 | GPIO_ALT_FN_2_OUT);	/* USBHPEN1 */

	/* Set the Power Control Polarity Low and Power Sense
	   Polarity Low to active low. */
	UHCHR = (UHCHR | UHCHR_PCPL | UHCHR_PSPL) &
		~(UHCHR_SSEP1 | UHCHR_SSEP2 | UHCHR_SSEP3 | UHCHR_SSE);

	return 0;
}

static struct pxaohci_platform_data ghi270_ohci_platform_data = {
	.port_mode	= PMM_PERPORT_MODE,
	.init		= ghi270_ohci_init,
};

static struct platform_device *ghi270_devices[] __initdata = {
	&ghi270_flash,
	&ghi270_nand,
	&ghi270_audio,
	&ghi270_bl,
	&ghi270_key_led,
	&ghi270_kbd,
	&ghi270hg_barometer,
};

extern struct pm_ops pxa_pm_ops;

static int (*pm_enter_orig)(suspend_state_t state);

static unsigned int Resume_GPCR0, Resume_GPCR1, Resume_GPCR2, Resume_GPCR3;
static unsigned int Resume_GPSR0, Resume_GPSR1, Resume_GPSR2, Resume_GPSR3;
static unsigned int Resume_GPDR0, Resume_GPDR1, Resume_GPDR2, Resume_GPDR3;
static unsigned int Resume_GAFR0_L, Resume_GAFR0_U;
static unsigned int Resume_GAFR1_L, Resume_GAFR1_U;
static unsigned int Resume_GAFR2_L, Resume_GAFR2_U;
static unsigned int Resume_GAFR3_L, Resume_GAFR3_U;

static int ghi270_pm_enter(suspend_state_t state)
{
	int ret;

	/* Only suspend is supported on ghi270 devices */
	if(state != PM_SUSPEND_STANDBY) {
		state = PM_SUSPEND_STANDBY;
	}

	PWER = PWER_RTC   | PWER_GPIO0;
	PFER = PWER_GPIO0;		// Falling Edge Detect
	PRER = 0;			// Rising Edge Detect

	/*
	 * Save the GPIO settings for resume.  We do this by anding the
	 * direction register and the current state of the GPIOs.  Then we set
	 * the suspend state.  The other settings are already stored in the
	 * pm.c, but we have to store them here because it will store only the
	 * suspend values, but we want things to be as we left them before
	 * suspend.
	 */
	Resume_GPCR0 = GPDR0 & ~GPLR0;
	Resume_GPCR1 = GPDR1 & ~GPLR1;
	Resume_GPCR2 = GPDR2 & ~GPLR2;
	Resume_GPCR3 = GPDR3 & ~GPLR3;

	Resume_GPSR0 = GPDR0 & GPLR0;
	Resume_GPSR1 = GPDR1 & GPLR1;
	Resume_GPSR2 = GPDR2 & GPLR2;
	Resume_GPSR3 = GPDR3 & GPLR3;

	Resume_GPDR0 = GPDR0;
	Resume_GPDR1 = GPDR1;
	Resume_GPDR2 = GPDR2;
	Resume_GPDR3 = GPDR3;

	Resume_GAFR0_L = GAFR0_L;
	Resume_GAFR0_U = GAFR0_U;
	Resume_GAFR1_L = GAFR1_L;
	Resume_GAFR1_U = GAFR1_U;
	Resume_GAFR2_L = GAFR2_L;
	Resume_GAFR2_U = GAFR2_U;
	Resume_GAFR3_L = GAFR3_L;
	Resume_GAFR3_U = GAFR3_U;

	/* Standby Values */
	GPSR0 = 0x00008200;
	GPSR1 = 0x00608002;
	GPSR2 = 0x0041c000;
	GPSR3 = 0x00100000;

	GPCR0 = 0xc39b1c04;
	GPCR1 = 0xfc8f2b89;
	GPCR2 = 0x6a343fff;
	GPCR3 = 0x01aa1f80;

	GPDR0 = 0xC39B9E04;
	GPDR1 = 0xFCEFAB8B;
	GPDR2 = 0x6A75FFFF;
	GPDR3 = 0x01BA1F80;

	GAFR0_L = 0x80000000;
	GAFR0_U = 0x00188002;
	GAFR1_L = 0x09900000;
	GAFR1_U = 0x00050000;
	GAFR2_L = 0x00000000;
	GAFR2_U = 0x00000000;
	GAFR3_L = 0x00000000;
	GAFR3_U = 0x00000000;

	/* Turn off the 13 MHz Oscillator */
	PCFR |= 1;
	PSTR = 4;

	{
		/* As per PXA270 manual, wait for OOK when turning off
			* 13 MHz Oscillator. */
		volatile int i;
		for(i = 0; i < 100000; i++) {
			if(OSCC & (1U << 0)) {
				break;
			}
		}
	}

	/* Turn off peripheral power. */
	gpio_set_value(GHI270_GPIO9_PRF_EN, 0);

	/* Sleep. */
	ret = pm_enter_orig(state);

	/* Turn on the 13 MHz Oscillator */
	PCFR |= (1U << 0);

	GPDR0 = Resume_GPDR0;
	GPDR1 = Resume_GPDR1;
	GPDR2 = Resume_GPDR2;
	GPDR3 = Resume_GPDR3;

	GAFR0_L = Resume_GAFR0_L;
	GAFR0_U = Resume_GAFR0_U;
	GAFR1_L = Resume_GAFR1_L;
	GAFR1_U = Resume_GAFR1_U;
	GAFR2_L = Resume_GAFR2_L;
	GAFR2_U = Resume_GAFR2_U;
	GAFR3_L = Resume_GAFR3_L;
	GAFR3_U = Resume_GAFR3_U;

	GPCR0 = Resume_GPCR0;
	GPCR1 = Resume_GPDR1;
	GPCR2 = Resume_GPCR2;
	GPCR3 = Resume_GPCR3;

	GPSR0 = Resume_GPSR0;
	GPSR1 = Resume_GPSR1;
	GPSR2 = Resume_GPSR2;
	GPSR3 = Resume_GPSR3;

	/* Turn on peripheral power. */
	pxa_gpio_mode(GHI270_GPIO9_PRF_EN | GPIO_OUT);
	gpio_set_value(GHI270_GPIO9_PRF_EN, 1);

	/* There is a bug in the PXA270 that causes glitches on the power I2C
	 * bus if these are set.  So make sure they're cleared. */
	gpio_set_value(3, 0);
	gpio_set_value(4, 0);

	return ret;
}

/* The following are tags used by SDGBoot */
static int bootversion;
static int bootdate;
static int __init
do_bootversion(const struct tag *tag)
{
	bootversion = tag->u.initrd.start;
	bootdate = tag->u.initrd.size;
	return 0;
}
__tagtable(0xadbadbae, do_bootversion);

static int nand_size_mb;
static int __init
do_nand_size_tag(const struct tag *tag)
{
	nand_size_mb = tag->u.initrd.start;
	printk("Flash Storage: %d MB\n", nand_size_mb);
	return 0;
}
__tagtable(0xadbadbad, do_nand_size_tag);

static int boot_flags;
static int __init
do_bootflags(const struct tag *tag)
{
	boot_flags = tag->u.initrd.start;
	printk("Boot Flags: 0x%08x\n", boot_flags);
	return 0;
}
__tagtable(0xadbadbac, do_bootflags);

static int
duramax_boot_read(char *page, char **start, off_t off, int count, int *eof, void *junk)
{
	int len;
	len = 0;
	len += sprintf(page + len, "0x%08x\n", boot_flags);
	return len;
}

static int
duramax_info_read(char *page, char **start, off_t off, int count, int *eof, void *junk)
{
	int len;
	int MHz;

	MHz = (((CCCR >> 7) & 0xf) * ((CCCR >> 0) & 0x1f) * 13) / 2;
	len = 0;
	len += sprintf(page + len, "Manufacturer: Grayhill\n");
	if(machine_is_ghi270hg()) {
		len += sprintf(page + len, "Device: Duramax HG\n");
	} else {
		len += sprintf(page + len, "Device: Duramax H\n");
	}
	len += sprintf(page + len, "Processor: PXA270\n");
	len += sprintf(page + len, "Speed: %d MHz\n", MHz);
	len += sprintf(page + len, "SDGBoot Version: %d@%08x\n", bootversion, bootdate);
	return len;
}


static void __init ghi270_init(void)
{
	/* Configure 3.3V peripheral power gpio. */
	gpio_set_value(GHI270_GPIO9_PRF_EN, 1);
	gpio_direction_output(GHI270_GPIO9_PRF_EN);

	/* Configure LCD. */
	ghi270_pxafb_modes[0].left_margin = 18;
	ghi270_pxafb_modes[0].upper_margin = 1;

	/* Set PCDDIV = 0 since pxafb doesn't know about it. */
	LCCR4 &= ~LCCR4_PCDDIV;

	set_pxa_fb_info(&ghi270_pxafb_mach_info);

	/* Configure LCD backlight PWM. */
	pxa_gpio_mode(GPIO16_PWM0_MD);

	/* Configure key backlight PWM. */
	pxa_gpio_mode(GHI270_GPIO11_KEYPAD_LED | GPIO_ALT_FN_2_OUT);

	/* Configure USB client. */
	gpio_direction_input(GHI270_GPIO90_USBC_DET);
	gpio_direction_output(GHI270_GPIO91_USBC_ACT);
	pxa_set_udc_info(&ghi270_udc_mach_info);

	/* Configure MMC. */
	pxa_set_mci_info(&ghi270_mci_platform_data);

	/* Configure ts. */
	gpio_direction_input(GHI270_GPIO52_EXT_GPIO0);

	if(machine_is_ghi270hg()) {
		((struct ghi270_nand_data *)ghi270_nand.dev.platform_data)
			->rnb_gpio = GHI270_HG_GPIO87_NAND_RNB;
		gpio_direction_input(GHI270_HG_GPIO87_NAND_RNB);
		ghi270_kbd.dev.platform_data = &ghi270_hg_kbd_data;
	} else {
		((struct ghi270_nand_data *)ghi270_nand.dev.platform_data)
			->rnb_gpio = GHI270_H_GPIO106_NAND_RNB;
		gpio_direction_input(GHI270_H_GPIO106_NAND_RNB);
		ghi270_kbd.dev.platform_data = &ghi270_h_kbd_data;
	}

	/* Add all the devices. */
	platform_add_devices(ghi270_devices, ARRAY_SIZE(ghi270_devices));

	/* Notify system of OHCI support */
	pxa_set_ohci_info(&ghi270_ohci_platform_data);

	/* Set up STUART for GPS interface. */
	pxa_gpio_mode(GPIO46_STRXD_MD);
	pxa_gpio_mode(GPIO47_STTXD_MD);
	pxa_set_stuart_info(&ghi270_gps_funcs);

	/* Hook into pm_enter. */
	pm_enter_orig = pxa_pm_ops.enter;
	pxa_pm_ops.enter = ghi270_pm_enter;

	create_proc_read_entry("platforminfo", 0, 0, duramax_info_read, 0);
	create_proc_read_entry("boot", 0, 0, duramax_boot_read, 0);
}

MACHINE_START(GHI270, "Grayhill Duramax H")
        /* Maintainer SDG Systems */
	.phys_io	= 0x40000000,
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
	.boot_params	= 0xa0000100,
	.map_io		= ghi270_map_io,
	.init_irq	= pxa_init_irq,
	.timer		= &pxa_timer,
	.init_machine	= ghi270_init,
MACHINE_END

MACHINE_START(GHI270HG, "Grayhill Duramax HG")
        /* Maintainer SDG Systems */
	.phys_io	= 0x40000000,
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
        .boot_params	= 0xa0000100,
        .map_io		= ghi270_map_io,
        .init_irq	= pxa_init_irq,
        .timer		= &pxa_timer,
        .init_machine	= ghi270_init,
MACHINE_END
/* vim600: set noexpandtab sw=8 : */

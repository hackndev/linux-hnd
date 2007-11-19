/*
 * linux/arch/arm/mach-sa1100/jornada720.c
 *
 * HP Jornada720 init code
 *
 * Copyright (C) 2006 Filip Zyzniewski <filip.zyzniewski@tefnet.pl>
 *  Copyright (C) 2005 Michael Gernoth <michael@gernoth.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/ioport.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>

#include <asm/hardware.h>
#include <asm/hardware/sa1111.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/setup.h>
#include <asm/arch/jornada720.h>
#include <asm/apm.h>

#include <asm/mach/arch.h>
#include <asm/mach/flash.h>
#include <asm/mach/map.h>
#include <asm/mach/serial_sa1100.h>
#include <asm/mach/flash.h>
#include <asm/mach/irda.h>

#include <linux/lcd.h>
#include <linux/backlight.h>
#include <linux/fb.h>

#include "generic.h"

#ifdef CONFIG_SA1100_JORNADA720_FLASH
static unsigned char *pRegs = REGISTER_OFFSET;
void e1356fb_init_hardware(void);
#endif

/***********************************
 *                                 *
 * Jornada 720 mcu functions       *
 *                                 *
 ***********************************/

static char mcu_invert[256];

void jornada720_init_ser(void)
{
int i;

	GPSR = GPIO_GPIO25;
	Ser4SSCR0 = 0x0307;
	Ser4MCCR0 = 0;
	Ser4SSSR = 0; 
	Ser4SSCR1 = 0x18;
	Ser4SSCR0 = 0x0387;
	while (Ser4SSSR & SSSR_RNE)
	   i = Ser4SSDR;  
 }

int mcu_byte(int arg_data)
{
        int i;

	while ((Ser4SSSR & SSSR_TNF) == 0);
	i = 0;
	while ((GPLR & 0x400) && i++ < 400000)
		; /* wait for MCU */
	if (i >= 400000) {
		printk("mcu_byte: timed out\n");
                return -1;
        }
	Ser4SSDR = mcu_invert[arg_data] << 8;
	udelay(100);
	while ((Ser4SSSR & SSSR_RNE) == 0);
	i = Ser4SSDR;
	if (i > 0xff)
		printk("mcu_byte: read %x\n", i);
	return mcu_invert[ i & 0xff ] & 0xff;
}

int mcu_start(int arg_data)
{
        int i;

	jornada720_mcu_init();
	GPCR = GPIO_GPIO25; /* clear -> enable */
	udelay(100);
	i = mcu_byte(arg_data);
	if (i != MCU_TxDummy)
	{
		printk("mcu_start: sent %x got %x\n", arg_data, i);
	        for (i = 0; i < 256; i++)
			if (mcu_read() == -1)
 				break;
		jornada720_init_ser();
		return -1;
  	}
  	return 0;
}

void mcu_end(void)
{
	udelay(100);
	GPSR = GPIO_GPIO25; /* set */
}

void jornada720_mcu_init(void)
{
        int i;
        static int initialized = 0;

	if (initialized)
		return;
 	initialized = 1;
	PPSR &= ~PPC_L_FCLK;
	udelay(500);
	PPDR |= PPC_L_FCLK;
	udelay(500);
	/* Take the MCU out of reset mode */
	PPSR |= PPC_L_FCLK;
	udelay(500);
	for (i = 0; i <= 255; i++)
		mcu_invert[i] = ( ((0x80 & i) >> 7) | ((0x40 & i) >> 5)
                                  | ((0x20 & i) >> 3) | ((0x10 & i) >> 1) | ((0x08 & i) << 1)
                                  | ((0x04 & i) << 3) | ((0x02 & i) << 5) | ((0x01 & i) << 7) );

	GPSR = GPIO_GPIO25; /* set */
	GPDR |= GPIO_GPIO25; /* GPIO(25) low -> enable MCU */
	Ser4SSCR0 = 0x0307;
	Ser4MCCR0 = 0;
	Ser4SSSR = 0;	/* remove any rcv overrun errors */
	Ser4SSCR1 = 0x18;
	Ser4SSCR0 = 0x0387;

	while (Ser4SSSR & SSSR_RNE)
		i = Ser4SSDR;		/* drain any data already there */
}

/***********************************
 *                                 *
 * Jornada 720 screen controls     *
 *                                 *
 ***********************************/

static int jornada720_lcd_set_power(struct lcd_device *ld, int state)
{
	if ( state ) {
		PPSR &= ~PPC_LDD1;
		PPDR |= PPC_LDD1;
	}
	else {
		PPSR |= PPC_LDD1;
	}
	return 0;
}

static int jornada720_lcd_get_contrast(struct lcd_device *ld)
{
	int contrast;
	mcu_start(MCU_GetContrast);
	contrast = mcu_read();
	mcu_end();
	return contrast;
}

static int jornada720_lcd_set_contrast(struct lcd_device *ld, int contrast)
{
	mcu_start(MCU_SetContrast);
	mcu_byte(contrast);
	mcu_end();
	return 0;
}

static int jornada720_backlight_get_brightness (struct backlight_device *bd)
{
	int brightness;
	mcu_start(MCU_GetBrightness);
	brightness = mcu_read();
	mcu_end();
	return brightness;
}

static int jornada720_backlight_set_brightness (struct backlight_device *bd, int value)
{
	int brightness = 255 - value; /* brightness hack for the user environments */
	mcu_start(MCU_SetBrightness);
	mcu_byte(brightness); /* range is from 0 (brightest) to 255 (darkest) */
	mcu_end();
	return 0;
}


static struct backlight_properties jornada720_backlight_properties = {
	.owner          = THIS_MODULE,
	.get_brightness = jornada720_backlight_get_brightness,
	.set_brightness = jornada720_backlight_set_brightness,
	.max_brightness = 255,
};

static struct lcd_properties jornada720_lcd_properties = {
	.owner		= THIS_MODULE,
	.set_power	= jornada720_lcd_set_power,
	.set_contrast	= jornada720_lcd_set_contrast,
	.get_contrast	= jornada720_lcd_get_contrast,
	.max_contrast   = 255,
};

/***********************************
 *                                 *
 * Jornada 720 IrDA                *
 *                                 *
 ***********************************/
 
static int jornada720_irda_set_power(struct device *dev, unsigned int state)
{
	if (state) {
		PPSR &= ~PPC_L_BIAS;
		PPDR |= PPC_L_BIAS;
	}
	else
		PPSR |= PPC_L_BIAS;
		
	return 0;
}

static struct irda_platform_data jornada720_irda_data = {
	.set_power	= jornada720_irda_set_power,
};

#ifdef CONFIG_SA1100_JORNADA720_FLASH
/***********************************
 *                                 *
 * Jornada 720 system flash        *
 *                                 *
 ***********************************/

static struct mtd_partition jornada720_partitions[] = {
	{
		.name		= "bootldr",
		.size		= 0x00040000,
		.offset		= 0,
		.mask_flags	= MTD_WRITEABLE,
	}, {
		.name		= "rootfs",
		.size		= MTDPART_SIZ_FULL,
		.offset		= MTDPART_OFS_APPEND,
	}
};

static void jornada720_set_vpp(int vpp)
{
	if (vpp)
		PPSR |= PPC_LDD7;
	else
		PPSR &= ~PPC_LDD7;
	PPDR |= PPC_LDD7;
};

static struct flash_platform_data jornada720_flash_data = {
	.map_name	= "cfi_probe",
	.parts		= jornada720_partitions,
	.nr_parts	= ARRAY_SIZE(jornada720_partitions),
	.set_vpp	= jornada720_set_vpp,
};

static struct resource jornada720_flash_resource = {
	.start	= SA1100_CS0_PHYS,
	.end	= SA1100_CS0_PHYS + SZ_32M - 1,
	.flags	= IORESOURCE_MEM,
};
#endif

/***********************************
 *                                 *
 * Jornada 720 apm functions       *
 *                                 *
 ***********************************/

typedef void (*apm_get_power_status_t)(struct apm_power_info*);

static void jornada720_apm_get_power_status(struct apm_power_info* info)
{
	int value[2];
	mcu_start(MCU_GetBatteryData);
	value[0] = mcu_read();
	value[1] = mcu_read();
	value[2] = mcu_read();
	mcu_end();
	
	value[0] |= (value[2] & 3) << 8; 	/* main battery value */
	value[1] |= (value[2] & 0xc) << 6; 	/* backup battery value, unused currently */
	
	info->battery_life = ((value[0] - 430) * 10) / 23; /* to get an accurate 100% - 0% range */
	
	if (GPLR & GPIO_GPIO4) 			/* AC line status connected to GPIO4; 1 == Battery, 0 == AC */
		info->ac_line_status = APM_AC_OFFLINE;
	else
		info->ac_line_status = APM_AC_ONLINE;
		
	if (!(GPLR & GPIO_GPIO26)) 		/* Battery status connected to GPIO26; 0 == charging, 1 == not charging */
		info->battery_status = APM_BATTERY_STATUS_CHARGING;
	else
		info->battery_status = 2 - info->battery_life / 36;	 /* this fits quite well */

	return;
}

int set_apm_get_power_status( apm_get_power_status_t t)
{
	apm_get_power_status = t;
	return 0;
}

static int jornada720_probe(struct device *dev)
{
#ifdef CONFIG_SA1100_JORNADA720_FLASH
	sa11x0_set_flash_data(&jornada720_flash_data, &jornada720_flash_resource, 1); //set flash data
#endif
		
	jornada720_mcu_init();	/* initialize the mcu... */
		
	jornada720_lcd_set_contrast(0, 115); /* initial contrast setting */
		
	backlight_device_register("e1356fb", 0, &jornada720_backlight_properties);
	lcd_device_register("e1356fb", 0, &jornada720_lcd_properties);
		
	set_apm_get_power_status(jornada720_apm_get_power_status); /* apm handler */
	
	sa11x0_set_irda_data(&jornada720_irda_data);
		
	return 0;
}

#ifdef CONFIG_SA1100_JORNADA720_FLASH
struct pm_save_data {
	int contrast;
	int brightness;
};

static int jornada720_suspend(struct device *dev, u32 state, u32 level)
{
	struct pm_save_data *save;
	
	if ( level != SUSPEND_DISABLE)
		return 0;
		
	save = kmalloc(sizeof(struct pm_save_data), GFP_KERNEL);
	if (!save)
		return -ENOMEM;
	dev->power.saved_state = save;
	
	/* disable keyboard and mouse irq as they would kill the whole suspend/resume cycle */
	disable_irq(GPIO_JORNADA720_KEYBOARD_IRQ);
	disable_irq(GPIO_JORNADA720_MOUSE_IRQ);
	
	PPSR |= PPC_L_FCLK;	/* make sure the mcu is not being reset */
	
	/* save current contrast and brightness */
	save->contrast		= jornada720_lcd_get_contrast(0);
	save->brightness	= jornada720_backlight_get_brightness(0);
	
	pRegs[0x1F0] = 1; /* put framebuffer into zZz */

	return 0;
}

static int jornada720_resume(struct device *dev, u32 level)
{
	struct pm_save_data *save;
	
	if (level != RESUME_ENABLE)
		return 0;

	save = (struct pm_save_data *)dev->power.saved_state;
	if (!save)
		return 0;
	
	GPSR = GPIO_GPIO8; /* reenable the rs232 receiver */
	e1356fb_init_hardware(); /* wake up the framebuffer */
	jornada720_init_ser(); /* reinitialize the mcu */
	
	/* restore saved contrast and brightness */
	jornada720_lcd_set_contrast(0,save->contrast);
	jornada720_backlight_set_brightness(0,save->brightness);
	
	TUCR = JORTUCR_VAL; /* this gets lost during zZz */

	/* now reenable keyboard and mouse irq */
	enable_irq(GPIO_JORNADA720_KEYBOARD_IRQ);
	enable_irq(GPIO_JORNADA720_MOUSE_IRQ);

	return 0;
}
#else
#define jornada720_suspend NULL
#define jornada720_resume NULL
#endif

static struct device_driver jornada720_driver = {
		.name		= "jornada720",
		.bus		= &platform_bus_type,
		.probe		= jornada720_probe,
		.suspend	= jornada720_suspend,
		.resume		= jornada720_resume,
};

/***********************************
 *                                 *
 * Jornada 720 system functions    *
 *                                 *
 ***********************************/
 
static struct platform_device jornada720_device = {
	.name		= "jornada720",
	.id		= 0,
};

static struct resource sa1111_resources[] = {
	[0] = {
		.start		= 0x40000000,
		.end		= 0x40001fff,
		.flags		= IORESOURCE_MEM,
	},
	[1] = {
		.start		= IRQ_GPIO1,
		.end		= IRQ_GPIO1,
		.flags		= IORESOURCE_IRQ,
	},
};

static u64 sa1111_dmamask = 0xffffffffUL;

static struct platform_device sa1111_device = {
	.name		= "sa1111",
	.id		= 0,
	.dev		= {
		.dma_mask = &sa1111_dmamask,
		.coherent_dma_mask = 0xffffffff,
	},
	.num_resources	= ARRAY_SIZE(sa1111_resources),
	.resource	= sa1111_resources,
};

static struct platform_device *devices[] __initdata = {
	&sa1111_device,
	&jornada720_device,
};

static void __init jornada720_init(void)
{

	if (machine_is_jornada720()) {
		GPDR |= GPIO_GPIO20;
		TUCR = JORTUCR_VAL;	/* set the oscillator out to the SA-1101 */

		GPSR = GPIO_GPIO20;
		udelay(1);
		GPCR = GPIO_GPIO20;
		udelay(1);
		GPSR = GPIO_GPIO20;
		udelay(20);

		platform_add_devices(devices, ARRAY_SIZE(devices));
		
		driver_register(&jornada720_driver);
	}
}

static struct map_desc jornada720_io_desc[] __initdata = {
	{	/* Epson registers */
		.virtual	= 0xf0000000,
		.pfn		= __phys_to_pfn(EPSONREGSTART),
		.length		= EPSONREGLEN,
		.type		= MT_DEVICE
	}, {	/* Epson frame buffer */
		.virtual	= 0xf1000000,
		.pfn		= __phys_to_pfn(EPSONFBSTART),
		.length		= EPSONFBLEN,
		.type		= MT_DEVICE
	}, {	/* SA-1111 */
		.virtual	= 0xf4000000,
		.pfn		= __phys_to_pfn(SA1111REGSTART),
		.length		= SA1111REGLEN,
		.type		= MT_DEVICE
	}
};

static void __init jornada720_map_io(void)
{
	sa1100_map_io();
	iotable_init(jornada720_io_desc, ARRAY_SIZE(jornada720_io_desc));

	sa1100_register_uart(0, 3);
	sa1100_register_uart(1, 1);

	/* configure suspend conditions */
	PGSR = 0;
	PWER = 0;
	PCFR = 0;
	PSDR = 0;
	
	PGSR |= GPIO_GPIO20; /* preserve GPIO for the sa1111 */
	PWER |= GPIO_JORNADA720_KEYBOARD | PWER_RTC;
	PCFR |= PCFR_OPDE;
	PSDR |= PPC_L_FCLK; /* make sure not to reset the mcu */
}

MACHINE_START(JORNADA720, "HP Jornada 720")
	/* Maintainer: Alex Lange - chicken@handhelds.org */
	.phys_ram	= 0xc0000000,
	.phys_io	= 0x80000000,
	.io_pg_offst	= ((0xf8000000) >> 18) & 0xfffc,
	.boot_params	= 0xc0000100,
	.map_io		= jornada720_map_io,
	.init_irq	= sa1100_init_irq,
	.timer		= &sa1100_timer,
	.init_machine 	= jornada720_init,
MACHINE_END

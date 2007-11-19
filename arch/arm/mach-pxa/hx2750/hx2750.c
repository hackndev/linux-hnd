/*
 * Machine definitions for HP iPAQ hx2750
 *
 * Copyright 2005 Openedhand Ltd.
 *
 * Author: Richard Purdie <richard@o-hand.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */


#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/gpio_keys.h>
#include <linux/mmc/host.h>
#include <linux/mfd/tsc2101.h>

#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/mach/arch.h>

#include <asm/arch/hx2750.h>
#include <asm/arch/pxa-regs.h>
#include <asm/mach/map.h>
#include <asm/arch/udc.h>	
#include <asm/arch/mmc.h>
#include <asm/arch/audio.h>
#include <asm/arch/pxafb.h>
#include <asm/arch/ssp.h>

#include "../generic.h"


/* 
 * Keys Support
 */
static struct gpio_keys_button hx2750_button_table[] = {
	{ KEY_POWER, HX2750_GPIO_KEYPWR, 0 },   
	{ KEY_LEFT, HX2750_GPIO_KEYLEFT, 0 },
	{ KEY_RIGHT, HX2750_GPIO_KEYRIGHT, 0 },
	{ KEY_KP0, HX2750_GPIO_KEYCAL, 0 },
	{ KEY_KP1, HX2750_GPIO_KEYTASK, 0 },
	{ KEY_KP2, HX2750_GPIO_KEYSIDE, 0 },
	{ KEY_ENTER, HX2750_GPIO_KEYENTER, 0 },
	{ KEY_KP3, HX2750_GPIO_KEYCON, 0 }, //KEY_CONTACTS
	{ KEY_MAIL, HX2750_GPIO_KEYMAIL, 0 }, 
	{ KEY_UP, HX2750_GPIO_KEYUP, 0 },
	{ KEY_DOWN, HX2750_GPIO_KEYDOWN, 0 },
};  
    
static struct gpio_keys_platform_data hx2750_pxa_keys_data = {
	.buttons = hx2750_button_table,
	.nbuttons = ARRAY_SIZE(hx2750_button_table),
};  
    
static struct platform_device hx2750_pxa_keys = {
	.name = "gpio-keys",
	.dev = {
		.platform_data = &hx2750_pxa_keys_data,
	},
};


/*
 * Backlight Device
 */
extern struct platform_device pxafb_device; 
 
static struct platform_device hx2750_bl_device = {
	.name		= "hx2750-bl",
	.id		= -1,
//	.dev	= {
//		.parent = &pxafb_device.dev,
//	}
};


/* 
 * UDC/USB 
 */
static int hx2750_udc_is_connected (void)
{
	return GPLR0 & GPIO_bit(HX2750_GPIO_USBCONNECT);
}

//static void hx2750_udc_command (int cmd) 
//{
//}

static struct pxa2xx_udc_mach_info hx2750_udc_mach_info = {
	.udc_is_connected = hx2750_udc_is_connected,
//	.udc_command      = hx2750_udc_command,
};


/*
 * SSP Devices Setup
 */
static struct ssp_dev hx2750_ssp_dev1;
static struct ssp_dev hx2750_ssp_dev2;
static struct ssp_dev hx2750_ssp_dev3;

void hx2750_ssp_init(void)
{
	pxa_gpio_mode(HX2750_GPIO_TSC2101_SS | GPIO_OUT | GPIO_DFLT_HIGH);
	
	if (ssp_init(&hx2750_ssp_dev1,1,NULL))
		printk(KERN_ERR "Unable to register SSP1 handler!\n");
	else {
		ssp_disable(&hx2750_ssp_dev1);
		ssp_config(&hx2750_ssp_dev1, (SSCR0_Motorola | (SSCR0_DSS & 0x0f )), SSCR1_SPH, 0, SSCR0_SerClkDiv(6));
		ssp_enable(&hx2750_ssp_dev1);
		hx2750_set_egpio(HX2750_EGPIO_TSC_PWR);		
	}
	
//	if (ssp_init(&hx2750_ssp_dev2,2))
//		printk(KERN_ERR "Unable to register SSP2 handler!\n");
//	else {
//		ssp_disable(&hx2750_ssp_dev2);
//		ssp_config(&hx2750_ssp_dev2, (SSCR0_TI | (SSCR0_DSS & 0x09 )), 0, 0, SSCR0_SerClkDiv(140));
//		ssp_enable(&hx2750_ssp_dev2);
//	}
//	
	if (ssp_init(&hx2750_ssp_dev3,3,NULL))
		printk(KERN_ERR "Unable to register SSP3 handler!\n");
	else {
		ssp_disable(&hx2750_ssp_dev3);
		ssp_config(&hx2750_ssp_dev3, (SSCR0_Motorola | (SSCR0_DSS & 0x07 )), SSCR1_SPO | SSCR1_SPH, 0, SSCR0_SerClkDiv(166));
		ssp_enable(&hx2750_ssp_dev3);
	}

	printk("SSP Devices Initialised\n");

	return;
}

struct ssp_state ssp1;

void hx2750_ssp_suspend(void)
{
	ssp_disable(&hx2750_ssp_dev1);
	ssp_save_state(&hx2750_ssp_dev1,&ssp1); 
	hx2750_clear_egpio(HX2750_EGPIO_TSC_PWR);
}

void hx2750_ssp_resume(void)
{
	hx2750_set_egpio(HX2750_EGPIO_TSC_PWR);
	ssp_restore_state(&hx2750_ssp_dev1,&ssp1); 
	ssp_enable(&hx2750_ssp_dev1);
}

void hx2750_ssp_init2(void)
{
	if (ssp_init(&hx2750_ssp_dev2,2,NULL))
		printk(KERN_ERR "Unable to register SSP2 handler!\n");
	else {
		mdelay(1000);
		ssp_disable(&hx2750_ssp_dev2);
		mdelay(1000);
		ssp_config(&hx2750_ssp_dev2, (SSCR0_TI | (SSCR0_DSS & 0x09 )), 0, 0, SSCR0_SerClkDiv(140));
		mdelay(1000);
		ssp_enable(&hx2750_ssp_dev2);
		mdelay(1000);
	}
	printk("SSP Device2 Initialised\n");

	return;
}


/*
 * Extra hx2750 Specific GPIOs
 */
void hx2750_send_egpio(unsigned int val)
{
	int i;

	GPCR0 = GPIO_bit(HX2750_GPIO_SR_STROBE);
	GPCR1 = GPIO_bit(HX2750_GPIO_SR_CLK1);

	for (i=0;i<12;i++) {  
		if (val & 0x01)
			GPSR2 = GPIO_bit(HX2750_GPIO_GPIO_DIN);
		else
			GPCR2 = GPIO_bit(HX2750_GPIO_GPIO_DIN);
		val >>= 1;
		GPSR1 = GPIO_bit(HX2750_GPIO_SR_CLK1);
		GPCR1 = GPIO_bit(HX2750_GPIO_SR_CLK1);
	} 

	GPSR0 = GPIO_bit(HX2750_GPIO_SR_STROBE);
	GPCR0 = GPIO_bit(HX2750_GPIO_SR_STROBE);
}

EXPORT_SYMBOL(hx2750_send_egpio);

unsigned int hx2750_egpio_current;

void hx2750_set_egpio(unsigned int gpio)
{
	hx2750_egpio_current|=gpio;
	
	hx2750_send_egpio(hx2750_egpio_current);
}
EXPORT_SYMBOL(hx2750_set_egpio);

void hx2750_clear_egpio(unsigned int gpio)
{
	hx2750_egpio_current&=~gpio;
	
	hx2750_send_egpio(hx2750_egpio_current);
}
EXPORT_SYMBOL(hx2750_clear_egpio);


/*
 * Touchscreen/Sound/Battery Status
 */
int hx2750_tsc2101_send(int read, int command, int *values, int numval)
{
	int i;
	
	GPCR0 = GPIO_bit(HX2750_GPIO_TSC2101_SS);           
	
	ssp_write_word(&hx2750_ssp_dev1, command | read);
	/* dummy read */
	ssp_read_word(&hx2750_ssp_dev1); 

	for (i=0; i < numval; i++) {
		if (read) {
			ssp_write_word(&hx2750_ssp_dev1, 0);
			values[i]=ssp_read_word(&hx2750_ssp_dev1);
		} else {
			ssp_write_word(&hx2750_ssp_dev1, values[i]);
			ssp_read_word(&hx2750_ssp_dev1);
		}
	}
	
	GPSR0 = GPIO_bit(HX2750_GPIO_TSC2101_SS);
	return 0;
}

static int hx2750_tsc2101_pendown(void) 
{
	if ((GPLR(HX2750_GPIO_PENDOWN) & GPIO_bit(HX2750_GPIO_PENDOWN)) == 0) 
		return 1;
	return 0;
}

static struct tsc2101_platform_info hx2750_tsc2101_info = {
	.send     = hx2750_tsc2101_send,
	.suspend  = hx2750_ssp_suspend,
	.resume   = hx2750_ssp_resume,
	.irq      = HX2750_IRQ_GPIO_PENDOWN,
	.pendown  = hx2750_tsc2101_pendown,
};
 
struct platform_device tsc2101_device = {
	.name 		= "tsc2101",
	.dev		= {
 		.platform_data	= &hx2750_tsc2101_info,
 		//.parent = &corgissp_device.dev,
	},
	.id 		= -1,
};


/*
 * MMC/SD Device
 *
 * The card detect interrupt isn't debounced so we delay it by HZ/4
 * to give the card a chance to fully insert/eject.
 */
static struct mmc_detect {
	struct timer_list detect_timer;
	void *devid;
} mmc_detect;

static void mmc_detect_callback(unsigned long data)
{
	mmc_detect_change(mmc_detect.devid);
} 

static irqreturn_t hx2750_mmc_detect_int(int irq, void *devid, struct pt_regs *regs)
{
	mmc_detect.devid=devid;
	mod_timer(&mmc_detect.detect_timer, jiffies + HZ/4);
	return IRQ_HANDLED;
}
 
static int hx2750_mci_init(struct device *dev, irqreturn_t (*unused_detect_int)(int, void *, struct pt_regs *), void *data)
{
	int err;

	/*
	 * setup GPIO for PXA27x MMC controller
	 */
	pxa_gpio_mode(GPIO32_MMCCLK_MD);
	pxa_gpio_mode(GPIO112_MMCCMD_MD);
	pxa_gpio_mode(GPIO92_MMCDAT0_MD);
	pxa_gpio_mode(GPIO109_MMCDAT1_MD);
	pxa_gpio_mode(GPIO110_MMCDAT2_MD);
	pxa_gpio_mode(GPIO111_MMCDAT3_MD);
	pxa_gpio_mode(HX2750_GPIO_SD_DETECT | GPIO_IN);
	pxa_gpio_mode(HX2750_GPIO_SD_READONLY | GPIO_IN);  

	init_timer(&mmc_detect.detect_timer);
	mmc_detect.detect_timer.function = mmc_detect_callback;
	mmc_detect.detect_timer.data = (unsigned long) &mmc_detect;
	
	err = request_irq(HX2750_IRQ_GPIO_SD_DETECT, hx2750_mmc_detect_int, SA_INTERRUPT,
			     "MMC card detect", data);
	if (err) {
		printk(KERN_ERR "hx2750_mci_init: MMC/SD: can't request MMC card detect IRQ\n");
		return -1;
	}
	
	set_irq_type(HX2750_IRQ_GPIO_SD_DETECT, IRQT_BOTHEDGE);

	return 0;
}

static void hx2750_mci_setpower(struct device *dev, unsigned int vdd)
{
	struct pxamci_platform_data* p_d = dev->platform_data;

	if (( 1 << vdd) & p_d->ocr_mask) {
		//printk(KERN_DEBUG "%s: on\n", __FUNCTION__);
		hx2750_set_egpio(HX2750_EGPIO_SD_PWR);
	} else {
		//printk(KERN_DEBUG "%s: off\n", __FUNCTION__);
		hx2750_clear_egpio(HX2750_EGPIO_SD_PWR);
	}
}

static void hx2750_mci_exit(struct device *dev, void *data)
{
	free_irq(HX2750_IRQ_GPIO_SD_DETECT, data);
	del_timer(&mmc_detect.detect_timer);
}

static struct pxamci_platform_data hx2750_mci_platform_data = {
	.ocr_mask	= MMC_VDD_32_33|MMC_VDD_33_34,
	.init 		= hx2750_mci_init,
	.setpower 	= hx2750_mci_setpower,
	.exit		= hx2750_mci_exit,
};


/*
 * FrameBuffer
 */
static struct pxafb_mach_info hx2750_pxafb_info = {
	.pixclock       = 288462,
	.xres           = 240,
	.yres           = 320,
	.bpp            = 16,
	.hsync_len      = 20,
	.left_margin    = 42,
	.right_margin   = 18,
	.vsync_len		= 4,
	.upper_margin   = 3,
	.lower_margin   = 4,
	.sync           = 0,
	.lccr0          = LCCR0_Color | LCCR0_Sngl | LCCR0_Act,
	.lccr3          = LCCR3_PixFlEdg | LCCR3_OutEnH,
	.pxafb_backlight_power	= NULL,
};


/*
 * Test Device 
 */
static struct platform_device hx2750_test_device = {
	.name		= "hx2750-test",
	.id		= -1,
};


/* Initialization code */
static struct platform_device *devices[] __initdata = {
	&hx2750_bl_device,
	&hx2750_test_device,
	&hx2750_pxa_keys,
	&tsc2101_device,
};

static void __init hx2750_init( void )
{
	PWER = 0xC0000003;// | PWER_RTC;
	PFER = 0x00000003;
	PRER = 0x00000003;
	
	PGSR0=0x00000018;
	PGSR1=0x00000380;
	PGSR2=0x00800000;
	PGSR3=0x00500400;
	
	//PCFR |= PCFR_OPDE;
	PCFR=0x77;
	PSLR=0xff100000;
	//PCFR=0x10; - does not return from suspend
		
	//PCFR=  0x00004040;
	//PSLR=  0xff400f04;
	
	/* Setup Extra GPIO Bank access */
	pxa_gpio_mode(HX2750_GPIO_GPIO_DIN | GPIO_OUT | GPIO_DFLT_HIGH);
	pxa_gpio_mode(HX2750_GPIO_SR_CLK1 | GPIO_OUT | GPIO_DFLT_LOW);	
	pxa_gpio_mode(HX2750_GPIO_SR_CLK2 | GPIO_IN);	
	pxa_gpio_mode(HX2750_GPIO_SR_STROBE | GPIO_OUT | GPIO_DFLT_LOW);
	
	/* Init Extra GPIOs - Bootloader reset default is 0x484 */
	/* This is 0xe84 */
	hx2750_set_egpio(HX2750_EGPIO_2 | HX2750_EGPIO_7 | HX2750_EGPIO_LCD_PWR | HX2750_EGPIO_BL_PWR | HX2750_EGPIO_WIFI_PWR);
	
	pxa_set_udc_info(&hx2750_udc_mach_info);
	pxa_set_mci_info(&hx2750_mci_platform_data);
	set_pxa_fb_info(&hx2750_pxafb_info);
	hx2750_ssp_init();
	platform_add_devices (devices, ARRAY_SIZE (devices));
}


MACHINE_START(HX2750, "HP iPAQ HX2750")
	.phys_ram	= 0xa0000000,
	.phys_io	= 0x40000000,
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
        .boot_params	= 0xa0000100,
        .map_io		= pxa_map_io,
        .init_irq	= pxa_init_irq,
        .timer = &pxa_timer,
        .init_machine = hx2750_init,
MACHINE_END


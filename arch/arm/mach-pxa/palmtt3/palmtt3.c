/*
 * Hardware definitions for Palm Tungsten T3
 *
 * Author:
 * 	Vladimir "Farcaller" Pouzanov <farcaller@gmail.com>
 *
 * Based on Palm LD patch by Alex Osborne
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/corgi_bl.h>

#include <asm/mach/arch.h>
#include <asm/mach-types.h>
#include <asm/arch/hardware.h>
#include <asm/mach/map.h>
#include <asm/domain.h>

#include <linux/device.h>
#include <linux/fb.h>

#include <asm/arch/bitfield.h>
#include <asm/arch/hardware.h>
#include <asm/arch/pxa-dmabounce.h>
#include <asm/arch/pxafb.h>
#include <asm/arch/pxa-regs.h>

#include <linux/platform_device.h>
#include <asm/irq.h> //test
#include <asm/arch/ssp.h>
#include <asm/arch/irq.h> //test
#include <asm/arch/hardware.h> //test
#include <asm/arch/irda.h>
#include <linux/delay.h>
#include <asm/arch/udc.h>
#include <asm/arch/serial.h>

#include <linux/input.h>
#include <linux/mfd/tsc2101.h>
#include <asm/arch/pxa-pm_ll.h>
#include <asm/arch/palmtt3-gpio.h>
#include <asm/arch/palmtt3-init.h>
#include <asm/arch/tps65010.h>

#if defined(CONFIG_PALMTT3_BLUETOOTH) || defined(CONFIG_PALMTT3_BLUETOOTH_MODULE)
#include "palmtt3_bt.h"
#endif

#include "../generic.h"

#define DEBUG 1

/*** FRAMEBUFFER ***/

static void palmtt3_pxafb_lcd_power(int level, struct fb_var_screeninfo *var)
{
	if(level) {
		GPSR1 = GPIO_bit(38);
		GPSR1 = GPIO_bit(41);

	} else {
		GPLR1 = GPIO_bit(38);
		GPLR1 = GPIO_bit(41);
	}
}




/*** framebuffer ***/

static struct pxafb_mode_info palmtt3_lcd_modes[] = {
    {
	.pixclock		= 0,
	.bpp			= 16,
#ifdef CONFIG_PALMTT3_DISABLE_BORDER
	.hsync_len		= 6,
	.vsync_len		= 3,
	.xres			= 324,
	.yres			= 484,
	.left_margin		= 28,
	.right_margin		= 8,
	.upper_margin		= 3,
	.lower_margin		= 6,
#else
	.hsync_len		= 4,
	.vsync_len		= 1,
	.xres			= 320,
	.yres			= 480,
	.left_margin		= 31,
	.right_margin		= 3,
	.upper_margin		= 8,
	.lower_margin		= 7,
#endif
	.sync			= FB_SYNC_HOR_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT,
    }
};

static struct pxafb_mach_info palmtt3_lcd_screen = {
	.modes			= palmtt3_lcd_modes,
	.num_modes		= ARRAY_SIZE(palmtt3_lcd_modes),
	.lccr0 			= PALMTT3_INIT_LCD_LCCR0,
	.lccr3 			= PALMTT3_INIT_LCD_LCCR3,
	.pxafb_backlight_power	= NULL,
	.pxafb_lcd_power 	= &palmtt3_pxafb_lcd_power,
};


/*** SSP ***/

static struct ssp_dev palmtt3_ssp_dev;

void palmtt3_ssp_init(void)
{
	printk(KERN_WARNING "palmtt3_tsc2101: Resetting SSP, move this to garux?\n");
	SSCR0 &= ~SSCR0_SSE;
	SSCR1 = SSCR1 & 0x3FFC;

	if (ssp_init(&palmtt3_ssp_dev, 1, 0))
		printk(KERN_ERR "palmtt3_tsc2101: Unable to register SSP handler!\n");
	else {
		ssp_enable(&palmtt3_ssp_dev);
		printk(KERN_INFO "palmtt3_tsc2101: SSP device initialized\n");
	}

	return;
}

struct ssp_state ssp1;

void palmtt3_ssp_suspend(void)
{
	ssp_disable(&palmtt3_ssp_dev);
	ssp_save_state(&palmtt3_ssp_dev,&ssp1);
	// FIXME power off TSC2101?
}

void palmtt3_ssp_resume(void)
{
	// FIXME power on TSC2101?
	ssp_restore_state(&palmtt3_ssp_dev,&ssp1);
	ssp_enable(&palmtt3_ssp_dev);
}

/*** Touchscreen/Sound/Battery Status ***/

void palmtt3_tsc2101_send(int read, int command, int *values, int numval)
{
	u32 ret;

	int i;

	GPCR0 = GPIO_bit(PALMTT3_GPIO_TSC2101_SS);

	ssp_write_word(&palmtt3_ssp_dev, command | read);
	ssp_read_word(&palmtt3_ssp_dev, &ret); /* Dummy read */

	for (i = 0; i < numval; i++) {
		if (read) {
			ssp_write_word(&palmtt3_ssp_dev, 0);
			ssp_read_word(&palmtt3_ssp_dev, &values[i]);
		} else {
			ssp_write_word(&palmtt3_ssp_dev, values[i]);
			ssp_read_word(&palmtt3_ssp_dev, &ret); /* Dummy read */
		}
	}
	GPSR0 = GPIO_bit(PALMTT3_GPIO_TSC2101_SS);
}

static int palmtt3_tsc2101_pendown(void)
{
	// FIXME PALMT3_GPIO_PENDOWN
	if ((GPLR(PALMTT3_GPIO_PENDOWN) & GPIO_bit(PALMTT3_GPIO_PENDOWN)) == 0)
		return 1;
	return 0;
}

static struct tsc2101_platform_info palmtt3_tsc2101_info = {
	.send     = palmtt3_tsc2101_send,
	.suspend  = palmtt3_ssp_suspend,
	.resume   = palmtt3_ssp_resume,
	.irq      = PALMTT3_IRQ_GPIO_PENDOWN,
	.pendown  = palmtt3_tsc2101_pendown,
};

struct platform_device tsc2101_device = {
	.name 		= "tsc2101",
	.dev		= {
		.platform_data	= &palmtt3_tsc2101_info,
	},
	.id 		= -1,
};

/*** Buttons ***/
static struct platform_device palmtt3_btn_device = {
	.name = "palmtt3-btn",
	.id = -1,
};

/* Backlight ***/
static void palmtt3_bl_power(int on)
{
 /*    SET_PALMTT3_GPIO(BL_POWER, on); */ /* to be determined */
	pxa_set_cken(CKEN0_PWM0, on);
	pxa_set_cken(CKEN1_PWM1, on);
	mdelay(50);
}

static void palmtt3_set_bl_intensity(int intensity)
{
	palmtt3_bl_power(intensity ? 1 : 0);
	if(intensity) {
		PWM_CTRL1   = 0x7;
		PWM_PERVAL1 = PALMTT3_PERIOD;
		PWM_PWDUTY1 = intensity;
	}
}

static struct corgibl_machinfo palmtt3_bl_machinfo = {
	.max_intensity		= PALMTT3_MAX_INTENSITY,
	.default_intensity	= PALMTT3_DEFAULT_INTENSITY,
	.set_bl_intensity   	= palmtt3_set_bl_intensity,
	.limit_mask		= PALMTT3_LIMIT_MASK,
};

/*** LEDs ***/
static struct platform_device palmtt3_led_device = {
          .name           = "palmtt3-led",
          .id             = -1,
};

/*** Power button ***/
static struct platform_device palmtt3_power_button = {
          .name           = "tps65010-pwr_btn",
          .id             = -1,
};


/*** IRDA ***/
static void palmtt3_irda_transceiver_mode(struct device *dev, int mode)
{
        unsigned long flags;

        local_irq_save(flags);

        if (mode & IR_SIRMODE){
		printk ("IrDA: setting mode to SIR\n");
	}
	else if (mode & IR_FIRMODE){
		printk ("IrDA: setting mode to FIR\n");
	}
	if (mode & IR_OFF){
		printk ("IrDA: turning OFF\n");
		SET_GPIO(GPIO_NR_PALMTT3_IR_DISABLE, 1);

	}
	else {
		printk (KERN_INFO "IrDA: turning ON\n");
		SET_GPIO(GPIO_NR_PALMTT3_IR_DISABLE, 0);
	}

        local_irq_restore(flags);
}


static struct pxaficp_platform_data palmtt3_ficp_platform_data = {
        .transceiver_cap  = IR_SIRMODE | IR_OFF,
        .transceiver_mode = palmtt3_irda_transceiver_mode,
};


/*** Backlight ***/
static struct platform_device palmtt3_backlight_device = {
	.name	= "corgi-bl",
	.id     = 0,
	.dev = {
		.platform_data = &palmtt3_bl_machinfo,
	},
};

/*******
 * USB *
 *******/

static int palmtt3_udc_is_connected (void){
	int ret = GET_GPIO(GPIO_NR_PALMTT3_USB_DETECT);
	if (ret)
		printk (KERN_INFO "palmtt3_udc: device detected [USB_DETECT: %d]\n",ret);
	else
		printk (KERN_INFO "palmtt3_udc: no device detected [USB_DETECT: %d]\n",ret);

	return ret;
}

static void palmtt3_udc_command (int cmd){

	switch (cmd) {
		case PXA2XX_UDC_CMD_DISCONNECT:
			SET_GPIO(GPIO_NR_PALMTT3_PUC_USB_POWER, 0);
			SET_GPIO(GPIO_NR_PALMTT3_USB_POWER, 1);
			printk(KERN_INFO "palmtt3_udc: got command PXA2XX_UDC_CMD_DISCONNECT\n");
			break;
		case PXA2XX_UDC_CMD_CONNECT:
			SET_GPIO(GPIO_NR_PALMTT3_USB_POWER, 0);
			SET_GPIO(GPIO_NR_PALMTT3_PUC_USB_POWER, 1);
			printk(KERN_INFO "palmtt3_udc: got command PXA2XX_UDC_CMD_CONNECT\n");
			break;
	default:
			printk("palmtt3_udc: unknown command '%d'\n", cmd);
	}
}

static struct pxa2xx_udc_mach_info palmtt3_udc_mach_info __initdata = {
	.udc_is_connected = palmtt3_udc_is_connected,
	.udc_command      = palmtt3_udc_command,
};


/*** HW UART - serial in PUC ***/
int palmtt3_hwuart_state;

void palmtt3_hwuart_configure(int state)
{
	switch (state) {
		case PXA_UART_CFG_PRE_STARTUP:
			break;
		case PXA_UART_CFG_POST_STARTUP:
			SET_GPIO(35,0);
			SET_GPIO(35,1);
			palmtt3_hwuart_state = 1;
			break;
		case PXA_UART_CFG_PRE_SHUTDOWN:
			SET_GPIO(35,0);
			palmtt3_hwuart_state = 0;
			break;
		case PXA_UART_CFG_POST_SHUTDOWN:
			break;
		default:
			printk("palmtt3_hwuart_configure: bad request %d\n",state);
			break;
	}
}

static int palmtt3_hwuart_suspend(struct platform_device *dev, pm_message_t state)
{
	palmtt3_hwuart_configure(PXA_UART_CFG_PRE_SHUTDOWN);
	return 0;
}

static int palmtt3_hwuart_resume(struct platform_device *dev)
{
	palmtt3_hwuart_configure(PXA_UART_CFG_POST_STARTUP);
	return 0;
}

struct platform_pxa_serial_funcs palmtt3_hwuart = {
	.configure	= palmtt3_hwuart_configure,
	.suspend	= palmtt3_hwuart_suspend,
	.resume		= palmtt3_hwuart_resume,
};

/******************************************************************************
 * Bluetooth
 ******************************************************************************/
#if defined(CONFIG_PALMTT3_BLUETOOTH) || defined(CONFIG_PALMTT3_BLUETOOTH_MODULE)
static struct palmtt3_bt_funcs bt_funcs;

static struct platform_device palmtt3_bt = {
	.name	= "palmtt3_bt",
	.id	= -1,
	.dev	= {
		.platform_data	= &bt_funcs,
	},
};

static void palmtt3_bt_configure(int state)
{
	if (bt_funcs.configure != NULL)
		bt_funcs.configure(state);
}

static struct platform_pxa_serial_funcs palmtt3_pxa_bt_funcs = {
	.configure = palmtt3_bt_configure,
};
#endif


/*** Suspend/Resume ***/
#ifdef CONFIG_PM
static long int _PM_backup[3];

void palmtt3_suspend(unsigned long ret)
{
	unsigned long * addr;

	addr  = (unsigned long *) 0xC0000000;
	_PM_backup[0] = *addr;
	*addr = 0xFEEDC0DE;

	addr  = (unsigned long *) 0xC0000004;
	_PM_backup[1] = *addr;
	*addr = 0xBEEFF00D;

	addr  = (unsigned long *) 0xC0000008;
	_PM_backup[2] = *addr;
	*addr = ret;

	/* event settings for waking-up */
	PWER  = 0;
	PWER  |= PWER_RTC;	/* enabling RTC alarm for wake-up */
	PWER  |= PWER_WEP1;	/* enabling RTC alarm for wake-up */
	PWER  |= PWER_GPIO0;	/* calendar/contacts/voice */
	PWER  |= PWER_GPIO1;	/* Active low GP_reset */
/*	PWER  |= PWER_GPIO2;	 card insert - disabled - linux dislike ejecting card with mounted filesystem :) */
	PWER  |= PWER_GPIO3;	/* slider */
	PWER  |= PWER_GPIO10;	/* 5nav up/down/left/right */
	PWER  |= PWER_GPIO11;	/* memo/todo/center */
	PWER  |= PWER_GPIO12;	/* HotSync button on cradle */
	PWER  |= PWER_GPIO14;	/* power button */
	PWER  |= PWER_GPIO15;	/* Bluetooth wakeup? */

	PFER  = PWER_GPIO0 | PWER_GPIO1 | PWER_GPIO11 | PWER_GPIO14;
	PEDR  = PWER_GPIO0 | PWER_GPIO1 | PWER_GPIO15;
}

void palmtt3_resume(void)
{
	unsigned long * addr;

	addr  = (unsigned long *) 0xC0000000;
	*addr = _PM_backup[0];

	addr  = (unsigned long *) 0xC0000004;
	*addr = _PM_backup[1];

	addr  = (unsigned long *) 0xC0000008;
	*addr = _PM_backup[2];
}

static struct pxa_ll_pm_ops palmtt3_pm_ops = {
	.suspend = palmtt3_suspend,
	.resume = palmtt3_resume,
};
#endif

/*** INIT ***/

static struct platform_device *devices[] __initdata = {
	//&palmt3_bl_device,
	&tsc2101_device,
	&palmtt3_btn_device,
	&palmtt3_backlight_device,
	&palmtt3_led_device,
	&palmtt3_power_button,
#if defined(CONFIG_PALMTT3_BLUETOOTH) || defined(CONFIG_PALMTT3_BLUETOOTH_MODULE)
	&palmtt3_bt,
#endif
};


static void __init palmtt3_init(void)
{
#ifdef CONFIG_PM
	pxa_pm_set_ll_ops(&palmtt3_pm_ops);
#endif
	palmtt3_ssp_init();
	pxa_set_ficp_info(&palmtt3_ficp_platform_data);
	set_pxa_fb_info(&palmtt3_lcd_screen);
	pxa_set_udc_info(&palmtt3_udc_mach_info);
	pxa_set_hwuart_info(&palmtt3_hwuart);
#if defined(CONFIG_PALMTT3_BLUETOOTH) || defined(CONFIG_PALMTT3_BLUETOOTH_MODULE)
	pxa_set_btuart_info(&palmtt3_pxa_bt_funcs);
#endif
	platform_add_devices (devices, ARRAY_SIZE (devices));
}

MACHINE_START(T3XSCALE, "Palm Tungsten T3")
	/* Maintainer: Vladimir Pouzanov <farcaller@gmail.com> */
	.phys_io	= PALMTT3_PHYS_IO_START,
	.boot_params	= 0xa0000100,
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
	.map_io		= pxa_map_io,
	.init_irq	= pxa_init_irq,
	.timer		= &pxa_timer,
	.init_machine	= palmtt3_init,
MACHINE_END

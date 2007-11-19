/*
 * pxa-wm97xx.c  --  pxa-wm97xx Continuous Touch screen driver for
 *                   Wolfson WM97xx AC97 Codecs.
 *
 * Copyright 2004 Wolfson Microelectronics PLC.
 * Author: Liam Girdwood
 *         liam.girdwood@wolfsonmicro.com or linux@wolfsonmicro.com
 * Parts Copyright : Ian Molton <spyro@f2s.com>
 *                   Andrew Zabolotny <zap@homelink.ru>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 * Notes:
 *     This is a wm97xx extended touch driver to capture touch
 *     data in a continuous manner on the Intel XScale archictecture
 *
 *  Features:
 *       - codecs supported:- WM9705, WM9712, WM9713
 *       - processors supported:- Intel XScale PXA25x, PXA26x, PXA27x
 *
 *  Revision history
 *    18th Aug 2004   Initial version.
 *    26th Jul 2005   Improved continous read back and added FIFO flushing.
 *    06th Sep 2005   Mike Arthur <linux@wolfsonmicro.com>
 *                    Moved to using the wm97xx bus
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/wm97xx.h>
#include <asm/io.h>
#include <asm/arch/pxa-regs.h>

#define VERSION		"0.13"

struct continuous {
	u16 id;    /* codec id */
	u8 code;   /* continuous code */
	u8 reads;  /* number of coord reads per read cycle */
	u32 speed; /* number of coords per second */
};

#define WM_READS(sp) ((sp / HZ) + 1)

static const struct continuous cinfo[] = {
	{WM9705_ID2, 0, WM_READS(94), 94},
	{WM9705_ID2, 1, WM_READS(188), 188},
	{WM9705_ID2, 2, WM_READS(375), 375},
	{WM9705_ID2, 3, WM_READS(750), 750},
	{WM9712_ID2, 0, WM_READS(94), 94},
	{WM9712_ID2, 1, WM_READS(188), 188},
	{WM9712_ID2, 2, WM_READS(375), 375},
	{WM9712_ID2, 3, WM_READS(750), 750},
	{WM9713_ID2, 0, WM_READS(94), 94},
	{WM9713_ID2, 1, WM_READS(120), 120},
	{WM9713_ID2, 2, WM_READS(154), 154},
	{WM9713_ID2, 3, WM_READS(188), 188},
};

/* continuous speed index */
static int sp_idx = 0;
static u16 last = 0, tries = 0;

/*
 * Pen sampling frequency (Hz) in continuous mode.
 */
static int cont_rate = 200;
module_param(cont_rate, int, 0);
MODULE_PARM_DESC(cont_rate, "Sampling rate in continuous mode (Hz)");

/*
 * Pen down detection.
 *
 * This driver can either poll or use an interrupt to indicate a pen down
 * event. If the irq request fails then it will fall back to polling mode.
 */
static int pen_int = 1;
module_param(pen_int, int, 0);
MODULE_PARM_DESC(pen_int, "Pen down detection (1 = interrupt, 0 = polling)");

/*
 * Pressure readback.
 *
 * Set to 1 to read back pen down pressure
 */
static int pressure = 0;
module_param(pressure, int, 0);
MODULE_PARM_DESC(pressure, "Pressure readback (1 = pressure, 0 = no pressure)");

/*
 * AC97 touch data slot.
 *
 * Touch screen readback data ac97 slot
 */
static int ac97_touch_slot = 5;
module_param(ac97_touch_slot, int, 0);
MODULE_PARM_DESC(ac97_touch_slot, "Touch screen data slot AC97 number");


/* flush AC97 slot 5 FIFO on pxa machines */
#ifdef CONFIG_PXA27x
void wm97xx_acc_pen_up (struct wm97xx* wm)
{
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(1);

	while (MISR & (1 << 2))
		MODR;
}
#else
void wm97xx_acc_pen_up (struct wm97xx* wm)
{
	int count = 16;
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(1);

	while (count < 16) {
		MODR;
		count--;
	}
}
#endif

int wm97xx_acc_pen_down (struct wm97xx* wm)
{
	u16 x, y, p = 0x100 | WM97XX_ADCSEL_PRES;
	int reads = 0;

	/* data is never immediately available after pen down irq */
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(1);

	if (tries > 5){
		tries = 0;
		return RC_PENUP;
	}

	x = MODR;
	if (x == last) {
		tries++;
		return RC_AGAIN;
	}
	last = x;
	do {
		if (reads)
			x= MODR;
		y= MODR;
		if (pressure)
			p = MODR;

		/* are samples valid */
		if ((x & 0x7000) != WM97XX_ADCSEL_X ||
			(y & 0x7000) != WM97XX_ADCSEL_Y ||
			(p & 0x7000) != WM97XX_ADCSEL_PRES)
			goto up;

		/* coordinate is good */
		tries = 0;
		//printk("x %x y %x p %x\n", x,y,p);
		input_report_abs (wm->input_dev, ABS_X, x & 0xfff);
		input_report_abs (wm->input_dev, ABS_Y, y & 0xfff);
		input_report_abs (wm->input_dev, ABS_PRESSURE, p & 0xfff);
		input_sync (wm->input_dev);
		reads++;
	} while (reads < cinfo[sp_idx].reads);
up:
	return RC_PENDOWN | RC_AGAIN;
}

int wm97xx_acc_startup(struct wm97xx* wm)
{
	int idx = 0;

	/* check we have a codec */
	if (wm->ac97 == NULL)
		return -ENODEV;

	/* Go you big red fire engine */
	for (idx = 0; idx < ARRAY_SIZE(cinfo); idx++) {
		if (wm->id != cinfo[idx].id)
			continue;
		sp_idx = idx;
		if (cont_rate <= cinfo[idx].speed)
			break;
	}
	wm->acc_rate = cinfo[sp_idx].code;
	wm->acc_slot = ac97_touch_slot;
	printk(KERN_INFO "pxa2xx accelerated touchscreen driver, %d samples (sec)\n",
		cinfo[sp_idx].speed);

	/* codec specific irq config */
	if (pen_int) {
		switch (wm->id) {
			case WM9705_ID2:
				wm->pen_irq = IRQ_GPIO(4);
				set_irq_type(IRQ_GPIO(4), IRQT_BOTHEDGE);
				break;
			case WM9712_ID2:
			case WM9713_ID2:
				/* enable pen down interrupt */
				/* use PEN_DOWN GPIO 13 to assert IRQ on GPIO line 2 */
				wm->pen_irq = MAINSTONE_AC97_IRQ;
				wm97xx_config_gpio(wm, WM97XX_GPIO_13, WM97XX_GPIO_IN,
					WM97XX_GPIO_POL_HIGH, WM97XX_GPIO_STICKY, WM97XX_GPIO_WAKE);
				wm97xx_config_gpio(wm, WM97XX_GPIO_2, WM97XX_GPIO_OUT,
					WM97XX_GPIO_POL_HIGH, WM97XX_GPIO_NOTSTICKY, WM97XX_GPIO_NOWAKE);
				break;
			default:
				printk(KERN_WARNING "pen down irq not supported on this device\n");
				pen_int = 0;
				break;
		}
	}

	return 0;
}

void wm97xx_acc_shutdown(struct wm97xx* wm)
{
    /* codec specific deconfig */
	if (pen_int) {
		switch (wm->id & 0xffff) {
			case WM9705_ID2:
				wm->pen_irq = 0;
				break;
			case WM9712_ID2:
			case WM9713_ID2:
				/* disable interrupt */
				wm->pen_irq = 0;
				break;
		}
	}
}

static struct wm97xx_mach_ops pxa_mach_ops = {
	.acc_enabled = 1,
	.acc_pen_up = wm97xx_acc_pen_up,
    .acc_pen_down = wm97xx_acc_pen_down,
    .acc_startup = wm97xx_acc_startup,
    .acc_shutdown = wm97xx_acc_shutdown,
};

int pxa_wm97xx_probe(struct device *dev)
{
    struct wm97xx *wm = dev->driver_data;
    return wm97xx_register_mach_ops (wm, &pxa_mach_ops);
}

int pxa_wm97xx_remove(struct device *dev)
{
	struct wm97xx *wm = dev->driver_data;
    wm97xx_unregister_mach_ops (wm);
    return 0;
}

static struct device_driver  pxa_wm97xx_driver = {
    .name = "wm97xx-touchscreen",
    .bus = &wm97xx_bus_type,
    .owner = THIS_MODULE,
    .probe = pxa_wm97xx_probe,
    .remove = pxa_wm97xx_remove
};

static int __init pxa_wm97xx_init(void)
{
    return driver_register(&pxa_wm97xx_driver);
}

static void __exit pxa_wm97xx_exit(void)
{
    driver_unregister(&pxa_wm97xx_driver);
}

module_init(pxa_wm97xx_init);
module_exit(pxa_wm97xx_exit);

/* Module information */
MODULE_AUTHOR("Liam Girdwood <liam.girdwood@wolfsonmicro.com>");
MODULE_DESCRIPTION("wm97xx continuous touch driver for pxa2xx");
MODULE_LICENSE("GPL");

/*
 * linux/drivers/input/keyboard/pxa27x_keyboard.c
 *
 * Driver for the pxa27x matrix keyboard controller.
 *
 * Created:	Jun 17, 2006 
 * Author:	Kevin O'Connor <kevin_at_koconnor.net> 
 *
 * fixed by:	Rodolfo Giometti <giometti@linux.it>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Based on code by: Alex Osborne <bobofdoom@gmail.com>
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <asm/arch/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/irqs.h>
#include <asm/arch/pxa27x_keyboard.h>

#define DRIVER_NAME		"pxa27x-keyboard"

#define KPASMKP(col)		(col/2 == 0 ? KPASMKP0 : \
				 col/2 == 1 ? KPASMKP1 : \
				 col/2 == 2 ? KPASMKP2 : KPASMKP3)
#define KPASMKPx_MKC(row, col)	(1<<(row+16*(col%2)))

static irqreturn_t pxakbd_irq_handler(int irq, void *dev_id)
{
	struct platform_device *dev = dev_id;
	struct pxa27x_keyboard_platform_data *pdev = dev->dev.platform_data;
	struct input_dev *button_dev = dev->dev.driver_data;
	int mi, p;
	int row, col;

	/* Notify controller that interrupt was handled, otherwise it'll
	 * constantly send interrupts and lock up the device */
	mi = KPC & KPC_MI;

	/* report the status of every button */
	for (row = 0; row < pdev->nr_rows; row++) {
		for (col = 0; col < pdev->nr_cols; col++) {
			p = KPASMKP(col)&KPASMKPx_MKC(row, col) ? 1 : 0;

			pr_debug("keycode %x - pressed %x\n",
					pdev->keycodes[row][col], p);
			input_report_key(button_dev,
					pdev->keycodes[row][col], p);
		}
	}
	input_sync(button_dev);

	return IRQ_HANDLED;
}

static void
init_kpc(struct platform_device *dev)
{
       struct pxa27x_keyboard_platform_data *pdev = dev->dev.platform_data;
       int col;
       u32 kpc = 0;

       kpc |= (pdev->nr_rows-1) << 26;
       kpc |= (pdev->nr_cols-1) << 23;

       kpc |= KPC_ASACT;       /* enable automatic scan on activity */
       kpc &= ~KPC_AS;         /* disable automatic scan */
       kpc &= ~KPC_IMKP;       /* do not ignore multiple keypresses */

       for (col = 0; col < pdev->nr_cols; col++)
               kpc |= KPC_MS0<<col;

       kpc &= ~KPC_DE;         /* disable direct keypad */
       kpc &= ~KPC_DIE;        /* disable direct keypad interrupt */

       kpc |= KPC_ME;          /* matrix keypad enabled */
       kpc |= KPC_MIE;         /* matrix keypad interrupt enabled */

       KPC = kpc;

       if (pdev->debounce_ms)
       	KPKDI = pdev->debounce_ms & 0xff;
}


static int pxakbd_probe(struct platform_device *dev)
{
	struct input_dev *button_dev;
	struct pxa27x_keyboard_platform_data *pdev = dev->dev.platform_data;
	int i, row, col, err;

	/* Create and register the input driver. */
	dev->dev.driver_data = button_dev = input_allocate_device();
	if (!dev->dev.driver_data) {
		printk(KERN_ERR "Cannot request keypad device\n");
		return -1;
	}

	set_bit(EV_KEY, button_dev->evbit);
	set_bit(EV_REP, button_dev->evbit);
	for (row = 0; row < pdev->nr_rows; row++) {
		for (col = 0; col < pdev->nr_cols; col++) {
			int code = pdev->keycodes[row][col];
			if (code <= 0)
				continue;
			set_bit(code, button_dev->keybit);
		}
	}

        err = request_irq(IRQ_KEYPAD, pxakbd_irq_handler, SA_INTERRUPT,
			  DRIVER_NAME, dev);
	if (err) {
		printk(KERN_ERR "Cannot request keypad IRQ\n");
		pxa_set_cken(CKEN19_KEYPAD, 0);
		return -1;
	}

	/* Register the input device */
	button_dev->name = DRIVER_NAME;
	button_dev->id.bustype = BUS_HOST;
	input_register_device(button_dev);

	/*
	 * Enable keyboard.
	 */

	pxa_set_cken(CKEN19_KEYPAD, 1);

	/* Setup GPIOs. */
	for (i = 0; i < pdev->nr_rows + pdev->nr_cols; i++)
		pxa_gpio_mode(pdev->gpio_modes[i]);

	init_kpc(dev);
	
	printk(KERN_INFO "PXA27x keyboard controller enabled\n");

	return 0;
}

static int pxakbd_remove(struct platform_device *dev)
{
	struct input_dev *button_dev = dev->dev.driver_data;
	input_unregister_device(button_dev);
	free_irq(IRQ_KEYPAD, NULL);
	pxa_set_cken(CKEN19_KEYPAD, 0);
	return 0;
}

#ifdef CONFIG_PM

static int
pxakbd_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int
pxakbd_resume(struct platform_device *pdev)
{

	init_kpc(pdev);
	return 0;
}

#else
#define pxakbd_suspend   NULL
#define pxakbd_resume    NULL
#endif

static struct platform_driver pxakbd_driver = {
	.driver = {
		.name   = DRIVER_NAME,
	},
	.probe		= pxakbd_probe,
	.remove		= pxakbd_remove,
	.suspend	= pxakbd_suspend,
	.resume		= pxakbd_resume,
};

static int __init pxakbd_init(void)
{
	return platform_driver_register(&pxakbd_driver);
}

static void __exit pxakbd_exit(void)
{
	platform_driver_unregister(&pxakbd_driver);
}

module_init(pxakbd_init);
module_exit(pxakbd_exit);

MODULE_DESCRIPTION("PXA27x Matrix Keyboard Driver");
MODULE_LICENSE("GPL");

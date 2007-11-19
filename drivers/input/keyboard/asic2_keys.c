/*
 * Driver interface to HTC "ASIC2"
 *
 * Copyright 2001 Compaq Computer Corporation.
 * Copyright 2003 Hewlett-Packard Company.
 * Copyright 2003, 2004, 2005 Phil Blundell
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
 * AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
 * FITNESS FOR ANY PARTICULAR PURPOSE.
 *
 * Author:  Andrew Christian
 *          <Andrew.Christian@compaq.com>
 *          October 2001
 *
 * Restrutured June 2002
 * 
 * Jamey Hicks <jamey.hicks@hp.com>
 * Re-Restrutured September 2003
 *
 * 2006-11-11 Paul Sokolovsky	Revamped for 2.6.18
 */

#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/input.h>
#include <linux/input_pda.h>
#include <linux/platform_device.h>

#include <asm/irq.h>
#include <asm/arch/hardware.h>

#include <asm/hardware/ipaq-asic2.h>

#include <linux/mfd/asic2_base.h>


/***********************************************************************************
 *      Keyboard ISRs
 *
 *   Resources used:     KPIO interface on ASIC2
 *                       GPIO for Power button on SA1110
 ***********************************************************************************/

struct key_data {
	struct device *asic;
	struct input_dev *input;
	int irq;
};

struct asic_to_button {
	int keycode;
	u32 mask;
	u32 button;
	u8  down;
};

static struct asic_to_button asic_to_button[] = {
	{ _KEY_RECORD,		KPIO_RECORD_BTN_N,   KPIO_RECORD_BTN_N },  // 1:  Record button
	{ _KEY_CALENDAR,	KPIO_KEY_LEFT_N,     KPIO_KEY_LEFT_N },    // 2:  Calendar
	{ _KEY_CONTACTS,	KPIO_KEY_RIGHT_N,    KPIO_KEY_RIGHT_N },   // 3:  Contacts (looks like Outlook)
	{ _KEY_MAIL,		KPIO_KEY_AP1_N,      KPIO_KEY_AP1_N },     // 4:  Envelope (Q on older iPAQs)
	{ _KEY_HOMEPAGE,	KPIO_KEY_AP2_N,      KPIO_KEY_AP2_N },     // 5:  Start (looks like swoopy arrow)
	{ KEY_UP,		KPIO_ALT_KEY_ALL,    KPIO_KEY_5W1_N | KPIO_KEY_5W4_N | KPIO_KEY_5W5_N }, // 6:  Up
	{ KEY_RIGHT,		KPIO_KEY_AP4_N,      KPIO_KEY_AP4_N },     // 7:  Right
	{ KEY_LEFT,		KPIO_KEY_AP3_N,      KPIO_KEY_AP3_N },     // 8:  Left
	{ KEY_DOWN,		KPIO_ALT_KEY_ALL,    KPIO_KEY_5W2_N | KPIO_KEY_5W3_N | KPIO_KEY_5W5_N }, // 9:  Down
	{ KEY_ENTER,		KPIO_ALT_KEY_ALL,    KPIO_KEY_5W5_N },     // 10: Action
};

static irqreturn_t
asic2_key_isr (int irq, void *dev_id)
{
	int i;
	struct key_data *k = dev_id;
	u32 keys = asic2_read_register (k->asic, IPAQ_ASIC2_KPIOPIOD);

	for (i = 0; i < (sizeof (asic_to_button) / sizeof (struct asic_to_button)); i++) {
		int down = ((~keys & asic_to_button[i].mask) == asic_to_button[i].button) ? 1 : 0;
		if (down != asic_to_button[i].down) {
			input_report_key(k->input, asic_to_button[i].keycode, down);
			asic_to_button[i].down = down;
		}
	}

	asic2_write_register (k->asic, IPAQ_ASIC2_KPIINTALSEL, KPIO_KEY_ALL & ~keys);

	return IRQ_HANDLED;
}

static void 
asic2_key_setup(struct key_data *k) 
{
	unsigned long val, flags;
	local_irq_save (flags);
	asic2_write_register (k->asic, IPAQ_ASIC2_KPIODIR, KPIO_KEY_ALL);  /* Inputs    */
	asic2_write_register (k->asic, IPAQ_ASIC2_KPIOALT, KPIO_ALT_KEY_ALL);
	val = asic2_read_register (k->asic, IPAQ_ASIC2_KPIINTTYPE);
	asic2_write_register (k->asic, IPAQ_ASIC2_KPIINTTYPE, val & ~KPIO_KEY_ALL);  /* Level */
	val = asic2_read_register (k->asic, IPAQ_ASIC2_KPIOPIOD);
	asic2_write_register (k->asic, IPAQ_ASIC2_KPIINTALSEL, KPIO_KEY_ALL & ~val);
	local_irq_restore (flags);
}

static int
asic2_key_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct key_data *k = pdev->dev.driver_data;

	disable_irq (k->irq);
	return 0;
}

static int
asic2_key_resume(struct platform_device *pdev)
{
	struct key_data *k = pdev->dev.driver_data;

	asic2_key_setup (k);
	enable_irq (k->irq);
	
	return 0;
}

static int
asic2_key_probe(struct platform_device *pdev)
{
	int result;
	struct key_data *k;
	int i;

	k = kmalloc (sizeof (*k), GFP_KERNEL);
	if (!k)
		return -ENOMEM;

	memset (k, 0, sizeof (*k));
	k->input = input_allocate_device();

	k->irq = pdev->resource[1].start;
	k->asic = pdev->dev.parent;

	result = request_irq (k->irq, asic2_key_isr, SA_INTERRUPT | SA_SAMPLE_RANDOM,
			      "asic2 keyboard", k);
	if (result) {
		printk("asic2_key: unable to claim irq %d; error %d\n", k->irq, result);
		kfree (k);
		return result;
	}

	set_bit(EV_KEY, k->input->evbit);
	for (i = 0; i < ARRAY_SIZE(asic_to_button); i++) {
		int code = asic_to_button[i].keycode;
		set_bit(code, k->input->keybit);
	}

	k->input->name = "asic2 keyboard";
	k->input->private = k;

	input_register_device(k->input);

	asic2_key_setup (k);

	pdev->dev.driver_data = k;

	return result;
}

static int
asic2_key_remove(struct platform_device *pdev)
{
	struct key_data *k = pdev->dev.driver_data;
	
	input_unregister_device(k->input);

	free_irq (k->irq, k);
	kfree (k);

	return 0;
}

//static platform_device_id asic2_key_device_ids[] = { { IPAQ_ASIC2_KPIO_DEVICE_ID }, { 0 } };

struct platform_driver asic2_key_device_driver = {
	.driver   = {
	    .name     = "asic2-keys",
	},
	.probe    = asic2_key_probe,
	.remove   = asic2_key_remove,
	.suspend  = asic2_key_suspend,
	.resume   = asic2_key_resume
};

static int
asic2_key_init (void)
{
	return platform_driver_register(&asic2_key_device_driver);
}

static void
asic2_key_cleanup (void)
{
	platform_driver_unregister(&asic2_key_device_driver);
}

module_init (asic2_key_init);
module_exit (asic2_key_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Phil Blundell <pb@handhelds.org> and others");
MODULE_DESCRIPTION("Keyboard driver for iPAQ ASIC2");
//MODULE_DEVICE_TABLE(soc, asic2_key_device_ids);

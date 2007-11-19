/*
 * Asus Mypal A716 Buttons driver
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Creates input events for the buttons on the device
 * Based on Asus Mypal A730 buttons driver
 *
 * Copyright (C) 2005-2007 Pawel Kolodziejski
 * Copyright (C) 2004 Matthew Garrett, Nicolas Pouillon
 *
 */

#include <linux/input.h>
#include <linux/input_pda.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/platform_device.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/asus716-gpio.h>

#define GET_GPIO(gpio) (GPLR(gpio) & GPIO_bit(gpio))

static struct input_dev *buttons_dev;

extern u32 pca9535_read_input(void);
extern void a716_audio_earphone_isr(u16 data);

static u16 buttons_state = 0;

static struct {
        u16 keyno;
        u8 irq;
        u8 gpio;
        char *desc;
} buttontab [] = {
#define KEY_DEF(x) A716_IRQ (x), GPIO_NR_A716_##x
        { KEY_POWER,    KEY_DEF(POWER_BUTTON_N),    "POWER"    },
        { _KEY_CALENDAR, KEY_DEF(CALENDAR_BUTTON_N), "CALENDAR" },
        { _KEY_CONTACTS, KEY_DEF(CONTACTS_BUTTON_N), "CONTACTS" },
        { _KEY_MAIL,     KEY_DEF(TASKS_BUTTON_N),    "TASKS"    },
        { _KEY_HOMEPAGE, KEY_DEF(HOME_BUTTON_N),     "HOME"     },
#undef KEY_DEF
};

static irqreturn_t gpio_irq_handler(int irq, void *dev_id)
{
        int button, down;

        for (button = 0; button < ARRAY_SIZE(buttontab); button++)
                if (buttontab[button].irq == irq)
                        break;

        if (likely(button < ARRAY_SIZE(buttontab))) {
                down = GET_GPIO(buttontab[button].gpio) ? 0 : 1;
                input_report_key(buttons_dev, buttontab[button].keyno, down);
                input_sync(buttons_dev);
        }

	return IRQ_HANDLED;
}

static void i2c_buttons_handler(struct work_struct *unused)
{
	u32 i2c_data, buttons, button1, button2;

	i2c_data = pca9535_read_input();

	if (i2c_data == (u32)-1)
		return;

#ifdef CONFIG_SND_A716
	a716_audio_earphone_isr(i2c_data);
#endif

	buttons = i2c_data & 0xff00;
	button1 = buttons_state & 0x2f00;
	button2 = buttons & 0x2f00;

	if (!(buttons_state & GPIO_I2C_BUTTON_RECORD_N) && (buttons & GPIO_I2C_BUTTON_RECORD_N)) {
		input_report_key(buttons_dev, _KEY_RECORD, 0);
		input_sync(buttons_dev);
	}

	if (!(buttons_state & GPIO_I2C_BUTTON_UP_N) && (buttons & GPIO_I2C_BUTTON_UP_N)) {
		input_report_key(buttons_dev, KEY_PAGEUP, 0);
		input_sync(buttons_dev);
	}

	if (!(buttons_state & GPIO_I2C_BUTTON_DOWN_N) && (buttons & GPIO_I2C_BUTTON_DOWN_N)) {
		input_report_key(buttons_dev, KEY_PAGEDOWN, 0);
		input_sync(buttons_dev);
	}

	if ((buttons_state & GPIO_I2C_BUTTON_RECORD_N) && !(buttons & GPIO_I2C_BUTTON_RECORD_N)) {
		input_report_key(buttons_dev, _KEY_RECORD, 1);
		input_sync(buttons_dev);
	}

	if ((buttons_state & GPIO_I2C_BUTTON_UP_N) && !(buttons & GPIO_I2C_BUTTON_UP_N)) {
		input_report_key(buttons_dev, KEY_PAGEUP, 1);
		input_sync(buttons_dev);
	}

	if ((buttons_state & GPIO_I2C_BUTTON_DOWN_N) && !(buttons & GPIO_I2C_BUTTON_DOWN_N)) {
		input_report_key(buttons_dev, KEY_PAGEDOWN, 1);
		input_sync(buttons_dev);
	}

	if ((button1 == 0x0c00) && (button2 != 0x0c00)) {
		input_report_key(buttons_dev, KEY_UP, 0);
		input_sync(buttons_dev);
	}

	if ((button1 == 0x0a00) && (button2 != 0x0a00)) {
		input_report_key(buttons_dev, KEY_RIGHT, 0);
		input_sync(buttons_dev);
	}

	if ((button1 == 0x2200) && (button2 != 0x2200)) {
		input_report_key(buttons_dev, KEY_DOWN, 0);
		input_sync(buttons_dev);
	}

	if ((button1 == 0x2400) && (button2 != 0x2400)) {
		input_report_key(buttons_dev, KEY_LEFT, 0);
		input_sync(buttons_dev);
	}

	if ((button1 == 0x2e00) && (button2 != 0x2e00)) {
		input_report_key(buttons_dev, KEY_ENTER, 0);
		input_sync(buttons_dev);
	}

	if ((button1 != 0x0c00) && (button2 == 0x0c00)) {
		input_report_key(buttons_dev, KEY_UP, 1);
		input_sync(buttons_dev);
	}

	if ((button1 != 0x0a00) && (button2 == 0x0a00)) {
		input_report_key(buttons_dev, KEY_RIGHT, 1);
		input_sync(buttons_dev);
	}

	if ((button1 != 0x2200) && (button2 == 0x2200)) {
		input_report_key(buttons_dev, KEY_DOWN, 1);
		input_sync(buttons_dev);
	}

	if ((button1 != 0x2400) && (button2 == 0x2400)) {
		input_report_key(buttons_dev, KEY_LEFT, 1);
		input_sync(buttons_dev);
	}

	if ((button1 != 0x2e00) && (button2 == 0x2e00)) {
		input_report_key(buttons_dev, KEY_ENTER, 1);
		input_sync(buttons_dev);
	}

	buttons_state = buttons;
}

DECLARE_WORK(i2c_buttons_workqueue, i2c_buttons_handler);

static irqreturn_t i2c_irq_handler(int irq, void *dev_id)
{
	schedule_work(&i2c_buttons_workqueue);

	return IRQ_HANDLED;
}

static int a716_button_probe(struct platform_device *pdev)
{
	int i;

	if (!machine_is_a716())
		return -ENODEV;

	buttons_dev = input_allocate_device();
	if (!buttons_dev)
		return -ENOMEM;

	buttons_dev->name = "Asus MyPal A716 buttons";
	buttons_dev->phys = "a716-buttons/input0";

	set_bit(EV_KEY, buttons_dev->evbit);

	for (i = 0; i < ARRAY_SIZE(buttontab); i++) {
		set_bit(buttontab[i].keyno, buttons_dev->keybit);
	}

	set_bit(KEY_UP, buttons_dev->keybit);
	set_bit(KEY_DOWN, buttons_dev->keybit);
	set_bit(KEY_LEFT, buttons_dev->keybit);
	set_bit(KEY_RIGHT, buttons_dev->keybit);
	set_bit(KEY_ENTER, buttons_dev->keybit);
	set_bit(_KEY_RECORD, buttons_dev->keybit);
	set_bit(KEY_PAGEUP, buttons_dev->keybit);
	set_bit(KEY_PAGEDOWN, buttons_dev->keybit);

	input_register_device(buttons_dev);

	for (i = 0; i < ARRAY_SIZE(buttontab); i++) {
		set_irq_type(buttontab[i].irq, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING);
		request_irq(buttontab[i].irq, gpio_irq_handler, SA_SAMPLE_RANDOM, "a716-gpiobuttons", NULL);
	}

	set_irq_type(A716_IRQ(PCA9535_IRQ), IRQF_TRIGGER_FALLING);
	request_irq(A716_IRQ(PCA9535_IRQ), i2c_irq_handler, SA_SAMPLE_RANDOM, "a716-i2cbuttons", NULL);

	return 0;
}

static int a716_button_remove(struct platform_device *pdev)
{
	int i;

	free_irq(A716_IRQ(PCA9535_IRQ), NULL);

	for (i = 0; i < ARRAY_SIZE(buttontab); i++)
		free_irq(buttontab[i].irq, NULL);

	input_unregister_device(buttons_dev);
	input_free_device(buttons_dev);

	return 0;
}

static int a716_button_suspend(struct platform_device *pdev, pm_message_t state)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(buttontab); i++) {
		disable_irq(buttontab[i].irq);
	}

	disable_irq(A716_IRQ(PCA9535_IRQ));

	return 0;
}								

static int a716_button_resume(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(buttontab); i++) {
		set_irq_type(buttontab[i].irq, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING);
		request_irq(buttontab[i].irq, gpio_irq_handler, SA_SAMPLE_RANDOM, "a716-gpiobuttons", NULL);
		enable_irq(buttontab[i].irq);
	}

	set_irq_type(A716_IRQ(PCA9535_IRQ), IRQF_TRIGGER_FALLING);
	request_irq(A716_IRQ(PCA9535_IRQ), i2c_irq_handler, SA_SAMPLE_RANDOM, "a716-i2cbuttons", NULL);
	enable_irq(A716_IRQ(PCA9535_IRQ));

	return 0;
}								

static struct platform_driver a716_buttons_driver = {
	.driver		= {
		.name           = "a716-buttons",
	},
	.probe          = a716_button_probe,
	.remove         = a716_button_remove,
	.suspend        = a716_button_suspend,
	.resume         = a716_button_resume,
};

static int __init a716_button_init(void)
{
	if (!machine_is_a716())
		return -ENODEV;

	return platform_driver_register(&a716_buttons_driver);
}

static void __exit a716_button_exit(void)
{
        platform_driver_unregister(&a716_buttons_driver);
}

module_init(a716_button_init);
module_exit(a716_button_exit);

MODULE_AUTHOR ("Nicolas Pouillon, Pawel Kolodziejski");
MODULE_DESCRIPTION ("Asus MyPal A716 Buttons driver");
MODULE_LICENSE ("GPL");

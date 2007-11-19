/*
 * Buttons support for Asus MyPal A730(W). GPIO & I2C buttons
 * History:
 *	Jan 08 2006: Initial version by Michal Sroczynski
 *	Apr 03 2006:
 *		Serge Nikolaenko	Split a730_keys.c to a730_buttons.c & a730_joupad.c
 *	Nov 22 2006:
 *		Serge Nikolaenko	Put them all together again.
 */

#include <linux/input.h>
#include <linux/input_pda.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/gpio_keys.h>

#include <asm/mach-types.h>

#include <asm/arch/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/asus730-gpio.h>

#define GET_GPIO(gpio) (GPLR(gpio) & GPIO_bit(gpio))

static DECLARE_MUTEX_LOCKED(i2c_buttons_mutex);

static struct workqueue_struct *i2c_buttons_workqueue;
static struct work_struct i2c_buttons_irq_task;

extern u32 pca9535_read_input(void);

static struct gpio_keys_button gpio_buttons[] = {
    {_KEY_APP4,	A730_GPIO(BUTTON_LAUNCHER),	1, "LAUNCHER"},
    {_KEY_APP1,	A730_GPIO(BUTTON_CALENDAR),	1, "CALENDAR"},
    {_KEY_APP2,	A730_GPIO(BUTTON_CONTACTS),	1, "CONTACTS"},
    {_KEY_APP3,		A730_GPIO(BUTTON_TASKS),	1, "TASKS"},
    {KEY_POWER,		A730_GPIO(BUTTON_POWER),	0, "POWER"},
    {_KEY_RECORD,	A730_GPIO(BUTTON_RECORD),	1, "RECORD"},
};

//gpio field here is a bitmask
static struct gpio_keys_button i2c_buttons[] = {
    {KEY_UP,	0x1,	1, "UP"},
    {KEY_RIGHT,	0x2,	1, "RIGHT"},
    {KEY_DOWN,	0x4,	1, "DOWN"},
    {KEY_LEFT,	0x8,	1, "LEFT"},
    {KEY_ENTER,	0x10,	1, "ENTER"},
};

static struct input_dev *input_dev = NULL;
static u32 last_i2c_data = 0;

static void i2c_buttons_handler(struct work_struct *unused)
{
	int i, state;
	u32 i2c_data, bits;

	i2c_data = pca9535_read_input();

	if (i2c_data == (u32)-1) goto out;

	i2c_data = (~(i2c_data >> 8) & 0x1f);
	bits = last_i2c_data ^ i2c_data;

	if (!bits) goto out;

	for (i = 0; i < ARRAY_SIZE(i2c_buttons); i++)
	{
		if (bits & i2c_buttons[i].gpio)
		{
			state = ((bits & i2c_data) != 0);
			input_report_key(input_dev, i2c_buttons[i].keycode, state);
			input_sync(input_dev);
		}
	}
	last_i2c_data = i2c_data;
out:
	up(&i2c_buttons_mutex);
}

static irqreturn_t i2c_irq_handler(int irq, void *dev_id)
{
	if (!down_trylock(&i2c_buttons_mutex)) queue_work(i2c_buttons_workqueue, &i2c_buttons_irq_task);
	return IRQ_HANDLED;
}

static irqreturn_t gpio_irq_handler(int irq, void *dev_id)
{
	int i, state;

	for (i = 0; i < ARRAY_SIZE(gpio_buttons); i++)
	{
		if (IRQ_GPIO(gpio_buttons[i].gpio) == irq)
		{
			state = (GET_GPIO(gpio_buttons[i].gpio) ? 1 : 0) ^ (gpio_buttons[i].active_low);
			state = (gpio_buttons[i].active_low ? !state : state);
			input_report_key(input_dev, gpio_buttons[i].keycode, state);
			input_sync(input_dev);
		}
	}
	return IRQ_HANDLED;
}

static int buttons_probe(struct platform_device *pdev)
{
	int i, err;
	int irqflag = IRQF_DISABLED;
#ifdef CONFIG_PREEMPT_RT
	irqflag |= IRQF_NODELAY;
#endif
	
	if (!(input_dev = input_allocate_device())) return -ENOMEM;
	
	input_dev = input_allocate_device();
	
	input_dev->name = "Asus A730(W) buttons";
	input_dev->phys = "a730-buttons/input0";
	set_bit(EV_KEY, input_dev->evbit);
	
	for (i = 0; i < ARRAY_SIZE(gpio_buttons); i++)
	{
		set_bit(gpio_buttons[i].keycode, input_dev->keybit);
	}
	
	for (i = 0; i < ARRAY_SIZE(i2c_buttons); i++)
	{
		set_bit(i2c_buttons[i].keycode, input_dev->keybit);
	}
	
	input_register_device(input_dev);
	
	i2c_buttons_workqueue = create_singlethread_workqueue("buttond");
	INIT_WORK(&i2c_buttons_irq_task, i2c_buttons_handler);

	set_irq_type(A730_IRQ(PCA9535_IRQ), IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_TRIGGER_LOW | IRQF_TRIGGER_HIGH);
	err = request_irq(A730_IRQ(PCA9535_IRQ), i2c_irq_handler, irqflag, "a730-i2cbuttons", NULL);
	if (err)
	{
		printk(KERN_ERR "%s: Cannot assign i2c IRQ\n", __FUNCTION__);
		return err;
	}
	
	up(&i2c_buttons_mutex);

	for (i = 0; i < ARRAY_SIZE(gpio_buttons); i++)
	{
		//assign irq and keybit
		set_irq_type(IRQ_GPIO(gpio_buttons[i].gpio), IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING);
		err = request_irq(IRQ_GPIO(gpio_buttons[i].gpio), gpio_irq_handler, irqflag, "a730-gpiobuttons", NULL);
		if (err)
		{
			printk(KERN_ERR "%s: Cannot assign GPIO(%d) IRQ\n", __FUNCTION__, gpio_buttons[i].gpio);
			return err;
		}
	}
	
	return 0;
}

static int buttons_remove(struct platform_device *pdev)
{
	int i;
	
	down(&i2c_buttons_mutex);
	free_irq(A730_IRQ(PCA9535_IRQ), NULL);
	
	for (i = 0; i < ARRAY_SIZE(gpio_buttons); i++)
	{
		free_irq(IRQ_GPIO(gpio_buttons[i].gpio), NULL);
	}
	
	input_unregister_device(input_dev);
	input_free_device(input_dev);
	return 0;
}

static int buttons_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int buttons_resume(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver buttons_driver = {
    .driver = {
	.name           = "a730-buttons",
    },
    .probe          = buttons_probe,
    .remove         = buttons_remove,
#ifdef CONFIG_PM
    .suspend        = buttons_suspend,
    .resume         = buttons_resume,
#endif
};

static int __init buttons_init(void)
{
	if (!machine_is_a730()) return -ENODEV;
	return platform_driver_register(&buttons_driver);
}

static void __exit buttons_exit(void)
{
	platform_driver_unregister(&buttons_driver);
}

module_init(buttons_init);
module_exit(buttons_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michal Sroczynski <msroczyn@elka.pw.edu.pl>");
MODULE_DESCRIPTION("Buttons driver for Asus MyPal A730(W)");

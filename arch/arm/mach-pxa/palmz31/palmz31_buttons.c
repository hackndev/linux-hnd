/*
 * linux/arch/arm/mach-pxa/palmz31/palmz31-buttons.c
 *
 *  Button driver for Palm Zire 31
 *
 *  Author: Scott Mansell <phiren@gmail.com>
 */
 
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <asm/arch/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/irqs.h>

#define GET_GPIO(gpio) (GPLR(gpio) & GPIO_bit(gpio))

#define MAX_BUTTONS 23

struct input_dev *button_dev;
static struct device_driver palmz31_buttons_driver;

static struct {
	int keycode;
	char *desc;
} palmz31_buttons[MAX_BUTTONS] = {
	{ -1,		NULL }, /* GPIO 0 */
	{ -1,		NULL },
	{ -1,		"Power" },
	{ -1,		NULL },
	{ -1,		NULL },
	{ -1,		NULL }, /* GPIO 5 */
	{ -1,		NULL },
	{ -1,		NULL },
	{ -1,		NULL },
	{ -1,		NULL },
	{ -1,		NULL }, /* GPIO 10 */
	{ KEY_LEFTSHIFT,"Contacts" },
	{ -1,		NULL },
	{ KEY_PAGEUP,	"Calander" },
	{ KEY_ENTER,	"Center" },
	{ -1,		NULL }, /* GPIO 15 */
	{ -1,		NULL },
	{ -1,		NULL },
	{ -1,		NULL },
	{ KEY_LEFT,	"Left" },
	{ KEY_RIGHT,	"Right" }, /* GPIO 20 */
	{ KEY_DOWN,	"Down" },
	{ KEY_UP,	"Up" }
};

static irqreturn_t palmz31_keypad_irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	input_report_key(button_dev, palmz31_buttons[IRQ_TO_GPIO(irq)].keycode,
			GET_GPIO(IRQ_TO_GPIO(irq)) ? 0 : 1);
	return IRQ_HANDLED;
}

static int palmz31_buttons_probe(struct device *dev)
{
	int err = 0;
	int i;
        
	//if(!machine_is_tunge2())
	//	return -ENODEV;

	button_dev = input_allocate_device();
	button_dev->evbit[0] = BIT(EV_KEY);
	for(i=0;i<MAX_BUTTONS;i++) {
		if(palmz31_buttons[i].keycode >= 0) {
			button_dev->keybit[LONG(palmz31_buttons[i].keycode)] |=
				BIT(palmz31_buttons[i].keycode);
		}
	}
	button_dev->name = "Palm Zire 31 buttons";
	button_dev->id.bustype = BUS_HOST;
	input_register_device(button_dev);	
	
	for(i=0;i<MAX_BUTTONS;i++) {
		if(palmz31_buttons[i].keycode >= 0) {
			err += request_irq(IRQ_GPIO(i),
				palmz31_keypad_irq_handler,
				SA_SAMPLE_RANDOM | SA_INTERRUPT,
				"keypad", (void*)i);
			set_irq_type(IRQ_GPIO(i), IRQT_BOTHEDGE);
		}
	}
	
	if(err) {
		printk("err = %d\n", err);
	}

	return 0;
}

static int palmz31_buttons_remove (struct device *dev)
{
	int i;
	for(i=0;i<MAX_BUTTONS;i++) {
		if(palmz31_buttons[i].keycode >= 0) {
			free_irq(IRQ_GPIO(i), (void*)i);
		}
	}
        return 0;
}

static struct device_driver palmz31_buttons_driver = {
	.name           = "palmz31-buttons",
	.bus            = &platform_bus_type,
	.probe          = palmz31_buttons_probe,
        .remove		= palmz31_buttons_remove,
#ifdef CONFIG_PM
	.suspend        = NULL,
	.resume         = NULL,
#endif
};

static int __init palmz31_buttons_init(void)
{
	//if(!machine_is_tunge2())
	//	return -ENODEV;

	return driver_register(&palmz31_buttons_driver);
}

static void __exit palmz31_buttons_exit(void)
{
	input_unregister_device(button_dev);
	driver_unregister(&palmz31_buttons_driver);
}

module_init(palmz31_buttons_init);
module_exit(palmz31_buttons_exit);

MODULE_AUTHOR ("Scott Mansell <phiren@gmail.com");
MODULE_DESCRIPTION ("Button support for Palm Zire 31");
MODULE_LICENSE ("GPL");

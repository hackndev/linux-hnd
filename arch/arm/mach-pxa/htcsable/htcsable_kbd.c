/*
 *  htcsable_kbd.c
 *  Keyboard support for the h4350 ipaq
 *
 *  (c) Shawn Anderson, March, 2006
 *  This code is released under the GNU General Public License
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/input_pda.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <asm/arch/h4000-gpio.h>
#include <asm/arch/h4000-asic.h>
#include <asm/arch/pxa-regs.h>
#include <asm/hardware.h>
#include <asm/arch/bitfield.h>
#include <asm/mach-types.h>     /* machine_is_hw6900 */

static struct input_dev *htcsable_kbd;

#define KEY_FUNC 0x32

static irqreturn_t htcsable_pwr_btn(int irq, void* data)
{
	int pressed;
	pressed = !GET_H4000_GPIO(POWER_BUTTON_N);

	input_report_key(htcsable_kbd, KEY_POWER, pressed);
	input_sync(htcsable_kbd);

	return IRQ_HANDLED;
}

static int __init htcsable_kbd_probe(struct platform_device * pdev)
{
	if (!machine_is_hw6900())
		return -ENODEV;

	if (!(htcsable_kbd = input_allocate_device()))
		return -ENOMEM;

	htcsable_kbd->name        = "HP iPAQ hw6915 power key driver";
	htcsable_kbd->evbit[0]    = BIT(EV_KEY) | BIT(EV_REP);

	set_bit(KEY_POWER, htcsable_kbd->keybit);
	request_irq(IRQ_GPIO(GPIO_NR_H4000_POWER_BUTTON_N), htcsable_pwr_btn,
			IRQF_SAMPLE_RANDOM, "Power button", NULL);
	set_irq_type(IRQ_GPIO(GPIO_NR_H4000_POWER_BUTTON_N), IRQT_BOTHEDGE);

	input_register_device(htcsable_kbd);

	return 0;
}

static int htcsable_kbd_remove(struct platform_device * pdev)
{       
	input_unregister_device(htcsable_kbd);
	free_irq(IRQ_GPIO(GPIO_NR_H4000_POWER_BUTTON_N), NULL);

	return 0;
}

static struct platform_driver htcsable_kbd_driver = {
	.probe    = htcsable_kbd_probe,
	.remove   = htcsable_kbd_remove,
	.driver   = {
	    .name = "htcsable_kbd",
	},
};

static int __init htcsable_kbd_init(void)
{
	return platform_driver_register(&htcsable_kbd_driver);
}

static void __exit htcsable_kbd_exit(void)
{
	platform_driver_unregister(&htcsable_kbd_driver);
}

module_init(htcsable_kbd_init);
module_exit(htcsable_kbd_exit);

MODULE_AUTHOR("Shawn Anderson");
MODULE_DESCRIPTION("Keyboard support for the iPAQ h43xx");
MODULE_LICENSE("GPL");

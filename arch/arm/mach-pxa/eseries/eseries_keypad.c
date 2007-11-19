#include <linux/module.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h> 

#include <asm/mach-types.h>

static int counter;

static irqreturn_t eseries_keypad_irq(int irq, void *dummy) {
	printk("eseries_keypad: irq!  %d\n", ++counter);
	return IRQ_HANDLED;
}


static int __init eseries_keypad_init(void) {
	unsigned int irq;
	int retval;

	/* I suspect the keypad IRQ is intended largely as a wakeup source
	 * Although it might be useful for detecting the first keypress.
	 */
        if(machine_is_e740() || machine_is_e750())
		irq=IRQ_GPIO(14);
        else if(machine_is_e800())
                irq=IRQ_GPIO(7);
        else
                return -ENODEV;

	retval = request_irq (irq, eseries_keypad_irq, SA_INTERRUPT,
                              "keypad", NULL);

	if(retval)
		return -EBUSY;

	set_irq_type(irq, IRQT_FALLING);

	printk("eseries keypad driver initialised\n");

        return 0;
}

module_init(eseries_keypad_init);


/*
 * Jornada 720 touchscreen interface based on Jornada 56x interface
 */
 
#include <linux/input.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/interrupt.h>

#include <asm/arch/hardware.h>
#include <asm/arch/jornada720.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>

MODULE_AUTHOR("Alex Lange <chicken@handhelds.org>");
MODULE_DESCRIPTION("Jornada 720 touchscreen driver");
MODULE_LICENSE("GPL");

static char jornada720_ts_name[] = "Jornada 720 touchscreen";

static struct input_dev dev;

static irqreturn_t jornada720_mouse_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	int down;
	int X[3], Y[3], high_x, high_y, x, y;
	
	udelay(1);
	
	down = ( (GPLR & GPIO_JORNADA720_MOUSE) == 0);
	
	if(!down) {
		input_report_key(&dev, BTN_TOUCH, down); /* report a pen up */
		input_sync(&dev);
		return IRQ_HANDLED;
	}
	
	/* read x & y data from mcu interface and pass it on */
	
	mcu_start(MCU_GetTouchSamples);
	X[0] = mcu_read();
	X[1] = mcu_read();
	X[2] = mcu_read();
	Y[0] = mcu_read();	
	Y[1] = mcu_read();
	Y[2] = mcu_read();
	high_x = mcu_read(); /* msbs of samples */
	high_y = mcu_read();	
	mcu_end();
	
	X[0] |= (high_x & 3) << 8;
	X[1] |= (high_x & 0xc) << 6;
	X[2] |= (high_x & 0x30) << 4;

	Y[0] |= (high_y & 3) << 8;
	Y[1] |= (high_y & 0xc) << 6;
	Y[2] |= (high_y & 0x30) << 4;

        /* simple averaging filter */
	x = (X[0] + X[1] + X[2])/3;
	y = (Y[0] + Y[1] + Y[2])/3;
	
	input_report_key(&dev, BTN_TOUCH, down);
	input_report_abs(&dev, ABS_X, x);
	input_report_abs(&dev, ABS_Y, y);
	input_sync(&dev);
	
	return IRQ_HANDLED;
	
}

static unsigned char jornada720_normal_keymap[128] = {
	0, 1, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 87, 0, 0, 0,
	0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0, 0, 0,
	0, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 43, 14, 0, 0, 0,
	0, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 0, 0, 0, 0, 0,
	0, 44, 45, 46, 47, 48, 49, 50, 51, 52, 0, 40, 28, 0, 0, 0,
	0, 15, 0, 42, 0, 40, 0, 0, 0, 0, 103, 0, 54, 0, 0, 0,
	0, 0, 0, 0, 0, 56, 0, 0, 0, 105, 108, 106, 0, 0, 0, 0,
	0, 55, 29, 0, 57, 0, 0, 0, 53, 111, 0, 0, 0, 0, 0, 116,
};

static irqreturn_t jornada720_keyboard_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	int key, keycode;
	int count, mcu_data=0;
	
	mcu_start(MCU_GetScanKeyCode);
	count = mcu_read();
	
	while (count-- > 0) {
		mcu_data = mcu_read();
	}
	
	key = mcu_data;
	
	if (key > 128)
		key = key - 128;
	
	mcu_end();
	
	keycode = jornada720_normal_keymap[key];
	
	if (mcu_data < 128) {
		input_report_key(&dev, keycode, 1);
		input_sync(&dev);
	}
	else {
		input_report_key(&dev, keycode, 0);
		input_sync(&dev);
	}
	
	return IRQ_HANDLED;
}

static int __init jornada720_ts_init(void)
{
	int i;
	printk("Initialising the Jornada 720 touchscreen and keyboard...\n");
	
	init_input_dev(&dev);
	dev.evbit[0] = BIT(EV_KEY) | BIT(EV_ABS) | BIT(EV_REP);
	dev.absbit[0] = BIT(ABS_X) | BIT(ABS_Y);
	dev.keybit[LONG(BTN_TOUCH)] = BIT(BTN_TOUCH);
	dev.keybit[LONG(KEY_SUSPEND)] |= BIT(KEY_SUSPEND);
	
	dev.absmin[ABS_X] = 270; dev.absmin[ABS_Y] = 180;
	dev.absmax[ABS_X] = 3900;  dev.absmax[ABS_Y] = 3700;
	
	for ( i=0 ; i<=128 ; i++ ) {
		if (!(jornada720_normal_keymap[i])) {
		}
		else
			set_bit(jornada720_normal_keymap[i], dev.keybit);
	}
	
	dev.name = jornada720_ts_name;

	if (request_irq(GPIO_JORNADA720_MOUSE_IRQ, jornada720_mouse_interrupt, SA_INTERRUPT, "Jornada720 Mouse", NULL))
		printk("Unable to grab Jornada 720 touchscreen IRQ!\n");
		
	set_irq_type(GPIO_JORNADA720_MOUSE_IRQ, IRQT_RISING);
	
	if (request_irq(GPIO_JORNADA720_KEYBOARD_IRQ, jornada720_keyboard_interrupt, SA_INTERRUPT, "Jornada720 Keyboard", NULL))
		printk("Unable to grab Jornada 720 keyboard IRQ!\n");
		
	set_irq_type(GPIO_JORNADA720_KEYBOARD_IRQ, IRQT_FALLING);
			
	input_register_device(&dev);
	
	return 0;
}

module_init(jornada720_ts_init);


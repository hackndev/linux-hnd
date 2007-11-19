/*
 * Jornada 56x touchscreen interface
 */
 
#include <linux/input.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/interrupt.h>

#include <asm/arch/jornada56x.h>
#include <asm/arch/hardware.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>

#define ASIC_DELAY 10

MODULE_AUTHOR("Alex Lange <chicken@handhelds.org>");
MODULE_DESCRIPTION("Jornada 56x touchscreen driver");
MODULE_LICENSE("GPL");

static char jornada56x_ts_name[] = "Jornada 56x touchscreen";
static void timer_callback( unsigned long nr);
static int inside_timer;

extern void jornada_microwire_start(void);
extern void jornada_microwire_read(int *value1, int *value2);
extern void jornada_microwire_init(void);

static struct timer_list timer;
static struct input_dev *input_dev;

static int scancode_map[3][5] = {
	{KEY_LEFT, KEY_RIGHT, KEY_ENTER, 0, 0},
	{KEY_UP, KEY_DOWN, 0, KEY_UP, KEY_DOWN},
	{KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12}
};

static irqreturn_t mouse_interrupt(int irq, void *dev_id)
{
	
	if (inside_timer)
		printk("inside!!$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
	else {
		inside_timer = 1;
		timer_callback(1);
		disable_irq(GPIO_JORNADA56X_TOUCH_IRQ);
	}
	return IRQ_HANDLED;
}

static irqreturn_t display_led_button_interrupt(int irq, void *dev_id)
{
	if (!(JORNADA_GPDPLR & GPIO_GPIO14)) {
		JORNADA_GPDPSR = GPIO_GPIO14;
                udelay(2);      // Delay 2 milliseconds.
                GPSR = GPIO_GPIO24;
                JORNADA_GPDPCR = JORNADA_BACKLIGHT;
	}
	
	else {
		JORNADA_GPDPCR = GPIO_GPIO14;
                udelay(2);      // Delay 2 milliseconds.
                GPCR = GPIO_GPIO24;
                JORNADA_GPDPSR = JORNADA_BACKLIGHT;
	}
	return IRQ_HANDLED;
}


static irqreturn_t power_button_interrupt(int irq, void *dev_id)
{
	input_report_key(input_dev, KEY_SUSPEND, 1);
	input_report_key(input_dev, KEY_SUSPEND, 0);
	input_sync(input_dev);
	
	return IRQ_HANDLED;
}

static irqreturn_t button_interrupt(int irq, void *dev_id)
{
	static unsigned char scancode;
	int row = 0, col = 0, oldint, this_key;

	oldint = JORNADA_GPIOB_FE_EN & 0x3e00; /* save old int mask */
	JORNADA_GPIOB_FE_EN &= ~0x3e00; /* mask off interrupts */
	JORNADA_GPIOB_RE_EN &= ~0x3e00; /* mask off interrupts */
	JORNADA_GPBPCR = 0x1c0;		/* all output 0 */
	for (row = 0; row <= 2; row++) {
		JORNADA_GPBPDR = (1<<(row+6))	/* output 1 row at a time */
			| (JORNADA_GPBPDR & ~0x1c0); /* others are inputs */
		udelay(100);
		if ((this_key = 0x1f - ((JORNADA_GPBPLR >> 9) & 0x1f))) {
			while(!(this_key & 1)) {
				col++;
				this_key >>= 1;
			}
			scancode = scancode_map[row][col];
			break;
		}
	}
	JORNADA_GPBPCR = 0x1c0;		/* all output 0 */
	JORNADA_GPBPDR |= 0x1c0;	/* turn row lines into outputs */
	JORNADA_GPIOB_FE_EN |= oldint;
	JORNADA_GPIOB_RE_EN |= oldint;
	if (scancode) {
		input_report_key(input_dev, scancode, (row != 3));
		input_sync(input_dev);
	}
	
	return IRQ_HANDLED;
}

static int jornada56x_pen(void)
{
	int down;
	int x, y;
	
	down = ( (GPLR & GPIO_JORNADA56X_TOUCH) == 0);
	
	if(!down) {
		input_report_key(input_dev, BTN_TOUCH, down);
		input_sync(input_dev);
		return 0;
	}
	
	/* read x & y data from microwire interface and pass it on */
	
	jornada_microwire_start();
	JORNADA_MWDR = JORNADA_MW_TOUCH_X;
	JORNADA_MWDR = JORNADA_MW_TOUCH_X;
	JORNADA_MWDR = JORNADA_MW_TOUCH_X;
	JORNADA_MWDR = JORNADA_MW_TOUCH_X;
	JORNADA_MWDR = JORNADA_MW_TOUCH_Y;
	JORNADA_MWDR = JORNADA_MW_TOUCH_Y;
	JORNADA_MWDR = JORNADA_MW_TOUCH_Y;
	JORNADA_MWDR = JORNADA_MW_TOUCH_Y;
	jornada_microwire_read(&x, &y);
	
	if ( y < 200 )
		return down;
	
	input_report_key(input_dev, BTN_TOUCH, down);
	input_report_abs(input_dev, ABS_X, x);
	input_report_abs(input_dev, ABS_Y, y);
	input_sync(input_dev);
	
	return down;
	
}

static void timer_callback( unsigned long nr )
{
	int down = 1;

	if (!nr)
		down = jornada56x_pen();
	if (down) {
		mod_timer(&timer, jiffies + (nr ? 0: ((ASIC_DELAY * HZ) / 1000)) );
		GRER &= ~GPIO_JORNADA56X_TOUCH;
		GFER &= ~GPIO_JORNADA56X_TOUCH;
	}
	else {
		del_timer_sync(&timer);
		inside_timer = 0;
		GRER &= ~GPIO_JORNADA56X_TOUCH;
		GFER &= ~GPIO_JORNADA56X_TOUCH;
		udelay(10);
		enable_irq(GPIO_JORNADA56X_TOUCH_IRQ);
	}
}

static int __init jornada56x_ts_init(void)
{
	int i;
	printk("Initialising the Jornada 56x touchscreen...\n");
	
	jornada_microwire_init();
	
	input_dev = input_allocate_device();
	input_dev->evbit[0] = BIT(EV_KEY) | BIT(EV_ABS) | BIT(EV_REP);
	input_dev->absbit[0] = BIT(ABS_X) | BIT(ABS_Y);
	input_dev->keybit[LONG(BTN_TOUCH)] = BIT(BTN_TOUCH);
	input_dev->keybit[LONG(KEY_SUSPEND)] |= BIT(KEY_SUSPEND);

	input_set_abs_params(input_dev, ABS_X, 300, 3800, 0, 0);
	input_set_abs_params(input_dev, ABS_Y, 200, 3700, 0, 0);
	
	set_bit(KEY_F8, input_dev->keybit);
	set_bit(KEY_F9, input_dev->keybit);
	set_bit(KEY_F10, input_dev->keybit);
	set_bit(KEY_F11, input_dev->keybit);
	set_bit(KEY_F12, input_dev->keybit);
	set_bit(KEY_UP, input_dev->keybit);
	set_bit(KEY_RIGHT, input_dev->keybit);
	set_bit(KEY_LEFT, input_dev->keybit);
	set_bit(KEY_DOWN, input_dev->keybit);
	set_bit(KEY_ENTER, input_dev->keybit);


	input_dev->name = jornada56x_ts_name;
	input_dev->phys = "j56x/input0";

	timer.function = timer_callback;
	timer.data = (unsigned long)NULL;
	init_timer(&timer);
	

	if (request_irq(GPIO_JORNADA56X_TOUCH_IRQ, mouse_interrupt, SA_INTERRUPT, "Jornada56x Mouse", NULL))
		printk("Unable to grab Jornada 56x touchscreen IRQ!\n");
		
	set_irq_type(GPIO_JORNADA56X_TOUCH_IRQ, IRQT_FALLING);
	
	if(request_irq(GPIO_JORNADA56X_POWER_SWITCH_IRQ, power_button_interrupt, SA_INTERRUPT, "Power Button", NULL))
		printk("Unable to grab Jornada 56x Power Button IRQ!\n");
		
	set_irq_type(GPIO_JORNADA56X_POWER_SWITCH_IRQ, IRQT_FALLING);
	
	for (i = 0; i <= 4; i++) {
		JORNADA_GPIOB_RE_EN |= 1 << (9+i);
		if (request_irq(IRQ_JORNADA_KEY_COL0+i, button_interrupt, SA_INTERRUPT, "Jornada56x Buttons", NULL)) 
			printk("Unable to grab Jornada 56x Button IRQ's!\n");
	}
		
	if(request_irq(IRQ_JORNADA_MMC+10, display_led_button_interrupt, SA_INTERRUPT, "Jornada56x LED Button", NULL))
		printk("Unable to grap Jornada 56x display led button IRQ!\n");
		
	set_irq_type(IRQ_JORNADA_MMC+10, IRQT_FALLING);
		
	input_register_device(input_dev);
	
	return 0;
}

module_init(jornada56x_ts_init);


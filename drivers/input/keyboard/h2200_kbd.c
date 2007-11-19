/*
 * h2200 buttons
 *
 * Author: Matthew Reimer
 *
 */

#include <linux/device.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/input_pda.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>

#include <asm/irq.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/irqs.h>

#include <asm/arch/h2200.h>
#include <asm/arch/h2200-irqs.h>
#include <asm/arch/h2200-gpio.h>

#include <asm/hardware/ipaq-hamcop.h>
#include <linux/mfd/hamcop_base.h>

#define JOYPAD_FILTER_WIDTH	(32768 / 20)	/* 1/20 second, using RTC */
#define JOYPAD_REPEAT_DELAY	250		/* 250ms delay before repeat */
#define JOYPAD_REPEAT_PERIOD	100		/* 100ms repeat */

#define JOYPAD_NW	(HAMCOP_GPIO_MON_GPA(5))
#define JOYPAD_SE	(HAMCOP_GPIO_MON_GPA(6))
#define JOYPAD_SW	(HAMCOP_GPIO_MON_GPB(0))
#define JOYPAD_NE	(HAMCOP_GPIO_MON_GPB(1))
#define JOYPAD_ACTION   (HAMCOP_GPIO_MON_GPB(2))

#define JOYPAD_DIRECTIONS (JOYPAD_NW | JOYPAD_NE | JOYPAD_SW | JOYPAD_SE)

struct h2200_kbd {
	struct input_dev *input;
	struct clk *clk_gpio;
	struct timer_list joypad_timer;
};

struct h2200_button_data {
	int irq;
	int keycode;
	unsigned long bitmask;
	unsigned char *name;
};

static struct h2200_button_data h2200_buttons[] = {
	{ _HAMCOP_IC_INT_RSTBUTTON,	KEY_POWER,     1 << 0, "power button" },
	{ _HAMCOP_IC_INT_APPBUTTON1,	_KEY_CALENDAR, 1 << 1, "app button 1" },
	{ _HAMCOP_IC_INT_APPBUTTON2,	_KEY_CONTACTS, 1 << 2, "app button 2" },
	{ _HAMCOP_IC_INT_APPBUTTON3,	_KEY_MAIL,     1 << 3, "app button 3" },
	{ _HAMCOP_IC_INT_APPBUTTON4,	_KEY_HOMEPAGE, 1 << 4, "app button 4" },
};

static struct h2200_button_data h2200_joypad[] = {
	{ _HAMCOP_IC_INT_JOYPAD_LEFT,	KEY_LEFT,      1 << 5,  "joypad NW"  },
	{ _HAMCOP_IC_INT_JOYPAD_RIGHT,	KEY_RIGHT,     1 << 6,  "joypad SE"  },
	{ _HAMCOP_IC_INT_JOYPAD_DOWN,	KEY_DOWN,      1 << 8,  "joypad SW"  },
	{ _HAMCOP_IC_INT_JOYPAD_UP,  	KEY_UP,        1 << 9,  "joypad NE"  },
	{ _HAMCOP_IC_INT_JOYPAD_ACTION, KEY_ENTER,     1 << 10, "joypad action"},
};

static irqreturn_t
h2200_button_isr(int isr, void *dev_id)
{
	struct h2200_kbd *h2200_kbd = (struct h2200_kbd *)dev_id;
	struct device *hamcop = &h2200_hamcop.dev;
	struct h2200_button_data *b;
	int down;

	b = &h2200_buttons[isr - hamcop_irq_base(hamcop)];

        down = (((hamcop_get_gpio_a(hamcop) & 0xff) |
		 (hamcop_get_gpio_b(hamcop) & 0xff) << 8) & b->bitmask) ? 0 : 1;

	input_report_key(h2200_kbd->input, b->keycode, down);
	input_sync(h2200_kbd->input);

	return IRQ_HANDLED;
}

static irqreturn_t
h2200_joypad_isr(int isr, void *dev_id)
{
	struct device *hamcop = &h2200_hamcop.dev;
	struct h2200_kbd *h2200_kbd = (struct h2200_kbd *)dev_id;
	struct input_dev *input = h2200_kbd->input;
	int mon, keycode;
	static int last_keycode = 0;

	mon = hamcop_get_gpiomon(hamcop);

	/* The GPIOs go to 0 when pressed, so XOR them to simplify the test. */
	switch ((mon & JOYPAD_DIRECTIONS) ^ JOYPAD_DIRECTIONS) {

	    case JOYPAD_NW | JOYPAD_SW:
		keycode = KEY_LEFT;
		break;
	    case JOYPAD_NE | JOYPAD_SE:
		keycode = KEY_RIGHT;
		break;
	    case JOYPAD_NW | JOYPAD_NE:
		keycode = KEY_UP;
		break;
	    case JOYPAD_SW | JOYPAD_SE:
		keycode = KEY_DOWN;
		break;
	    default:
		keycode = 0;
		break;
	}

	/* Only generate an 'action' key press if none of the joypad
	 * directions are also being pressed. */
	if (!((mon & JOYPAD_DIRECTIONS) ^ JOYPAD_DIRECTIONS) &&
	     ((mon & JOYPAD_ACTION) ^ JOYPAD_ACTION))
		keycode = KEY_ENTER;

	/* Generate release and press events. */
	if (keycode != last_keycode) {

		if (last_keycode) {
		    del_timer_sync(&h2200_kbd->joypad_timer);
		    input_report_key(input, last_keycode, 0);
		}

		if (keycode) {
		    input_report_key(input, keycode, 1);
		    input->repeat_key = keycode;
		    if (keycode != KEY_ENTER)
			    mod_timer(&h2200_kbd->joypad_timer, jiffies +
				      msecs_to_jiffies(input->rep[REP_DELAY]));
		}

		input_sync(input);
		last_keycode = keycode;
	}

	return IRQ_HANDLED;
}

static void
h2200_joypad_repeat(unsigned long data)
{
	struct h2200_kbd *h2200_kbd = (void *)data;

	input_event(h2200_kbd->input, EV_KEY, h2200_kbd->input->repeat_key, 2);
	input_sync(h2200_kbd->input);

	mod_timer(&h2200_kbd->joypad_timer, jiffies +
		  msecs_to_jiffies(h2200_kbd->input->rep[REP_PERIOD]));
}

#ifdef CONFIG_PM

static int
h2200_kbd_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int
h2200_kbd_resume(struct platform_device *pdev)
{
	return 0;
}

#else
#define h2200_kbd_suspend   NULL
#define h2200_kbd_resume    NULL
#endif

/* XXX Needs error recovery. */
static int __init
h2200_kbd_probe(struct platform_device *pdev)
{
	struct device *hamcop = &h2200_hamcop.dev;
	struct h2200_kbd *h2200_kbd;
	struct input_dev *input;
	int i, ret;

	h2200_kbd = kzalloc(sizeof(struct h2200_kbd), GFP_KERNEL);
	if (!h2200_kbd) {
		ret = -ENOMEM;
		goto err0;
	}

	input = input_allocate_device();
	if (!input) {
		ret = -ENOMEM;
		goto err1;
	}

	platform_set_drvdata(pdev, h2200_kbd);
	h2200_kbd->input = input;
        input->private = h2200_kbd;
        input->name = "h2200 buttons";
        input->id.bustype = BUS_HOST;
        input->id.vendor = 0x0001;
        input->id.product = 0x0001;
        input->id.version = 0x0100;
        input->evbit[0] = BIT(EV_KEY) | BIT(EV_REP);
	input->rep[REP_DELAY] = JOYPAD_REPEAT_DELAY;
	input->rep[REP_PERIOD] = JOYPAD_REPEAT_PERIOD;

	for (i = 0; i < ARRAY_SIZE(h2200_buttons); i++)
		set_bit(h2200_buttons[i].keycode, input->keybit);

	for (i = 0; i < ARRAY_SIZE(h2200_joypad); i++)
		set_bit(h2200_joypad[i].keycode, input->keybit);

        input_register_device(input);

	/* Configure and enable the button and joypad GPIOs, including
	 * hardware debouncing for the joypad.
	 *
	 * Surprisingly, the debouncing seems to work even when
	 * HAMCOP's RTC clock is turned off. */

	h2200_kbd->clk_gpio = clk_get(NULL, "gpio");
	if (IS_ERR(h2200_kbd->clk_gpio)) {
		printk(KERN_ERR "h2200_kbd: failed to get gpio clock");
		return -ENOENT;
	}
	clk_enable(h2200_kbd->clk_gpio);

	hamcop_set_gpio_a_con(hamcop,
			      HAMCOP_GPIO_GPx_CON_MASK(0) |
			      HAMCOP_GPIO_GPx_CON_MASK(1) |
			      HAMCOP_GPIO_GPx_CON_MASK(2) |
			      HAMCOP_GPIO_GPx_CON_MASK(3) |
			      HAMCOP_GPIO_GPx_CON_MASK(4) |
			      HAMCOP_GPIO_GPx_CON_MASK(5) |
			      HAMCOP_GPIO_GPx_CON_MASK(6),
			      HAMCOP_GPIO_GPx_CON_MODE(0, INPUT) |
			      HAMCOP_GPIO_GPx_CON_MODE(1, INPUT) |
			      HAMCOP_GPIO_GPx_CON_MODE(2, INPUT) |
			      HAMCOP_GPIO_GPx_CON_MODE(3, INPUT) |
			      HAMCOP_GPIO_GPx_CON_MODE(4, INPUT) |
			      HAMCOP_GPIO_GPx_CON_MODE(5, INPUT) |
			      HAMCOP_GPIO_GPx_CON_MODE(6, INPUT));

	hamcop_set_gpio_a_int(hamcop,
	      HAMCOP_GPIO_GPx_INT_MASK(0) |
	      HAMCOP_GPIO_GPx_INT_MASK(1) |
	      HAMCOP_GPIO_GPx_INT_MASK(2) |
	      HAMCOP_GPIO_GPx_INT_MASK(3) |
	      HAMCOP_GPIO_GPx_INT_MASK(4) |
	      HAMCOP_GPIO_GPx_INT_MASK(5) |
	      HAMCOP_GPIO_GPx_INT_MASK(6),

	      HAMCOP_GPIO_GPx_INT_MODE(0, HAMCOP_GPIO_GPx_INT_INTEN_ENABLE |
					  HAMCOP_GPIO_GPx_INT_INTLV_BOTHEDGES) |
	      HAMCOP_GPIO_GPx_INT_MODE(1, HAMCOP_GPIO_GPx_INT_INTEN_ENABLE |
					  HAMCOP_GPIO_GPx_INT_INTLV_BOTHEDGES) |
	      HAMCOP_GPIO_GPx_INT_MODE(2, HAMCOP_GPIO_GPx_INT_INTEN_ENABLE |
					  HAMCOP_GPIO_GPx_INT_INTLV_BOTHEDGES) |
	      HAMCOP_GPIO_GPx_INT_MODE(3, HAMCOP_GPIO_GPx_INT_INTEN_ENABLE |
					  HAMCOP_GPIO_GPx_INT_INTLV_BOTHEDGES) |
	      HAMCOP_GPIO_GPx_INT_MODE(4, HAMCOP_GPIO_GPx_INT_INTEN_ENABLE |
					  HAMCOP_GPIO_GPx_INT_INTLV_BOTHEDGES) |
	      HAMCOP_GPIO_GPx_INT_MODE(5, HAMCOP_GPIO_GPx_INT_INTEN_ENABLE |
					  HAMCOP_GPIO_GPx_INT_INTLV_BOTHEDGES) |
	      HAMCOP_GPIO_GPx_INT_MODE(6, HAMCOP_GPIO_GPx_INT_INTEN_ENABLE |
					  HAMCOP_GPIO_GPx_INT_INTLV_BOTHEDGES));

	hamcop_set_gpio_a_flt(hamcop, 5, HAMCOP_GPIO_GPxFLT0_SELCLK_RTC |
			      HAMCOP_GPIO_GPxFLT0_FLTEN, JOYPAD_FILTER_WIDTH);
	hamcop_set_gpio_a_flt(hamcop, 6, HAMCOP_GPIO_GPxFLT0_SELCLK_RTC |
			      HAMCOP_GPIO_GPxFLT0_FLTEN, JOYPAD_FILTER_WIDTH);

	hamcop_set_gpio_b_con(hamcop,
			      HAMCOP_GPIO_GPx_CON_MASK(0) |
			      HAMCOP_GPIO_GPx_CON_MASK(1) |
			      HAMCOP_GPIO_GPx_CON_MASK(2),
			      HAMCOP_GPIO_GPx_CON_MODE(0, INPUT) |
			      HAMCOP_GPIO_GPx_CON_MODE(1, INPUT) |
			      HAMCOP_GPIO_GPx_CON_MODE(2, INPUT));

	hamcop_set_gpio_b_int(hamcop,
	      HAMCOP_GPIO_GPx_INT_MASK(0) |
	      HAMCOP_GPIO_GPx_INT_MASK(1) |
	      HAMCOP_GPIO_GPx_INT_MASK(2),

	      HAMCOP_GPIO_GPx_INT_MODE(0, HAMCOP_GPIO_GPx_INT_INTEN_ENABLE |
					  HAMCOP_GPIO_GPx_INT_INTLV_BOTHEDGES) |
	      HAMCOP_GPIO_GPx_INT_MODE(1, HAMCOP_GPIO_GPx_INT_INTEN_ENABLE |
					  HAMCOP_GPIO_GPx_INT_INTLV_BOTHEDGES) |
	      HAMCOP_GPIO_GPx_INT_MODE(2, HAMCOP_GPIO_GPx_INT_INTEN_ENABLE |
					  HAMCOP_GPIO_GPx_INT_INTLV_BOTHEDGES));

	hamcop_set_gpio_b_flt(hamcop, 0, HAMCOP_GPIO_GPxFLT0_SELCLK_RTC |
			      HAMCOP_GPIO_GPxFLT0_FLTEN, JOYPAD_FILTER_WIDTH);
	hamcop_set_gpio_b_flt(hamcop, 1, HAMCOP_GPIO_GPxFLT0_SELCLK_RTC |
			      HAMCOP_GPIO_GPxFLT0_FLTEN, JOYPAD_FILTER_WIDTH);
	hamcop_set_gpio_b_flt(hamcop, 2, HAMCOP_GPIO_GPxFLT0_SELCLK_RTC |
			      HAMCOP_GPIO_GPxFLT0_FLTEN, JOYPAD_FILTER_WIDTH);

	/* Initialize the joypad repeat timer. */
        init_timer(&h2200_kbd->joypad_timer);
	h2200_kbd->joypad_timer.function = h2200_joypad_repeat;
	h2200_kbd->joypad_timer.data = (unsigned long)h2200_kbd;

	/* Allocate irqs for the buttons. */
	for (i = 0; i < ARRAY_SIZE (h2200_buttons); i++)
		request_irq(hamcop_irq_base(hamcop) + h2200_buttons[i].irq,
			    h2200_button_isr, SA_SAMPLE_RANDOM,
			    h2200_buttons[i].name, h2200_kbd);

	/* Allocate irqs for the joypad. */
	for (i = 0; i < ARRAY_SIZE (h2200_joypad); i++)
		request_irq(hamcop_irq_base(hamcop) + h2200_joypad[i].irq,
			    h2200_joypad_isr, SA_SAMPLE_RANDOM,
			    h2200_joypad[i].name, h2200_kbd);

	return 0;

err1:
	kfree(h2200_kbd);
err0:
	return ret;
}

static int
h2200_kbd_remove(struct platform_device *pdev)
{
	struct h2200_kbd *h2200_kbd = platform_get_drvdata(pdev);
	struct device *hamcop = &h2200_hamcop.dev;
	int i;

	del_timer_sync(&h2200_kbd->joypad_timer);

	/* Turn off HAMCOP's button and joypad GPIO interrupts and
	   unregister our interrupts.
	   XXX Should these GPIOs be configured as outputs to save power? */

	hamcop_set_gpio_a_int(hamcop,
	      HAMCOP_GPIO_GPx_INT_MASK(0) |
	      HAMCOP_GPIO_GPx_INT_MASK(1) |
	      HAMCOP_GPIO_GPx_INT_MASK(2) |
	      HAMCOP_GPIO_GPx_INT_MASK(3) |
	      HAMCOP_GPIO_GPx_INT_MASK(4) |
	      HAMCOP_GPIO_GPx_INT_MASK(5) |
	      HAMCOP_GPIO_GPx_INT_MASK(6),

	      HAMCOP_GPIO_GPx_INT_MODE(0, HAMCOP_GPIO_GPx_INT_INTEN_DISABLE) |
	      HAMCOP_GPIO_GPx_INT_MODE(1, HAMCOP_GPIO_GPx_INT_INTEN_DISABLE) |
	      HAMCOP_GPIO_GPx_INT_MODE(2, HAMCOP_GPIO_GPx_INT_INTEN_DISABLE) |
	      HAMCOP_GPIO_GPx_INT_MODE(3, HAMCOP_GPIO_GPx_INT_INTEN_DISABLE) |
	      HAMCOP_GPIO_GPx_INT_MODE(4, HAMCOP_GPIO_GPx_INT_INTEN_DISABLE) |
	      HAMCOP_GPIO_GPx_INT_MODE(5, HAMCOP_GPIO_GPx_INT_INTEN_DISABLE) |
	      HAMCOP_GPIO_GPx_INT_MODE(6, HAMCOP_GPIO_GPx_INT_INTEN_DISABLE));

	hamcop_set_gpio_b_int(hamcop,
	      HAMCOP_GPIO_GPx_INT_MASK(0) |
	      HAMCOP_GPIO_GPx_INT_MASK(1) |
	      HAMCOP_GPIO_GPx_INT_MASK(2),

	      HAMCOP_GPIO_GPx_INT_MODE(0, HAMCOP_GPIO_GPx_INT_INTEN_DISABLE) |
	      HAMCOP_GPIO_GPx_INT_MODE(1, HAMCOP_GPIO_GPx_INT_INTEN_DISABLE) |
	      HAMCOP_GPIO_GPx_INT_MODE(2, HAMCOP_GPIO_GPx_INT_INTEN_DISABLE));

	clk_disable(h2200_kbd->clk_gpio);
	clk_put(h2200_kbd->clk_gpio);

	for (i = 0; i < ARRAY_SIZE(h2200_buttons); i++)
		free_irq(hamcop_irq_base(hamcop) + h2200_buttons[i].irq,
			 h2200_kbd);

	for (i = 0; i < ARRAY_SIZE(h2200_joypad); i++)
		free_irq(hamcop_irq_base(hamcop) + h2200_joypad[i].irq,
			 h2200_kbd);

	input_unregister_device(h2200_kbd->input);
	kfree(h2200_kbd);

	return 0;
}


static struct platform_driver h2200_kbd_driver = {
	.driver	    = {
		.name = "h2200 buttons",
	},
	.probe	    = h2200_kbd_probe,
	.remove	    = h2200_kbd_remove,
	.suspend    = h2200_kbd_suspend,
	.resume     = h2200_kbd_resume,

};

static int __devinit
h2200_kbd_init(void)
{
	return platform_driver_register(&h2200_kbd_driver);
}

static void __exit
h2200_kbd_exit(void)
{
	platform_driver_unregister(&h2200_kbd_driver);
}

module_init(h2200_kbd_init);
module_exit(h2200_kbd_exit);

MODULE_AUTHOR("Matthew Reimer <mreimer@vpop.net>");
MODULE_DESCRIPTION("HP iPAQ h2200 buttons/joypad driver");
MODULE_LICENSE("Dual BSD/GPL");

/*
 *  h4300_kbd.c
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
#include <asm/mach-types.h>
#include <asm/arch/h4000-gpio.h>
#include <asm/arch/h4000-asic.h>
#include <linux/mfd/asic3_base.h>
#include <asm/arch/pxa-regs.h>
#include <asm/hardware.h>

static struct input_dev *h4300_kbd;
#define a3 &h4000_asic3.dev

#define KEY_FUNC 0x32
static unsigned int h4300_keycode[0x80] = {
/*0*/   0, 0, 0, 0, 0, 0, 0, 0,
        0,             0,              _KEY_CALENDAR,  _KEY_CONTACTS,
        0,             0,              _KEY_MAIL,      _KEY_HOMEPAGE,
/*1*/   KEY_R,         KEY_RIGHT,      KEY_W,          KEY_E,
        KEY_SELECT,    KEY_U,          KEY_I,          KEY_P,
        KEY_F,         KEY_Y,          KEY_Q,          KEY_D,
        KEY_DOWN,      KEY_K,          KEY_O,          KEY_BACKSPACE,
/*2*/   KEY_X,         KEY_H,          KEY_A,          KEY_S,
        KEY_T,         KEY_J,          KEY_L,          KEY_ENTER,
        KEY_C,         KEY_B,          KEY_LEFTSHIFT,  KEY_Z,
        KEY_G,         KEY_M,          KEY_1,          KEY_APOSTROPHE,
/*3*/   KEY_ESC,       KEY_SPACE,      0/*KEY_FUNC*/,  KEY_TAB,
        KEY_V,         KEY_N,          KEY_COMMA,      KEY_SLASH,
        KEY_LEFT,      KEY_UP,         0, 0,
        KEY_LEFTCTRL,  KEY_DOT,        0, 0,
/*4*/   0, 0, 0, 0, 0, 0, 0, 0,
        0,             0,              KEY_PROG1,      KEY_PROG2,
        0,             0,              KEY_PROG3,      KEY_PROG4,
/*5*/   KEY_5,         KEY_RIGHT,      KEY_2,          KEY_4,
        KEY_SELECT,    KEY_1,          KEY_2,          KEY_SLASH,
        KEY_EQUAL,     KEY_MINUS,      KEY_GRAVE,      KEY_SEMICOLON,
        KEY_DOWN,      KEY_5,          KEY_3,          KEY_DELETE,
/*6*/   KEY_SEMICOLON, KEY_KPPLUS,     KEY_9,          KEY_0,
        KEY_MINUS,     KEY_4,          KEY_6,          KEY_ENTER,
        KEY_LEFT,      KEY_RIGHT,      KEY_CAPSLOCK,   KEY_7,
        KEY_UP,        KEY_8,          KEY_9,          KEY_APOSTROPHE,
/*7*/   KEY_ESC,       KEY_COMPOSE,    0/*KEY_FUNC*/,  KEY_TAB,
        KEY_DOWN,      KEY_7,          KEY_0,          KEY_3,
        KEY_LEFT,      KEY_UP,         0, 0,
        KEY_BACKSLASH, KEY_KPASTERISK, 0, 0
};

static int asic3_spi_process_byte(unsigned char data)
{
#define ASIC3_SPI_CTRL    (_IPAQ_ASIC3_SPI_Base + _IPAQ_ASIC3_SPI_Control)
#define ASIC3_SPI_TXDATA  (_IPAQ_ASIC3_SPI_Base + _IPAQ_ASIC3_SPI_TxData)
#define ASIC3_SPI_RXDATA  (_IPAQ_ASIC3_SPI_Base + _IPAQ_ASIC3_SPI_RxData)
#define SPI_CTRL_SEL      (1 << 6)
#define SPI_CTRL_SPIE     (1 << 5)
#define SPI_CTRL_SPE      (1 << 4)
	unsigned long flags;
	unsigned int timeout;
	unsigned int ctrl;
	int result = 0;

	local_irq_save(flags);

	/* Enable interrupts */
	ctrl = asic3_read_register(a3, ASIC3_SPI_CTRL) | SPI_CTRL_SPIE;
	asic3_write_register(a3, ASIC3_SPI_CTRL, ctrl);

	/* Set data to send */
	asic3_write_register(a3, ASIC3_SPI_TXDATA, data);
	/* Start the transfer (duplex) */
	asic3_write_register(a3, ASIC3_SPI_CTRL, (ctrl | SPI_CTRL_SPE));

	/* Wait for transfer completion and receive data being ready */
	for (timeout = 255; timeout > 0; timeout--) {
		if (!(asic3_read_register(a3, ASIC3_SPI_CTRL) & SPI_CTRL_SPE)){
			udelay(20);    /* wait for a while, or we'll miss it */
			result = asic3_read_register(a3, ASIC3_SPI_RXDATA);
			break;
		}
	}

	/* Disable interrupts */
	ctrl = asic3_read_register(a3, ASIC3_SPI_CTRL) & ~SPI_CTRL_SPIE;
	asic3_write_register(a3, ASIC3_SPI_CTRL, ctrl);

	local_irq_restore(flags);
	return result & 0xff;
}

void kbd_task(unsigned long na)
{
#define FNC_PRESS   (1<<0)
#define FNC_REL     (1<<1)
#define KEY_REL     (1<<2)
#define FAKE_SHIFT  (1<<3)
	static int pressed;
	unsigned char scancode = 0;
	static unsigned char flags = 0;

	scancode = asic3_spi_process_byte(0);
	pressed  = (scancode & 0x80) ? 0 : 1;
	scancode &= ~0x80;

	if ((scancode != KEY_FUNC) && (flags & FNC_PRESS)) {
		if (!pressed) {
			if (flags & FNC_REL)
				flags &= ~(FNC_PRESS | FNC_REL | KEY_REL);
			else
				flags |= KEY_REL;
		}
		scancode += 0x40; // the keys alternate function
	}

	switch (scancode) {
	case KEY_FUNC:
		if (pressed)
			flags = FNC_PRESS;
		else if (flags & KEY_REL)
			flags &= ~(FNC_PRESS | FNC_REL | KEY_REL);
		else
			flags |= FNC_REL;
		break;

	case 0x2e:case 0x37:case 0x50:case 0x52:case 0x53:case 0x5a: case 0x5b:
	case 0x62:case 0x63:case 0x64:case 0x6b:case 0x6f:case 0x77: case 0x7c:
		flags |= FAKE_SHIFT;
		input_report_key(h4300_kbd, KEY_LEFTSHIFT, pressed);
		break;

	default:
		if (flags & FAKE_SHIFT) {
			input_report_key(h4300_kbd, KEY_LEFTSHIFT, 0);
			flags &= ~FAKE_SHIFT;
		}
		break;
	}

	input_report_key(h4300_kbd, h4300_keycode[scancode], pressed);
	input_sync(h4300_kbd);
}
DECLARE_TASKLET(task, kbd_task, 0);

static irqreturn_t h4300_keyboard(int irq, void *dev_id)
{
	tasklet_schedule(&task);
	return IRQ_HANDLED;
}

static void setup_h4300kbd(void)
{
	asic3_set_clock_cdex(a3, CLOCK_CDEX_SPI, CLOCK_CDEX_SPI);
	asic3_set_clock_cdex(a3, CLOCK_CDEX_EX1, CLOCK_CDEX_EX1);
	udelay(20);

	asic3_set_gpio_dir_b(a3, GPIOB_KEYBOARD_IRQ, 0);
	asic3_set_gpio_out_b(a3,
	                (GPIOB_MICRO_3V3_EN | GPIOB_KEYBOARD_WAKE_UP),
	                (GPIOB_MICRO_3V3_EN | GPIOB_KEYBOARD_WAKE_UP) );

	asic3_set_gpio_dir_c(a3, GPIOC_KEY_RXD, 0);

	asic3_set_gpio_alt_fn_c(a3,
	                GPIOC_KEY_RXD | GPIOC_KEY_TXD | GPIOC_KEY_CLK,
	                GPIOC_KEY_RXD | GPIOC_KEY_TXD | GPIOC_KEY_CLK);

	asic3_set_gpio_dir_d(a3,  // disable h4100 buttons
	                (GPIOD_TASK_BUTTON_N     | GPIOD_MAIL_BUTTON_N |
	                 GPIOD_CONTACTS_BUTTON_N | GPIOD_CALENDAR_BUTTON_N),
	                (GPIOD_TASK_BUTTON_N     | GPIOD_MAIL_BUTTON_N |
	                 GPIOD_CONTACTS_BUTTON_N | GPIOD_CALENDAR_BUTTON_N));
	asic3_write_register(a3, 0x400, 0xa104);
}

static int __init h4300_kbd_probe(struct platform_device * pdev)
{
	int i, base_irq = asic3_irq_base(a3);

	if (!(h4300_kbd = input_allocate_device()))
		return -ENOMEM;

	setup_h4300kbd();

	h4300_kbd->name        = "HP iPAQ h4300 keyboard driver";
	h4300_kbd->evbit[0]    = BIT(EV_KEY) | BIT(EV_REP);
	h4300_kbd->keycode     = h4300_keycode;
	h4300_kbd->keycodesize = sizeof(unsigned char);
	h4300_kbd->keycodemax  = ARRAY_SIZE(h4300_keycode);

	for (i = 0; i < h4300_kbd->keycodemax; i++)
		if (h4300_keycode[i])
			set_bit(h4300_keycode[i], h4300_kbd->keybit);
	request_irq(base_irq + H4000_KEYBOARD_IRQ, &h4300_keyboard,
			SA_SAMPLE_RANDOM, "Keyboard", NULL);

	input_register_device(h4300_kbd);

	return 0;
}

static int h4300_kbd_remove(struct platform_device * pdev)
{       
	int irq_base = asic3_irq_base(a3);

	input_unregister_device(h4300_kbd);
	free_irq(irq_base + H4000_KEYBOARD_IRQ, NULL);

	return 0;
}

void h4300_kbd_shutdown(struct platform_device * pdev)
{
	asic3_set_gpio_dir_b(a3, GPIOB_KEYBOARD_IRQ, GPIOB_KEYBOARD_IRQ);
	asic3_set_gpio_out_b(a3,
	                (GPIOB_MICRO_3V3_EN | GPIOB_KEYBOARD_WAKE_UP), 0);
	asic3_set_gpio_dir_c(a3,GPIOC_KEY_RXD,GPIOC_KEY_RXD);
}

int h4300_kbd_suspend(struct platform_device * pdev, pm_message_t state)
{
	h4300_kbd_shutdown(pdev);
	return 0;
}
int h4300_kbd_resume(struct platform_device * pdev)
{
	setup_h4300kbd();
	return 0;
}

static struct platform_driver h4300_kbd_driver = {
	.probe    = h4300_kbd_probe,
	.remove   = h4300_kbd_remove,
	.shutdown = h4300_kbd_shutdown,
#ifdef CONFIG_PM
	.suspend  = h4300_kbd_suspend,
	.resume   = h4300_kbd_resume,
#endif
	.driver   = {
	    .name = "h4300-kbd",
	},
};

static int __init h4300_kbd_init(void)
{
	if (!machine_is_h4300()) {
		printk("h4300_kbd: h4300 not detected\n");
		return -ENODEV;
	}
	printk("h4300_kbd: Keyboard support for the iPAQ h4300\n");
	return platform_driver_register(&h4300_kbd_driver);
}

static void __exit h4300_kbd_exit(void)
{
	platform_driver_unregister(&h4300_kbd_driver);
}

module_init(h4300_kbd_init);
module_exit(h4300_kbd_exit);

MODULE_AUTHOR("Shawn Anderson");
MODULE_DESCRIPTION("Keyboard support for the iPAQ h4300");
MODULE_LICENSE("GPL");

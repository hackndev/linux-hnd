/*
 * LED interface for Himalaya, the HTC PocketPC.
 *
 * License: GPL
 *
 * Author: Luke Kenneth Casson Leighton, Copyright(C) 2004
 *
 * Copyright(C) 2004, Luke Kenneth Casson Leighton.
 *
 * History:
 *
 * 2004-02-19	Luke Kenneth Casson Leighton	created.
 *
 */
 
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/mfd/asic3_base.h>

#include <asm/hardware/ipaq-asic3.h>
#include <asm/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/mach-types.h>
#include <asm/arch/htcblueangel-gpio.h>
#include <asm/arch/htcblueangel-asic.h>

#ifdef DEBUG
#define dprintk(x...) printk(x)
#else
#define dprintk(x...)
#endif

#define FN_BIT 0x80
#define SHIFT_BIT 0x80

static struct qkey {
	unsigned char code;
	unsigned char key;
	
} qkeys[] = {
	{0x01, KEY_ESC},	/* operator button */
	{0x05, KEY_L},
	{0x06, KEY_O},
	{0x07, KEY_P},
	{0x08, KEY_ENTER},
	{0x12, KEY_LEFTSHIFT},
	{0x14, KEY_RESERVED},	/* FN */
	{0x17, KEY_MINUS},	/* ? */
	{0x23, KEY_TAB},
	{0x26, KEY_DOT},	/* . */
	{0x27, KEY_SLASH},	/* _ */
	{0x28, KEY_LEFTCTRL},	/* Menu */
	{0x34, KEY_BACKSPACE},
	{0x35, KEY_T},
	{0x36, KEY_Y},
	{0x37, KEY_U},
	{0x38, KEY_I},
	{0x41, KEY_Q},
	{0x42, KEY_W},
	{0x43, KEY_E},
	{0x44, KEY_R},
	{0x45, KEY_G},
	{0x46, KEY_H},
	{0x47, KEY_J},
	{0x48, KEY_K},
	{0x51, KEY_A},
	{0x52, KEY_S},
	{0x53, KEY_D},
	{0x54, KEY_F},
	{0x55, KEY_V},
	{0x56, KEY_B},
	{0x57, KEY_N},
	{0x58, KEY_M},
	{0x61, KEY_Z},
	{0x62, KEY_X},
	{0x63, KEY_C},
	{0x64, KEY_LEFTALT},
	{0x65, KEY_SPACE},
	{0x67, KEY_COMMA},	/* , */
	/* With FN pressed */
	{0x01|FN_BIT, KEY_ESC},	/* vodafone button */
	{0x05|FN_BIT, KEY_6},
	{0x06|FN_BIT, KEY_3},
	{0x07|FN_BIT, KEY_MINUS|SHIFT_BIT}, /* sharp s */
	{0x08|FN_BIT, KEY_ENTER},
	{0x12|FN_BIT, KEY_CAPSLOCK},
	{0x14|FN_BIT, KEY_RESERVED},	/* FN */
	{0x17|FN_BIT, KEY_9},		/* ? */
	{0x23|FN_BIT, KEY_TAB},
	{0x26|FN_BIT, KEY_0},
	{0x27|FN_BIT, KEY_BACKSLASH},	/* _ */
	{0x28|FN_BIT, KEY_LEFTCTRL},	/* Menu */
	{0x34|FN_BIT, KEY_BACKSPACE},
	{0x35|FN_BIT, KEY_5|SHIFT_BIT},
	{0x36|FN_BIT, KEY_6|SHIFT_BIT},
	{0x37|FN_BIT, KEY_1},
	{0x38|FN_BIT, KEY_2},
	{0x41|FN_BIT, KEY_1|SHIFT_BIT},
	{0x42|FN_BIT, KEY_2|SHIFT_BIT},
	{0x43|FN_BIT, KEY_3|SHIFT_BIT},
	{0x44|FN_BIT, KEY_4|SHIFT_BIT},
	{0x45|FN_BIT, KEY_UP},
	{0x46|FN_BIT, KEY_RIGHTBRACE},	/* + */
	{0x47|FN_BIT, KEY_4},
	{0x48|FN_BIT, KEY_5},
	{0x51|FN_BIT, KEY_8|SHIFT_BIT},
	{0x52|FN_BIT, KEY_9|SHIFT_BIT},
	{0x53|FN_BIT, KEY_7|SHIFT_BIT},
	{0x54|FN_BIT, KEY_0|SHIFT_BIT},
	{0x55|FN_BIT, KEY_DOWN},
	{0x56|FN_BIT, KEY_RIGHT},
	{0x57|FN_BIT, KEY_7},
	{0x58|FN_BIT, KEY_8},
	{0x61|FN_BIT, KEY_RESERVED},	/* @ */
	{0x62|FN_BIT, KEY_RESERVED},	/* Euro Sign */
	{0x63|FN_BIT, KEY_LEFT},
	{0x64|FN_BIT, KEY_LEFTALT},
	{0x65|FN_BIT, KEY_SPACE},
	{0x67|FN_BIT, KEY_RIGHTBRACE|SHIFT_BIT},	/* * */
};

static int num_qkeys=sizeof(qkeys)/sizeof(struct qkey);

static struct input_dev *blueangel_kp_input_dev;

static int fn_bit;

static irqreturn_t blueangel_qkbd_irq(int irq, void *data)
{
	int timeout=0,
	    key,
	    dummy,
	    pressed,
	    i,
	    keysym,
	    shift;
	
	dprintk("%s: got interrupt for %d...\n", __FUNCTION__, irq);

	/* ASIC3 SPI read begin ------- */

	dummy=asic3_read_register(&blueangel_asic3.dev, _IPAQ_ASIC3_SPI_Base+_IPAQ_ASIC3_SPI_TxData);
	asic3_write_register(&blueangel_asic3.dev, _IPAQ_ASIC3_SPI_Base+_IPAQ_ASIC3_SPI_TxData, 0);
	asic3_write_register(&blueangel_asic3.dev, _IPAQ_ASIC3_SPI_Base+_IPAQ_ASIC3_SPI_Control, 
	asic3_read_register(&blueangel_asic3.dev, _IPAQ_ASIC3_SPI_Base+_IPAQ_ASIC3_SPI_Control) | 0x8000);
	
	asic3_write_register(&blueangel_asic3.dev, _IPAQ_ASIC3_SPI_Base+_IPAQ_ASIC3_SPI_Control, 
	asic3_read_register(&blueangel_asic3.dev, _IPAQ_ASIC3_SPI_Base+_IPAQ_ASIC3_SPI_Control) | 0x10);
	
	for (;;) {
		if (!(asic3_read_register(&blueangel_asic3.dev, _IPAQ_ASIC3_SPI_Base+_IPAQ_ASIC3_SPI_Status) & 0x10))
			break;
		udelay(10);
		if (timeout++ > 10) {
			printk("blueangel_kp: Timeout reading key\n");
			break;
		}
	}
	key=asic3_read_register(&blueangel_asic3.dev, _IPAQ_ASIC3_SPI_Base+_IPAQ_ASIC3_SPI_RxData);

	/* ASIC3 SPI read end   ------- */
	
	pressed=(key & 0x80) == 0;
	key&=0x7f;
	key|=fn_bit;
	dprintk("received key=0x%x\n", key);
	for (i = 0 ; i < num_qkeys ; i++) {
		if (qkeys[i].code == key) {
			dprintk("reporting keysym %d pressed=%d\n", qkeys[i].key, pressed);
			if ((key & ~FN_BIT) == 0x14) { /* Fn */
				fn_bit=pressed ? FN_BIT : 0;
			} else {
				keysym=qkeys[i].key;
				shift=keysym & SHIFT_BIT;
				keysym &= ~SHIFT_BIT;
				if (shift && pressed) {
					input_report_key(blueangel_kp_input_dev, KEY_RIGHTSHIFT, 1);
					input_sync(blueangel_kp_input_dev);
				}
				input_report_key(blueangel_kp_input_dev, keysym, pressed);
				input_sync(blueangel_kp_input_dev);
				if (shift && !pressed) {
					input_report_key(blueangel_kp_input_dev, KEY_RIGHTSHIFT, 0);
					input_sync(blueangel_kp_input_dev);
				}
			}
			return IRQ_HANDLED;
		}
	}
	printk("key unhandled\n");
        return IRQ_HANDLED;
}

static int blueangel_kp_init (void)
{
	int i,irq,key;

	dprintk("%s\n", __FUNCTION__);

	/* ASIC3 SPI init begin ----- */
	disable_irq(ASIC3_SPI_IRQ);
	asic3_write_register(&blueangel_asic3.dev, _IPAQ_ASIC3_SPI_Base+_IPAQ_ASIC3_SPI_Int, 0);
	/* ASIC3 SPI init   end ----- */

	irq=asic3_irq_base(&blueangel_asic3.dev) + BLUEANGEL_QKBD_IRQ;
	request_irq(irq, blueangel_qkbd_irq, IRQF_SAMPLE_RANDOM, "blueangel_qkbd", NULL);
	set_irq_type(irq, IRQ_TYPE_EDGE_FALLING);

	blueangel_kp_input_dev = input_allocate_device();	
        blueangel_kp_input_dev->evbit[0] = BIT(EV_KEY) | BIT(EV_REP);
	
	for (i = 0 ; i < num_qkeys ; i++) {
		key=qkeys[i].key;

		if (key) {
			dprintk("enabling key %d\n", qkeys[i].key);
			set_bit(key, blueangel_kp_input_dev->keybit);
		}
	}

        blueangel_kp_input_dev->name = "blueangelkb";
        blueangel_kp_input_dev->phys = "keyboard/blueangelkb";

        blueangel_kp_input_dev->id.vendor = 0x0001;
        blueangel_kp_input_dev->id.product = 0x0001;
        blueangel_kp_input_dev->id.version = 0x0100;

        input_register_device(blueangel_kp_input_dev);

	return 0;
}

static void blueangel_kp_exit (void)
{
	int irq;
	
	printk("%s\n", __FUNCTION__);
	input_unregister_device(blueangel_kp_input_dev);

	irq=asic3_irq_base(&blueangel_asic3.dev) + BLUEANGEL_QKBD_IRQ;

	free_irq(irq, NULL);
}

module_init (blueangel_kp_init);
module_exit (blueangel_kp_exit);
MODULE_LICENSE("GPL");

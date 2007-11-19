/*
 * Palm Tungsten|T3 hardware buttons
 * 
 * Author: Vladimir "Farcaller" Pouzanov <farcaller@gmail.com>, Martin Kupec
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/irq.h>

#include <asm/mach/arch.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/irqs.h>
#include <linux/platform_device.h>

//#define PALMT3_BUTTONS_DEBUG

#ifdef PALMT3_BUTTONS_DEBUG
#define DBG(x...) \
	printk(KERN_DEBUG "T3 buttons: " x)
#else
#define DBG(x...)	do { } while (0)
#endif

#define GET_GPIO(gpio) (GPLR(gpio) & GPIO_bit(gpio))

/**********************************************************
 * 		Keypad map
 * GPIO		0		10		11
 * Ground(18)	Calendar	Up		Unused
 * 19		Contact		Down		Unused
 * 20		Unused		Left		Memo
 * 21		Unused		Right		Todo
 * 22		Voice		Unused		Center
 **********************************************************/

#define T3BTN_CALENDAR 	0
#define T3BTN_VOICE 	1
#define T3BTN_CONTACT 	2

#define T3BTN_UP 	3
#define T3BTN_DOWN 	4
#define T3BTN_LEFT 	5
#define T3BTN_RIGHT 	6

#define T3BTN_MEMO 	7
#define T3BTN_TODO 	8
#define T3BTN_CENTER 	9

#define T3KEY_VOICE 	KEY_F7

#define T3KEY_CALENDAR 	KEY_F9
#define T3KEY_CONTACT 	KEY_F11
#define T3KEY_MEMO 	KEY_F10
#define T3KEY_TODO 	KEY_F12

#define T3KEY_UP 	KEY_UP
#define T3KEY_DOWN 	KEY_DOWN
#define T3KEY_LEFT 	KEY_LEFT
#define T3KEY_RIGHT 	KEY_RIGHT
#define T3KEY_CENTER 	KEY_ENTER

#define T3SLIDER_OPEN	KEY_F4
#define T3SLIDER_CLOSE	KEY_F3

#define GET_KEY_BIT(vkey, bit) ((vkey >> (bit) ) & 0x01)
#define SET_KEY_BIT(vkey, bit) (vkey |= (1 << (bit)) )
#define CLEAR_KEY_BIT(vkey, bit) (vkey &= ~(1 << (bit)) )

struct input_dev *buttons_dev; 

static struct workqueue_struct *palmtt3_workqueue;
static struct work_struct palmtt3_irq_task;

static spinlock_t btn_lock = SPIN_LOCK_UNLOCKED;

u16 key_status;
u8 slider;

static irqreturn_t palmtt3_btn_handle(int irq, void *dev_id)
{
	if((int) dev_id == 3)
		slider = 1;

	queue_work(palmtt3_workqueue, &palmtt3_irq_task);
	return IRQ_HANDLED;
}

static void palmtt3_irq_queuework(struct work_struct *data)
{
	unsigned long flags;
	int row, gpio, i;
	int key[3] = {0, 10, 11};

	if(slider)
	{
		mdelay(1);
		spin_lock_irqsave(&btn_lock, flags);
		slider = !!GET_GPIO(3);
		spin_unlock_irqrestore(&btn_lock, flags);

		input_report_key(buttons_dev, slider ? T3SLIDER_OPEN : T3SLIDER_CLOSE, 1);
		input_report_key(buttons_dev, slider ? T3SLIDER_OPEN : T3SLIDER_CLOSE, 0);
		input_sync(buttons_dev);

		slider = 0;
		return;
	}

	set_irq_type (IRQ_GPIO(0), IRQT_NOEDGE);
	set_irq_type (IRQ_GPIO(10), IRQT_NOEDGE);
	set_irq_type (IRQ_GPIO(11), IRQT_NOEDGE);

	spin_lock_irqsave(&btn_lock, flags);
	GPSR(19) = GPIO_bit(19);
	GPSR(20) = GPIO_bit(20);
	GPSR(21) = GPIO_bit(21);
	GPSR(22) = GPIO_bit(22);
	spin_unlock_irqrestore(&btn_lock, flags);

	for(row = 18; row < 23; row++)
	{
		if(row != 18)
		{
			spin_lock_irqsave(&btn_lock, flags);
			GPCR(row) = GPIO_bit(row);
			spin_unlock_irqrestore(&btn_lock, flags);
		}

		udelay(100);

		for(i = 0; i < 3; i++)
		{
			spin_lock_irqsave(&btn_lock, flags);
			gpio = GET_GPIO(key[i]);
			spin_unlock_irqrestore(&btn_lock, flags);

#define SET_KEY(x) { if(!gpio) { SET_KEY_BIT(key_status, x); } else { CLEAR_KEY_BIT(key_status, x); } }

			switch(key[i])
			{
			case 0:
				switch(row)
				{
				case 18: SET_KEY(T3BTN_CALENDAR); break;
				case 19: if(!GET_KEY_BIT(key_status, T3BTN_CALENDAR)) SET_KEY(T3BTN_CONTACT); break;
				case 22: if(!GET_KEY_BIT(key_status, T3BTN_CALENDAR)) SET_KEY(T3BTN_VOICE); break;
				}
				break;
			case 10:
				switch(row)
				{
				case 18: SET_KEY(T3BTN_UP); break;
				case 19: if(!GET_KEY_BIT(key_status, T3BTN_UP) ) SET_KEY(T3BTN_DOWN); break;
				case 20: if(!GET_KEY_BIT(key_status, T3BTN_UP) ) SET_KEY(T3BTN_LEFT); break;
				case 21: if(!GET_KEY_BIT(key_status, T3BTN_UP) ) SET_KEY(T3BTN_RIGHT); break;
				}
				break;
			case 11:
				switch(row)
				{
				case 20: SET_KEY(T3BTN_MEMO); break;
				case 21: SET_KEY(T3BTN_TODO); break;
				case 22: SET_KEY(T3BTN_CENTER); break;
				}
				break;
			}
		}
		
		if(row != 18)
		{
			spin_lock_irqsave(&btn_lock, flags);
			GPSR(row) = GPIO_bit(row);
			spin_unlock_irqrestore(&btn_lock, flags);
		}
	}

	spin_lock_irqsave(&btn_lock, flags);
	GPCR(19) = GPIO_bit(19);
	GPCR(20) = GPIO_bit(20);
	GPCR(21) = GPIO_bit(21);
	GPCR(22) = GPIO_bit(22);
	spin_unlock_irqrestore(&btn_lock, flags);

	if(key_status & 0x0007) //0000 0000 0000 0111
		set_irq_type (IRQ_GPIO(0), IRQT_RISING);
	else
		set_irq_type (IRQ_GPIO(0), IRQT_FALLING);
	if(key_status & 0x0078) //0000 0000 0111 1000
		set_irq_type (IRQ_GPIO(10), IRQT_RISING);
	else
		set_irq_type (IRQ_GPIO(10), IRQT_FALLING);

	if(key_status & 0x0380) //0000 0011 1000 0000
		set_irq_type (IRQ_GPIO(11), IRQT_RISING);
	else
		set_irq_type (IRQ_GPIO(11), IRQT_FALLING);


	for(i = 0; i < 10; i++)
	{
		switch(i)
		{
		case T3BTN_CALENDAR: row = T3KEY_CALENDAR; break;
		case T3BTN_CONTACT: row = T3KEY_CONTACT; break;
		case T3BTN_VOICE: row = T3KEY_VOICE; break;
		case T3BTN_MEMO: row = T3KEY_MEMO; break;
		case T3BTN_TODO: row = T3KEY_TODO; break;
		case T3BTN_CENTER: row = T3KEY_CENTER; break;
		case T3BTN_UP: row = T3KEY_UP; break;
		case T3BTN_DOWN: row = T3KEY_DOWN; break;
		case T3BTN_RIGHT: row = T3KEY_RIGHT; break;
		case T3BTN_LEFT: row = T3KEY_LEFT; break;
		}
		input_report_key(buttons_dev, row, GET_KEY_BIT(key_status, i) );
		input_sync(buttons_dev);
	}
}

static int palmtt3_btn_probe(struct platform_device *dev)
{
	unsigned long flags, ret;

	buttons_dev = input_allocate_device();
	buttons_dev->evbit[0] = BIT(EV_KEY) | BIT(EV_REP);
	buttons_dev->name = "Palm T|T3 buttons";
	buttons_dev->id.bustype = BUS_HOST;

	buttons_dev->keybit[LONG(T3KEY_CALENDAR)] |= BIT(T3KEY_CALENDAR);
	buttons_dev->keybit[LONG(T3KEY_CONTACT)] |= BIT(T3KEY_CONTACT);
	buttons_dev->keybit[LONG(T3KEY_VOICE)] |= BIT(T3KEY_VOICE);
	buttons_dev->keybit[LONG(T3KEY_MEMO)] |= BIT(T3KEY_MEMO);
	buttons_dev->keybit[LONG(T3KEY_TODO)] |= BIT(T3KEY_TODO);
	buttons_dev->keybit[LONG(T3KEY_UP)] |= BIT(T3KEY_UP);
	buttons_dev->keybit[LONG(T3KEY_CENTER)] |= BIT(T3KEY_CENTER);
	buttons_dev->keybit[LONG(T3KEY_DOWN)] |= BIT(T3KEY_DOWN);
	buttons_dev->keybit[LONG(T3KEY_LEFT)] |= BIT(T3KEY_LEFT);
	buttons_dev->keybit[LONG(T3KEY_RIGHT)] |= BIT(T3KEY_RIGHT);
	buttons_dev->keybit[LONG(T3SLIDER_OPEN)] |= BIT(T3SLIDER_OPEN);
	buttons_dev->keybit[LONG(T3SLIDER_CLOSE)] |= BIT(T3SLIDER_CLOSE);

	input_register_device(buttons_dev);

	key_status = 0;

	palmtt3_workqueue = create_workqueue("palmtt3btnw");
	INIT_WORK(&palmtt3_irq_task, palmtt3_irq_queuework);

	/* Configure GPIOs as Output and low */
	spin_lock_irqsave(&btn_lock, flags);

	// programing as outputs
	GPDR(19) |= GPIO_bit(19);
	GPDR(20) |= GPIO_bit(20);
	GPDR(21) |= GPIO_bit(21);
	GPDR(22) |= GPIO_bit(22);

	// setting low
	GPCR(19) = GPIO_bit(19);
	GPCR(20) = GPIO_bit(20);
	GPCR(21) = GPIO_bit(21);
	GPCR(22) = GPIO_bit(22);

	/* Configure GPIOs as inputs and falling edge detection */

	spin_unlock_irqrestore(&btn_lock, flags);

#define REG_GPIO(x, e) \
	ret = request_irq (IRQ_GPIO(x), palmtt3_btn_handle, SA_SAMPLE_RANDOM, "palmtt3_btn", (void*)x); \
	set_irq_type (IRQ_GPIO(x), e); \
	if(ret!=0) { \
		DBG("Request GPIO: %d failed\n", x); \
		return 1; \
	} else { \
		DBG("Registered GPIO %d\n", x); \
	}

	REG_GPIO(0, IRQT_FALLING);
	REG_GPIO(3, IRQT_BOTHEDGE); //slider
	REG_GPIO(10, IRQT_FALLING);
	REG_GPIO(11, IRQT_FALLING);

	return 0;
}

static int palmtt3_btn_remove (struct platform_device *dev)
{
	destroy_workqueue(palmtt3_workqueue);
	input_unregister_device(buttons_dev);

	free_irq(IRQ_GPIO(0), (void*) 0);
	free_irq(IRQ_GPIO(3), (void*) 3);
	free_irq(IRQ_GPIO(10), (void*) 10);
	free_irq(IRQ_GPIO(11), (void*) 11);
        return 0;
}

static struct platform_driver palmtt3_buttons_driver = {
	.driver		= {
		.name	= "palmtt3-btn",
		.owner	= THIS_MODULE,
	},
	.probe		= palmtt3_btn_probe,
	.remove		= palmtt3_btn_remove,
#ifdef CONFIG_PM
	.suspend	= NULL,
	.resume		= NULL,
#endif
};

static int __init palmtt3_btn_init(void)
{
	return platform_driver_register(&palmtt3_buttons_driver);
}

static void __exit palmtt3_btn_cleanup(void)
{
	platform_driver_unregister(&palmtt3_buttons_driver);
}

module_init(palmtt3_btn_init);
module_exit(palmtt3_btn_cleanup);

MODULE_AUTHOR("Martin Kupec");
MODULE_DESCRIPTION("Palm T|T3 Buttons driver");
MODULE_LICENSE("GPL");

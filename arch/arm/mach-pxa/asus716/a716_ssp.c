/*
 *  Asus MyPal A716 touchscreen and battery driver
 *
 *  Copyright (C) 2005-2007 Pawel Kolodziejski
 *  Copyright (C) 2005 Jacek Migdal
 *  Copyright (C) 2004 Richard Purdie
 *  Copyright (C) 2004 Adam Turowski, Nicolas Pouillon
 *  Copyright (C) 2002-2003 SHARP Corporation
 *  Copyright (C) 2003 Joshua Wise
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/device.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/platform_device.h>

#include <asm/mach/map.h>
#include <asm/mach-types.h>
#include <asm/arch/hardware.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>
#include <asm/arch/ssp.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/asus716-gpio.h>

static spinlock_t a716_ssp_lock = SPIN_LOCK_UNLOCKED;
static struct ssp_dev a716_ssp_dev;
static struct ssp_state a716_ssp_state;

signed long a716_ssp_putget(ulong data)
{
	unsigned long flag;
	u32 ret;

	if (a716_ssp_dev.port != 1)
		return -ETIMEDOUT;
			
	spin_lock_irqsave(&a716_ssp_lock, flag);

	ssp_write_word(&a716_ssp_dev, data);
	ssp_read_word(&a716_ssp_dev, &ret);

	spin_unlock_irqrestore(&a716_ssp_lock, flag);

	return ret;
}

#define SAMPLE_TIMEOUT 20

static struct timer_list timer_pen;
static struct input_dev *idev;

static int touch_pressed;
static int irq_disable;

static void report_touchpanel(int x, int y, int pressure)
{
	input_report_key(idev, BTN_TOUCH, pressure != 0);
	input_report_abs(idev, ABS_PRESSURE, pressure);
	input_report_abs(idev, ABS_X, x);
	input_report_abs(idev, ABS_Y, y);
	input_sync(idev);
}

#define CTRL_START  0x80
#define CTRL_YPOS   0x10
#define CTRL_Z1POS  0x30
#define CTRL_Z2POS  0x40
#define CTRL_XPOS   0x50
#define CTRL_TEMP0  0x04
#define CTRL_TEMP1  0x74
#define CTRL_VBAT   0x24
#define CTRL_AUX    0x64
#define CTRL_PD0    0x01
#define CTRL_PD1    0x02

#define ADSCTRL_ADR_SH    4

typedef struct ts_pos_s {
	unsigned long xd;
	unsigned long yd;
} ts_pos_t;

static void read_xydata(ts_pos_t *tp)
{
#define	abscmpmin(x,y,d) ( ((int)((x) - (y)) < (int)(d)) && ((int)((y) - (x)) < (int)(d)) )
	unsigned long cmd;
	unsigned int t, x, y, z[2];
	unsigned long pressure;
	int i, j, k;
	int d = 8, c = 10;
	int err = 0;

	for (i = j = k = 0, x = y = 0;; i = 1) {
		// Pressure
		cmd = CTRL_PD0 | CTRL_PD1 | CTRL_START | CTRL_Z1POS;
		t = a716_ssp_putget(cmd);
		z[i] = a716_ssp_putget(cmd);
 
		if (i)
		    break;

		// X-axis
		cmd = CTRL_PD0 | CTRL_PD1 | CTRL_START | CTRL_XPOS;
		x = a716_ssp_putget(cmd);
		for (j = 0; !err; j++) {
			t = x;
			x = a716_ssp_putget(cmd);
			if (abscmpmin(t, x, d))
				break;
			if (j > c) {
				err = 1;
				//printk("ts: x(%d,%d,%d)\n", t, x, t - x);
			}
		}

		// Y-axis
		cmd = CTRL_PD0 | CTRL_PD1 | CTRL_START | CTRL_YPOS;
		y = a716_ssp_putget(cmd);
		for (k = 0; !err; k++) {
			t = y;
			y = a716_ssp_putget(cmd);
			if (abscmpmin(t ,y , d))
				break;
			if (k > c) {
				err = 1;
				//printk("ts: y(%d,%d,%d)\n", t, y, t - y);
			}
		}
	}
	pressure = 1;
	for (i = 0; i < 2; i++) {
		if (!z[i])
			pressure = 0;
	}
	if (pressure) {
		for (i = 0; i < 2; i++){
			if (z[i] < 10)
				err = 1;
		}
		if (x >= 4095)
			err = 1;
	}

	cmd &= ~(CTRL_PD0 | CTRL_PD1);
	t = a716_ssp_putget(cmd);

	if (err == 0 && pressure != 0) {
		//printk("ts: pxyp=%d(%d/%d,%d/%d)%d\n", z[0], x, j, y, k, z[1]);
	} else {
		//printk("pxype=%d,%d,%d,%d\n", z[0], x, y, z[1]);
		x = 0; y = 0;
	}
	tp->xd = x;
	tp->yd = y;
}

static irqreturn_t a716_stylus(int irq, void *dev_id)
{
	ts_pos_t ts_pos;

	if (irq == IRQ_GPIO(GPIO_NR_A716_STYLUS_IRQ_N) && irq_disable == 0) {
		//printk("IRQ\n");
		irq_disable = 1;
		disable_irq(IRQ_GPIO(GPIO_NR_A716_STYLUS_IRQ_N));
	}

	read_xydata(&ts_pos);

	if (ts_pos.xd == 0 || ts_pos.yd == 0) {
		report_touchpanel(0, 0, 0);
		//printk("touch released\n");
		if (irq_disable == 1) {
			request_irq(IRQ_GPIO(GPIO_NR_A716_STYLUS_IRQ_N), a716_stylus, SA_SAMPLE_RANDOM, "ad7873 stylus", NULL);
			set_irq_type(IRQ_GPIO(GPIO_NR_A716_STYLUS_IRQ_N), IRQT_FALLING);
			enable_irq(IRQ_GPIO(GPIO_NR_A716_STYLUS_IRQ_N));
			irq_disable = 0;
		}
		return IRQ_HANDLED;
	}

	//printk("%04d %04d\n", (int)ts_pos.xd, (int)ts_pos.yd);
	//printk("touch pressed\n");
	report_touchpanel(ts_pos.xd, ts_pos.yd, 1);

	mod_timer(&timer_pen, jiffies + (SAMPLE_TIMEOUT * HZ) / 1000);

	//printk("callback\n");
	return IRQ_HANDLED;
};

static void a716_ts_timer(unsigned long nr)
{
	a716_stylus(-1, NULL);
};

static int __init a716_ssp_probe(struct platform_device *pdev)
{
	int ret;

	if (!machine_is_a716()) {
		return -ENODEV;
	}

	a716_gpo_set(0x4000);

	ret = ssp_init(&a716_ssp_dev, 1, 0);

	if (ret) {
		printk(KERN_ERR "Unable to register SSP handler!\n");
		return ret;
	} else {
		ssp_disable(&a716_ssp_dev);
		ssp_config(&a716_ssp_dev, (SSCR0_National | 0x0b), 0, 0, SSCR0_SerClkDiv(36));
		ssp_enable(&a716_ssp_dev);
	}

	a716_ssp_putget(CTRL_YPOS | CTRL_START);
	mdelay(1);
	a716_ssp_putget(CTRL_Z1POS | CTRL_START);
	mdelay(1);
	a716_ssp_putget(CTRL_Z2POS | CTRL_START);
	mdelay(1);
	a716_ssp_putget(CTRL_XPOS | CTRL_START);

	init_timer(&timer_pen);
	timer_pen.function = a716_ts_timer;
	timer_pen.data = (unsigned long)NULL;

	idev = input_allocate_device();
	if (!idev)
		return -ENOMEM;

	idev->name = "ad7873";
	idev->phys = "touchscreen/ad7873";

	set_bit(EV_ABS, idev->evbit);
	set_bit(EV_KEY, idev->evbit);
	set_bit(ABS_X, idev->absbit);
	set_bit(ABS_Y, idev->absbit);
	set_bit(ABS_PRESSURE, idev->absbit);
	set_bit(BTN_TOUCH, idev->keybit);
	idev->absmax[ABS_PRESSURE] = 1;
	idev->absmin[ABS_X] = 212;
	idev->absmin[ABS_Y] = 180;
	idev->absmax[ABS_X] = 3880;
	idev->absmax[ABS_Y] = 3940;

	input_register_device(idev);

	touch_pressed = 0;
	irq_disable = 0;

	set_irq_type(IRQ_GPIO(GPIO_NR_A716_STYLUS_IRQ_N), IRQT_FALLING);
	request_irq(IRQ_GPIO(GPIO_NR_A716_STYLUS_IRQ_N), a716_stylus, SA_SAMPLE_RANDOM, "ad7873 stylus", NULL);

	return 0;
}

static int a716_ssp_suspend(struct platform_device *pdev, pm_message_t state)
{
	del_timer_sync(&timer_pen);

	disable_irq(IRQ_GPIO(GPIO_NR_A716_STYLUS_IRQ_N));

	ssp_flush(&a716_ssp_dev);
	ssp_save_state(&a716_ssp_dev, &a716_ssp_state);

	return 0;
}

static int a716_ssp_resume(struct platform_device *pdev)
{
	a716_gpo_set(0x4000);

	ssp_restore_state(&a716_ssp_dev, &a716_ssp_state);
	ssp_enable(&a716_ssp_dev);

	irq_disable = 0;
	touch_pressed = 0;
	enable_irq(IRQ_GPIO(GPIO_NR_A716_STYLUS_IRQ_N));

	return 0;
}

static int a716_ssp_remove(struct platform_device *pdev)
{
	del_timer_sync(&timer_pen);

	free_irq(IRQ_GPIO(GPIO_NR_A716_STYLUS_IRQ_N), NULL);

	ssp_exit(&a716_ssp_dev);

	input_unregister_device(idev);
	input_free_device(idev);

	return 0;
}

static struct platform_driver a716_ssp_driver = {
	.driver		= {
		.name           = "a716-ssp",
	},
	.probe          = a716_ssp_probe,
	.remove         = a716_ssp_remove,
	.suspend        = a716_ssp_suspend,
	.resume         = a716_ssp_resume,
};

static int __init a716_ssp_init(void)
{
	if (!machine_is_a716())
		return -ENODEV;

	memset(&a716_ssp_dev, 0, sizeof(struct ssp_dev));

	return platform_driver_register(&a716_ssp_driver);
}

static void __exit a716_ssp_exit(void)
{
	platform_driver_unregister(&a716_ssp_driver);
}

module_init(a716_ssp_init)
module_exit(a716_ssp_exit)

MODULE_AUTHOR("Adam Turowski, Nicolas Pouillon, Pawel Kolodziejski, Jacek Migdal");
MODULE_DESCRIPTION("Asus MyPal A716 Touchscreen and Battery driver");
MODULE_LICENSE("GPL");

/*
 *  linux/arch/arm/mach-pxa/asus620_ts.c
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  Copyright (c) 2003 Adam Turowski
 *  Copyright (C) 2004 Nicolas Pouillon
 *  Copyright (C) 2006 Vincent Benony
 *
 *  2004-09-23: Nicolas Pouillon
 *      Initial code based on 2.6.0 from Adam Turowski ported to 2.6.7
 *
 *  2006-01-14: Vincent Benony
 *      Merge with A716 touchscreen code
 *      Some corrections
 */

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/apm-emulation.h>

#include <asm/hardware.h>
#include <asm/arch/hardware.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/asus620-gpio.h>

#include <linux/input.h>

#include "../generic.h"

#define SSCR0_SSE_ENABLED	0x00000080

#define SAMPLE_TIMEOUT 10

struct timer_list timer_stylus;
struct timer_list timer_bat;
struct input_dev *idev;

spinlock_t ts_lock;
int touch_pressed;
int irq_disable;

/*
 * Send a touchscreen event to the input Linux system
 */

static void report_touchpanel (int x, int y, int pressure)
{
	//printk("report (%d, %d) with pressure %d\n", x, y, pressure);
	input_report_key (idev, BTN_TOUCH, pressure != 0);
	input_report_abs (idev, ABS_PRESSURE, pressure);
	input_report_abs (idev, ABS_X, x);
	input_report_abs (idev, ABS_Y, y);
	input_sync (idev);
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

#define SSSR_TNF_MSK    (1u << 2)
#define SSSR_RNE_MSK    (1u << 3)

#define ADSCTRL_ADR_SH    4

unsigned long spi_ctrl(ulong data)
{
	unsigned long ret, flags;

	spin_lock_irqsave(&ts_lock, flags);

	SSDR = data;
	while ((SSSR & SSSR_TNF_MSK) != SSSR_TNF_MSK) udelay(0);
	udelay(1);
	while ((SSSR & SSSR_RNE_MSK) != SSSR_RNE_MSK) udelay(0);
	ret = (SSDR);

	spin_unlock_irqrestore(&ts_lock, flags);

	return ret;
}

typedef struct ts_pos_s {
	unsigned long xd;
	unsigned long yd;
} ts_pos_t;

void read_xydata(ts_pos_t *tp)
{
#define	abscmpmin(x,y,d) ( ((int)((x) - (y)) < (int)(d)) && ((int)((y) - (x)) < (int)(d)) )
	unsigned long cmd;
	unsigned int t, x, y, z[2];
	unsigned long pressure;
	int i, j, k;
	int d = 8, c = 10;
	int err = 0;

	for (i = j = k = 0, x = y = 0;; i = 1) {
		/* Pressure */
		cmd = CTRL_PD0 | CTRL_PD1 | CTRL_START | CTRL_Z1POS;
		t = spi_ctrl(cmd);
		z[i] = spi_ctrl(cmd);
 
		if (i)
		    break;

		/* X-axis */
		cmd = CTRL_PD0 | CTRL_PD1 | CTRL_START | CTRL_XPOS;
		x = spi_ctrl(cmd);
		for (j = 0; !err; j++) {
			t = x;
			x = spi_ctrl(cmd);
			if (abscmpmin(t, x, d))
				break;
			if (j > c) {
				err = 1;
				//printk("ts: x(%d,%d,%d)\n", t, x, t - x);
			}
		}

		/* Y-axis */
		cmd = CTRL_PD0 | CTRL_PD1 | CTRL_START | CTRL_YPOS;
		y = spi_ctrl(cmd);
		for (k = 0; !err; k++) {
			t = y;
			y = spi_ctrl(cmd);
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
	t = spi_ctrl(cmd);

	if (err == 0 && pressure != 0) {
		//printk("ts: pxyp=%d(%d/%d,%d/%d)%d\n", z[0], x, j, y, k, z[1]);
	} else {
		//printk("pxype=%d,%d,%d,%d\n", z[0], x, y, z[1]);
		x = 0; y = 0;
	}
	tp->xd = x;
	tp->yd = y;
}

static irqreturn_t asus620_pen(int irq, void* data)
{
	ts_pos_t ts_pos;

	//printk("IRQ TS !\n");

	if (irq == IRQ_GPIO(GPIO_NR_A620_STYLUS_IRQ) && irq_disable == 0)
	{
		irq_disable = 1;
		disable_irq(IRQ_GPIO(GPIO_NR_A620_STYLUS_IRQ));
	}

	read_xydata(&ts_pos);

	if (ts_pos.xd == 0 || ts_pos.yd == 0)
	{
		report_touchpanel(0, 0, 0);
		if (irq_disable == 1)
		{
			//set_irq_type(IRQ_GPIO(GPIO_NR_A620_STYLUS_IRQ), IRQT_FALLING);
			//request_irq(IRQ_GPIO(GPIO_NR_A620_STYLUS_IRQ), asus620_pen, SA_SAMPLE_RANDOM, "ad7873 stylus", NULL);
			enable_irq(IRQ_GPIO(GPIO_NR_A620_STYLUS_IRQ));
			irq_disable = 0;
		}
		return IRQ_HANDLED;
	}

	//printk("%04d %04d\n", (int)ts_pos.xd, (int)ts_pos.yd);
	report_touchpanel(ts_pos.xd, ts_pos.yd, 1);

	mod_timer(&timer_stylus, jiffies + (SAMPLE_TIMEOUT * HZ) / 1000);

	return IRQ_HANDLED;
};

static void asus620_ts_timer(unsigned long nr)
{
	asus620_pen(-1, NULL);
};

/*
 * Retrieve battery informations
 */

#define ASUS620_MAIN_BATTERY_MAX 1676 // ~ sometimes it's more or less (> 1700 - AC)
#define ASUS620_MAIN_BATTERY_MIN 1347
#define ASUS620_MAIN_BATTERY_RANGE (ASUS620_MAIN_BATTERY_MAX - ASUS620_MAIN_BATTERY_MIN)

//#define ASUS620_BAT_SAMPS	240
int	battery_power;
int	battery_time = 99999999;
//int	battery_samples[ASUS620_BAT_SAMPS], battery_samples_nb;
//
//static void asus620_battery(unsigned long nr)
//{
//	int sample1, sample2, sample3, sample;
//
//	sample1 = spi_ctrl(CTRL_PD0 | CTRL_START | CTRL_VBAT); // main battery: min - 1347, max - 1676 (1700 AC)
//	mdelay(100);
//	sample2 = spi_ctrl(CTRL_PD0 | CTRL_START | CTRL_VBAT);
//	mdelay(100);
//	sample3 = spi_ctrl(CTRL_PD0 | CTRL_START | CTRL_VBAT);
//	spi_ctrl(CTRL_START | CTRL_VBAT);
//
//	sample = (sample1 + sample2 + sample3) / 3;
//	sample = ((sample - ASUS620_MAIN_BATTERY_MIN) * 100) / ASUS620_MAIN_BATTERY_RANGE;
//	if (sample > 100)
//		battery_power = 100;
//	else
//		battery_power = sample;
///*
//	if (battery_power < 10 && !(GPLR(GPIO_NR_A620_AC_IN) & GPIO_bit(GPIO_NR_A620_AC_IN)))
//		a716_gpo_set(GPO_A716_POWER_LED_RED);
//	else
//		a716_gpo_clear(GPO_A716_POWER_LED_RED);
//*/
//	//printk("battery: %d\n", battery_power);
//
//
//	if (battery_samples_nb < ASUS620_BAT_SAMPS)
//	{
//		battery_samples[battery_samples_nb++] = (sample1 + sample2 + sample3) / 3 - ASUS620_MAIN_BATTERY_MIN;
//		battery_time = 999999999;
//	}
//	else
//	{
//		int	i, div;
//		for (i=1; i<ASUS620_BAT_SAMPS; i++) battery_samples[i-1] = battery_samples[i];
//		battery_samples[ASUS620_BAT_SAMPS-1] = (sample1 + sample2 + sample3) / 3 - ASUS620_MAIN_BATTERY_MIN;
//		div = battery_samples[0] - battery_samples[ASUS620_BAT_SAMPS-1];
//		if (div) battery_time = ASUS620_BAT_SAMPS * battery_samples[0] / div;
//	}
//
//	mod_timer(&timer_bat, jiffies + (1000 * HZ) / 1000);
//}

static void asus620_apm_get_power_status(struct apm_power_info *info)
{
	int	sample = spi_ctrl(CTRL_PD0|CTRL_START|CTRL_VBAT);
	battery_power = ((sample - ASUS620_MAIN_BATTERY_MIN) * 100) / ASUS620_MAIN_BATTERY_RANGE;

	info->battery_life = battery_power;

	if (!(GPLR(GPIO_NR_A620_AC_IN) & GPIO_bit(GPIO_NR_A620_AC_IN)))
		info->ac_line_status = APM_AC_OFFLINE;
	else
		info->ac_line_status = APM_AC_ONLINE;

	if (battery_power > 50)
		info->battery_status = APM_BATTERY_STATUS_HIGH;
	else if (battery_power < 10)
		info->battery_status = APM_BATTERY_STATUS_CRITICAL;
	else
		info->battery_status = APM_BATTERY_STATUS_LOW;

	info->time  = battery_time;
	info->units = APM_UNITS_SECS;
}

/*
 * Initialize touchscreen chip
 */

void asus620_ts_init_chip(void)
{
	GPDR(GPIO23_SCLK) |=  GPIO_bit(GPIO23_SCLK);
	GPDR(GPIO24_SFRM) |=  GPIO_bit(GPIO24_SFRM);
	GPDR(GPIO25_STXD) |=  GPIO_bit(GPIO25_STXD);
	GPDR(GPIO26_SRXD) &= ~GPIO_bit(GPIO26_SRXD);
	pxa_gpio_mode(GPIO23_SCLK_MD);
	pxa_gpio_mode(GPIO24_SFRM_MD);
	pxa_gpio_mode(GPIO25_STXD_MD);
	pxa_gpio_mode(GPIO26_SRXD_MD);

	SSCR0 = 0;
	SSCR0 |= 0xB; /* 12 bits */
	SSCR0 |= SSCR0_National;
	SSCR0 |= 0x1100; /* 100 mhz */

	SSCR1 = 0;

	SSCR0 |= SSCR0_SSE;

	spi_ctrl(CTRL_YPOS | CTRL_START);
	mdelay(5);
	spi_ctrl(CTRL_Z1POS | CTRL_START);
	mdelay(5);
	spi_ctrl(CTRL_Z2POS | CTRL_START);
	mdelay(5);
	spi_ctrl(CTRL_XPOS | CTRL_START);
	mdelay(5);
}

/*
 * Driver init function
 */

int asus620_ts_init( void )
{
	printk("%s: initializing the touchscreen.\n", __FUNCTION__);
	asus620_ts_init_chip();

	// Init timers
	init_timer(&timer_stylus);
	timer_stylus.function = asus620_ts_timer;
	timer_stylus.data = (unsigned long)NULL;

	/*
	battery_samples_nb = 0;
	init_timer(&timer_bat);
	timer_bat.function = asus620_battery;
	timer_bat.data = (unsigned long)NULL;
	mod_timer(&timer_bat, jiffies + (1000 * HZ) / 1000);
	*/

	apm_get_power_status = asus620_apm_get_power_status;

	// Init input device
	idev = input_allocate_device();
	idev->name = "Asus MyPal 620 touchscreen and battery driver";
	idev->phys = "ad7873";

	set_bit(EV_KEY,		idev->evbit);
	set_bit(EV_ABS,		idev->evbit);
	set_bit(BTN_TOUCH,	idev->keybit);
	set_bit(ABS_PRESSURE,	idev->absbit);
	set_bit(ABS_X,		idev->absbit);
	set_bit(ABS_Y,		idev->absbit);
	idev->absmin[ABS_X] = 212;
	idev->absmin[ABS_Y] = 180;
	idev->absmax[ABS_X] = 3880;
	idev->absmax[ABS_Y] = 3940;
	idev->absmin[ABS_PRESSURE] = 0;
	idev->absmax[ABS_PRESSURE] = 1;

	input_register_device(idev);

	spin_lock_init(&ts_lock);

	touch_pressed = 0;
	irq_disable = 0;

	set_irq_type(IRQ_GPIO(GPIO_NR_A620_STYLUS_IRQ), IRQT_FALLING);
	request_irq(IRQ_GPIO(GPIO_NR_A620_STYLUS_IRQ), asus620_pen, SA_SAMPLE_RANDOM, "ad7873 stylus", NULL);

	battery_power = 50;
	
	return 0;
}

void asus620_ts_exit( void )
{
	apm_get_power_status = NULL;
	input_unregister_device(idev);
	input_free_device(idev);

	SSCR0 &= ~SSCR0_SSE;

	del_timer_sync(&timer_stylus);
	del_timer_sync(&timer_bat);

	free_irq(IRQ_GPIO(GPIO_NR_A620_STYLUS_IRQ), NULL);
}

module_init(asus620_ts_init)
module_exit(asus620_ts_exit)

MODULE_AUTHOR("Adam Turowski, Nicolas Pouillon, Vincent Benony");
MODULE_DESCRIPTION("Touchscreen and battery support for the MyPal A620");
MODULE_LICENSE("GPL");

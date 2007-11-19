/*
 * Duramax HG Barometer Driver
 *
 * Copyright (c) 2007, SDG Systems, LLC
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * The barometer is hooked up to the SSP signals of the PXA270, but the
 * interface is so simple and doesn't need to be fast, it was easier to debug
 * by twiddling GPIOs.
 *
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/irq.h>

#include <asm/mach-types.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <asm/hardware.h>
#include <asm/arch/ghi270.h>

static struct completion hgbar_completion;

static irqreturn_t
hgbar_isr(int irq, void *junk)
{
	if((0 == gpio_get_value(GHI270_HG_GPIO26_BARRXD)?1:0)) {
		disable_irq(gpio_to_irq(GHI270_HG_GPIO26_BARRXD));
		complete(&hgbar_completion);
	}
	return IRQ_HANDLED;
}

/*
 * The maximum clock rate this device can take is 500 kHz which is a period of
 * 2 us.  Thus, each clock transition can have a 1 us delay.
 */

static void
hgbar_clock_cycle(void)
{
	gpio_set_value(GHI270_HG_GPIO23_BARCLK, 1); /* Rising edge */
	udelay(2);
	gpio_set_value(GHI270_HG_GPIO23_BARCLK, 0); /* Falling edge */
	udelay(2);
}

static void
hgbar_write(unsigned int X)
{
	gpio_set_value(GHI270_HG_GPIO25_BARTXD, (X));
	hgbar_clock_cycle();
}

static void
start_bits(void)
{
	hgbar_write(1);
	hgbar_clock_cycle();
	hgbar_clock_cycle();
}

static void
stop_bits(void)
{
	hgbar_write(0);
	hgbar_clock_cycle();
	hgbar_clock_cycle();
}

static void
hgbar_reset(void)
{
	int count;
	/* RESET Sequence */
	count = 7;
	while(count--) {
		hgbar_write(1);
		hgbar_write(0);
	}
	hgbar_write(1);
	count = 6;
	while(count--) {
		hgbar_write(0);
	}
}

static unsigned int
hgbar_result(void)
{
	int i;
	unsigned int retval = 0;
	for(i = 0; i < 16; i++) {
		hgbar_clock_cycle();
		retval = (retval << 1)
			| (gpio_get_value(GHI270_HG_GPIO26_BARRXD)?1:0);
	}
	/* Extra clock */
	hgbar_clock_cycle();
	return retval;
}

static unsigned int
hgbar_w1(void)
{
	start_bits();

	hgbar_write(0);
	hgbar_write(1);
	hgbar_write(0);
	hgbar_write(1);
	hgbar_write(0);
	hgbar_write(1);

	stop_bits();

	/* Single clock */
	hgbar_clock_cycle();

	return hgbar_result();
}

static unsigned int
hgbar_w2(void)
{
	start_bits();

	hgbar_write(0);
	hgbar_write(1);
	hgbar_write(0);
	hgbar_write(1);
	hgbar_write(1);
	hgbar_write(0);

	stop_bits();

	/* Single clock */
	hgbar_clock_cycle();

	return hgbar_result();
}

static unsigned int
hgbar_w3(void)
{
	start_bits();

	hgbar_write(0);
	hgbar_write(1);
	hgbar_write(1);
	hgbar_write(0);
	hgbar_write(0);
	hgbar_write(1);

	stop_bits();

	/* Single clock */
	hgbar_clock_cycle();

	return hgbar_result();
}

static unsigned int
hgbar_w4(void)
{
	start_bits();

	hgbar_write(0);
	hgbar_write(1);
	hgbar_write(1);
	hgbar_write(0);
	hgbar_write(1);
	hgbar_write(0);

	stop_bits();

	/* Single clock */
	hgbar_clock_cycle();

	return hgbar_result();
}

static unsigned int
hgbar_pressure(void)
{
	start_bits();

	/* Setup Bits = b'1010, Read Pressure */
	hgbar_write(1);
	hgbar_write(0);
	hgbar_write(1);
	hgbar_write(0);

	stop_bits();

	hgbar_clock_cycle();
	enable_irq(gpio_to_irq(GHI270_HG_GPIO26_BARRXD));
	hgbar_clock_cycle();


	if (!wait_for_completion_timeout(&hgbar_completion, HZ)) {
		disable_irq(gpio_to_irq(GHI270_HG_GPIO26_BARRXD));
		printk("hgbar timeout\n");
	}

	return hgbar_result();
}

static unsigned int
hgbar_temp(void)
{
	start_bits();

	/* Setup Bits = b'1001, Read Temperature */
	hgbar_write(1);
	hgbar_write(0);
	hgbar_write(0);
	hgbar_write(1);

	stop_bits();

	hgbar_clock_cycle();
	enable_irq(gpio_to_irq(GHI270_HG_GPIO26_BARRXD));
	hgbar_clock_cycle();

	if (!wait_for_completion_timeout(&hgbar_completion, 30 * HZ)) {
		disable_irq(gpio_to_irq(GHI270_HG_GPIO26_BARRXD));
		printk("hgbar timeout\n");
	}

	return hgbar_result();
}

static int
calc_temp_press(unsigned short data[6], unsigned int *temp_x10,
	unsigned int *press_x10)
{
	unsigned int c1, c2, c3, c4, c5, c6;
	unsigned int d1, d2, ut1, dt, off, sens, x;

	if(!temp_x10 || !press_x10) {
		return 0;
	}

	/* Test data in the data sheet */
	/*
	   data[0] = 50426;
	   data[1] = 9504;
	   data[2] = 48029;
	   data[3] = 55028;
	   data[5] = 17000;
	   data[4] = 22500;
	 */

	c1 = data[0] >> 1;
	c2 = ((data[2] & 0x3f) << 6) | (data[3] & 0x3f);
	c3 = data[3] >> 6;
	c4 = data[2] >> 6;
	c5 = (data[1] >> 6) | ((data[0] & 1) << 10);
	c6 = data[1] & 0x3f;
	d1 = data[5];
	d2 = data[4];

	ut1 = 8 * c5 + 20224;
	dt = d2 - ut1;
	*temp_x10 =  200 + ((dt * (c6 + 50)) >> 10);
	off = c2 * 4 + (((c4 - 512) * dt) >> 12);
	sens = c1 + ((c3 * dt) >> 10) + 24576;
	x = ((sens * (d1 - 7168)) >> 14) - off;
	*press_x10 = ((x * 10) >> 5) + 250 * 10;

	/* second order temperature compensation */
	{
		unsigned int t2, p2;
		/*
		 * The don't have test values in the data sheet for this, so
		 * this is untested.
		 */
		if(*temp_x10 < 200) {
			t2 = (11 * (c6 + 24) * (200 - *temp_x10)
				* (200 - *temp_x10)) >> 20;
			p2 = (3 * t2 * (*press_x10 - 3500)) >> 14;
		} else if(*temp_x10 <= 450) {
			/* No change */
			t2 = 0;
			p2 = 0;
		} else {
			t2 = (3 * (c6 + 24)
				* (450 - *temp_x10) * (450 - *temp_x10)) >> 20;
			p2 = (t2 * (*press_x10 - 10000)) >> 13;
		}

		*temp_x10 -= t2;
		*press_x10 -= p2;
	}

	return 1;
}

static ssize_t
hgbar_read(struct file *fp, char __user *data, size_t bytes, loff_t *offset)
{
	unsigned short rdata[6];
	unsigned int retval[2];

	if(bytes < sizeof(retval)) {
		return 0;
	}

	hgbar_reset();
	rdata[0] = hgbar_w1();
	rdata[1] = hgbar_w2();
	rdata[2] = hgbar_w3();
	rdata[3] = hgbar_w4();
	rdata[4] = hgbar_temp();
	rdata[5] = hgbar_pressure();

	calc_temp_press(rdata, &retval[0], &retval[1]);

	memcpy(data, retval, sizeof(retval));

	*offset += sizeof(retval);
	return sizeof(retval);
}

static struct file_operations hgbar_fops = {
	.owner = THIS_MODULE,
	.read  = hgbar_read,
};

static struct miscdevice hgbar_dev = {
	.minor              = MISC_DYNAMIC_MINOR,
	.name               = "hgbar",
	.fops               = &hgbar_fops,
};

/* Core Module Initialization and Exit Functions */
static int
hgbar_probe(struct platform_device *dev)
{
	int retval = 0;

	if (machine_is_ghi270()) {
		return -EINVAL;
	}

	/* Make sure 32.768 kHz oscillator is on. */
	OSCC |= OSCC_TOUT_EN | OSCC_OON;

	gpio_set_value(GHI270_HG_GPIO23_BARCLK, 0);
	gpio_direction_output(GHI270_HG_GPIO23_BARCLK);
	gpio_direction_output(GHI270_HG_GPIO25_BARTXD);
	gpio_direction_input (GHI270_HG_GPIO26_BARRXD);

	set_irq_type(gpio_to_irq(GHI270_HG_GPIO26_BARRXD), IRQT_BOTHEDGE);
	if (request_irq(gpio_to_irq(GHI270_HG_GPIO26_BARRXD), hgbar_isr,
		IRQF_DISABLED | IRQF_TRIGGER_FALLING, "HGBar", 0)) {
		printk(KERN_ERR "Unable to allocate hgbar IRQ\n");
		retval = -ENODEV;
		goto exit1;
	}
	disable_irq(gpio_to_irq(GHI270_HG_GPIO26_BARRXD));

	if (misc_register(&hgbar_dev)) {
		printk("Could not register hgbar device.\n");
		retval = -ENODEV;
		goto exit1;
	}

	printk(KERN_NOTICE "Duramax HG Barometer Driver\n");


	return 0;

exit1:
	return retval;
}

static int
hgbar_remove(struct platform_device *dev)
{

	misc_deregister(&hgbar_dev);
	free_irq(gpio_to_irq(GHI270_HG_GPIO26_BARRXD), 0);
	flush_scheduled_work();
	OSCC &= ~OSCC_TOUT_EN;
	printk(KERN_NOTICE "Duramax HG Barometer Driver Remove\n");
	return 0;
}

#ifdef CONFIG_PM

static int
hgbar_suspend(struct platform_device *dev, pm_message_t state)
{
	OSCC &= ~OSCC_TOUT_EN;
	return 0;
}

static int
hgbar_resume(struct platform_device *dev)
{
	OSCC |= OSCC_TOUT_EN | OSCC_OON;
	return 0;
}

#else
#   define hgbar_suspend 0
#   define hgbar_resume 0
#endif

struct platform_driver hgbar_driver = {
	.driver = {
		.name     = "duramax-bar",
	},
	.probe    = hgbar_probe,
	.remove   = hgbar_remove,
	.suspend  = hgbar_suspend,
	.resume   = hgbar_resume,
};

static int __init
hgbar_init(void)
{
	init_completion(&hgbar_completion);
	return platform_driver_register(&hgbar_driver);
}


static void __exit
hgbar_exit(void)
{
	platform_driver_unregister(&hgbar_driver);
}

module_init(hgbar_init);
module_exit(hgbar_exit);

MODULE_AUTHOR("SDG Systems, LLC");
MODULE_DESCRIPTION("Duramax HG Barometer Driver");
MODULE_LICENSE("GPL");

/* vim600: set noexpandtab sw=8 ts=8 :*/

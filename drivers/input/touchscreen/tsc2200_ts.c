/*
 * Copyright (C) 2003 Joshua Wise
 * Copyright (c) 2002,2003 SHARP Corporation
 * Copyright (C) 2005 Pawel Kolodziejski
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * HAL code based on h5400_asic_io.c, which is
 *  Copyright (C) 2003 Compaq Computer Corporation.
 *
 * Author:  Joshua Wise <joshua at joshuawise.com>
 *          June 2003
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
#include <linux/mfd/tsc2200.h>
#include <linux/platform_device.h>

#include <asm/mach/map.h>
#include <asm/mach-types.h>
#include <asm/arch/hardware.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>
#include <asm/arch/pxa-regs.h>
#include <asm/hardware/ipaq-asic3.h>

#define SAMPLE_TIMEOUT 20	/* sample every 20ms */

static struct timer_list timer_pen;
static struct input_dev *idev;

//static spinlock_t ts_lock;
static int irq_disable;
static int touch_pressed;

static void report_touchpanel(int x, int y, int pressure)
{
    	printk("report: x=%04d y=%04d pressure=%04d\n", 
	    x, 
	    y,
	    pressure
	    );
	    
//	input_report_key(idev, BTN_TOUCH, pressure != 0);
	input_report_abs(idev, ABS_PRESSURE, pressure);
	input_report_abs(idev, ABS_X, x);
	input_report_abs(idev, ABS_Y, y);
	input_sync(idev);
}

static void ts_check( struct tsc2200_ts_data *devdata ) {

        struct tsc2200_ts_platform_info *platform_info = devdata->platform_info;

	unsigned short x_pos, y_pos, pressure;

	int touched = tsc2200_read(devdata->tsc2200_dev, TSC2200_CTRLREG_ADC) & TSC2200_CTRLREG_ADC_PSM_TSC2200;

	if ( touched ) {

	    tsc2200_write(devdata->tsc2200_dev, 
		    TSC2200_CTRLREG_ADC,                                                                                                       
    		    TSC2200_CTRLREG_ADC_AD1 |                                                                                                        
        	    TSC2200_CTRLREG_ADC_RES (TSC2200_CTRLREG_ADC_RES_12BIT) |                                                                        
		    TSC2200_CTRLREG_ADC_AVG (TSC2200_CTRLREG_ADC_16AVG) |                                                                            
    		    TSC2200_CTRLREG_ADC_CL  (TSC2200_CTRLREG_ADC_CL_4MHZ_10BIT) |                                                                    
		    TSC2200_CTRLREG_ADC_PV  (TSC2200_CTRLREG_ADC_PV_1mS) );              



		x_pos = tsc2200_read(devdata->tsc2200_dev, TSC2200_DATAREG_X);
		y_pos = tsc2200_read(devdata->tsc2200_dev, TSC2200_DATAREG_Y);

		printk("%d\n", tsc2200_read(devdata->tsc2200_dev, TSC2200_DATAREG_KPDATA));

	//	pressure = tsc2200_read(devdata->tsc2200_dev, TSC2200_DATAREG_Z2) -
	//		   tsc2200_read(devdata->tsc2200_dev, TSC2200_DATAREG_Z1);
	    	pressure = 0;
		report_touchpanel(x_pos, y_pos, 1);

		mod_timer(&timer_pen, jiffies + (SAMPLE_TIMEOUT * HZ) / 1000);
	} else {
		//tsc2200_stop( devdata->tsc2200_dev );
		report_touchpanel(0, 0, 0);
		//printk("touch released irq=%d\n", platform_info->irq);
		enable_irq(platform_info->irq);
		printk("touch released irq=%d\n", platform_info->irq);
	}

}

static irqreturn_t tsc2200_stylus(int irq, void* data)
{
	struct tsc2200_ts_data *devdata = data;
        struct tsc2200_ts_platform_info *platform_info = devdata->platform_info;

	disable_irq( platform_info->irq );
	printk("touch pressed\n");
	ts_check( devdata );
	
	return IRQ_HANDLED;
};

static void tsc2200_ts_timer(unsigned long data)
{
	struct tsc2200_ts_data *devdata = (void*) data;
        //struct tsc2200_ts_platform_info *platform_info = devdata->platform_info;

	ts_check( devdata );
}

int tsc2200_ts_probe(struct device *dev)
{
	struct tsc2200_ts_data *devdata = dev->platform_data;
        struct tsc2200_ts_platform_info *platform_info = devdata->platform_info;

	printk("tsc2200_ts_probe IRQ: %d\n", platform_info->irq);
	
	init_timer(&timer_pen);
	timer_pen.function = tsc2200_ts_timer;
	timer_pen.data = (unsigned long) devdata;

        idev = input_allocate_device();
        if (!idev)
                return -ENOMEM;

	
  //      idev->keybit[LONG(BTN_TOUCH)] = BIT(BTN_TOUCH);
	set_bit(EV_ABS, idev->evbit);
	set_bit(ABS_X, idev->absbit);
	set_bit(ABS_Y, idev->absbit);
	set_bit(ABS_PRESSURE, idev->absbit);
//	set_bit(BTN_TOUCH, idev->keybit);

	input_set_abs_params(idev, ABS_X, 0, 5000, 0, 0);
	input_set_abs_params(idev, ABS_Y, 0, 5000, 0, 0);
	input_set_abs_params(idev, ABS_PRESSURE, 0, 1, 0 ,0);

	idev->name = "tsc2200-ts";
	idev->phys = "touchscreen/tsc2200-ts";

	touch_pressed = 0;
	irq_disable = 0;
	
	request_irq(platform_info->irq, tsc2200_stylus, SA_ONESHOT, "tsc2200-ts", devdata);
	set_irq_type(platform_info->irq, IRQF_TRIGGER_FALLING);

	input_register_device(idev);


	return 0;
}

static int tsc2200_ts_remove(struct device *dev)
{
	struct tsc2200_ts_data *devdata = dev->platform_data;
        struct tsc2200_ts_platform_info *platform_info = devdata->platform_info;

	printk("tsc2200_ts: Removing...\n");
	del_timer_sync(&timer_pen);
	disable_irq(platform_info->irq);
	free_irq(platform_info->irq, NULL);

	input_unregister_device(idev);

	printk("tsc2200_ts: done!\n");
	return 0;
}

#ifdef CONFIG_PM
static int tsc2200_ts_resume(struct device *dev)
{
	return 0;
}
#else
#define h4000_ts_resume NULL
#endif

static struct platform_driver tsc2200_ts_driver = {
	.driver = {
		.name		= "tsc2200-ts",
		.probe		= tsc2200_ts_probe,
		.remove 	= tsc2200_ts_remove,
	#ifdef CONFIG_PM
		.suspend	= NULL,
		.resume		= tsc2200_ts_resume,
	#endif
	}
};

//struct platform_device z = { .name = "h4000_ts", };

static int __init tsc2200_ts_init(void)
{
	return platform_driver_register(&tsc2200_ts_driver);
}

static void __exit tsc2200_ts_exit(void)
{
	platform_driver_unregister(&tsc2200_ts_driver);
}

module_init(tsc2200_ts_init)
module_exit(tsc2200_ts_exit)

//EXPORT_SYMBOL(h4000_spi_putget);

MODULE_AUTHOR("Joshua Wise, Pawel Kolodziejski");
MODULE_DESCRIPTION("Touchscreen support for the iPAQ h4xxx");
MODULE_LICENSE("GPL");

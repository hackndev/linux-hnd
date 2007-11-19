/*
 * TI TSC2200 Common Code
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <asm/mach-types.h>
#include <asm/arch/hardware.h>
#include <asm/arch/pxa-regs.h>

#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/mfd/tsc2200.h>
#include <linux/platform_device.h>

unsigned short tsc2200_read(struct device *dev, unsigned short reg)
{
	struct tsc2200_data *devdata = dev_get_drvdata(dev);

	reg |= TSC2200_REG_READ;
	return devdata->platform->send(reg, 0);
}
EXPORT_SYMBOL(tsc2200_read);

void tsc2200_write(struct device *dev, unsigned short reg, unsigned short value)
{
	struct tsc2200_data *devdata = dev_get_drvdata(dev);

	devdata->platform->send(reg, value);
	return;
}
EXPORT_SYMBOL(tsc2200_write);

unsigned short tsc2200_dav(struct device *dev)
{
	return ( tsc2200_read(dev, TSC2200_CTRLREG_CONFIG) & TSC2200_CTRLREG_CONFIG_DAV ) ? 0 : 1;
}
EXPORT_SYMBOL(tsc2200_dav);

void tsc2200_lock( struct device *dev ) 
{
	unsigned long flags=0;
	struct tsc2200_data *devdata = dev_get_drvdata(dev);
	spin_lock_irqsave(&devdata->lock, flags);
}
EXPORT_SYMBOL(tsc2200_lock);

void tsc2200_unlock( struct device *dev ) 
{
	unsigned long flags=0;
	struct tsc2200_data *devdata = dev_get_drvdata(dev);
	spin_unlock_irqrestore(&devdata->lock, flags);
}
EXPORT_SYMBOL(tsc2200_unlock);

void tsc2200_stop(struct device *dev ) 
{
	printk("TSC2200_ADC: %d\n", tsc2200_read( dev, TSC2200_CTRLREG_ADC ) );
	tsc2200_write( dev, TSC2200_CTRLREG_ADC, 0);
	printk("TSC2200_ADC: %d\n", tsc2200_read( dev, TSC2200_CTRLREG_ADC ) );
}
EXPORT_SYMBOL(tsc2200_stop);

void tsc2200_reset( struct device *dev )
{
	unsigned long flags;
	struct tsc2200_data *devdata = dev_get_drvdata(dev);

	spin_lock_irqsave(&devdata->lock, flags);

	tsc2200_write(dev, TSC2200_CTRLREG_RESET, 0xBBFF);
	
	printk("%s: %X.\n", __FUNCTION__, tsc2200_read(dev, TSC2200_CTRLREG_ADC));
	msleep(100);
	printk("%s: %X.\n", __FUNCTION__, tsc2200_read(dev, TSC2200_CTRLREG_ADC));
	
	tsc2200_write(dev, TSC2200_CTRLREG_REF, 0x1F);
	tsc2200_write(dev, TSC2200_CTRLREG_CONFIG, 0xA);
	
	spin_unlock_irqrestore(&devdata->lock, flags);
}

static int tsc2200_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct tsc2200_data *devdata = platform_get_drvdata(pdev);
	if (devdata->platform->exit) {
		devdata->platform->exit();
	}
	return 0;
}

static int tsc2200_resume(struct platform_device *pdev)
{
	struct tsc2200_data *devdata = platform_get_drvdata(pdev);
	if (devdata->platform->init) {
		devdata->platform->init();
	}
//	tsc2200_reset( dev );
	return 0;
}

static void tsc2200_register_button_device(struct device *dev) 
{
	struct tsc2200_data *devdata = dev_get_drvdata(dev);
	struct tsc2200_platform_info *platform_info = devdata->platform;
	
        struct platform_device *sdev = kzalloc (sizeof (*sdev), GFP_KERNEL);       
        struct tsc2200_buttons_data *buttons_devdata = kzalloc (sizeof (*buttons_devdata), GFP_KERNEL);
	

	// Prepare some platform info for the tsc2200 buttons
	sdev->name = "tsc2200-keys";
	sdev->dev.platform_data = buttons_devdata;
	sdev->dev.parent = dev;
	
	buttons_devdata->tsc2200_dev = dev;
	buttons_devdata->platform_info = platform_info->buttons_info;	
	
	platform_device_register(sdev);

	devdata->buttons_dev = sdev;
}

static void tsc2200_unregister_button_device(struct device *dev) 
{
	struct tsc2200_data *devdata = dev_get_drvdata(dev);
	//struct tsc2200_platform_info *platform_info = devdata->platform;

	kfree (devdata->buttons_dev->dev.platform_data);
	kfree (devdata->buttons_dev);
}

static void tsc2200_register_adc_device(struct device *dev) 
{
	struct tsc2200_data *devdata = dev_get_drvdata(dev);
	struct tsc2200_platform_info *platform_info = devdata->platform;
	
        struct platform_device *sdev = kzalloc (sizeof (*sdev), GFP_KERNEL);       
        struct tsc2200_adc_data *adc_devdata = kzalloc (sizeof (*adc_devdata), GFP_KERNEL);
	

	// Prepare some platform info for the tsc2200 buttons
	sdev->name = "tsc2200-adc";
	sdev->dev.platform_data = adc_devdata;
	sdev->dev.parent = dev;
	
	adc_devdata->tsc2200_dev = dev;
	adc_devdata->platform_info = platform_info->touchscreen_info;	
	
	platform_device_register(sdev);

	devdata->ts_dev = sdev;
}

static void tsc2200_unregister_adc_device(struct device *dev)
{
	struct tsc2200_data *devdata = dev_get_drvdata(dev);

	kfree (devdata->ts_dev->dev.platform_data);
	kfree (devdata->ts_dev);
}

static int __init tsc2200_probe(struct platform_device *pdev)
{
	struct tsc2200_data *devdata;

	if (!(devdata = kcalloc(1, sizeof(struct tsc2200_data), GFP_KERNEL)))
		return -ENOMEM;
	
	platform_set_drvdata(pdev, devdata);
	spin_lock_init(&devdata->lock);
	devdata->platform = pdev->dev.platform_data;

	if (devdata->platform->init) {
		devdata->platform->init();
	}
	
	udelay(300);
//	tsc2200_reset( dev );
	//udelay(300);

	printk("%s: SPI: cr0 %08x cr1 %08x sr: %08x it: %08x to: %08x ps: %08x\n",
			__FUNCTION__,
			SSCR0_P2, SSCR1_P2, SSSR_P2, SSITR_P2, SSTO_P2, SSPSP_P2);

	// Are there keys on this device?
	if (devdata->platform->buttons_info) {
		tsc2200_register_button_device(&pdev->dev);
	}

	// Register adc class device
	tsc2200_register_adc_device(&pdev->dev);

	return 0;
}


static  int __exit tsc2200_remove(struct platform_device *pdev)
{
	struct tsc2200_data *devdata = platform_get_drvdata(pdev);

	// Are there keys on this device?
	if (devdata->platform->buttons_info) {
		tsc2200_unregister_button_device(&pdev->dev);
	}

	// Is there a touchscreen on this device?
	if (devdata->platform->touchscreen_info) {
		tsc2200_unregister_adc_device(&pdev->dev);
	}

	kfree(devdata);
	return 0;
}

static struct platform_driver tsc2200_driver = {
	.driver = {
		.name	= "tsc2200",
	},
	.probe		= tsc2200_probe,
	.remove 	= __exit_p(tsc2200_remove),
	.suspend 	= tsc2200_suspend,
	.resume 	= tsc2200_resume,
};

static int __init tsc2200_init(void)
{
	return platform_driver_register(&tsc2200_driver);
}

static void __exit tsc2200_exit(void)
{
	platform_driver_unregister(&tsc2200_driver);
}

#ifdef MODULE
module_init(tsc2200_init);
#else	/* start early for dependencies */
subsys_initcall(tsc2200_init);
#endif
module_exit(tsc2200_exit);

MODULE_LICENSE("GPL");

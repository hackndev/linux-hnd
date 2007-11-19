/*
 * TSC2200 SSP communication driver
 *
 * Copyright (C) 2006 Paul Sokolovsky
 * Copyright (C) 2005-2006 Pawel Kolodziejski
 * Copyright (C) 2003 Joshua Wise
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/adc.h>
#include <linux/mfd/tsc2200.h>

#include <asm/arch/hardware.h>
#include <asm/irq.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/ssp.h>

unsigned short pin_ctrl_cmd[] = {
	TSC2200_DATAREG_X,
	TSC2200_DATAREG_Y,
	TSC2200_DATAREG_Z1,
	TSC2200_DATAREG_Z2,
	TSC2200_DATAREG_BAT1,
	TSC2200_DATAREG_BAT2,
	TSC2200_DATAREG_AUX1,
	TSC2200_DATAREG_AUX2,
	TSC2200_DATAREG_TEMP1,
	TSC2200_DATAREG_TEMP2,
};

int tsc2200_sense(struct device *dev, struct adc_request *req)
{
	struct tsc2200_adc_data *params = dev->platform_data;
	int i;
//	unsigned char flags ;

	if (req->num_senses <= 0)
		return -EINVAL;

	down(&dev->sem);
	for (i = 0; i < req->num_senses; i++) {
	        int pin_id = req->senses[i].pin_id;
	        /* If this is custom pin, try to mux it in */
	        if (pin_id >= TSC2200_PIN_CUSTOM_START && params->mux_custom) {
	        	pin_id = params->mux_custom(pin_id);
	        } 

	        if (pin_id < 0) {
	        	req->senses[i].value = (unsigned)-1;
	        } else {
			int touched = tsc2200_read(dev->parent, TSC2200_CTRLREG_ADC) & TSC2200_CTRLREG_ADC_PSM_TSC2200;
			if (touched && pin_id < TSC2200_PIN_BAT1) {
			  tsc2200_write(dev->parent,
	                    TSC2200_CTRLREG_ADC,
	                    1 << TSC2200_CTRLREG_ADC_AD1 |
	                    TSC2200_CTRLREG_ADC_RES (TSC2200_CTRLREG_ADC_RES_12BIT) |
	                    TSC2200_CTRLREG_ADC_AVG (TSC2200_CTRLREG_ADC_16AVG) |
	                    TSC2200_CTRLREG_ADC_CL  (TSC2200_CTRLREG_ADC_CL_4MHZ_10BIT) |
	                    TSC2200_CTRLREG_ADC_PV  (TSC2200_CTRLREG_ADC_PV_1mS) );
  			  req->senses[i].value = tsc2200_read(dev->parent, pin_ctrl_cmd[pin_id]);
			}
			if (!touched && pin_id >= TSC2200_PIN_BAT1) {
			  tsc2200_write(dev->parent,
	                    TSC2200_CTRLREG_ADC,
	                    TSC2200_CTRLREG_ADC_AD  (TSC2200_CTRLREG_ADC_AD3 | TSC2200_CTRLREG_ADC_AD1 | TSC2200_CTRLREG_ADC_AD0 ) | 
	                    TSC2200_CTRLREG_ADC_RES (TSC2200_CTRLREG_ADC_RES_12BIT) |
	                    TSC2200_CTRLREG_ADC_AVG (TSC2200_CTRLREG_ADC_16AVG) |
	                    TSC2200_CTRLREG_ADC_CL  (TSC2200_CTRLREG_ADC_CL_4MHZ_10BIT) |
	                    TSC2200_CTRLREG_ADC_PV  (TSC2200_CTRLREG_ADC_PV_1mS) );
  			  req->senses[i].value = tsc2200_read(dev->parent, pin_ctrl_cmd[pin_id]);
			}
		}
	}
	/* Put chip to sleep mode*/
	//tsc2200_stop(dev->parent);
	up(&dev->sem);
	return 0;
}
EXPORT_SYMBOL(tsc2200_sense);

static struct adc_classdev acdevs[] = {
	{
		.name = "x",
		.pin_id = TSC2200_PIN_XPOS,
		.units = "coordinate",
	},
	{
		.name = "y",
		.pin_id = TSC2200_PIN_YPOS,
		.units = "coordinate",
	},
	{
		.name = "z1",
		.pin_id = TSC2200_PIN_Z1POS,
		.units = "coordinate",
	},
	{
		.name = "z2",
		.pin_id = TSC2200_PIN_Z2POS,
		.units = "coordinate",
	},
	{
		.name = "temp1",
		.pin_id = TSC2200_PIN_TEMP1,
	},
	{
		.name = "temp2",
		.pin_id = TSC2200_PIN_TEMP2,
	},
	{
		.name = "vbat1",
		.pin_id = TSC2200_PIN_BAT1,
	},
	{
		.name = "vbat2",
		.pin_id = TSC2200_PIN_BAT2,
	},
	{
		.name = "vaux1",
		.pin_id = TSC2200_PIN_AUX1,
	},
	{
		.name = "vaux2",
		.pin_id = TSC2200_PIN_AUX2,
	},
};

static int __devinit tsc2200_ssp_probe(struct platform_device *dev)
{
	struct tsc2200_adc_data *params = dev->dev.platform_data;
	int ret = 0;
	size_t i, j;

	platform_set_drvdata(dev, params);

	printk(KERN_INFO "tsc2200_adc_ssp: Probing\n");

	for (i = 0; i < ARRAY_SIZE(acdevs); i++) {
		acdevs[i].sense = tsc2200_sense;
		ret = adc_classdev_register(&dev->dev, &acdevs[i]);
		if (ret) {
			printk("tsc2200_adc_ssp: failed to register "
			       "class device %s", acdevs[i].name);
			goto adc_cdev_register_failed;
		}
	}

	for (j = 0; j < params->num_custom_adcs; j++) {
		params->custom_adcs[j].sense = tsc2200_sense;
		ret = adc_classdev_register(&dev->dev, &params->custom_adcs[j]);
		if (ret) {
			printk("tsc2200_adc_ssp: failed to register "
			       "class device %s", params->custom_adcs[j].name);
			goto plat_adc_cdev_register_failed;
		}
	}

	goto success;

plat_adc_cdev_register_failed:
	while (--j >= 0)
		adc_classdev_unregister(&params->custom_adcs[j]);
adc_cdev_register_failed:
	while (--i >= 0)
		adc_classdev_unregister(&acdevs[i]);
success:
	printk(KERN_INFO "tsc2200_adc_ssp: ok\n");
	return ret;
}

static int __devexit tsc2200_ssp_remove(struct platform_device *dev)
{
	struct tsc2200_adc_data *params = dev->dev.platform_data;
	size_t i;
	for (i = 0; i < params->num_custom_adcs; i++)
		adc_classdev_unregister(&params->custom_adcs[i]);
	for (i = 0; i < ARRAY_SIZE(acdevs); i++)
		adc_classdev_unregister(&acdevs[i]);
	return 0;
}

#ifdef CONFIG_PM
static int tsc2200_ssp_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

static int tsc2200_ssp_resume(struct platform_device *dev)
{
//	struct tsc2200_adc_data *params = dev->dev.platform_data;
	return 0;
}
#endif

static struct platform_driver tsc2200_ssp_driver = {
	.driver 	= {
		.name	= "tsc2200-adc",
	},
	.probe          = tsc2200_ssp_probe,
	.remove         = __devexit_p(tsc2200_ssp_remove),
#ifdef CONFIG_PM
	.suspend        = tsc2200_ssp_suspend,
	.resume         = tsc2200_ssp_resume,
#endif
};

static int __init tsc2200_adc_init(void)
{
	return platform_driver_register(&tsc2200_ssp_driver);
}

static void __exit tsc2200_adc_exit(void)
{
	platform_driver_unregister(&tsc2200_ssp_driver);
}

#ifdef MODULE
module_init(tsc2200_adc_init)
#else   /* start early for dependencies */
subsys_initcall(tsc2200_adc_init);
#endif

module_exit(tsc2200_adc_exit)

MODULE_AUTHOR("Pawel Kolodziejski, Paul Sokolovsky");
MODULE_DESCRIPTION("TSC2200 SSP channel driver");
MODULE_LICENSE("GPL");

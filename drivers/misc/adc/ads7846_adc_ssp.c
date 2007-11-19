/*
 * ADS7846 SSP communication driver
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
#include <linux/ads7846.h>

#include <asm/arch/hardware.h>
#include <asm/irq.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/ssp.h>

#if defined (CONFIG_PXA27x) || defined (CONFIG_PXA26x)
#define CKEN_SSP(x) (((x) == 1) ? CKEN23_SSP1 : ((x) == 2) ? CKEN3_SSP2 : ((x) == 3) ? CKEN4_SSP3 : 0)
#define SSP_DIVISOR(freq) (13000000/freq - 1)
#else
#define CKEN_SSP(x) (((x) == 1) ? CKEN3_SSP : ((x) == 2) ? CKEN9_NSSP : 0)
#define SSP_DIVISOR(freq) ((3686400/2)/freq - 1)
#endif

static int port;

u32 ads7846_ssp_putget(u32 data)
{
	u32 ret;
	int timeout;

	timeout = 100000;
	while (!(SSSR_P(port) & SSSR_TNF) && --timeout);
	if (timeout == 0) {
	    printk("%s: warning: timeout while waiting for SSSR_TNF\n", __FUNCTION__);
	};

	SSDR_P(port) = data;

	udelay(1);

	timeout = 100000;
	while (!(SSSR_P(port) & SSSR_RNE) && --timeout);
	if (timeout == 0) {
	    printk("%s: warning: timeout while waiting for SSSR_RNE\n", __FUNCTION__);
	};

	ret = SSDR_P(port);
	return ret;
}
EXPORT_SYMBOL(ads7846_ssp_putget);

#define CTRL_YPOS   0x10
#define CTRL_Z1POS  0x30
#define CTRL_Z2POS  0x40
#define CTRL_XPOS   0x50
#define CTRL_TEMP0  0x04
#define CTRL_TEMP1  0x74
#define CTRL_VBAT   0x24
#define CTRL_AUX_N  0x64

#define CTRL_START  0x80

unsigned char pin_ctrl_cmd[] = {
	CTRL_XPOS,
	CTRL_YPOS,
	CTRL_Z1POS,
	CTRL_Z2POS,
	CTRL_TEMP0,
	CTRL_TEMP1,
	CTRL_VBAT,
	CTRL_AUX_N,
};

int ads7846_sense(struct device *dev, struct adc_request *req)
{
	struct ads7846_ssp_platform_data *params = dev->platform_data;
	int i;

	if (req->num_senses <= 0)
		return -EINVAL;

	down(&dev->sem);
	for (i = 0; i < req->num_senses; i++) {
	        int pin_id = req->senses[i].pin_id;
	        /* If this is custom pin, try to mux it in */
	        if (pin_id >= AD7846_PIN_CUSTOM_START && params->mux_custom) {
	        	pin_id = params->mux_custom(pin_id);
	        } 

	        if (pin_id < 0) {
	        	req->senses[i].value = (unsigned)-1;
	        } else {
			req->senses[i].value = ads7846_ssp_putget(pin_ctrl_cmd[pin_id] | CTRL_START | params->pd_bits);
		}
	}
	/* Reset PD0&1, put chip to sleep */
	ads7846_ssp_putget(CTRL_START);
	up(&dev->sem);
	return 0;
}
EXPORT_SYMBOL(ads7846_sense);

static void ads7846_ssp_init(int freq)
{
	/* now to set up GPIOs... */
#if 0
	GPDR(GPIO23_SCLK) |=  GPIO_bit(GPIO23_SCLK);
	GPDR(GPIO24_SFRM) |=  GPIO_bit(GPIO24_SFRM);
	GPDR(GPIO25_STXD) |=  GPIO_bit(GPIO25_STXD);
	GPDR(GPIO26_SRXD) &= ~GPIO_bit(GPIO26_SRXD);
	pxa_gpio_mode(GPIO23_SCLK_MD);
	pxa_gpio_mode(GPIO24_SFRM_MD);
	pxa_gpio_mode(GPIO25_STXD_MD);
	pxa_gpio_mode(GPIO26_SRXD_MD);
#endif

	SSCR0_P(port) = 0;
	SSCR0_P(port) |= SSCR0_DataSize(12); /* 12 bits */
	SSCR0_P(port) |= SSCR0_National;
	SSCR0_P(port) |= SSP_DIVISOR(freq) << 8;

	SSCR1_P(port) = 0;
	SSPSP_P(port) = 0;

	SSCR0_P(port) |= SSCR0_SSE;
}

static struct adc_classdev acdevs[] = {
	{
		.name = "x",
		.pin_id = AD7846_PIN_XPOS,
		.units = "coordinate",
	},
	{
		.name = "y",
		.pin_id = AD7846_PIN_YPOS,
		.units = "coordinate",
	},
	{
		.name = "z1",
		.pin_id = AD7846_PIN_Z1POS,
		.units = "coordinate",
	},
	{
		.name = "z2",
		.pin_id = AD7846_PIN_Z2POS,
		.units = "coordinate",
	},
	{
		.name = "temp0",
		.pin_id = AD7846_PIN_TEMP0,
	},
	{
		.name = "temp1",
		.pin_id = AD7846_PIN_TEMP1,
	},
	{
		.name = "vbat",
		.pin_id = AD7846_PIN_VBAT,
	},
	{
		.name = "vaux",
		.pin_id = AD7846_PIN_VAUX,
	},
};

static int __devinit ads7846_ssp_probe(struct platform_device *dev)
{
	struct ads7846_ssp_platform_data *params = dev->dev.platform_data;
	int ret = 0;
	size_t i, j;

	platform_set_drvdata(dev, params);

	port = params->port;
	printk(KERN_INFO "ads7846_ssp: Using SSP%d\n", port);
	if (port < 1 || port > 3) {
		printk(KERN_ERR "ads7846_ssp: Invalid port\n");
		return -EINVAL;
	}

	pxa_set_cken(CKEN_SSP(port), 1);
	/* ~1.8Mhz - max frequency for PXA25x */
	ads7846_ssp_init(params->freq ? params->freq : 1800000);

	for (i = 0; i < ARRAY_SIZE(acdevs); i++) {
		acdevs[i].sense = ads7846_sense;
		ret = adc_classdev_register(&dev->dev, &acdevs[i]);
		if (ret) {
			printk("ads7846_adc_ssp: failed to register "
			       "class device %s", acdevs[i].name);
			goto adc_cdev_register_failed;
		}
	}

	for (j = 0; j < params->num_custom_adcs; j++) {
		params->custom_adcs[j].sense = ads7846_sense;
		ret = adc_classdev_register(&dev->dev, &params->custom_adcs[j]);
		if (ret) {
			printk("ads7846_adc_ssp: failed to register "
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
	return ret;
}

static int __devexit ads7846_ssp_remove(struct platform_device *dev)
{
	struct ads7846_ssp_platform_data *params = dev->dev.platform_data;
	size_t i;
	for (i = 0; i < params->num_custom_adcs; i++)
		adc_classdev_unregister(&params->custom_adcs[i]);
	for (i = 0; i < ARRAY_SIZE(acdevs); i++)
		adc_classdev_unregister(&acdevs[i]);
	SSCR0_P(port) &= ~SSCR0_SSE;
	pxa_set_cken(CKEN_SSP(port), 0);
	return 0;
}

#ifdef CONFIG_PM
static int ads7846_ssp_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

static int ads7846_ssp_resume(struct platform_device *dev)
{
	struct ads7846_ssp_platform_data *params = dev->dev.platform_data;
	ads7846_ssp_init(params->freq ? params->freq : 1800000);
	return 0;
}
#endif

static struct platform_driver ads7846_ssp_driver = {
	.driver 	= {
		.name	= "ads7846-ssp",
	},
	.probe          = ads7846_ssp_probe,
	.remove         = __devexit_p(ads7846_ssp_remove),
#ifdef CONFIG_PM
	.suspend        = ads7846_ssp_suspend,
	.resume         = ads7846_ssp_resume,
#endif
};

static int __init ads7846_adc_init(void)
{
	return platform_driver_register(&ads7846_ssp_driver);
}

static void __exit ads7846_adc_exit(void)
{
	platform_driver_unregister(&ads7846_ssp_driver);
}

#ifdef MODULE
module_init(ads7846_adc_init)
#else   /* start early for dependencies */
subsys_initcall(ads7846_adc_init);
#endif

module_exit(ads7846_adc_exit)

MODULE_AUTHOR("Pawel Kolodziejski, Paul Sokolovsky");
MODULE_DESCRIPTION("ADS7846 SSP channel driver");
MODULE_LICENSE("GPL");

/* Support for the ad7877 Analog to Digital Converter chip
 *
 * (c) Copyright 2006 Kevin O'Connor <kevin@koconnor.net>
 *
 * This file may be distributed under the terms of the GNU GPL license.
 */

#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/completion.h>
#include <linux/ad7877.h>

#include <asm/arch/ssp.h>
#include <asm/arch/hardware.h>
#include <asm/arch/pxa-regs.h>

// Some ad7877 register ids.
enum {
	REG_CR1     = 0x01,
	REG_CR2     = 0x02,
	REG_SEQREG0 = 0x0C,
	REG_DAC     = 0x0E,
	REG_SENSORS = 0x10,

	CR1_OFF_READADDR = 2,
	CR1_SLAVEMODE = 0x02,
};

struct ad7877 {
	struct ssp_dev ssp;
	struct ssp_state suspend_state;
	struct completion comp;
};

// Build a command that writes to an ad7877 register
static inline int WRITE_REG(int reg, int val)
{
	return (reg << 12) | val;
}

// Send a command and drain the receive buffer.
static u32
ssp_putget(struct ad7877 *ad, u32 data)
{
	u32 ret;
	ssp_write_word(&ad->ssp, data);
	ssp_read_word(&ad->ssp, &ret);
	return ret;
}

// Instruct the ad7877 to sense new values.
static int
sense(struct device *dev, struct adc_request *req)
{
	struct ad7877 *ad = platform_get_drvdata(to_platform_device(dev));
	int bitmap = 0;
	int i;
	int pin;

	down(&dev->sem);

	for (i=0; i < req->num_senses; i++)
		bitmap |= 1 << (AD7877_NR_SENSE - req->senses[i].pin_id);

	// Set pins to sense
	ssp_putget(ad, WRITE_REG(REG_SEQREG0, bitmap));
	// Start sense
	ssp_putget(ad, WRITE_REG(REG_CR1, CR1_SLAVEMODE));

	// Wait for IRQ, it will tell when sampling complete
	wait_for_completion(&ad->comp);

	pin = (REG_SENSORS + req->senses[0].pin_id) << CR1_OFF_READADDR;
	ssp_putget(ad, WRITE_REG(REG_CR1, pin));
	for (i = 0; i < req->num_senses - 1; i++) {
		pin = (REG_SENSORS + req->senses[i+1].pin_id) <<
		      CR1_OFF_READADDR;
		req->senses[i].value = ssp_putget(ad, WRITE_REG(REG_CR1, pin));
	}
	req->senses[i].value = ssp_putget(ad, 0);

	up(&dev->sem);

	return 0;
}

// Handle a data available interrupt.
static irqreturn_t davirq(int irq, void *handle)
{
	struct ad7877 *ad = handle;
	complete(&ad->comp);
	return IRQ_HANDLED;
}

// Setup the ad7877 with sensing values.
static void
initChip(struct ad7877 *ad)
{
	// TMR=0, REF=0, POL=1, FCD=1, PM=2, ACQ=0, AVG=2
	ssp_putget(ad, WRITE_REG(REG_CR2, 0x898));
	// Power down DAC
	ssp_putget(ad, WRITE_REG(REG_DAC, 0x8));
}

// Shutdown the ad7877.
static void
downChip(struct ad7877 *ad)
{
	ssp_putget(ad, WRITE_REG(REG_CR1, 0x0));
	ssp_putget(ad, WRITE_REG(REG_CR2, 0x0));
}

static struct adc_classdev acdevs[] = {
	{
		.name = "x",
		.pin_id = AD7877_SEQ_XPOS,
	},
	{
		.name = "y",
		.pin_id = AD7877_SEQ_YPOS,
	},
	{
		.name = "z1",
		.pin_id = AD7877_SEQ_Z1,
	},
	{
		.name = "z2",
		.pin_id = AD7877_SEQ_Z2,
	},
	{
		.name = "bat1",
		.pin_id = AD7877_SEQ_BAT1,
	},
	{
		.name = "bat2",
		.pin_id = AD7877_SEQ_BAT2,
	},
	{
		.name = "temp1",
		.pin_id = AD7877_SEQ_TEMP1,
	},
	{
		.name = "temp2",
		.pin_id = AD7877_SEQ_TEMP2,
	},
	{
		.name = "aux1",
		.pin_id = AD7877_SEQ_AUX1,
	},
	{
		.name = "aux2",
		.pin_id = AD7877_SEQ_AUX2,
	},
	{
		.name = "aux3",
		.pin_id = AD7877_SEQ_AUX3,
	},
};

static int ad_probe(struct platform_device *pdev)
{
	struct ad7877_platform_data *pdata = pdev->dev.platform_data;
	struct ad7877 *ad;
	int ret;
	int i;

	// Initialize ad data structure.
	ad = kzalloc(sizeof(*ad), GFP_KERNEL);
	if (!ad)
		return -ENOMEM;

	init_completion(&ad->comp);

	ret = request_irq(pdata->dav_irq, davirq
			  , IRQF_DISABLED | IRQF_TRIGGER_FALLING
			  , "ad7877-dav", ad);
	if (ret) {
		kfree(ad);
		return ret;
	}

	ret = ssp_init(&ad->ssp, 1, 0);
	if (ret) {
		printk(KERN_ERR "Unable to register SSP handler!\n");
		free_irq(pdata->dav_irq, ad);
		kfree(ad);
		return ret;
	}
	platform_set_drvdata(pdev, ad);

	ssp_disable(&ad->ssp);
	ssp_config(&ad->ssp, SSCR0_DataSize(16), 0, 0, SSCR0_SerClkDiv(6));
	ssp_enable(&ad->ssp);
	initChip(ad);

	for (i = 0; i < ARRAY_SIZE(acdevs); i++) {
		acdevs[i].sense = sense;

		ret = adc_classdev_register(&pdev->dev, &acdevs[i]);
		if (ret) {
			printk("ad7877: failed to register adc class "
			       "device %s\n", acdevs[i].name);
			goto adc_cdev_register_failed;
		}
	}

	return 0;

adc_cdev_register_failed:
	while (--i >= 0)
		adc_classdev_unregister(&acdevs[i]);

	return ret;
}

static int ad_remove(struct platform_device *pdev)
{
	struct ad7877_platform_data *pdata = pdev->dev.platform_data;
	struct ad7877 *ad = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < ARRAY_SIZE(acdevs); i++)
		adc_classdev_unregister(&acdevs[i]);
	free_irq(pdata->dav_irq, ad);
	downChip(ad);
	ssp_exit(&ad->ssp);
	kfree(ad);

	return 0;
}

#ifdef CONFIG_PM
static int ad_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct ad7877 *ad = platform_get_drvdata(pdev);
	downChip(ad);
	ssp_save_state(&ad->ssp, &ad->suspend_state);
	return 0;
}

static int ad_resume(struct platform_device *pdev)
{
	struct ad7877 *ad = platform_get_drvdata(pdev);
	ssp_restore_state(&ad->ssp, &ad->suspend_state);
	initChip(ad);
	return 0;
}
#endif

static struct platform_driver ad_driver = {
	.driver = {
		.name           = "ad7877",
	},
	.probe          = ad_probe,
	.remove         = ad_remove,
#ifdef CONFIG_PM
	.suspend        = ad_suspend,
	.resume         = ad_resume,
#endif
};

static int __init ad_init(void)
{
	return platform_driver_register(&ad_driver);
}

static void __exit ad_exit(void)
{
	platform_driver_unregister(&ad_driver);
}

module_init(ad_init)
module_exit(ad_exit)

MODULE_LICENSE("GPL");

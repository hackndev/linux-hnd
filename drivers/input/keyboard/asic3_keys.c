/*
 * Generic buttons driver for ASIC3 SoC.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2003 Joshua Wise
 * Copyright (C) 2005 Pawel Kolodziejski
 * Copyright (C) 2006 Paul Sokolovsky
 *
 */

#include <linux/input.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/mfd/asic3_base.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/arch/irqs.h>
#include <asm/hardware.h>
#include <asm/hardware/ipaq-asic3.h>
#include <asm/hardware/asic3_keys.h>

static irqreturn_t asic3_keys_asic_handle(int irq, void *data)
{
	struct asic3_keys_platform_data *pdata = data;
	int i, base_irq;

	base_irq = asic3_irq_base(pdata->asic3_dev);
	for (i = 0; i < pdata->nbuttons; i++) {
		struct asic3_keys_button *b = &pdata->buttons[i];
		if ((base_irq + b->gpio) == irq) {
			int state = !!asic3_gpio_get_value(pdata->asic3_dev, b->gpio);

			if (pdata->buttons[i].type == EV_SW)
				input_report_switch(pdata->input, pdata->buttons[i].keycode, state ^ b->active_low);
			else
				input_report_key(pdata->input, b->keycode, state ^ b->active_low);
			input_sync(pdata->input);
		}
	}

	return IRQ_HANDLED;
}

static int __devinit asic3_keys_probe(struct platform_device *pdev)
{
	struct asic3_keys_platform_data *pdata = pdev->dev.platform_data;
	int i, base_irq;
	int j, ret;

	pdata->input = input_allocate_device();

	base_irq = asic3_irq_base(pdata->asic3_dev);

	for (i = 0; i < pdata->nbuttons; i++) {
		struct asic3_keys_button *b = &pdata->buttons[i];
		set_bit(b->keycode, pdata->input->keybit);
                ret=request_irq(base_irq + b->gpio, asic3_keys_asic_handle, SA_SAMPLE_RANDOM, b->desc, pdata);
                if (ret)
                {
                 printk(KERN_NOTICE "Failed to allocate asic3_keys irq=%d.\n",b->gpio);
 
                 for(j=0; j<i ; j++)
                  free_irq(base_irq + pdata->buttons[i].gpio, NULL);
 
                 input_unregister_device (pdata->input);
 
                 return -ENODEV;
                }
                
		set_irq_type(base_irq + b->gpio, IRQT_BOTHEDGE);
		if (pdata->buttons[i].type == EV_SW) {
			pdata->input->evbit[0] |= BIT(EV_SW);
			set_bit(b->keycode, pdata->input->swbit);
		} else {
			pdata->input->evbit[0] |= BIT(EV_KEY);
			set_bit(b->keycode, pdata->input->keybit);
		}
	}

	pdata->input->name = pdev->name;
	input_register_device(pdata->input);

	return 0;
}

static int __devexit asic3_keys_remove(struct platform_device *pdev)
{
	struct asic3_keys_platform_data *pdata = pdev->dev.platform_data;
	int i, base_irq;

	base_irq = asic3_irq_base(pdata->asic3_dev);
	for (i = 0; i < pdata->nbuttons; i++) {
		free_irq(base_irq + pdata->buttons[i].gpio, NULL);
	}

	input_unregister_device(pdata->input);

	return 0;
}


static struct platform_driver asic3_keys_driver = {
	.probe          = asic3_keys_probe,
        .remove         = __devexit_p(asic3_keys_remove),
	.driver		= {
	    .name       = "asic3-keys",
	},
};

static int __init asic3_keys_init(void)
{
	return platform_driver_register(&asic3_keys_driver);
}

static void __exit asic3_keys_exit(void)
{
	platform_driver_unregister(&asic3_keys_driver);
}

module_init(asic3_keys_init);
module_exit(asic3_keys_exit);

MODULE_AUTHOR("Joshua Wise, Pawel Kolodziejski, Paul Sokolovsky");
MODULE_DESCRIPTION("Buttons driver for HTC ASIC3 SoC");
MODULE_LICENSE("GPL");

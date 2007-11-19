#include <linux/module.h>
#include <linux/errno.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/irq.h>

#include <asm/mach-types.h>

#include <asm/io.h>
#include <asm/irq.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/palmld-gpio.h>

static int palmld_ide_probe(struct platform_device *dev)
{
	hw_regs_t hw;
	int *hd = NULL;

	if (!machine_is_xscale_palmld())
		return -ENODEV;

	if (!GET_PALMLD_GPIO(IDE_PWEN)) {
		printk(KERN_INFO "palmld_ide_probe: enabling HDD power\n");
		SET_PALMLD_GPIO(IDE_PWEN, 1);
		/* fixme: should wait for a ready condition instead
		 * of just delaying */
		msleep(300);
	}

	ide_std_init_ports(&hw, PALMLD_IDE_VIRT + 0x10, PALMLD_IDE_VIRT + 0xe);
	hw.dev = &dev->dev;
	hw.irq = IRQ_GPIO_PALMLD_IDE_IRQ;
	set_irq_type(hw.irq, IRQT_RISING);

	hd = kmalloc(sizeof(int), GFP_KERNEL);
	*hd = ide_register_hw(&hw, NULL);

	if(*hd == -1) {
		printk(KERN_ERR "palmld_ide_probe: "
				"unable to register ide hw\n");
		kfree(hd);
		return -ENODEV;
	}

	platform_set_drvdata(dev, hd);

	return 0;
}

static int palmld_ide_remove(struct platform_device *dev)
{
	int *hd = platform_get_drvdata(dev);

	if (hd) {
		ide_unregister(*hd);
	}
	kfree(hd);
	/* conserve battery where we can */
	SET_PALMLD_GPIO(IDE_PWEN, 0);

	return 0;
}

#ifdef CONFIG_PM
static int palmld_ide_suspend(struct platform_device *dev, pm_message_t state)
{
	printk("palmld_ide_suspend: going zzz\n");
	return 0;
}

static int palmld_ide_resume(struct platform_device *dev)
{
	printk("palmld_ide_resume: resuming\n");
	return 0;
}
#else
#define palmld_ide_suspend NULL
#define palmld_ide_resume NULL
#endif


static struct platform_driver palmld_ide_driver = {
	.driver		= {
		.name	= "palmld-ide",
		.owner	= THIS_MODULE,
	},
	.probe	= palmld_ide_probe,
	.remove = palmld_ide_remove,
	.suspend= palmld_ide_suspend,
	.resume = palmld_ide_resume,
};

static int __init palmld_ide_init(void)
{
	return platform_driver_register(&palmld_ide_driver);
}

static void __exit palmld_ide_exit(void)
{
	platform_driver_unregister(&palmld_ide_driver);
}

module_init(palmld_ide_init);
module_exit(palmld_ide_exit);

MODULE_AUTHOR("Alex Osborne <bobofdoom@gmail.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Palm Lifedrive IDE driver");

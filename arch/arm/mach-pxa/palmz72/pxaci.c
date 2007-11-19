#include <linux/module.h>
#include <linux/platform_device.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/hardware.h>

static int pxaci_enabled = 0;
static DEFINE_MUTEX(enable_mutex);

static void show_ci_regs(void)
{
	printk(KERN_INFO "CI regs:\nCICR0=%08x CICR1=%08x\nCICR2=%08x CICR3=%08x\nCICR4=%08x\n", CICR0, CICR1, CICR2, CICR3, CICR4);
	printk(KERN_INFO "CISR=%08x\n", CISR);
}

static ssize_t pxaci_enable_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	printk(KERN_INFO "PXACIF: read\n");
	show_ci_regs();
	return sprintf(buf, "%u\n", pxaci_enabled);
}
/*
CICR0 = 0x800003f7 DMA_EN|~PAR_EN|~SL_CAP_EN|~ENB|~DIS|SIM=000 (master-parallel)|TOM|RDAVM|FEM|EOLM|PERRM|QDM|CDM|~SOFM|~EOFM
CICR1 = 0x009f8412 ~TBIT|RGBT_CONV=00 (No) |
CICR2 = 0
CICR3 = 0x000000ef
CICR4 = 0x00880001

*/


static ssize_t pxaci_enable_store(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t count)
{
	int state;

	printk(KERN_INFO "PXACIF: write\n");
	show_ci_regs();
	if (sscanf(buf, "%u", &state) != 1)
		return -EINVAL;

	if ((state != 1) && (state != 0))
		return -EINVAL;

	mutex_lock(&enable_mutex);
	if (state != pxaci_enabled) {
		pxaci_enabled = state;
		pxa_set_cken(CKEN24_CAMERA, state);
		if(pxaci_enabled) {
			CICR0 = 0x3ff;
			CICR1 = 0x009f8412;
			CICR2 = 0x0;
			CICR3 = 0xef;
			CICR4 = 0x00880001;
			CICR0 = CICR0_ENB|0x3ff;
		} else {
			CICR0 = 0x3ff;
			CICR1 = 0x009f8412;
			CICR2 = 0x0;
			CICR3 = 0xef;
			CICR4 = 0x00880001;
		}
	}
	mutex_unlock(&enable_mutex);

	return strnlen(buf, count);
}

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR, pxaci_enable_show, pxaci_enable_store);


#ifdef CONFIG_PM
static int pxaci_suspend(struct platform_device *pdev, pm_message_t state)
{
	pxa_set_cken(CKEN24_CAMERA, 0);
	return 0;
}

static int pxaci_resume(struct platform_device *pdev)
{
	pxa_set_cken(CKEN24_CAMERA, pxaci_enabled);
	return 0;
}
#else
#define pxaci_suspend NULL
#define pxaci_resume	NULL
#endif


static int pxaci_probe(struct platform_device *pdev)
{
	int ret;
	pxa_set_cken(CKEN24_CAMERA, 0);
	show_ci_regs();
	ret = device_create_file(&pdev->dev, &dev_attr_enable);
	printk(KERN_INFO "Initialized device PXACI\n");
	return 0;
}

static int pxaci_remove(struct platform_device *pdev)
{
	pxa_set_cken(CKEN24_CAMERA, 0);
	printk(KERN_INFO "Denitialized device PXACI\n");
	return 0;
}

static struct platform_driver pxaci_driver = {
	.probe		= pxaci_probe,
	.remove		= pxaci_remove,
	.suspend	= pxaci_suspend,
	.resume		= pxaci_resume,
	.driver		= {
		.name	= "pxacif",
	},
};

static int __init pxacif_init(void)
{
	int pdreg = platform_driver_register(&pxaci_driver);

	return pdreg;
}

static void __exit pxacif_exit(void)
{
	platform_driver_unregister(&pxaci_driver);
}

module_init(pxacif_init);
module_exit(pxacif_exit);

MODULE_AUTHOR("Sergey Lapin <slapin@hackndev.com>");
MODULE_DESCRIPTION("XScale Camera Interface Experiments Driver");
MODULE_LICENSE("GPL");

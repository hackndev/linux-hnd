/*
 * Based on arch/arm/mach-pxa/mainstone.h
 *
*/

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/irq.h>

#include <asm/arch/mmc.h>
#include <asm/hardware.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach-types.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/asus730-gpio.h>

static int a730_mci_init(struct device *dev, irqreturn_t (*a730_detect_int)(int, void *), void *data)
{
    int err;

    pxa_gpio_mode(GPIO32_MMCCLK_MD);
    pxa_gpio_mode(GPIO92_MMCDAT0_MD);
    pxa_gpio_mode(GPIO109_MMCDAT1_MD);
    pxa_gpio_mode(GPIO110_MMCDAT2_MD);
    pxa_gpio_mode(GPIO111_MMCDAT3_MD);
    pxa_gpio_mode(GPIO112_MMCCMD_MD);

    set_irq_type(A730_IRQ(SD_DETECT_N), IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING);
    err = request_irq(A730_IRQ(SD_DETECT_N), a730_detect_int, IRQF_DISABLED, "SD/MMC CD", data);
    if (err) {
	printk(KERN_ERR "a730_mci_init: SD/MMC: can't request SD/MMC card detect IRQ\n");
	return -1;
    }
    
    return 0;
}

static void a730_mci_set_power(struct device *dev, unsigned int vdd)
{
    struct pxamci_platform_data* p_d = dev->platform_data;

    if (( 1 << vdd) & p_d->ocr_mask)
    {
	SET_A730_GPIO(SD_PWR, 0);
    }
    else
    {
	SET_A730_GPIO(SD_PWR, 1);
    }
}

static int a730_mci_get_ro(struct device *dev)
{
    return GET_A730_GPIO(SD_RO);
}

static void a730_mci_exit(struct device *dev, void *data)
{
    free_irq(A730_IRQ(SD_DETECT_N), data);
}

static struct pxamci_platform_data a730_mci_platform_data = {
    .ocr_mask       = MMC_VDD_32_33 | MMC_VDD_33_34,
    .init           = a730_mci_init,
    .setpower       = a730_mci_set_power,
    .get_ro		= a730_mci_get_ro,
    .exit           = a730_mci_exit,
};

static int __init a730_mci_mod_init(void)
{
    if (!machine_is_a730()) return -ENODEV;

    pxa_set_mci_info(&a730_mci_platform_data);
    return 0;
}
					
static void __exit a730_mci_mod_exit(void)
{
}

module_init(a730_mci_mod_init);
module_exit(a730_mci_mod_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Serge Nikolaenko <mypal_hh@utl.ru>");
MODULE_DESCRIPTION("MCI platform support for Asus MyPal A730(W)");

/* platform specific code to drive the TC6387XB chip on the e330. */

//#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/major.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/delay.h>

#include <asm/mach-types.h>
#include <asm/arch/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/irq.h>

#include <asm/arch/eseries-irq.h>
#include <asm/arch/eseries-gpio.h>

#include <linux/mfd/tc6387xb.h>

static struct resource e330_tc6387xb_resources[] = {
	[0] = {
		.start  = PXA_CS4_PHYS,
		.end    = PXA_CS4_PHYS + 0x1fffff,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = IRQ_GPIO(5),
		.end    = IRQ_GPIO(5),
		.flags  = IORESOURCE_IRQ,
	},

};

//FIXME - who should really be setting up the clock? bootloader or kernel ?
static void e330_tc6387xb_hwinit(void) {
	GPCR(45) = GPIO_bit(45); // #SUSPEND low
	GPCR(19) = GPIO_bit(19); // #PCLR low (reset)
	// 33MHz is supplied from an Xtal on the e330 (I think)
	pxa_gpio_mode(GPIO12_32KHz_MD);
	mdelay(10);
	GPSR(45) = GPIO_bit(45); // #SUSPEND high
	mdelay(10);
	GPSR(19) = GPIO_bit(19); // #PCLR high
	mdelay(10);
}

static void e330_tc6387xb_suspend(void) {
	GPCR(45) = GPIO_bit(45); // #SUSPEND low
	mdelay(10);
//	GPCR(19) = GPIO_bit(19); // #PCLR low
}

static void e330_tc6387xb_resume(void) {
	GPSR(45) = GPIO_bit(45); // #SUSPEND high
        mdelay(10);
}

static struct tc6387xb_platform_data e330_tc6387xb_info = {
	.hw_init                = &e330_tc6387xb_hwinit,
	.suspend                = &e330_tc6387xb_suspend,
	.resume                 = &e330_tc6387xb_resume,
};

static struct platform_device e330_tc6387xb_device = {
	.name           = "tc6387xb",
	.id             = -1,
	.dev            = {
		.platform_data = &e330_tc6387xb_info, 
	},
	.num_resources = ARRAY_SIZE(e330_tc6387xb_resources),
	.resource      = e330_tc6387xb_resources,
}; 

static int __init e330_tc6387xb_init(void)
{
	if(!machine_is_e330())
		return -ENODEV;

	platform_device_register(&e330_tc6387xb_device);
	return 0;
}

//module_init(e330_tc6387xb_init);
late_initcall(e330_tc6387xb_init);

//MODULE_AUTHOR("Ian Molton <spyro@f2s.com>");
//MODULE_DESCRIPTION("e330 tc6387xb device support");
//MODULE_LICENSE("GPL");



#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/major.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

#include <asm/mach-types.h>
#include <asm/arch/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/irq.h>

#include <asm/arch/eseries-irq.h>
#include <asm/arch/eseries-gpio.h>

#include <linux/mfd/tc6393.h>

static struct resource e750_tc6393xb_resources[] = {
	[0] = {
		.start  = PXA_CS4_PHYS,
		.end    = PXA_CS4_PHYS + 0x1fffff,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = IRQ_GPIO(GPIO_ESERIES_TMIO_IRQ),
		.end    = IRQ_GPIO(GPIO_ESERIES_TMIO_IRQ),
		.flags  = IORESOURCE_IRQ,
	},

};

//FIXME - who should really be setting up the clock? bootloader or kernel ?
static void e750_tc6393xb_hwinit(void) {
	pxa_gpio_mode(GPIO11_3_6MHz_MD);

	GPCR(45) = GPIO_bit(45); // #SUSPEND low
	GPCR(19) = GPIO_bit(19); // #PCLR low (reset)
	mdelay(1);
	GPSR(45) = GPIO_bit(45); // #SUSPEND high
	mdelay(10);              //FIXME - pronbably 1000x too long
	GPSR(19) = GPIO_bit(19); // #PCLR high
}

static void e750_tc6393xb_suspend(void) {
	GPSR(45) = GPIO_bit(45); // #SUSPEND high
	mdelay(10);
	GPSR(19) = GPIO_bit(19); // #PCLR high
	
	pxa_gpio_mode(GPIO11_3_6MHz_MD|GPIO_OUT);
	GPSR0 = GPIO_bit(GPIO11_3_6MHz);
}

/*
MCR : 80aa
CCR: 1310
PLL2CR: 0c01
PLL1CR1: f743
PLL1CR2: 00f2
SYS_DCR: 1033
*/

static struct tc6393xb_platform_data e750_tc6393xb_info = {
	.irq_base 		= IRQ_BOARD_START,
	.sys_pll2cr             = 0x0cc1,
	.sys_ccr                = 0x1310,
	.sys_mcr                = 0x80aa,
	.sys_gper		= 0,
	.sys_gpodsr1            = 0,
	.sys_gpooecr1           = 0,
	.hw_init                = &e750_tc6393xb_hwinit,
	.suspend                = &e750_tc6393xb_suspend,
};

static struct platform_device e750_tc6393xb_device = {
	.name           = "tc6393xb",
	.id             = -1,
	.dev            = {
		.platform_data = &e750_tc6393xb_info, 
	},
	.num_resources = ARRAY_SIZE(e750_tc6393xb_resources),
	.resource      = e750_tc6393xb_resources,
}; 

static int __init e750_tc6393xb_init(void)
{
	if(!(machine_is_e750() || machine_is_e800()))
		return -ENODEV;

	platform_device_register(&e750_tc6393xb_device);
	return 0;
}

module_init(e750_tc6393xb_init);

MODULE_AUTHOR("Ian Molton <spyro@f2s.com>");
MODULE_DESCRIPTION("e740 tc6393 device support");
MODULE_LICENSE("GPL");



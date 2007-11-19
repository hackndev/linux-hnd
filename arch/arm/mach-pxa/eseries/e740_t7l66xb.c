/* platform specific code to drive the T7L66XB chip on the e740. */

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

#include <linux/mfd/t7l66xb.h>

static struct resource e740_t7l66xb_resources[] = {
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
static void e740_t7l66xb_hwinit(void) {

	GPCR(19) = GPIO_bit(19); // #SUSPEND low
	GPCR(7) = GPIO_bit(7); // #PCLR low (reset)
	pxa_gpio_mode(GPIO7_48MHz_MD);
	pxa_gpio_mode(GPIO12_32KHz_MD);
	mdelay(10);
	GPSR(19) = GPIO_bit(19); // #SUSPEND high
	mdelay(10);
	GPSR(7) = GPIO_bit(7); // #PCLR high
	mdelay(10);
}

static void e740_t7l66xb_suspend(void) {
	GPCR(19) = GPIO_bit(19); // #SUSPEND low
	mdelay(10);
	GPCR(7) = GPIO_bit(7); // #PCLR low
	
	/* kill clock */
//	pxa_gpio_mode(GPIO7_48MHz_MD|GPIO_OUT);
//	GPSR0 = GPIO_bit(GPIO7_48MHz);
}

static void e740_t7l66xb_resume(void) {
	e740_t7l66xb_hwinit();
}

static struct t7l66xb_platform_data e740_t7l66xb_info = {
	.irq_base 		= IRQ_BOARD_START,
	.hw_init                = &e740_t7l66xb_hwinit,
	.suspend                = &e740_t7l66xb_suspend,
	.resume                 = &e740_t7l66xb_resume,
};

static struct platform_device e740_t7l66xb_device = {
	.name           = "t7l66xb",
	.id             = -1,
	.dev            = {
		.platform_data = &e740_t7l66xb_info, 
	},
	.num_resources = ARRAY_SIZE(e740_t7l66xb_resources),
	.resource      = e740_t7l66xb_resources,
}; 

static int __init e740_t7l66xb_init(void)
{
	if(!machine_is_e740())
		return -ENODEV;

	platform_device_register(&e740_t7l66xb_device);
	return 0;
}

module_init(e740_t7l66xb_init);

MODULE_AUTHOR("Ian Molton <spyro@f2s.com>");
MODULE_DESCRIPTION("e740 t7l66xb device support");
MODULE_LICENSE("GPL");



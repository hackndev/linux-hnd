
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <linux/platform_device.h>
#include <linux/input.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/hardware.h>
#include <asm/arch/irq.h>

#include <asm/arch/htcblueangel-gpio.h>
#include <asm/arch/htcblueangel-asic.h>
#include <asm/arch/ssp.h>

#include <linux/mfd/tsc2200.h>


/*
 * SSP Devices Setup
 */
struct ssp_dev blueangel_tsc2200_ssp;
struct platform_device blueangel_tsc2200;

static void blueangel_tsc2200_init(void)
{

	pxa_gpio_mode(GPIO_NR_BLUEANGEL_TS_IRQ_N | GPIO_IN);
	
	printk("Initializing blueangel tsc2200 ssp... ");
	pxa_gpio_mode(GPIO81_NSSP_CLK_OUT);
	pxa_gpio_mode(GPIO82_NSSP_FRM_OUT);
	pxa_gpio_mode(GPIO83_NSSP_TX);
	pxa_gpio_mode(GPIO84_NSSP_RX);
	GPSR(GPIO82_NSFRM) = GPIO_bit(GPIO82_NSFRM);
	
	ssp_init(&blueangel_tsc2200_ssp, 2, 0 );
	ssp_config(&blueangel_tsc2200_ssp,
			SSCR0_Motorola | SSCR0_DataSize(0x10), 
			SSCR1_TTE | SSCR1_EBCEI | SSCR1_SPH |
            			SSCR1_TxTresh(1) | SSCR1_RxTresh(1),
			0,
			SSCR0_SerClkDiv(0x14)
	);
	ssp_enable(&blueangel_tsc2200_ssp);
	printk("done\n");

	// Setup some healthy defaults for the blueangel
	tsc2200_write( &blueangel_tsc2200.dev, TSC2200_CTRLREG_REF, 0x1F);
	tsc2200_write( &blueangel_tsc2200.dev, TSC2200_CTRLREG_CONFIG, 0xA);

	return;
}

static void blueangel_tsc2200_exit(void) {
	
	printk("Closing blueangel tsc2200 ssp... ");
	
	ssp_disable(&blueangel_tsc2200_ssp);
	ssp_exit(&blueangel_tsc2200_ssp);
	
	printk("done!\n");

	return;
}

static unsigned short blueangel_tsc2200_send(unsigned short reg, unsigned short val)
{   
	
	u32 dummy, data;
	ssp_write_word(&blueangel_tsc2200_ssp, reg);
	ssp_write_word(&blueangel_tsc2200_ssp, val);
	ssp_read_word(&blueangel_tsc2200_ssp, &dummy); // dummy read for reg
	ssp_read_word(&blueangel_tsc2200_ssp, &data); 
	return data;
	
}

static struct tsc2200_key blueangel_tsc2200_keys[] = {
        {"contacts",            0,      KEY_F5		},
        {"calendar",            1,      KEY_F6		},
	{"up",                  3,      KEY_UP		},
	{"select",              6,      KEY_KPENTER 	},
	{"left",                7,      KEY_LEFT	},
	{"phone_lift",          9,      KEY_F7		},
	{"down",                11,     KEY_DOWN 	},
	{"phone_hangup",        12,     KEY_F8 		},
	{"right",               15,     KEY_RIGHT	},
};

static struct tsc2200_buttons_platform_info blueangel_tsc2200_buttons = {
	.keys 		= blueangel_tsc2200_keys,
	.num_keys	= ARRAY_SIZE(blueangel_tsc2200_keys),
	.irq		= IRQ_NR_BLUEANGEL_TSC2200_KB,
};

static struct tsc2200_platform_info blueangel_tsc2200_info = {

	// Various callbacks to deal with the ssp
	.init 	  = blueangel_tsc2200_init,
	.exit	  = blueangel_tsc2200_exit,
	.send     = blueangel_tsc2200_send,

	// Buttons and touchscreen data that this chip supplies on the blueangel
	.buttons_info 	  = &blueangel_tsc2200_buttons,
};
 
struct platform_device blueangel_tsc2200 = {
	.name 		= "tsc2200",
	.dev		= {
 		.platform_data	= &blueangel_tsc2200_info,
	},
};
EXPORT_SYMBOL(blueangel_tsc2200);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michael Horne <asylumed@gmail.com>");

/*
 * Hardware definitions for HTC Blueangel
 *
 * Copyright 2004 Xanadux.org
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * Authors: w4xy@xanadux.org
 *
 * History:
 *
 * 2004-02-07	W4XY		   Initial port heavily based on h1900.c
 * 2006, 2007	Michael Horne <asylumed@gmail.com>	Elaboration/Improvements
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/input.h>                                                                                                                         
#include <linux/input_pda.h>   
#include <linux/pm.h>
#include <linux/bootmem.h>
#include <linux/platform_device.h>
#include <linux/gpio_keys.h>

#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/setup.h>
#include <asm/types.h>
#include <asm/delay.h>

#include <asm/mach/irq.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/irq.h>
#include <asm/arch/pxafb.h>
#include <asm/arch/udc.h>

#include <asm/hardware/ipaq-asic3.h>
#include <asm/hardware/asic3_leds.h>
#include <asm/hardware/asic3_keys.h>
#include <asm/arch/htcblueangel.h>
#include <asm/arch/htcblueangel-gpio.h>
#include <asm/arch/htcblueangel-asic.h>
#include <linux/mfd/tsc2200.h>
#include <linux/serial_core.h>
#include <linux/touchscreen-adc.h>
#include <linux/adc_battery.h>
#include <asm/arch/pxa2xx_udc_gpio.h>

#include <asm-arm/arch-pxa/serial.h>
#include "../generic.h"

#include <linux/lcd.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/mfd/asic3_base.h>

#include <linux/mfd/tmio_mmc.h>    /* TODO: replace with asic3 */

extern struct platform_device blueangel_tsc2200;

DEFINE_LED_TRIGGER_SHARED_GLOBAL(blueangel_radio_trig);
EXPORT_LED_TRIGGER_SHARED(blueangel_radio_trig);

/* LEDs */

static struct asic3_led blueangel_asic3_leds[] = {
	{
		.led_cdev  = {
 			.name	         = "blueangel:red",
 			.default_trigger = "ds2760-battery.0-charging-or-full",
		},
		.hw_num = 0,

	},
	{
		.led_cdev  = {
			.name	         = "blueangel:green",
 			.default_trigger = "ds2760-battery.0-full",
		},
		.hw_num = 1,
	},
	{
		.led_cdev  = {
			.name	         = "blueangel:phonebuttons",
			.default_trigger = "blueangel-phonebuttons",
		},
		.hw_num = -1,
		.gpio_num = ('B'-'A')*16+GPIOB_PHONEL_PWR_ON,
	},
	{
		.led_cdev  = {
			.name	         = "blueangel:vibra",
			.default_trigger = "blueangel-vibra",
		},
		.hw_num = -1,
		.gpio_num = ('B'-'A')*16+GPIOB_VIBRA_PWR_ON,
	},
	{
		.led_cdev  = {
			.name	         = "blueangel:kbdbacklight",
			.default_trigger = "blueangel-kbdbacklight",
		},
		.hw_num = -1,
		.gpio_num = ('C'-'A')*16+GPIOC_KEYBL_PWR_ON,
	},
};

static struct asic3_leds_machinfo blueangel_leds_machinfo = {
	.leds = blueangel_asic3_leds,
	.num_leds = ARRAY_SIZE(blueangel_asic3_leds),
	.asic3_pdev = &blueangel_asic3,
};

static struct platform_device blueangel_leds_pdev = {
	.name = "asic3-leds",
	.dev = {
		.platform_data = &blueangel_leds_machinfo,
	},
};

/* Serial */

static struct platform_pxa_serial_funcs pxa_serial_funcs [] = {
	{}, /* No special FFUART options */
	{}, /* No special BTUART options */
	{}, /* No special STUART options */
	{}, /* No special HWUART options */
};

/* Backup battery */

static struct battery_adc_platform_data blueangel_backup_batt_params = {
        .battery_info = {
                .name = "backup-battery",
                .voltage_max_design = 3200000,
                .voltage_min_design = 1000000,
        },
        .voltage_pin = "tsc2200-adc.0:vaux1",
};

static struct platform_device blueangel_backup_batt = {
        .name = "adc-battery",
        .id = -1,
        .dev = {
                .platform_data = &blueangel_backup_batt_params,
        }
};

struct tsadc_platform_data blueangel_ts_params = {
        .pen_gpio = GPIO_NR_BLUEANGEL_TS_IRQ_N,
        .x_pin = "tsc2200-adc.0:x",
        .y_pin = "tsc2200-adc.0:y",
        .z1_pin = "tsc2200-adc.0:z1",
        .z2_pin = "tsc2200-adc.0:z2",
        .pressure_factor = 100000,
        .min_pressure = 2,
        .max_jitter = 8,
};
static struct resource blueangel_pen_irq = {
	.start = IRQ_GPIO(GPIO_NR_BLUEANGEL_TS_IRQ_N),
	.end = IRQ_GPIO(GPIO_NR_BLUEANGEL_TS_IRQ_N),
	.flags = IORESOURCE_IRQ,
};
static struct platform_device tsc2200_ts = {
        .name = "ts-adc",
        .id = -1,
        .resource = &blueangel_pen_irq,
        .num_resources = 1,
        .dev = {
                .platform_data = &blueangel_ts_params,
        }
};

static void __init blueangel_init_irq( void )
{
	/* Initialize standard IRQs */
	pxa_init_irq();
}

static void ser_stuart_gpio_config(int enable)
{
	printk("ser_stuart_gpio_config %d\n", enable);
	if (enable == PXA_UART_CFG_PRE_STARTUP) {
    STISR=0;
  }
}

static void ser_hwuart_gpio_config(int enable)
{
	printk("ser_hwuart_gpio_config %d\n", enable);
	if (enable == PXA_UART_CFG_PRE_STARTUP) {
		GPDR(GPIO42_HWRXD) &= ~(GPIO_bit(GPIO42_HWRXD));
		GPDR(GPIO43_HWTXD) |= GPIO_bit(GPIO43_HWTXD);
		GPDR(GPIO44_HWCTS) &= ~(GPIO_bit(GPIO44_HWCTS));
		GPDR(GPIO45_HWRTS) |= GPIO_bit(GPIO45_HWRTS);
		pxa_gpio_mode(GPIO42_HWRXD_MD);
		pxa_gpio_mode(GPIO43_HWTXD_MD);
		pxa_gpio_mode(GPIO44_HWCTS_MD);
		pxa_gpio_mode(GPIO45_HWRTS_MD);
		asic3_set_gpio_dir_a(&blueangel_asic3.dev, 1<<GPIOA_BT_PWR1_ON, 1<<GPIOA_BT_PWR1_ON);
		asic3_set_gpio_out_a(&blueangel_asic3.dev, 1<<GPIOA_BT_PWR1_ON, 1<<GPIOA_BT_PWR1_ON);
		asic3_set_gpio_dir_b(&blueangel_asic3.dev, 1<<GPIOB_BT_PWR2_ON, 1<<GPIOB_BT_PWR2_ON);
		asic3_set_gpio_out_b(&blueangel_asic3.dev, 1<<GPIOB_BT_PWR2_ON, 1<<GPIOB_BT_PWR2_ON);
  } else if (enable == PXA_UART_CFG_POST_SHUTDOWN) {
		asic3_set_gpio_out_a(&blueangel_asic3.dev, 1<<GPIOA_BT_PWR1_ON, 0);
		asic3_set_gpio_out_b(&blueangel_asic3.dev, 1<<GPIOB_BT_PWR2_ON, 0);
	}
}

/*
 * Common map_io initialization
 */
static void __init blueangel_map_io(void)
{

	pxa_map_io();

	STISR=0;	/* Disable UART mode of STUART */
	printk("CKEN=0x%x CKEN11_USB=0x%x\n", CKEN, CKEN11_USB);	
	pxa_set_cken(CKEN11_USB, 1);
	printk("CKEN=0x%x\n", CKEN);	
#if 0

	GAFR0_L = 0x98000000;
	GAFR0_U = 0x494A8110;
	GAFR1_L = 0x699A8159;
	GAFR1_U = 0x0005AAAA;
	GAFR2_L = 0xA0000000;
	GAFR2_U = 0x00000002;

	/* don't do these for now because one of them turns the screen to mush */
	/* reason: the ATI chip gets reset / LCD gets disconnected:
	 * a fade-to-white means that the ati 3200 registers are set incorrectly */
	GPCR0   = 0xFF00FFFF;
	GPCR1   = 0xFFFFFFFF;
	GPCR2   = 0xFFFFFFFF;

	GPSR0   = 0x444F88EF;
	GPSR1   = 0x57BF7306;
	GPSR2   = 0x03FFE008;

	PGSR0   = 0x40DF88EF;
	PGSR1   = 0x53BF7206;
	PGSR2   = 0x03FFE000;

	GPDR0   = 0xD7E9A042;
	GPDR1   = 0xFCFFABA3;
	GPDR2   = 0x000FEFFE;
	GPSR0   = 0x444F88EF;
	GPSR1   = 0xD7BF7306;
	GPSR2   = 0x03FFE008;
	GRER0   = 0x00000000;
	GRER1   = 0x00000000;
	GRER2   = 0x00000000;
	GFER0   = 0x00000000;
	GFER1   = 0x00000000;
	GFER2   = 0x00000000;
#endif


	pxa_serial_funcs[2].configure = ser_stuart_gpio_config;
	pxa_serial_funcs[3].configure = ser_hwuart_gpio_config;
	pxa_set_stuart_info(&pxa_serial_funcs[2]);
	pxa_set_hwuart_info(&pxa_serial_funcs[3]);
}

/* 
 * All the asic3 dependant devices 
 */

extern struct platform_device blueangel_bl;
static struct platform_device blueangel_lcd = { .name = "blueangel-lcd", };
static struct platform_device blueangel_leds = { .name = "blueangel-leds", };
static struct platform_device blueangel_pcmcia    = { .name =
                                            "blueangel-pcmcia", };

/*
 *  ASIC3 buttons
 */

static struct asic3_keys_button blueangel_asic3_keys_table[] = {
	{KEY_RECORD,		BLUEANGEL_RECORD_BTN_IRQ,	1,	"Record Button"}, 
	{KEY_VOLUMEUP,		BLUEANGEL_VOL_UP_BTN_IRQ,	1,	"Volume Up Button"}, 
	{KEY_VOLUMEDOWN,	BLUEANGEL_VOL_DOWN_BTN_IRQ,	1,	"Volume Down Button"}, 
	{KEY_CAMERA,		BLUEANGEL_CAMERA_BTN_IRQ,	1,	"Camera Button"}, 
	{KEY_MENU,		BLUEANGEL_WINDOWS_BTN_IRQ,	1,	"Windows Button"}, 
	{KEY_EMAIL,		BLUEANGEL_MAIL_BTN_IRQ,		1,	"Mail Button"}, 
	{KEY_WWW,		BLUEANGEL_WWW_BTN_IRQ,		1,	"Internet Button"}, 
	{KEY_KPENTER,		BLUEANGEL_OK_BTN_IRQ,		1,	"Ok Button"}, 

};

static struct asic3_keys_platform_data blueangel_asic3_keys_data = {
	.buttons	= blueangel_asic3_keys_table,
	.nbuttons	= ARRAY_SIZE(blueangel_asic3_keys_table),
	.asic3_dev	= &blueangel_asic3.dev,
};

static struct platform_device blueangel_asic3_keys = {
	.name		= "asic3-keys",
	.dev = {
	    .platform_data = &blueangel_asic3_keys_data,
	}
};

/* UDC device */
struct pxa2xx_udc_gpio_info blueangel_udc_info = {
	.detect_gpio = {&pxagpio_device.dev, GPIO_NR_BLUEANGEL_USB_DETECT_N},
	.detect_gpio_negative = 1,
	.power_ctrl = {
		.power_gpio = { &blueangel_asic3.dev, ASIC3_GPIOC_IRQ_BASE + GPIOC_USB_PUEN},                                   
		.power_gpio_neg = 1,
	},                                                                                                                   
};                                                                                                                           

static struct platform_device blueangel_udc = {                                                                                  
	.name = "pxa2xx-udc-gpio",                                                                                           
	.dev = {                                                                                                             
		.platform_data = &blueangel_udc_info                                                                             
	}                                                                                                                    
};
 
static struct platform_device *blueangel_asic3_devices[] __initdata = {
	&tsc2200_ts,
	&blueangel_lcd,
	&blueangel_udc,
	&blueangel_backup_batt,
#ifdef CONFIG_MACH_BLUEANGEL_BACKLIGHT
	&blueangel_bl,
#endif
	&blueangel_asic3_keys,
	&blueangel_leds,
	&blueangel_pcmcia,
	&blueangel_leds_pdev,
};


/*
 * the ASIC3 should really only be referenced via the asic3_base
 * module.  it contains functions something like asic3_gpio_b_out()
 * which should really be used rather than macros.
 */

static int blueangel_get_mmc_ro(struct platform_device *dev)
{
 return (((asic3_get_gpio_status_d( &blueangel_asic3.dev )) & (1<<GPIOD_SD_WRITE_PROTECT)) != 0);
}

static struct tmio_mmc_hwconfig blueangel_mmc_hwconfig = {
        .mmc_get_ro = blueangel_get_mmc_ro,
};

static struct asic3_platform_data asic3_platform_data_o = {
	.gpio_a = {
		.dir		= 0xbffd,
		.init		= 0x0110,
		.sleep_out	= 0x0010,
		.batt_fault_out	= 0x0010,
		.sleep_mask	= 0xffff,
		.sleep_conf	= 0x0008,
		.alt_function	= 0x9800, /* Caution: need to be set to a correct value */
	},
	.gpio_b = {
		.dir		= 0xfffc,
		.init		= 0x40fc,
		.sleep_out	= 0x0000,
		.batt_fault_out	= 0x0000,
		.sleep_mask	= 0xffff,
		.sleep_conf	= 0x000c,
		.alt_function	= 0x0000, /* Caution: need to be set to a correct value */
	},
	.gpio_c = {
		.dir		= 0xfff7,
		.init		= 0xc344,            
		.sleep_out	= 0x04c4,            
		.batt_fault_out	= 0x0484,            
		.sleep_mask	= 0xffff,
		.sleep_conf	= 0x000c,
		.alt_function	= 0x003b, /* Caution: need to be set to a correct value */
	},
	.gpio_d = {
		.dir		= 0x0040,
		.init		= 0x3e1b,            
		.sleep_out	= 0x3e1b,            
		.batt_fault_out = 0x3e1b,  
		.sleep_mask	= 0x0000,
		.sleep_conf	= 0x000c,
		.alt_function	= 0x0000, /* Caution: need to be set to a correct value */
	},
	.bus_shift=1,
	.irq_base = HTCBLUEANGEL_ASIC3_IRQ_BASE,

	.child_devs     = blueangel_asic3_devices,                                                                      
	.num_child_devs = ARRAY_SIZE(blueangel_asic3_devices),       
	.tmio_mmc_hwconfig = &blueangel_mmc_hwconfig,
};

static struct resource asic3_resources[] = {
	[0] = {
		.start  = BLUEANGEL_ASIC3_GPIO_PHYS,
		.end    = BLUEANGEL_ASIC3_GPIO_PHYS + 0xfffff,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = IRQ_NR_BLUEANGEL_ASIC3,
		.end    = IRQ_NR_BLUEANGEL_ASIC3,
		.flags  = IORESOURCE_IRQ,
	},
	[2] = {
		.start  = BLUEANGEL_ASIC3_MMC_PHYS,
		.end    = BLUEANGEL_ASIC3_MMC_PHYS + IPAQ_ASIC3_MAP_SIZE,
		.flags  = IORESOURCE_MEM,
	},
	[3] = {
		.start  = IRQ_GPIO(GPIO_NR_BLUEANGEL_SD_IRQ_N),
		.flags  = IORESOURCE_IRQ,
	},
};

struct platform_device blueangel_asic3 = {
	.name       = "asic3",
	.id     = 0,
	.num_resources  = ARRAY_SIZE(asic3_resources),
	.resource   = asic3_resources,
	.dev = {
	  .platform_data = &asic3_platform_data_o,
         },
};
EXPORT_SYMBOL(blueangel_asic3);

/*
 * Magician LEDs
 */
static struct platform_device blueangel_led = {
        .name   = "htcblueangel-led",
        .id     = -1,
};

/*
 * GPIO buttons
 */
static struct gpio_keys_button blueangel_button_table[] = {                                                                                          
    { KEY_POWER, GPIO_NR_BLUEANGEL_POWER_BUTTON_N, 1 },                                                                                           
};                                                                                                                                               

static struct gpio_keys_platform_data blueangel_pxa_keys_data = {                                                                                    
     .buttons = blueangel_button_table,                                                                                                           
     .nbuttons = ARRAY_SIZE(blueangel_button_table),                                                                                              
};                                                                                                                                               
                                                                                                                                                  
static struct platform_device blueangel_pxa_keys = {
    .name = "gpio-keys",
    .dev = {
	.platform_data = &blueangel_pxa_keys_data,
    },
};

/*
 *
 */

static struct platform_device *devices[] __initdata = {
	&blueangel_asic3,
	&blueangel_led,
	&blueangel_tsc2200,
	&blueangel_pxa_keys,
};

static void __init blueangel_init(void)
{
	platform_add_devices (devices, ARRAY_SIZE (devices));
	led_trigger_register_shared("blueangel-radio", &blueangel_radio_trig);
}

MACHINE_START(BLUEANGEL, "HTC Blueangel")
        /* Maintainer xanadux.org */
	.phys_io	= 0x40000000,
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
        .boot_params	= 0xa0000100,
        .map_io		= blueangel_map_io,
        .init_irq	= blueangel_init_irq,
        .timer 		= &pxa_timer,
        .init_machine 	= blueangel_init,
MACHINE_END


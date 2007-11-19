/*
 * Machine initialization for Rover P5+
 *
 * Authors: Konstantine A. Beklemishev konstantine@r66.ru
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
//#include <linux/fb.h>
#include <linux/ioport.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach-types.h>

#include <asm/setup.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/hardware.h>

#include <asm/arch/irqs.h>
#include <asm/arch/roverp5p-init.h>
#include <asm/arch/roverp5p-gpio.h>

//#include "../drivers/video/pxafb.h"
#include "../drivers/soc/mq11xx.h"

#define MQ_BASE	0x14000000
#include "../generic.h"

#if 0

/* MediaQ 1188 init state */
static struct mediaq11xx_init_data mq1188_init_data = {
/*DC*/
{
/*DC0x0*/      0x00000001,
/*DC0x1*/      0x00000043,
/*DC0x2*/      0x00000001,
/*DC0x3*/      0x00000008,
/*DC0x4*/      0x00180004,
/*DC0x5*/      0x00000007,
},
/*CC*/
{
/*CC0x0*/      0x00000002,
/*CC0x1*/      0x00201010,
/*CC0x2*/      0x00a00a10,
/*CC0x3*/      0x1a000200,
/*CC0x4*/      0x00000004,
},
/*MIU*/
{
/*MIU0x0*/      0x00000001,
/*MIU0x1*/      0x1b676ca0,
/*MIU0x2*/      0x117700b0,
/*MIU0x3*/      0x00000000,
/*MIU0x4*/      0x00000000,
/*MIU0x5*/      0x00170211,
/*MIU0x6*/      0x00000000,
},
/*GC*/
{
/*GC0x0*/      0x080100c8,
/*GC0x1*/      0x00000200,
/*GC0x2*/      0x00f5011a,
/*GC0x3*/      0x013f014c,
/*GC0x4*/      0x010e0108,
/*GC0x5*/      0x01470146,
/*GC0x6*/      0x010b0000,
/*GC0x7*/      0x010d0000,
/*GC0x8*/      0x00ef0004,
/*GC0x9*/      0x013f0000,
/*GC0xa*/      0x0113010e,
/*GC0xb*/      0x00a78043,
/*GC0xc*/      0x00000000,
/*GC0xd*/      0x00000000,
/*GC0xe*/      0x000001e0,
/*GC0xf*/      0x00000000,
/*GC0x10*/      0x00000000,
/*GC0x11*/      0x000000ff,
/*GC0x12*/      0x00000000,
/*GC0x13*/      0x00000000,
/*GC0x14*/      0x00000000,
/*GC0x15*/      0x00000000,
/*GC0x16*/      0x00000000,
/*GC0x17*/      0x00000000,
/*GC0x18*/      0x00000000,
/*GC0x19*/      0x00000000,
/*GC0x1a*/      0x00000000,
},
/*FP*/
{
/*FP0x0*/      0x00006120,
/*FP0x1*/      0x071c9008,
/*FP0x2*/      0xdffdfcff,
/*FP0x3*/      0x00000000,
/*FP0x4*/      0x00bd0001,
/*FP0x5*/      0x80010000,
/*FP0x6*/      0x80000000,
/*FP0x7*/      0x00000000,
/*FP0x8*/      0x8fadb76e,
/*FP0x9*/      0xfffffdff,
/*FP0xa*/      0x00000000,
/*FP0xb*/      0x00000000,
/*FP0xc*/      0x00000003,
/*FP0xd*/      0x00000000,
/*FP0xe*/      0x00000000,
/*FP0xf*/      0x00005fa0,
/*FP0x10*/      0x9edffafb,
/*FP0x11*/      0x9ffddffb,
/*FP0x12*/      0xfbefffae,
/*FP0x13*/      0xfdb6179c,
/*FP0x14*/      0xee7d5d68,
/*FP0x15*/      0xfff793dc,
/*FP0x16*/      0xe2fdfdd7,
/*FP0x17*/      0x2b726d5b,
/*FP0x18*/      0x785f3b0c,
/*FP0x19*/      0xbfee6fdb,
/*FP0x1a*/      0x7ed7eaeb,
/*FP0x1b*/      0xad4bff57,
/*FP0x1c*/      0x7fa96377,
/*FP0x1d*/      0x73e72bf8,
/*FP0x1e*/      0xfddddf5d,
/*FP0x1f*/      0xeeeefee7,
/*FP0x20*/      0xfff6d4bf,
/*FP0x21*/      0x67f1ab2f,
/*FP0x22*/      0xd6fbdddd,
/*FP0x23*/      0xbe76bcff,
/*FP0x24*/      0xbcd7fefe,
/*FP0x25*/      0xdf7318ff,
/*FP0x26*/      0xe838b777,
/*FP0x27*/      0x35d4dbb3,
/*FP0x28*/      0x3c6f9bfa,
/*FP0x29*/      0x795ffb6d,
/*FP0x2a*/      0x7f7df49f,
/*FP0x2b*/      0xfe169e7d,
/*FP0x2c*/      0xeebdf697,
/*FP0x2d*/      0x3b5da966,
/*FP0x2e*/      0xa7b7932e,
/*FP0x2f*/      0x4d7bbf68,
/*FP0x30*/      0x5afc7dfd,
/*FP0x31*/      0x5f353796,
/*FP0x32*/      0x03ad77dd,
/*FP0x33*/      0xf56b9ddc,
/*FP0x34*/      0xbff7a7ed,
/*FP0x35*/      0x97b1fb9b,
/*FP0x36*/      0xdbb36dff,
/*FP0x37*/      0xdbddfff3,
/*FP0x38*/      0xb49f0000,
/*FP0x39*/      0x00007d53,
/*FP0x3a*/      0x0000b8f7,
/*FP0x3b*/      0x0000378a,
/*FP0x3c*/      0xbdb8ffef,
/*FP0x3d*/      0xf37fef8d,
/*FP0x3e*/      0x39ff7cd0,
/*FP0x3f*/      0xb96de15c,
/*FP0x40*/      0x00000000,
/*FP0x41*/      0x00000000,
/*FP0x42*/      0x00000000,
/*FP0x43*/      0x00000000,
/*FP0x44*/      0x00000000,
/*FP0x45*/      0x00000000,
/*FP0x46*/      0x00000000,
/*FP0x47*/      0x00000000,
/*FP0x48*/      0x00000000,
/*FP0x49*/      0x00000000,
/*FP0x4a*/      0x00000000,
/*FP0x4b*/      0x00000000,
/*FP0x4c*/      0x00000000,
/*FP0x4d*/      0x00000000,
/*FP0x4e*/      0x00000000,
/*FP0x4f*/      0x00000000,
/*FP0x50*/      0x00000000,
/*FP0x51*/      0x00000000,
/*FP0x52*/      0x00000000,
/*FP0x53*/      0x00000000,
/*FP0x54*/      0x00000000,
/*FP0x55*/      0x00000000,
/*FP0x56*/      0x00000000,
/*FP0x57*/      0x00000000,
/*FP0x58*/      0x00000000,
/*FP0x59*/      0x00000000,
/*FP0x5a*/      0x00000000,
/*FP0x5b*/      0x00000000,
/*FP0x5c*/      0x00000000,
/*FP0x5d*/      0x00000000,
/*FP0x5e*/      0x00000000,
/*FP0x5f*/      0x00000000,
/*FP0x60*/      0x00000000,
/*FP0x61*/      0x00000000,
/*FP0x62*/      0x00000000,
/*FP0x63*/      0x00000000,
/*FP0x64*/      0x00000000,
/*FP0x65*/      0x00000000,
/*FP0x66*/      0x00000000,
/*FP0x67*/      0x00000000,
/*FP0x68*/      0x00000000,
/*FP0x69*/      0x00000000,
/*FP0x6a*/      0x00000000,
/*FP0x6b*/      0x00000000,
/*FP0x6c*/      0x00000000,
/*FP0x6d*/      0x00000000,
/*FP0x6e*/      0x00000000,
/*FP0x6f*/      0x00000000,
/*FP0x70*/      0x00000000,
/*FP0x71*/      0x00000000,
/*FP0x72*/      0x00000000,
/*FP0x73*/      0x00000000,
/*FP0x74*/      0x00000000,
/*FP0x75*/      0x00000000,
/*FP0x76*/      0x00000000,
/*FP0x77*/      0x00000000,
},
/*GE*/
{
/*GE0x0*/      0x00000008,
/*GE0x1*/      0x00000000,
/*GE0x2*/      0x00000000,
/*GE0x3*/      0x00000000,
/*GE0x4*/      0x00000000,
/*GE0x5*/      0x00000000,
/*GE0x6*/      0x00000000,
/*GE0x7*/      0x00001400,
/*GE0x8*/      0x93edfd2f,
/*GE0x9*/      0x00000000,
/*GE0xa*/      0x00000000,
/*GE0xb*/      0x00000000,
}
};

static void __init roverp5p_init_irq (void)
{
	/* Initialize standard IRQs */
	pxa_init_irq();
//	printk("init_irq done \n");

}

//#ifdef DEBUG
/* A debug mapping giving access to the screen RAM */
//static struct map_desc roverp5p_io_desc[] __initdata = {
  /*virtual       physical     length      domain                 */
//  { 0xf0000000,  0x0c100000,  0x02000000, DOMAIN_IO}
//};
//#endif
                                                                                
static void __init roverp5p_map_io(void)
{
	/* Initialize standard IO maps */
        pxa_map_io();
//#ifdef DEBUG
//        iotable_init(roverp5p_io_desc, ARRAY_SIZE(roverp5p_io_desc));
//#endif	
	/* Configure power management stuff. */
	PWER = PWER_GPIO0 | PWER_RTC;
	PFER = PWER_GPIO0 | PWER_RTC;
	PRER = 0;
	PCFR = PCFR_OPDE;
	CKEN = CKEN6_FFUART;
	
	PGSR0 = GPSRx_SleepValue;
	PGSR1 = GPSRy_SleepValue;
	PGSR2 = GPSRz_SleepValue;

	/* Set up GPIO direction and alternate function registers */
	GAFR0_L = GAFR0x_InitValue;
	GAFR0_U = GAFR1x_InitValue;
	GAFR1_L = GAFR0y_InitValue;
	GAFR1_U = GAFR1y_InitValue;
	GAFR2_L = GAFR0z_InitValue;
	GAFR2_U = GAFR1z_InitValue;
	
	GPDR0 = GPDRx_InitValue;
	GPDR1 = GPDRy_InitValue;
	GPDR2 = GPDRz_InitValue;
	
	GPSR0 = GPSRx_InitValue;
	GPSR1 = GPSRy_InitValue;
	GPSR2 = GPSRz_InitValue;

	GPCR0 = ~GPSRx_InitValue;
	GPCR1 = ~GPSRy_InitValue;
	GPCR2 = ~GPSRz_InitValue;
	
//	GPCR0 = 0x0fffffff;       /* All outputs are set low by default */
//	printk("map_io done \n");
}

static struct resource mq1188_resources[] = {
	/* Synchronous memory */
	[0] = {
		.start	= MQ_BASE,
		.end	= MQ_BASE + MQ11xx_FB_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
        /* Non-synchronous memory */
	[1] = {
		.start	= MQ_BASE + MQ11xx_FB_SIZE + MQ11xx_REG_SIZE,
		.end	= MQ_BASE + MQ11xx_FB_SIZE * 2 - 1,
		.flags	= IORESOURCE_MEM,
	},
        /* MediaQ registers */
	[2] = {
		.start	= MQ_BASE + MQ11xx_FB_SIZE,
		.end	= MQ_BASE + MQ11xx_FB_SIZE + MQ11xx_REG_SIZE - 1,
		.flags	= IORESOURCE_MEM,
        },
        /* MediaQ interrupt number */
        [3] = {
		.start	= IRQ_GPIO (GPIO_NR_ROVERP5P_PXA_nMQINT),
		.flags	= IORESOURCE_IRQ,
        }
};

struct platform_device roverp5p_mq1188 = {
	.name		= "mq11xx",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(mq1188_resources),
	.resource	= mq1188_resources,
	.dev		= {
		.platform_data = &mq1188_init_data,
	}
};

static void __init roverp5p_init (void)
{
	platform_device_register (&roverp5p_mq1188);
//	memset ((void *)0x14000000, 0x99, 320*200*2);
//	printk("mach_init done \n");
}

MACHINE_START(ROVERP5P, "Rover Rover P5+")
	/* Maintainer Konstantine A. Beklemishev */
	.phys_ram	= 0xa0000000,
	.phys_io	= 0x40000000,
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
	.boot_params	= 0xa0000100,
	.map_io		= roverp5p_map_io,
	.init_irq	= roverp5p_init_irq,
	.init_machine = roverp5p_init,
MACHINE_END

#endif

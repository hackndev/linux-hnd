/*
 * Machine definition for Asus MyPal A730.
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * 2005-03-07   Markus Wagner           Started a730.c, based on hx4700.c
 * 2006-01-19   Nickolay Kopitonenko    USB Client proper initialization
 * 2006-03-29   Serge Nikolaenko        USB ohci platform init
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/apm-emulation.h>

#include <linux/mtd/map.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <asm/mach/flash.h>

#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>
#include <asm/arch/asus730-init.h>
#include <asm/arch/asus730-gpio.h>
#include <asm/arch/bitfield.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/ohci.h>
#include <asm/arch/udc.h>
#include <asm/arch/pxafb.h>
#include <asm/arch/audio.h>
#include <asm/irq.h>
#include <asm/arch/mmc.h>
#include <linux/delay.h>

#include <linux/platform_device.h>

#include "../generic.h"

struct pxafb_mode_info a730_fb_modes[] = {
    {
	.pixclock	= 52000,
	.bpp		= 16,
	.xres		= 480,
	.yres		= 640,
	.hsync_len	= 4,
        .vsync_len	= 4,
        .left_margin	= 77,
        .upper_margin	= 0,
        .right_margin	= 144,
        .lower_margin	= 0,
        .sync		= 0,
        .cmap_greyscale	= 0,
    },
    {
	.pixclock	= 52000,
	.bpp		= 16,
	.xres		= 240,
	.yres		= 320,
	.hsync_len	= 4,
        .vsync_len	= 4,
        .left_margin	= 40,
        .upper_margin	= 0,
        .right_margin	= 46,
        .lower_margin	= 0,
        .sync		= 0,
        .cmap_greyscale	= 0,
    },
};

static struct pxafb_mach_info asus730_fb_info  = {
	.modes = a730_fb_modes,
	.num_modes = ARRAY_SIZE(a730_fb_modes),
	.fixed_modes = 1,
	//.cmap_inverse = 0,
	//.cmap_static = 0,
	.lccr0 =	(LCCR0_ENB | LCCR0_Act | LCCR0_Sngl | LCCR0_Color | LCCR0_LDDALT | LCCR0_OUC | LCCR0_CMDIM | LCCR0_RDSTM),
	.lccr3 =        (LCCR3_Acb(0) | LCCR3_OutEnH | LCCR3_PixRsEdg),//0x04700001,
	//.pxafb_backlight_power = a730_backlight_power,
	//.pxafb_lcd_power = a730_lcd_power,
};

static struct platform_device a730_device = {
    .name = "a730",
    .id = 0,
};

static struct platform_device a730_power = {
    .name = "a730-power",
};

static struct platform_device a730_buttons = {
    .name = "a730-buttons",
};

static struct platform_device a730_lcd = {
    .name = "a730-lcd",
};

static struct platform_device a730_bl = {
    .name = "a730-bl",
    .id = -1,
};

static struct platform_device a730_bt = {
    .name = "a730-bt",
};

static struct platform_device a730_pcmcia = {
    .name = "a730-pcmcia",
};

static struct platform_device a730_mmc = {
    .name = "a730-mmc",
};

static struct platform_device a730_udc = {
    .name = "a730-udc",
};

static struct platform_device a730_ac97 = {
    .name = "pxa2xx-ac97",
    .id   = -1,
};

static struct platform_device a730_flash = {
    .name = "a730-physmap-flash",
    .id = 0,
};

static struct platform_device *a730_devices[] __initdata = {
    &a730_lcd,
    &a730_bl,
    &a730_device,
    &a730_power,
    &a730_buttons,
    &a730_bt,
    &a730_pcmcia,
    &a730_mmc,
    &a730_udc,
    &a730_ac97,
    &a730_flash,
};

/* Serial cable */
static void check_serial_cable( void )
{
	/* XXX: Should this be handled better? */
	int connected = GET_A730_GPIO( COM_DCD );
	/* Toggle rs232 transceiver power according to connected status */
	SET_A730_GPIO(RS232_ON, connected);
	SET_A730_GPIO(COM_CTS, connected);
	SET_A730_GPIO(COM_TXD, connected);
	SET_A730_GPIO(COM_DTR, connected);
	SET_A730_GPIO(COM_RTS, connected);
}

/* Initialization code */
static void __init a730_map_io(void)
{
	pxa_map_io();
}

static void __init a730_init_irq(void)
{
    pxa_init_irq();
}

extern u32 pca9535_read_input(void);

static void a730_apm_get_power_status(struct apm_power_info *info)
{
    u32 data;
    
    if (!info) return;
    
    data = pca9535_read_input();
    
    if ((data & 0x2) && (data & 0x4000)) info->ac_line_status = APM_AC_OFFLINE;
    else info->ac_line_status = APM_AC_ONLINE;

    info->time = 0;
    info->units = 0;
}

static int a730_hw_init(void)
{
	GCR &= ~(GCR_PRIRDY_IEN | GCR_SECRDY_IEN);

	//PWER = PWER_RTC | PWER_GPIO0 | PWER_GPIO1 | PWER_GPIO10 | PWER_GPIO12 | PWER_GPIO(17) | PWER_GPIO13 | PWER_WEP1 | /*PWER_USBC |*/ (0x1 << 24 /*wake
        //PFER = PWER_GPIO1 | PWER_GPIO12 | PWER_GPIO10;
        //PRER = PWER_GPIO0 | PWER_GPIO12;
        
        //PGSR0 = GPSRx_SleepValue;
        //PGSR1 = GPSRy_SleepValue;
        //PGSR2 = GPSRz_SleepValue;
        
        GAFR0_L = GAFR0x_InitValue;
        GAFR0_U = GAFR1x_InitValue;
        GAFR1_L = GAFR0y_InitValue;
        GAFR1_U = GAFR1y_InitValue;
        GAFR2_L = GAFR0z_InitValue;
        GAFR2_U = GAFR1z_InitValue;
        
//	GPCR0 = ~GPSRx_InitValue;
//	GPCR1 = ~GPSRy_InitValue;
//	GPCR2 = ~GPSRz_InitValue;

//	GPDR0 = GPDRx_InitValue;
//	GPDR1 = GPDRy_InitValue;
//	GPDR2 = GPDRz_InitValue;

//	GPSR0 = GPSRx_InitValue;
//	GPSR1 = GPSRy_InitValue;
//	GPSR2 = GPSRz_InitValue;

        PCFR |= PCFR_GPR_EN;

        check_serial_cable();

	return 0;
}

extern void a730_ll_pm_init(void);

static void __init a730_init (void)
{
	a730_hw_init();
	
	platform_add_devices(a730_devices, ARRAY_SIZE(a730_devices));
	
	set_pxa_fb_info(&asus730_fb_info);
//	pxa_set_ohci_info(&a730_ohci_platform_data);
#ifdef CONFIG_PM
	apm_get_power_status = &a730_apm_get_power_status;
	a730_ll_pm_init();
#endif
}

/***********************************************************/
MACHINE_START(A730, "Asus MyPal A730(W)")
	/* "Markus1108wagner@t-online.de" */
//	.phys_ram	= 0xa0000000,
	.phys_io	= 0x40000000,
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
	.boot_params	= 0xa0000100,
	.map_io		= a730_map_io,
	.init_irq	= a730_init_irq,
	.timer		= &pxa_timer,
	.init_machine	= a730_init,
MACHINE_END

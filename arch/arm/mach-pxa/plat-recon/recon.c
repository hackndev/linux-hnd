/*
 * Copyright 2005, SDG Systems, LLC.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/corgi_bl.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/mtd/partitions.h>

#include <asm/mach-types.h>
#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/arch/pxafb.h>
#include <asm/arch/recon.h>
#include <asm/arch/udc.h>
#include <asm/arch/pm.h>
#include <asm/arch/pxa-pm_ll.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/serial.h>

#include <asm/setup.h>  /* For tags */

#include "../generic.h"
#include "recon.h"

static void lcd_power(int power, struct fb_var_screeninfo *si);
static void recon_backlight_power(int power);

static struct pxafb_mode_info recon_lcd_mode __initdata = {
    .pixclock               = 174220,   /* pixel clock period in picoseconds: 174.22 ns */
    .xres                   = 240,      /* LCCR1:PPL + 1 */
    .yres                   = 320,      /* LCCR2:LPP + 1 */
    .bpp                    = 16,
    .hsync_len              = 0x11,     /* LCCR1:HSW + 1 */
    .left_margin            = 1,        /* LCCR1:BLW + 1 */
    .right_margin           = 0x13,     /* LCCR1:ELW + 1 */
    .vsync_len              = 1,        /* LCCR2:VSW + 1 */
    .upper_margin           = 0,        /* LCCR2:BFW + 0  (nice consistency!) */
    .lower_margin           = 0xc,      /* LCCR2:EFW + 0  (nice consistency!) */
    .sync                   = FB_SYNC_HOR_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT,
};

static struct pxafb_mach_info recon_lcd_params __initdata = {
    .modes                  = &recon_lcd_mode,
    .num_modes              = 1,
    .lccr0                  = LCCR0_Act,
#ifndef CONFIG_RECON_X
    .lccr3                  = LCCR3_PCP,
#else
    .lccr3                  = 0,
#endif
    .pxafb_lcd_power        = lcd_power,
    .pxafb_backlight_power  = recon_backlight_power,
};


int recon_pwm1_duty = 0xff;
EXPORT_SYMBOL( recon_pwm1_duty );

/* Backlight/LCD Support funcitons */ /* (= */

static
void recon_set_bl_intensity(int intensity)
{
    PWM_CTRL1 = 3;
    PWM_PERVAL1 = 0x100;
    recon_pwm1_duty = PWM_PWDUTY1 = intensity & 0xff;
}

static
struct corgibl_machinfo recon_bl_machinfo = {
    .max_intensity = 0xFF,
    .default_intensity = 0xFF/4,
    .limit_mask = 0xFF,
    .set_bl_intensity = recon_set_bl_intensity
};

struct platform_device recon_bl = {
    .name = "corgi-bl",
    .dev = {
            .platform_data = &recon_bl_machinfo
    }
};

/*
 * turn off backlight before lcd, so use this routine here in
 * addition to backlight class driver.
 */
static void
recon_backlight_power(int on)
{
    if (!on)
        PWM_PWDUTY1 = 0;
    else
        PWM_PWDUTY1 = recon_pwm1_duty;
}

static void
lcd_power(int on, struct fb_var_screeninfo *si)
{
    if(on) {
        SET_RECON_GPIO( RECON_GPIO_nL3V_ON, 0 );
        mdelay(1);
        SET_RECON_GPIO( RECON_GPIO_5V_ON, 1 );
        mdelay(1);
        SET_RECON_GPIO( RECON_GPIO_L_ON, 1 );
        mdelay(1);
    } else {
        mdelay(50); /* wait for pxafb to shutdown */
        SET_RECON_GPIO( RECON_GPIO_L_ON, 0 );
        mdelay(1);
        SET_RECON_GPIO( RECON_GPIO_5V_ON, 0 );
        mdelay(1);
        SET_RECON_GPIO( RECON_GPIO_nL3V_ON, 1 );
        mdelay(1);
    }
}

/****************************************************/
static char *recon_serial;

static int __init
recon_serial_setup(char *data)
{
    recon_serial = data;
    return 0;
}
__setup("recon_serial=", recon_serial_setup);
/****************************************************/
/*
 * Note that there are list manipulation routines in the Linux kernel, but the
 * documentation about how to use them is so bad (non-existant), I gave up and
 * did my own.
 */

struct suspres {
    struct suspres *next;
    void (*callback)(int);
};

static struct suspres *suspres_head;

static void
recon_pxa_ll_pm_suspend(unsigned long resume_addr)
{
    struct suspres *sp;

    for(sp = suspres_head; sp; sp = sp->next) {
        sp->callback(1);
    }
}

static void
recon_pxa_ll_pm_resume(void)
{
    struct suspres *sp;

    for(sp = suspres_head; sp; sp = sp->next) {
        sp->callback(0);
    }
}

struct pxa_ll_pm_ops recon_ll_pm_ops = {
	.suspend = recon_pxa_ll_pm_suspend,
	.resume  = recon_pxa_ll_pm_resume,
};

void
recon_register_suspres_callback(void (*callback)(int))
{
    struct suspres *sp;

    for(sp = suspres_head; sp && sp->next; sp = sp->next)
        ;
    if(sp) {
        sp->next = kmalloc(sizeof(*sp), GFP_KERNEL);
        sp = sp->next;
    } else {
        sp = suspres_head = kmalloc(sizeof(*sp), GFP_KERNEL);
    }
    sp->next = 0;
    sp->callback = callback;
}
EXPORT_SYMBOL(recon_register_suspres_callback);

void
recon_unregister_suspres_callback(void (*callback)(int))
{
    struct suspres *sp, *prev;

    for(prev = 0, sp = suspres_head; sp; prev = sp, sp = sp->next) {
        if(sp->callback == callback) {
            if(prev) {
                prev->next = sp->next;
            } else {
                suspres_head = sp->next;
            }
            kfree(sp);
            break;
        }
    }
}
EXPORT_SYMBOL(recon_unregister_suspres_callback);

#if 0
static int
recon_pm_enter(suspend_state_t state)
{
    struct suspres *sp;

    /* suspend */
    for(sp = suspres_head; sp; sp = sp->next) {
        sp->callback(1);
    }
    /* actually suspend, you can re-enter suspend by calling this
     * function again (looping)
     */
    (void) pm_enter_orig(state);

    /* resume */
    for(sp = suspres_head; sp; sp = sp->next) {
        sp->callback(0);
    }
    return 0;
}
#endif

#ifdef CONFIG_RECON_X
#   define is_recon_x 1
#else
#   define is_recon_x 0
#endif

/****************************************************/

#ifdef CONFIG_RECON_X
/*
 * Bluetooth - Relies on other loadable modules so make the calls indirectly
 * through pointers. Requires that the recon bluetooth module be loaded before
 * any attempt to use bluetooth (obviously).
 */

static void
recon_bt_configure(int state)
{
    switch(state) {
        case PXA_UART_CFG_PRE_STARTUP:
            SET_RECON_GPIO(RECON_GPIO_BT_OFF, 0);
            mdelay(5);
            break;
        case PXA_UART_CFG_PRE_SHUTDOWN:
            SET_RECON_GPIO(RECON_GPIO_BT_OFF, 1);
            break;
    }
}

static struct platform_pxa_serial_funcs bt_funcs = {
    .configure = recon_bt_configure,
};
#endif

static void __init
recon_map_io( void )
{
    pxa_map_io();
#   ifdef CONFIG_RECON_X
    pxa_set_btuart_info(&bt_funcs);
#   endif
}

static void __init
recon_init_irq( void )
{
    pxa_init_irq();
}

static int bootversion;
static int bootdate;
static int __init
do_bootversion(const struct tag *tag)
{
    bootversion = tag->u.initrd.start;
    bootdate = tag->u.initrd.size;
    return 0;
}
__tagtable(0xadbadbae, do_bootversion);


static int
recon_info_read(char *page, char **start, off_t off, int count, int *eof, void *junk)
{
    int len;
    int MHz;

    switch(CCCR & 0xf00) {
        case 0x200:
            MHz = 400;
            break;
        case 0x100:
            MHz = 200;
            break;
        default:
            MHz = -1;
            break;
    }
    len = 0;
    len += sprintf(page + len, "Manufacturer: TDS\n");
#   ifdef CONFIG_RECON_X
        len += sprintf(page + len, "Device: Recon X\n");
#   else
        len += sprintf(page + len, "Device: Recon\n");
#   endif
    len += sprintf(page + len, "Processor: PXA255\n");
    len += sprintf(page + len, "Speed: %d MHz\n", MHz);
    len += sprintf(page + len, "Serial Number: ");
    if(!recon_serial) len += sprintf(page + len, "?\n");
    else              len += sprintf(page + len, "%s\n", recon_serial);
    len += sprintf(page + len, "SDGBoot Version: %d@%08x\n", bootversion, bootdate);
    return len;
}

#ifdef CONFIG_RECON_X
static struct mtd_partition recon_partition_256M[] = {
    {
        .name   = "kernel",
        .offset = (6 * 64) * 2048,              /* block = 38 */
        .size   = (32 * 64) * 2048,             /* blocks = 986 */
    },
    {
        .name   = "root",
        .offset = (38 * 64) * 2048,             /* block = 38 */
        .size   = (986 * 64) * 2048,            /* blocks = 986 */
    },
    {
        .name   = "home",                       /* 128 MB */
        .offset = (1024 * 64) * 2048,           /* block = 1024 */
        .size   = (1024 * 64) * 2048,           /* blocks = 1024 */
    },
};
static struct mtd_partition recon_partition_128M[] = {
    {
        .name   = "kernel",
        .offset = (6 * 64) * 2048,              /* block = 38 */
        .size   = (32 * 64) * 2048,             /* blocks = 986 */
    },
    {
        .name   = "root",
        .offset = (38 * 64) * 2048,             /* block = 38 */
        .size   = (986 * 64) * 2048,            /* blocks = 986 */
    },
};
static struct mtd_partition recon_partition_64M[] = {
        /* XXX This has not been determined yet. */
    {
        .name   = "kernel",
        .offset = (6 * 64) * 2048,              /* block = 38 */
        .size   = (32 * 64) * 2048,             /* blocks = 986 */
    },
    {
        .name   = "root",
        .offset = (38 * 64) * 2048,             /* block = 38 */
        .size   = (474 * 64) * 2048,            /* blocks = 474 */
    },
};
#else
static struct mtd_partition recon_partition_128M[] = {
    {
        .name   = "root",
        .offset = (288 * 32) * 512,             /* block = 288 */
        .size   = ((3808 + 4096) * 32) * 512,   /* blocks = 3808 + 4096 */
    },
};

static struct mtd_partition recon_partition_64M[] = {
    {
        .name   = "root",
        .offset = (288 * 32) * 512,             /* block = 288 */
        .size   = ((3808 +    0) * 32) * 512,   /* blocks = 3808 */
    },
};
#endif

struct mtd_partition *recon_partition_table;
int recon_partitions;
EXPORT_SYMBOL(recon_partition_table);
EXPORT_SYMBOL(recon_partitions);

#ifdef CONFIG_RECON_X
int
recon_flash_ready(void)
{
    return GET_RECON_GPIO(19)? 1:0;
}
EXPORT_SYMBOL(recon_flash_ready);
#endif


static int boot_flags;
static int __init
dotag(const struct tag *tag)
{
    boot_flags = tag->u.initrd.start;
    printk("Boot Flags: 0x%08x\n", boot_flags);
    return 0;
}
__tagtable(0xadbadbac, dotag);

#ifdef CONFIG_RECON_X
static int nand_size_mb;
static int __init
do_nand_size_tag(const struct tag *tag)
{
    nand_size_mb = tag->u.initrd.start;
    printk("Flash Storage: %d MB\n", nand_size_mb);
    return 0;
}
__tagtable(0xadbadbad, do_nand_size_tag);
#endif

static int
recon_boot_read(char *page, char **start, off_t off, int count, int *eof, void *junk)
{
    int len;
    len = 0;
    len += sprintf(page + len, "0x%08x\n", boot_flags);
    return len;
}

/* Hacks in an attempt to get robustness with IDE/CF removal */
static int cf_presence0;
static int cf_presence1;

int *cf_presence_slot0 = &cf_presence0;
int *cf_presence_slot1 = &cf_presence1;
EXPORT_SYMBOL(cf_presence_slot0);
EXPORT_SYMBOL(cf_presence_slot1);


#ifdef CONFIG_RECON_X
int
recon_x_pcmcia_space(int slot)
{
    switch(slot) {
        case 0:
            return 0x20000000;
            break;
        case 1:
            return 0x30000000;
            break;
        case 2:
            return 0x30000000 | (1 << 25);
            break;
    }
    return 0x20000000;
}
EXPORT_SYMBOL(recon_x_pcmcia_space);
#endif

void
recon_enable_i2s( int enable )
{
    unsigned long flags;
    static int refcount = 0;

    local_irq_save( flags );
    pxa_gpio_mode(GPIO32_SYSCLK_I2S_MD);
    pxa_gpio_mode(GPIO28_BITCLK_OUT_I2S_MD);
    if (enable) {
        if(SADIV == 0) SADIV = 0xd;
        SACR0 |= SACR0_ENB | SACR0_BCKD;
        pxa_set_cken( CKEN8_I2S, 1 );
        ++refcount;
    }
    else {
        if (refcount == 0)
            printk( KERN_ERR "recon_enable_i2s: refcount error!\n" );
        else
            --refcount;
        if (refcount == 0) {
            SACR0 = SACR0_RST;
            pxa_set_cken( CKEN8_I2S, 0 );
        }
    }
    local_irq_restore(flags);
}
EXPORT_SYMBOL(recon_enable_i2s);


/*
 * USB Device Client (UDC)
 */

int usben;
EXPORT_SYMBOL(usben);

static int
usb_is_connected(void)
{
    if(usben && GET_RECON_GPIO(RECON_GPIO_USB_ON))
    /* if(GET_RECON_GPIO(RECON_GPIO_USB_ON)) */
    {
        return 1;
    }
    return 0;
}

static void
usb_enable(int cmd)
{
    switch (cmd) {
        case PXA2XX_UDC_CMD_DISCONNECT:
            printk( KERN_INFO "USB Command Disconnect\n" );
            SET_RECON_GPIO(RECON_GPIO_USB_SS, 0);
            break;

        case PXA2XX_UDC_CMD_CONNECT:
            printk( KERN_INFO "USB Command Connect\n");
            SET_RECON_GPIO(RECON_GPIO_USB_SS, usben?1:0);
            /* SET_RECON_GPIO(RECON_GPIO_USB_SS, 1); */
            break;
    }
}

static struct pxa2xx_udc_mach_info udc_mach_info = {
    .udc_is_connected = usb_is_connected,
    .udc_command      = usb_enable,
};

static struct platform_device recon_keys_device = {
    .name = "recon-keys",
};

#if 0
static struct pm_ops recon_pm_ops = {
	.enter	= recon_pm_enter
};
#endif

static void __init
recon_init( void )
{
    if(is_recon_x) {
        printk( KERN_NOTICE "Booting TDS Recon X.\n" );
    } else {
        printk( KERN_NOTICE "Booting TDS Recon.\n" );
    }

#   ifdef CONFIG_RECON_X
        switch(nand_size_mb) {
            case 256:
                recon_partition_table = recon_partition_256M;
                recon_partitions = sizeof(recon_partition_256M)/sizeof(recon_partition_256M[0]);
                break;
            default:
                printk("WARNING: Unexpected Flash Size of %d MB.  Assuming 128 MB.\n",
                    nand_size_mb);
                /* Fall Through */
            case 128:
                recon_partition_table = recon_partition_128M;
                recon_partitions = sizeof(recon_partition_128M)/sizeof(recon_partition_128M[0]);
                break;
            case 64:
                recon_partition_table = recon_partition_64M;
                recon_partitions = sizeof(recon_partition_64M)/sizeof(recon_partition_64M[0]);
                break;
        }
#   else
        switch(CCCR & 0xf00) {
            case 0x100:
                printk("200 MHz, 64 MB Flash\n");
                recon_partition_table = recon_partition_64M;
                recon_partitions = sizeof(recon_partition_64M)/sizeof(recon_partition_64M[0]);
                break;
            case 0x200:
                printk("400 MHz, 128 MB Flash\n");
                recon_partition_table = recon_partition_128M;
                recon_partitions = sizeof(recon_partition_128M)/sizeof(recon_partition_128M[0]);
                break;
            default:
                printk("Unknown Recon type!!\n");
                break;
        }
#   endif

    PFER = 0;
    PRER = 0;
    set_pxa_fb_info(&recon_lcd_params);
    pxa_set_udc_info(&udc_mach_info);
    create_proc_read_entry("recon", 0, 0, recon_info_read, 0);
    create_proc_read_entry("boot", 0, 0, recon_boot_read, 0);
    if (platform_device_register( &recon_keys_device ) != 0) {
        printk( KERN_WARNING "Unable to register Recon keypad device\n" );
    }
    if (platform_device_register( &recon_bl ) != 0) {
        printk( KERN_WARNING "Unable to register Recon backlight device\n" );
    }

    if(is_recon_x) {
        /* configure bluetooth UART */
        pxa_gpio_mode(RECON_GPIO_BT_RXD);
        pxa_gpio_mode(RECON_GPIO_BT_TXD);
        pxa_gpio_mode(RECON_GPIO_BT_CTS);
        pxa_gpio_mode(RECON_GPIO_BT_RTS);

        /* Set USB GPIO mode */
        pxa_gpio_mode(RECON_GPIO_USB_SS | GPIO_OUT);
        pxa_gpio_mode(RECON_GPIO_USB_ON | GPIO_IN);
    }

#if 0
    /* Override the 'enter' hook. */
    pm_enter_orig = pm_ops->enter;

    recon_pm_ops.pm_disk_mode = pm_ops->pm_disk_mode;
    recon_pm_ops.prepare      = pm_ops->prepare;
    recon_pm_ops.enter	      = recon_pm_enter;
    recon_pm_ops.finish	      = pm_ops->finish;

    pm_set_ops(&recon_pm_ops);
#endif
    pxa_pm_set_ll_ops(&recon_ll_pm_ops);
}

#ifdef CONFIG_RECON_X
    MACHINE_START(RECON, "TDS Recon X")
#else
    MACHINE_START(RECON, "TDS Recon")
#endif
    .phys_io = 0x40000000,
    .io_pg_offst = (io_p2v(0x40000000) >> 18) & 0xfffc,
    .boot_params = 0xa0000100,
    .map_io = recon_map_io,
    .init_irq = recon_init_irq,
    .timer = &pxa_timer,
    .init_machine = recon_init,
MACHINE_END

/* vim600: set sw=4 expandtab : */

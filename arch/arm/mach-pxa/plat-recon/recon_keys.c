/*
 * Recon Key Driver
 *
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
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>

#include <asm/arch/recon.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>
#include <asm/io.h>

#include "recon.h"

#ifdef CONFIG_RECON_X
#   define GBIT(x)  (1u << ((x) - 64))
#   define is_recon_x 1
#else
#   define GBIT(x)  (1u << (x))
#   define is_recon_x 0
#endif

static struct recon_key_dir {
    int col;
    int irq;
    unsigned gpio;
    unsigned dir_off;
} key_dir[] =
{
    { 0, 0, GBIT(RECON_GPIO_KYO0), ~(GBIT(RECON_GPIO_KYO1) | GBIT(RECON_GPIO_KYO2)) },
    { 1, 0, GBIT(RECON_GPIO_KYO1), ~(GBIT(RECON_GPIO_KYO0) | GBIT(RECON_GPIO_KYO2)) },
    { 2, 0, GBIT(RECON_GPIO_KYO2), ~(GBIT(RECON_GPIO_KYO0) | GBIT(RECON_GPIO_KYO1)) },
};

static struct recon_key_info {
    int keycode;
    int key_pressed;
} key_info[] =
#ifdef CONFIG_RECON_X
{   /* order significant! matrix row order! */
    { KEY_F9, 0 },      /* calendar */
    { KEY_HOME, 0 },
    { KEY_ENTER, 0 },
    { KEY_UP, 0 },
    { KEY_LEFT, 0 },
    { KEY_DOWN, 0 },
    { KEY_F10, 0 },     /* contacts */
    { KEY_RIGHT, 0 },
    { KEY_F11, 0 },
    { KEY_POWER, 0 },   /* must be after matrix keys */
    { KEY_RESERVED, 0 },
};
#else
{   /* order significant! matrix row order! */
    { KEY_HOME, 0 },
    { KEY_F9, 0 },      /* calendar */
    { KEY_F10, 0 },     /* contacts */
    { KEY_UP, 0 },
    { KEY_LEFT, 0 },
    { KEY_DOWN, 0 },
    { KEY_ENTER, 0 },
    { KEY_RIGHT, 0 },
    { KEY_F11, 0 },     /* mail */
    { KEY_POWER, 0 },   /* must be after matrix keys */
    { KEY_RESERVED, 0 },
};
#endif

static struct input_dev *keydev;

static int ky0_irq = -1;
static int ky1_irq = -1;
static int ky2_irq = -1;
static int on_irq = -1;

static int pbto = 0;
static int do_esc = 0;

static ssize_t
pbto_read (struct file *file, char __user *buffer, size_t bytes, loff_t *off)
{
    if(bytes && (*off == 0)) {
        int pb;
        pb = snprintf(buffer, bytes, "%d", (do_esc?-1:1) * pbto);
        *off += pb;
        return pb;
    } return 0;
}

static ssize_t
pbto_write(struct file *file, const char __user *buffer, size_t bytes, loff_t *off)
{

    pbto = simple_strtol(buffer, 0, 10);
    if(pbto < 0) {
        do_esc = 1;
        pbto   = -pbto;
    } else         do_esc = 0;
    printk("PBTO now %d/10 sec (do_escape: %d)\n", pbto, do_esc);
    return bytes;
}

static struct file_operations pbto_fops = {
    .owner              = THIS_MODULE,
    .read               = pbto_read,
    .write              = pbto_write,
};

static struct miscdevice pbto_dev = {
    .minor              = MISC_DYNAMIC_MINOR,
    .name               = "pbto",
    .fops               = &pbto_fops,
};

/*
 * Power button time-out:  It requires three states:  Idle, Debounce, and
 * Down.  The for the debounce state, we ignore everything, and then we start
 * looking at the GPIO to see if they change it.  This is implemented by the
 * ISR moving from the Idle state to the Debounce state.  It does so by
 * kicking off a timer which then re-enables the interrupt and changes its
 * polarity if the button is still held.  It also kicks off a timer for the
 * timeout period.  If an interrupt occurs before the timer expires, then we
 * do not suspend.  If the timer expires, we do suspend.
 */

static void power_debounce_timer_func(unsigned long junk);
DEFINE_TIMER(power_debounce_timer, power_debounce_timer_func, 0, 0);

static volatile enum { PB_IDLE, PB_DEBOUNCE, PB_DOWN, PB_DOWN_POWER_OFF } isr_state = PB_IDLE;

static void
power_debounce_timer_func(unsigned long junk)
{
    unsigned long flags;
    local_irq_save(flags);

    switch(isr_state) {
        case PB_IDLE:
            break;
        case PB_DEBOUNCE:
            if(GPLR0 & 1) {
                /* Power is still pressed */
                set_irq_type(RECON_IRQ(RECON_GPIO_ON_KEY), IRQT_FALLING); /* Look for release */
                isr_state = PB_DOWN;
                mod_timer(&power_debounce_timer, jiffies + (pbto * HZ) / 10);
            } else {
                isr_state = PB_IDLE;
            }
            enable_irq(RECON_IRQ(RECON_GPIO_ON_KEY));
            break;
        case PB_DOWN:
            if(GPLR0 & 1) {
                /* Still down, so let's suspend. */
                input_report_key( keydev, KEY_POWER, 1 );
                input_report_key( keydev, KEY_POWER, 0 );
                input_sync( keydev );
                PWM_PWDUTY1 = 0;        /* Turn off the LCD light for feedback */
                isr_state = PB_DOWN_POWER_OFF;
            } else {
                /* It's no longer down, so be safe and don't do power off. */
                isr_state = PB_IDLE;
                set_irq_type(RECON_IRQ(RECON_GPIO_ON_KEY), IRQT_RISING);
            }
            break;
        case PB_DOWN_POWER_OFF:
            printk("State error in pbto.\n");
            isr_state = PB_IDLE;
            break;
    }

    local_irq_restore(flags);
}

static irqreturn_t
power_isr(int irq, void *irq_desc)
{
    switch(isr_state) {
        case PB_IDLE:
            disable_irq(RECON_IRQ(RECON_GPIO_ON_KEY));
            mod_timer(&power_debounce_timer, jiffies + 2);
            isr_state = PB_DEBOUNCE;
            break;
        case PB_DEBOUNCE:
            printk("IRQ in debounce\n");
            break;
        case PB_DOWN:
            del_timer(&power_debounce_timer);
            set_irq_type(RECON_IRQ(RECON_GPIO_ON_KEY), IRQT_RISING);
            isr_state = PB_IDLE;
            /* Key was released. */
            if(do_esc) {
                input_report_key( keydev, KEY_ESC, 1 );
                input_report_key( keydev, KEY_ESC, 0 );
                input_sync( keydev );
            }
            break;
        case PB_DOWN_POWER_OFF:
            /*
             * We use this state to halt the suspend until they release the
             * power.  This allows the hardware reset to kick in.
             */
            isr_state = PB_IDLE;
            set_irq_type(RECON_IRQ(RECON_GPIO_ON_KEY), IRQT_RISING);
            break;
    }

    return IRQ_HANDLED;
}

static void
keys_suspres(int suspend)
{ /* (= */
    if(suspend) {
        if(isr_state == PB_DOWN_POWER_OFF) {
            /* Wait until release */
            while(isr_state != PB_IDLE)
                ;
        }
    }
}


static struct timer_list keypress_timer;

/*
 * matrix keypad requires that during key scan only one column
 * may be driven high at a time. Turn the others off
 */

static int
key_scan( struct recon_key_dir *kdir )
{
    unsigned keys;
    int j;
    int detect = 0;
    int gpdr;


    if(is_recon_x) {
        gpdr = GPDR2;
    } else {
        gpdr = GPDR0;
    }

    /*
     * Note: this code assumes that no one else is messing with the GPRD0
     * register. XXX
     */
    if(is_recon_x) {
        GPSR2 = kdir->gpio;
        GPDR2 = kdir->gpio | (GPDR2 & kdir->dir_off);
    } else {
        GPSR0 = kdir->gpio;
        GPDR0 = kdir->gpio | (GPDR0 & kdir->dir_off);
    }
    udelay(3);
#   ifdef CONFIG_RECON_X
        keys = (GPLR2 &
                (GBIT(RECON_GPIO_KYI0) | GBIT(RECON_GPIO_KYI1)
                    | GBIT(RECON_GPIO_KYI2))) >> (RECON_GPIO_KYI0 - 64);
#   else
        keys = (GPLR0 &
                (GBIT(RECON_GPIO_KYI0) | GBIT(RECON_GPIO_KYI1)
                    | GBIT(RECON_GPIO_KYI2))) >> RECON_GPIO_KYI0;
#   endif
    for (j = 0; j < 3; j++) {
        int k = (kdir->col * 3) + j;
        if (keys & (1 << j)) {
            if (key_info[k].key_pressed != 1) {
                key_info[k].key_pressed = 1;
                input_report_key( keydev, key_info[k].keycode, 1 );
            }
            detect++;
        } else {
            if (key_info[k].key_pressed == 1) {
                key_info[k].key_pressed = 0;
                input_report_key( keydev, key_info[k].keycode, 0 );
            }
        }
    }
    if(is_recon_x) {
        GPDR2 = gpdr;     /* put all keys back to inputs */
    } else {
        GPDR0 = gpdr;     /* put all keys back to inputs */
    }
    return detect;
}

static void
ts_keypress_callback(unsigned long data)
{
    int k;
    int detect;

    for(detect = 0, k = 0; k < sizeof(key_dir)/sizeof(key_dir[0]); k++) {
        if(key_scan(&key_dir[k])) {
            detect++;
        }
    }
    input_sync( keydev );
    if(detect) {
        mod_timer(&keypress_timer, jiffies + 1);
    } else {
        set_irq_type( ky0_irq, IRQT_BOTHEDGE );
        set_irq_type( ky1_irq, IRQT_BOTHEDGE );
        set_irq_type( ky2_irq, IRQT_BOTHEDGE );
    }
}


static irqreturn_t
key_isr(int irq, void *irq_desc)
{
    set_irq_type( irq, IRQT_NOEDGE );
    mod_timer(&keypress_timer, jiffies + 1);
    return IRQ_HANDLED;
}

static int
recon_keys_probe( struct device *dev )
{
    int i;
    int retval;

    keypress_timer.function = ts_keypress_callback;
    keypress_timer.data = 0;
    init_timer (&keypress_timer);

    retval = 0;

    printk( KERN_INFO "TDS Recon Keypad Driver\n" );
    /* make sure all keys start as inputs, incl. power */
    pxa_gpio_mode(RECON_GPIO_KYI0 | GPIO_IN);
    pxa_gpio_mode(RECON_GPIO_KYI1 | GPIO_IN);
    pxa_gpio_mode(RECON_GPIO_KYI2 | GPIO_IN);
    pxa_gpio_mode(RECON_GPIO_KYO0 | GPIO_IN);
    pxa_gpio_mode(RECON_GPIO_KYO1 | GPIO_IN);
    pxa_gpio_mode(RECON_GPIO_KYO2 | GPIO_IN);
    pxa_gpio_mode(RECON_GPIO_KYO3 | GPIO_IN);

    keydev = input_allocate_device();
    if(!keydev) {
        return -ENOMEM;
    }
    keydev->name = "recon_keys";
    keydev->evbit[0] = BIT(EV_KEY);
    for (i=0; key_info[i].keycode != KEY_RESERVED; i++) {
        keydev->keybit[LONG(key_info[i].keycode)] |= BIT(key_info[i].keycode);
    }
    keydev->keybit[LONG(KEY_ESC)] |= BIT(KEY_ESC);
    input_register_device( keydev );

    on_irq = RECON_IRQ( RECON_GPIO_ON_KEY );
    set_irq_type( on_irq, IRQT_RISING );
    if (request_irq( on_irq, power_isr, SA_INTERRUPT, "Recon Power", NULL ) != 0) {
        printk( KERN_NOTICE "Unable to request POWER interrupt.\n" );
        retval = -ENODEV;
    }

    key_dir[0].irq = ky0_irq = RECON_IRQ( RECON_GPIO_KYO0 );
    set_irq_type( ky0_irq, IRQT_BOTHEDGE );
    if (request_irq( ky0_irq, key_isr, SA_INTERRUPT, "Recon Key0", 0 ) != 0) {
        printk( KERN_NOTICE "Unable to request Key0 interrupt.\n" );
        retval = -ENODEV;
    }

    key_dir[1].irq = ky1_irq = RECON_IRQ( RECON_GPIO_KYO1 );
    set_irq_type( ky1_irq, IRQT_BOTHEDGE );
    if (request_irq( ky1_irq, key_isr, SA_INTERRUPT, "Recon Key1", 0 ) != 0) {
        printk( KERN_NOTICE "Unable to request Key1 interrupt.\n" );
        retval = -ENODEV;
    }

    key_dir[2].irq = ky2_irq = RECON_IRQ( RECON_GPIO_KYO2 );
    set_irq_type( ky2_irq, IRQT_BOTHEDGE );
    if (request_irq( ky2_irq, key_isr, SA_INTERRUPT, "Recon Key2", 0 ) != 0) {
        printk( KERN_NOTICE "Unable to request Key2 interrupt.\n" );
        retval = -ENODEV;
    }

    if(misc_register(&pbto_dev)) {
        printk("Could not register pbto device.\n");
        /* It's not fatal. */
    }

    recon_register_suspres_callback(keys_suspres);


    return retval;
}

static int
recon_keys_remove( struct device *dev )
{
    recon_unregister_suspres_callback(keys_suspres);
    del_timer(&keypress_timer);
    free_irq( on_irq, NULL );
    free_irq( ky0_irq, 0 );
    free_irq( ky1_irq, 0 );
    free_irq( ky2_irq, 0 );
    input_unregister_device( keydev );
    misc_deregister(&pbto_dev);
    del_timer_sync(&power_debounce_timer);
    return 0;
}

static struct device_driver recon_keys_driver = {
    .name       = "recon-keys",
    .bus        = &platform_bus_type,
    .probe      = recon_keys_probe,
    .remove     = recon_keys_remove,
};

static int __init
recon_keys_init( void )
{
    return driver_register( &recon_keys_driver );
}

static void __exit
recon_keys_exit( void )
{
    driver_unregister( &recon_keys_driver );
}

module_init( recon_keys_init );
module_exit( recon_keys_exit );

MODULE_AUTHOR( "Todd Blumer, SDG Systems, LLC" );
MODULE_DESCRIPTION( "Keypad driver for TDS Recon" );
MODULE_LICENSE( "GPL" );

/* vim600: set expandtab sw=4 ts=8 :*/


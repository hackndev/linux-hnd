/*
 * Copyright 2005, SDG Systems, LLC
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Sat 11 Jun 2005   Aric D. Blumer
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/mfd/asic3_base.h>
#include <linux/delay.h>
#include <linux/battery.h>
#include <linux/leds.h>

#include <asm/io.h>
#include <asm/apm.h>

#include <asm/hardware/ipaq-asic3.h>
#include <asm/hardware/asic3_leds.h>

#include <asm/arch/hx4700-gpio.h>
#include <asm/arch/hx4700-asic.h>
#include <asm/arch/hx4700-core.h>

#define DRIVER_NAME "hx4700_power"
static char driver_name[] = DRIVER_NAME;

static void w1_init(void);
static void w1_deinit(void);

static enum {
    POWER_NONE,
    POWER_AC,
    POWER_USB,
} power_status;

static int cdiv = 0x2;
static int net = 0x0;

module_param(cdiv, int, 0444);
MODULE_PARM_DESC(cdiv, "Clock Divisor");
module_param(net, int, 0444);
MODULE_PARM_DESC(net, "Use net address");

struct w1_data {
    volatile unsigned short __iomem *base;
    int w1_irq;
    wait_queue_head_t irqwait;
    unsigned short data_register;
    unsigned short irqstatus;
    int battery_class;
    struct timer_list irqtimer;

    unsigned int temp;
    unsigned int voltage;
    int Current;
    int current_accum;
    int acr_reset;
    int minutes;
    int battery_life;

    int ac_irq;
    int usb_irq;

    int initialized;

    char net_address[8];

    u64 jiffies_64;

} module_data;

#define DS1WM_COMMAND   0
#define DS1WM_TXRX      1
#define DS1WM_IRQ       2
#define DS1WM_IRQ_EN    3
#define DS1WM_CLOCK_DIV 4

#define WRITE_REGISTER(r, value) do {                           \
        /* printk("Write %p = 0x%04x\n", (module_data.base + (r)), value); */ \
        ((*(module_data.base + (r))) = (value));                \
    } while(0)

#define READ_REGISTER(R, r)  do {                               \
        (R = *(module_data.base + (r)));                        \
        /* printk("Read %p = 0x%04x\n", (module_data.base + (r)), R); */ \
    } while(0)

static int w1_detect(void);
static int w1_write_byte(unsigned short data);
static int w1_read_byte(unsigned int *data);
static void w1_send_net_address(void);

DECLARE_MUTEX(update_mutex);

static void
update_data(int force)
{
    int flag = 0;
    int retries = 30;

    /* Only allow an update every second. */
    if(!force && (module_data.jiffies_64 + 5 * HZ > jiffies_64)) {
        return;
    }

    if(down_trylock(&update_mutex)) {
        return;
    }

try_again:
    if(w1_detect()) {
        unsigned int readval;

        w1_send_net_address();
        w1_write_byte(0x69); /* Read */
        w1_write_byte(0x00); /* Starting at offset 0x0 */
        w1_read_byte(&readval);                     /* 0x0 */
        if(readval == 0xff) {
            /*
             * We have an unknown read problem. Try again.
             */
            msleep(1 * retries);
            if(0 == retries--) {
                // printk("hx4700_power: Whoops\n");
                goto exit_please;
            }
            goto try_again;
        }

        /* Get rid of stuff we don't want */
        w1_read_byte(&readval);                     /* 0x1 */
        w1_read_byte(&readval);                     /* 0x2 */
        w1_read_byte(&readval);                     /* 0x3 */
        w1_read_byte(&readval);                     /* 0x4 */
        w1_read_byte(&readval);                     /* 0x5 */
        w1_read_byte(&readval);                     /* 0x6 */
        w1_read_byte(&readval);                     /* 0x7 */
        w1_read_byte(&readval);                     /* 0x8 */
        w1_read_byte(&readval);                     /* 0x9 */
        w1_read_byte(&readval);                     /* 0xa */
        w1_read_byte(&readval);                     /* 0xb */

        /* VOLTAGE */
        w1_read_byte(&readval);                     /* 0xc */
        if(readval == 0xff) {
            /*
             * We have an unknown read problem. Try again.
             */
            msleep(1 * retries);
            if(0 == retries--) {
                // printk("hx4700_power: Whoops2\n");
                goto exit_please;
            }
            goto try_again;
        }
        module_data.voltage = readval << 8;
        w1_read_byte(&readval);                     /* 0xd */
        if(readval == 0xff) {
            /*
             * We have an unknown read problem. Try again.
             */
            msleep(1 * retries);
            if(0 == retries--) {
                // printk("hx4700_power: Whoops2\n");
                goto exit_please;
            }
            goto try_again;
        }
        module_data.voltage |= readval;
        module_data.voltage >>= 5;
        /* 4997 / 1024 is almost equal to 4880 / 1000 */
        module_data.voltage *= 4997;
        module_data.voltage /= 1024;
        /****************/

        /* Current */
        w1_read_byte(&readval);                     /* 0xe */
        if(readval == 0xff) {
            /*
             * We have an unknown read problem. Try again.
             */
            msleep(1 * retries);
            if(0 == retries--) {
                // printk("hx4700_power: Whoops2\n");
                goto exit_please;
            }
            goto try_again;
        }
        if(readval & (1 << 7)) {
            /* The sign bit is set */
            module_data.Current = -1 ^ 0x7fff;
        } else {
            module_data.Current = 0;
        }
        module_data.Current |= readval << 8;
        w1_read_byte(&readval);                     /* 0xf */
        if(readval == 0xff) {
            /*
             * We have an unknown read problem. Try again.
             */
            msleep(1 * retries);
            if(0 == retries--) {
                // printk("hx4700_power: Whoops2\n");
                goto exit_please;
            }
            goto try_again;
        }
        module_data.Current |= readval;
        module_data.Current >>= 3;
        /* 655360 / 1024*1024 is equal to 6250000 / 1000000 */
        module_data.Current *= 655360;
        module_data.Current /= 1024 * 1024;
        /****************/

        /* Current Accumulator */
        w1_read_byte(&readval);                     /* 0x10 */
        if(readval == 0xff) {
            /*
             * We have an unknown read problem. Try again.
             */
            msleep(1 * retries);
            if(0 == retries--) {
                // printk("hx4700_power: Whoops2\n");
                goto exit_please;
            }
            goto try_again;
        }
        if(readval & (1 << 7)) {
            /* The sign bit is set */
            module_data.current_accum = -1 ^ 0xffff;
        } else {
            module_data.current_accum = 0;
        }
        module_data.current_accum |= readval << 8;
        w1_read_byte(&readval);                     /* 0x11 */
        if(readval == 0xff) {
            /*
             * We have an unknown read problem. Try again.
             */
            msleep(1 * retries);
            if(0 == retries--) {
                // printk("hx4700_power: Whoops2\n");
                goto exit_please;
            }
            goto try_again;
        }
        module_data.current_accum |= readval;
        /* Units are 250 uAh. We wan't mAh. So each unit is 1/4 mAh.  But
            * we want fractions of hours, so let's keep the 1/4 mAh units. */
        /* Now convert to quarter hours by dividing by mA. */
        if(module_data.Current == 0) {
            module_data.minutes = module_data.current_accum * 15;
        } else {
            module_data.minutes = -((module_data.current_accum * 15) / module_data.Current);
        }
        /****************/

        /* Get rid of stuff we don't want */
        w1_read_byte(&readval);                     /* 0x12 */
        w1_read_byte(&readval);                     /* 0x13 */
        w1_read_byte(&readval);                     /* 0x14 */
        w1_read_byte(&readval);                     /* 0x15 */
        w1_read_byte(&readval);                     /* 0x16 */
        w1_read_byte(&readval);                     /* 0x17 */

        /* Temperature */
        w1_read_byte(&readval);                     /* 0x18 */
        if(readval == 0xff) {
            /*
             * We have an unknown read problem. Try again.
             */
            msleep(1 * retries);
            if(0 == retries--) {
                // printk("hx4700_power: Whoops2\n");
                goto exit_please;
            }
            goto try_again;
        }
        module_data.temp = readval << 8;
        w1_read_byte(&readval);                     /* 0x19 */
        if(readval == 0xff) {
            /*
             * We have an unknown read problem. Try again.
             */
            msleep(1 * retries);
            if(0 == retries--) {
                // printk("hx4700_power: Whoops2\n");
                goto exit_please;
            }
            goto try_again;
        }
        module_data.temp |= readval;
        module_data.temp >>= 5;
        module_data.temp += module_data.temp / 4;
        /****************/

        if(module_data.voltage > 8000) {
            /* There is an unknown problem where things get into a strange
             * state.  We know it because the voltage is reported as too
             * high.  This is an attempt at robustness to correct the problem.
             * I'd like to fix it permanently, though.
             */
            if(!flag) {
                printk("%s: Bad state detected. Retrying.\n", driver_name);
                flag = 1;
                mdelay(1);
                goto try_again;
            } else {
                printk("w1 fix didn't work. We give up.\n");
            }
        }

        /* Reset ACR when battery gets full. */
        if (module_data.Current >= 0 && module_data.Current < 32 &&
            !module_data.acr_reset) {
                printk(KERN_INFO "ACR reset\n");
                module_data.current_accum = 1800 * 4;
                if (w1_detect()) {
                        w1_send_net_address();
                        w1_write_byte(0x6c); /* write */
                        w1_write_byte(0x10); /* current accum msb */
                        w1_write_byte((module_data.current_accum >> 8) & 0xff);
                        w1_write_byte(module_data.current_accum & 0xff);
                        module_data.acr_reset = 1;
                }
        }

        module_data.battery_life   = (module_data.current_accum * 25) / 1800;
        if(module_data.battery_life > 100) module_data.battery_life = 100;
        if(module_data.battery_life < 0) module_data.battery_life = 0;
    } else {
        printk("%s: Device not detected\n", driver_name);
        module_data.temp          = 0;
        module_data.voltage       = 0;
        module_data.current_accum = 0;
        module_data.Current       = 0;
    }

exit_please:
    module_data.jiffies_64 = jiffies_64;
    up(&update_mutex);
}


static irqreturn_t
w1_isr(int irq, void *opaque)
{
    /*
     * Note that the w1 control logic is quite slow, running at 1 MHz.  It is
     * possible to read the IRQ status, and return and the interrupt still be
     * active.  In this case, we're OK because we only update the irqstatus
     * when an actual interrupt causing bit is set.
     */

    unsigned short tmp;
    /* Just reading the register clears the source */
    READ_REGISTER(tmp, DS1WM_IRQ);
    if(    tmp & (1 << 0)  /* Presence Detect */
        || tmp & (1 << 4)  /* Receive Buffer Full */
    ) {
        int junk, count;
        module_data.irqstatus = tmp;
        for(count = 0; count < 10; count++) {
            READ_REGISTER(module_data.data_register, DS1WM_TXRX);
            READ_REGISTER(junk, DS1WM_CLOCK_DIV); /* Delay */
            READ_REGISTER(tmp, DS1WM_IRQ);
            if(!(tmp & (1 << 4))) break;
        }
        wake_up_interruptible(&module_data.irqwait);
    }
    return IRQ_HANDLED;
}


static void
w1_init(void)
{
    /* Turn on external clocks and the OWM clock */
    asic3_set_clock_cdex(&hx4700_asic3.dev,
        CLOCK_CDEX_EX0 | CLOCK_CDEX_EX1 | CLOCK_CDEX_OWM,
        CLOCK_CDEX_EX0 | CLOCK_CDEX_EX1 | CLOCK_CDEX_OWM);

    mdelay(1);

    asic3_set_extcf_reset(&hx4700_asic3.dev, ASIC3_EXTCF_OWM_RESET, ASIC3_EXTCF_OWM_RESET);
    mdelay(1);
    asic3_set_extcf_reset(&hx4700_asic3.dev, ASIC3_EXTCF_OWM_RESET, 0);
    mdelay(1);

    /* Clear OWM_SMB, set OWM_EN */
    asic3_set_extcf_select(&hx4700_asic3.dev,
        ASIC3_EXTCF_OWM_SMB | ASIC3_EXTCF_OWM_EN,
        0                   | ASIC3_EXTCF_OWM_EN);

    mdelay(1);

    WRITE_REGISTER(DS1WM_CLOCK_DIV, cdiv);
    /* Enable interrupts: RBF, PD */
    /* Set Interrupt Active High (IAS = 1) */
    WRITE_REGISTER(DS1WM_IRQ_EN,
          (0 << 6)      /* ENBSY */
        | (0 << 5)      /* ESINT */
        | (1 << 4)      /* ERBF */
        | (0 << 3)      /* ETMT */
        | (0 << 2)      /* ETBE */
        | (1 << 1)      /* IAS  0 = active low interrupt, 1 = active high */
        | (1 << 0)      /* EPD */
        );

    mdelay(1);

    /* Set asic3 interrupt status to zero.  Hmmmmm. not accesible in
     * asic3_base.c */
}

static int
w1_detect(void)
{
    int retry_count = 5;

try_again:
    module_data.irqstatus = 0;

    /* Set 1WR (one wire reset) bit */
    WRITE_REGISTER(DS1WM_COMMAND, 1);
    if(wait_event_interruptible_timeout(module_data.irqwait,
                module_data.irqstatus & (1 << 0) /* PD set? */, HZ)) {

        if(0 == (module_data.irqstatus & (1 << 1)) /* PDR set? */) {
            mdelay(1);
            return 1;
        }
    } else {
        printk("%s: detect timeout\n", driver_name);
    }
    if(retry_count--) {
        mdelay(1);
        goto try_again;
    }
    return 0;
}

static int
w1_read_byte(unsigned int *data)
{
    module_data.irqstatus = 0;
    WRITE_REGISTER(DS1WM_TXRX, 0xff);
    if(wait_event_interruptible_timeout(module_data.irqwait,
                module_data.irqstatus & (1 << 4) /* RBF set? */, HZ)) {
        *data = module_data.data_register;
        return 1;
    } else {
        printk("%s: RBF not set\n", driver_name);
    }
    return 0;
}

static int
w1_write_byte(unsigned short data)
{
    module_data.irqstatus = 0;
    WRITE_REGISTER(DS1WM_TXRX, data);
    if(wait_event_interruptible_timeout(module_data.irqwait,
                module_data.irqstatus & (1 << 2) /* TBE set? */, HZ)) {
        return 1;
    } else {
        printk("%s: TBE not set\n", driver_name);
    }
    return 0;
}

int get_min_voltage(struct battery *b)
{
    return 0;
}
int get_min_current(struct battery *b)
{
    return -1900; /* negative 1900 mA */
}
int get_min_charge(struct battery *b)
{
    return 0;
}
int get_max_voltage(struct battery *b)
{
    return 4750; /* mV */
}
int get_max_current(struct battery *b)
{
    return 1900; /* positive 1900 mA */
}
int get_max_charge(struct battery *b)
{
    return 1;
}
int get_temp(struct battery *b)
{
    update_data(0);
    return module_data.temp;
}
int get_voltage(struct battery *b)
{
    update_data(0);
    return module_data.voltage;
}
int get_Current(struct battery *b)
{
    update_data(0);
    return module_data.Current;
}
int get_charge(struct battery *b)
{
    return module_data.current_accum / 4;
}
int get_status(struct battery *b)
{
    return power_status == POWER_NONE? 0: 1;
}

static struct battery hx4700_power = {
    .name               = "hx4700_primary",
    .id                 = "main",
    .get_min_voltage    = get_min_voltage,
    .get_min_current    = get_min_current,
    .get_min_charge     = get_min_charge,
    .get_max_voltage    = get_max_voltage,
    .get_max_current    = get_max_current,
    .get_max_charge     = get_max_charge,
    .get_temp           = get_temp,
    .get_voltage        = get_voltage,
    .get_current        = get_Current,
    .get_charge         = get_charge,
    .get_status         = get_status,
};

DEFINE_LED_TRIGGER(charging_trig);
DEFINE_LED_TRIGGER(chargefull_trig);

static void
set_leds(int status, int Current, int battery_life)
{
    if (status == APM_AC_ONLINE) {
        /* check life to update LEDs. LEDs are off when on battery */
        /*
         * It has been observed that the Current is greater when the device is
         * suspended compared to when it is awake.  So we have to use
         * different parameters here compared to bootldr
         */
        if((Current < 32) && (battery_life == 100)) {
            /* Green LED on solid, amber off */
            led_trigger_event(chargefull_trig, LED_FULL);
            led_trigger_event(charging_trig, LED_OFF);
        } else {
            /* Amber LED blinking, green off */
            led_trigger_event(chargefull_trig, LED_OFF);
            led_trigger_event(charging_trig, LED_FULL);
        }
    } else {
        /* No charging power is applied; both LEDs off */
        led_trigger_event(chargefull_trig, LED_OFF);
        led_trigger_event(charging_trig, LED_OFF);
    }
}


static void
hx4700_apm_get_power_status( struct apm_power_info *info )
{
    update_data(0);

    info->ac_line_status = (power_status == POWER_NONE)
        ? APM_AC_OFFLINE : APM_AC_ONLINE;

    /* It is possible to be hooked up to USB and getting some power from
     * it, but still having a current drain on the battery with a busy CPU
     */
    info->battery_life   = module_data.battery_life;

    if (info->ac_line_status == APM_AC_ONLINE) {
        info->battery_status = APM_BATTERY_STATUS_CHARGING;
    } else {
        info->time           = module_data.minutes;
        info->units          = APM_UNITS_MINS;
        module_data.acr_reset = 0;

        info->battery_status = APM_BATTERY_STATUS_CRITICAL;
        if(info->battery_life > 5) {
            info->battery_status = APM_BATTERY_STATUS_LOW;
        }
        if(info->battery_life > 20) {
            info->battery_status = APM_BATTERY_STATUS_HIGH;
        }
    }

    set_leds(info->ac_line_status, module_data.Current, module_data.battery_life);
}

static void
power_change_task_handler(unsigned long enableirq)
{
    unsigned int retval;
    int ac_in, usb_in;

    if(!module_data.initialized) return;

    retval = asic3_get_gpio_status_d( &hx4700_asic3.dev );
    ac_in  = (retval & (1<<GPIOD_AC_IN_N)) == 0;
    usb_in = (retval & (1<<GPIOD_USBC_DETECT_N)) == 0;
    printk( KERN_INFO "hx4700 power_change: ac_in=%d\n", ac_in );
    printk( KERN_INFO "hx4700 power_change: usb_in=%d\n", usb_in );

    if (usb_in) {
        set_irq_type( module_data.usb_irq, IRQT_RISING );
    } else {
        set_irq_type( module_data.usb_irq, IRQT_FALLING );
    }

    if (ac_in) {
        set_irq_type( module_data.ac_irq, IRQT_RISING );
    } else {
        set_irq_type( module_data.ac_irq, IRQT_FALLING );
    }

    if (ac_in) {
        /* If we're on AC, it doesn't matter if we're on USB or not, use AC
         * only */
        SET_HX4700_GPIO_N( CHARGE_EN, 1 );
        SET_HX4700_GPIO( USB_CHARGE_RATE, 0 );
        power_status = POWER_AC;
        set_leds(APM_AC_ONLINE, module_data.Current, module_data.battery_life);
    } else if (usb_in) {
        /* We're not on AC, but we are on USB, so charge with that */
        SET_HX4700_GPIO( USB_CHARGE_RATE, 1 );
        SET_HX4700_GPIO_N( CHARGE_EN, 1 );
        power_status = POWER_USB;
        set_leds(APM_AC_ONLINE, module_data.Current, module_data.battery_life);
    } else {
        /* We're not on AC or USB, don't charge */
        SET_HX4700_GPIO_N( CHARGE_EN, 0 );
        SET_HX4700_GPIO( USB_CHARGE_RATE, 0 );
        power_status = POWER_NONE;
        set_leds(APM_AC_OFFLINE, module_data.Current, module_data.battery_life);
        module_data.acr_reset = 0;
    }


    /* update_data(1); */
    module_data.jiffies_64 = 0; /* Force a re-read on next try */

    if(enableirq) {
        enable_irq(module_data.usb_irq);
        enable_irq(module_data.ac_irq);
    }
}

static int
attach_isr(int irq, void *dev_id)
{
if(irq != module_data.usb_irq
    && irq != module_data.ac_irq) {
    printk("Bad irq: %d, not %d or %d\n", irq, module_data.usb_irq, module_data.ac_irq);
}
    if(module_data.initialized) {
        SET_HX4700_GPIO_N( CHARGE_EN, 0 );
        mod_timer(&module_data.irqtimer, jiffies + HZ/10);
        disable_irq(module_data.usb_irq);
        disable_irq(module_data.ac_irq);
    }
    return IRQ_HANDLED;
}

static void
w1_send_net_address(void)
{
    if(net) {
        int i;
        w1_write_byte(0x55); /* Match Net Address */
        for(i = 0; i < sizeof(module_data.net_address)/sizeof(module_data.net_address[0]); i++) {
            w1_write_byte(module_data.net_address[i]);
        }
    } else {
        w1_write_byte(0xcc); /* Skip Net Address */
    }
}

static int
w1_probe(struct platform_device *pdev)
{
    int retval;

    led_trigger_register_simple("hx4700-chargefull", &chargefull_trig);
    led_trigger_register_hwtimer("hx4700-charging", &charging_trig);
    led_trigger_event(chargefull_trig, LED_OFF);
    led_trigger_event(charging_trig, LED_OFF);

    module_data.initialized = 0;
    module_data.w1_irq      = -1;
    module_data.usb_irq     = -1;
    module_data.ac_irq      = -1;
    module_data.base        = 0;
    module_data.jiffies_64  = 0;
    module_data.acr_reset   = 0;

    init_timer(&module_data.irqtimer);
    module_data.irqtimer.function = power_change_task_handler;
    module_data.irqtimer.data     = 1;

    init_waitqueue_head(&module_data.irqwait);

    platform_set_drvdata(pdev, &module_data);

    module_data.base = ioremap_nocache(0x0c000600, 64);
    module_data.battery_class = 0;

    if(!module_data.base) {
        printk(KERN_NOTICE "%s: Unable to map device\n", driver_name);
        return -ENODEV;
    }


    module_data.w1_irq = asic3_irq_base(&hx4700_asic3.dev) + ASIC3_OWM_IRQ;
    retval = request_irq(module_data.w1_irq, w1_isr, SA_INTERRUPT, driver_name, &module_data);
    if(retval) {
        printk(KERN_NOTICE "%s: Unable to get interrupt %d: %d\n",
            driver_name, module_data.w1_irq, retval);
        iounmap((void __iomem *)module_data.base);
        module_data.base = 0;
        module_data.w1_irq = -1;
        return -ENODEV;
    }

    w1_init();
    if(w1_detect()) {
        unsigned int readval;
        int i;
        w1_write_byte(0x33);
        for(i = 0; i < sizeof(module_data.net_address)/sizeof(module_data.net_address[0]); i++) {
            w1_read_byte(&readval);
            module_data.net_address[i] = readval & 0xff;
        }
        if(module_data.net_address[0] != 0x30) {
            printk("Looks like wrong net address is "
                "%02x %02x %02x %02x %02x %02x %02x %02x\n",
                module_data.net_address[0], module_data.net_address[1],
                module_data.net_address[2], module_data.net_address[3],
                module_data.net_address[4], module_data.net_address[5],
                module_data.net_address[6], module_data.net_address[7]
                );
        }
    } else {
        printk("Could not detect device for net address\n");
        iounmap((void __iomem *)module_data.base);
        free_irq(module_data.w1_irq, &module_data);
        module_data.base = 0;
        module_data.w1_irq = -1;
        return -ENODEV;
    }
    if(w1_detect()) {
        unsigned int readval;
        if(battery_class_register(&hx4700_power)) {
            printk(KERN_ERR "%s: Could not register battery class\n",
                driver_name);
        } else {
            module_data.battery_class = 1;
        }
        w1_send_net_address();
        w1_write_byte(0x69); /* Read */
        w1_write_byte(0x08); /* Special Feature Address */
        w1_read_byte(&readval);
        if(!(readval & (1 << 7))) {
            printk("PS is low. Writing 1.\n");
            /* The PS signal is low. The docs say to write a 1 to it to ensure
             * proper operation. */
            if(w1_detect()) {
                w1_send_net_address();
                w1_write_byte(0x6c); /* Write */
                w1_write_byte(0x08); /* Special Feature Address */
                w1_write_byte(readval | (1 << 7)); /* Data */
            } else {
                printk("No detect when writing PS\n");
            }
        }
    } else {
        printk("%s: Device not detected on init\n", driver_name);
        return -ENODEV;
    }

    module_data.usb_irq = asic3_irq_base( &hx4700_asic3.dev ) + ASIC3_GPIOD_IRQ_BASE
        + GPIOD_USBC_DETECT_N;
    module_data.ac_irq = asic3_irq_base( &hx4700_asic3.dev ) + ASIC3_GPIOD_IRQ_BASE
        + GPIOD_AC_IN_N;
    module_data.initialized = 1;

    /* Get first power state */
    power_change_task_handler(0);

    /* USB IRQ */
    if (request_irq( module_data.usb_irq, attach_isr, SA_INTERRUPT,
            "Hx4700 USB Detect", NULL ) != 0) {
        printk( KERN_ERR "Unable to configure USB detect interrupt.\n" );
        module_data.usb_irq = -1;
    }

    /* AC IRQ */
    if (request_irq( module_data.ac_irq, attach_isr, SA_INTERRUPT,
            "Hx4700 AC Detect", NULL ) != 0) {
        printk( KERN_ERR "Unable to configure AC detect interrupt.\n" );
        module_data.ac_irq = -1;
    }

#ifdef CONFIG_PM
    apm_get_power_status = hx4700_apm_get_power_status;
#endif

    return 0;
}

static void
w1_deinit(void)
{
    /* Turn off OWM clock.  I hate to touch the others because they might be
     * used by other devices like MMC. */
    asic3_set_clock_cdex(&hx4700_asic3.dev,
        /* CLOCK_CDEX_EX0 | CLOCK_CDEX_EX1 | */ CLOCK_CDEX_OWM,
        /* CLOCK_CDEX_EX0 | CLOCK_CDEX_EX1 | */ 0);

    /* Clear OWM_EN */
    asic3_set_extcf_select(&hx4700_asic3.dev,
        ASIC3_EXTCF_OWM_EN, 0);

    mdelay(1);
}

static int
w1_remove(struct platform_device *pdev)
{
    printk("w1_remove\n");

    led_trigger_unregister_simple(chargefull_trig);
    led_trigger_unregister_hwtimer(charging_trig);

#ifdef CONFIG_PM
    apm_get_power_status = NULL;
#endif

    if(module_data.base) {
        w1_deinit();
    }
    if(module_data.battery_class)  {
        battery_class_unregister(&hx4700_power);
        printk("battery class unregistered\n");
    }
    if(module_data.base) {
        iounmap((void __iomem *)module_data.base);
        printk("Base unmapped\n");
    }
    if(module_data.w1_irq != -1)   {
        disable_irq(module_data.w1_irq);
        free_irq(module_data.w1_irq, &module_data);
        printk("w1 irq freed\n");
    }
    if (module_data.ac_irq  != -1) {
        disable_irq(module_data.ac_irq);
        free_irq( module_data.ac_irq, NULL );
        printk("ac irq freed\n");
    }
    if (module_data.usb_irq != -1) {
        disable_irq(module_data.usb_irq);
        free_irq( module_data.usb_irq, NULL );
        printk("usb irq freed\n");
    }

    while(timer_pending(&module_data.irqtimer)) {
        msleep(100);
    }
    del_timer(&module_data.irqtimer);

    return 0;
}

static int
w1_suspend(struct platform_device *pdev, pm_message_t state)
{
    // printk("w1_suspend\n");
    w1_deinit();
    return 0;
}

static int
w1_resume(struct platform_device *pdev)
{
    // printk("w1_resume\n");
    w1_init();
    /* check for changes to power that may have occurred */
    power_change_task_handler(0);
    /* I think this will work to ensure the interrupt is unmasked. */
    disable_irq(module_data.w1_irq);
    enable_irq(module_data.w1_irq);
    /* Clear OWM_SMB, set OWM_EN */
    asic3_set_extcf_select(&hx4700_asic3.dev, ASIC3_EXTCF_OWM_EN, ASIC3_EXTCF_OWM_EN);
    return 0;
}

static struct platform_driver w1_driver = {
    .driver = {
         .name = "hx4700-power",
    },
    .probe    = w1_probe,
    .remove   = w1_remove,
    .suspend  = w1_suspend,
    .resume   = w1_resume
};

static int __init
w1init(void)
{
    printk(KERN_NOTICE "hx4700 Power Management Driver\n");
    return platform_driver_register(&w1_driver);
}

static void __exit
w1exit(void)
{
    platform_driver_unregister(&w1_driver);
}

module_init(w1init);
module_exit(w1exit);

MODULE_AUTHOR("Aric D. Blumer, SDG Systems, LLC");
MODULE_DESCRIPTION("hx4700 Power Management Driver");
MODULE_LICENSE("GPL");

/* vim600: set expandtab: */

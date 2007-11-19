/* Touch screen driver for the TI something-or-other
 *
 * Copyright © 2005 SDG Systems, LLC
 *
 * Based on code that was based on the SAMCOP driver.
 *     Copyright © 2003, 2004 Compaq Computer Corporation.
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
 * AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
 * FITNESS FOR ANY PARTICULAR PURPOSE.
 *
 * Author:  Keith Packard <keith.packard@hp.com>
 *          May 2003
 *
 * Updates:
 *
 * 2004-02-11   Michael Opdenacker      Renamed names from samcop to shamcop,
 *                                      Goal:support HAMCOP and SAMCOP.
 * 2004-02-14   Michael Opdenacker      Temporary fix for device id handling
 *
 * 2005-02-18   Aric Blumer             Converted  basic structure to support hx4700
 *
 * 2005-06-07   Aric Blumer             Added tssim device handling so we can
 *                                      hook in the fbvncserver.
 */

#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/irq.h>

#include <asm/arch/hardware.h>
#include <asm/mach/irq.h>
#include <asm/io.h>

/* remove me */
#include <asm/arch/pxa-regs.h>
#include <asm/arch/htcuniversal-gpio.h>
#include <asm/arch/htcuniversal-asic.h>
#include <asm/mach-types.h> 

#include <asm/hardware/ipaq-asic3.h>
#include <linux/mfd/asic3_base.h>


#include "tsc2046_ts.h"

enum touchscreen_state {
    STATE_WAIT_FOR_TOUCH,   /* Waiting for a PEN interrupt */
    STATE_SAMPLING          /* Actively sampling ADC */
};

struct touchscreen_data {
    enum touchscreen_state state;
    struct timer_list      timer;
    int                    irq;
    struct input_dev       *input;
    /* */
    int			port;	
    int			clock;
    int			pwrbit_X;
    int			pwrbit_Y;
    int			(*pen_down)(void);
};

static unsigned long poll_sample_time   = 10; /* Sample every 10 milliseconds */

static struct touchscreen_data *ts_data;

static int irqblock;

module_param(poll_sample_time, ulong, 0644);
MODULE_PARM_DESC(poll_sample_time, "Poll sample time");

static inline void
report_touchpanel(struct touchscreen_data *ts, int pressure, int x, int y)
{
    input_report_abs(ts->input, ABS_PRESSURE, pressure);
    input_report_abs(ts->input, ABS_X, x);
    input_report_abs(ts->input, ABS_Y, y);
    input_sync(ts->input);
}

static void start_read(struct touchscreen_data *touch);

static irqreturn_t
pen_isr(int irq, void *irq_desc)
{
    struct touchscreen_data *ts = ts_data;

    if(irq == ts->irq /* && !irqblock */) {
        irqblock = 1;

        /*
         * Disable the pen interrupt.  It's reenabled when the user lifts the
         * pen.
         */
        disable_irq(ts->irq);

        if (ts->state == STATE_WAIT_FOR_TOUCH) {
            ts->state = STATE_SAMPLING;
            start_read(ts);
        } else {
            /* Shouldn't happen */
            printk(KERN_ERR "Unexpected ts interrupt\n");
        }

    }
    return IRQ_HANDLED;
}

static void
ssp_init(int port, int clock)
{

    pxa_set_cken(clock, 0);

    pxa_gpio_mode(GPIO_NR_HTCUNIVERSAL_TOUCHSCREEN_SPI_CLK_MD);
    pxa_gpio_mode(GPIO_NR_HTCUNIVERSAL_TOUCHSCREEN_SPI_FRM_MD);
    pxa_gpio_mode(GPIO_NR_HTCUNIVERSAL_TOUCHSCREEN_SPI_DO_MD);
    pxa_gpio_mode(GPIO_NR_HTCUNIVERSAL_TOUCHSCREEN_SPI_DI_MD);

    SET_HTCUNIVERSAL_GPIO(SPI_FRM,1);

    /* *** Set up the SPI Registers *** */
    SSCR0_P(port) =
          SSCR0_EDSS         /* Extended Data Size Select */
        | SSCR0_SerClkDiv(7) /* Serial Clock Rate */
                             /* Synchronous Serial Enable (Disable for now) */
        | SSCR0_Motorola     /* Motorola SPI Interface */
        | SSCR0_DataSize(8)  /* Data Size Select  (24-bit) */
        ;
    SSCR1_P(port) = 0;
    SSPSP_P(port) = 0;

    /* Clear the Status */
    SSSR_P(port)  = SSSR_P(port) & 0x00fcfffc;

    /* Now enable it */
    SSCR0_P(port) =
          SSCR0_EDSS         /* Extended Data Size Select */
        | SSCR0_SerClkDiv(7) /* Serial Clock Rate */
        | SSCR0_SSE          /* Synchronous Serial Enable */
        | SSCR0_Motorola     /* Motorola SPI Interface */
        | SSCR0_DataSize(8)  /* Data Size Select  (24-bit) */
        ;

    pxa_set_cken(clock, 1);
}

static void
start_read(struct touchscreen_data *touch)
{
    unsigned long inc = (poll_sample_time * HZ) / 1000;
    int i;

    /* Write here to the serial port. We request X and Y only for now.
     * Then we have to wait for poll_sample_time before we read out the serial
     * port.  Then, when we read it out, we check to see if the pen is still
     * down.  If so, then we issue another request here.
     */
#define TS_SAMPLES 7

    /*
     * We do four samples for each, and throw out the highest and lowest, then
     * average the other two.
     */

    for(i = 0; i < TS_SAMPLES; i++) {
        while(!(SSSR_P(touch->port) & SSSR_TNF))
            ;
        /* It's not full. Write the command for X */
        SSDR_P(touch->port) = (TSC2046_SAMPLE_X|(touch->pwrbit_X))<<16;
    }

    for(i = 0; i < TS_SAMPLES; i++) {
        while(!(SSSR_P(touch->port) & SSSR_TNF))
            ;
        /* It's not full. Write the command for Y */
        SSDR_P(touch->port)  = (TSC2046_SAMPLE_Y|(touch->pwrbit_Y))<<16;
    }

    /*
     * Enable the timer. We should get an interrupt, but we want keep a timer
     * to ensure that we can detect missing data
     */
    mod_timer(&touch->timer, jiffies + inc);
}

static void
ts_timer_callback(unsigned long data)
{
    struct touchscreen_data *ts = (struct touchscreen_data *)data;
    int x, a[TS_SAMPLES], y;
    static int oldx, oldy;
    int ssrval;

    /*
     * Check here to see if there is anything in the SPI FIFO.  If so,
     * return it if there has been a change.  If not, then we have a
     * timeout.  Generate an erro somehow.
     */
    ssrval = SSSR_P(ts->port);

    if(ssrval & SSSR_RNE) { /* Look at Rx Not Empty bit */
        int number_of_entries_in_fifo;

        /* The FIFO is not emtpy. Good! Now make sure there are at least two
         * entries. (Should be two exactly.) */

        number_of_entries_in_fifo = ((ssrval >> 12) & 0xf) + 1;

        if(number_of_entries_in_fifo < TS_SAMPLES * 2) {
            /* Not ready yet. Come back later. */
            unsigned long inc = (poll_sample_time * HZ) / 1000;
            mod_timer(&ts->timer, jiffies + inc);
            return;
        }

        if(number_of_entries_in_fifo == TS_SAMPLES * 2) {
            int i, j; 

            for(i = 0; i < TS_SAMPLES; i++) {
                a[i] = SSDR_P(ts->port);
            }
            /* Sort them (bubble) */
            for(j = TS_SAMPLES - 1; j > 0; j--) {
                for(i = 0; i < j; i++) {
                    if(a[i] > a[i + 1]) {
                        int tmp;
                        tmp    = a[i+1];
                        a[i+1] = a[i];
                        a[i]   = tmp;
                    }
                }
            }

            /* Take the average of the middle two */
            /* x = (a[TS_SAMPLES/2 - 1] + a[TS_SAMPLES/2] + a[TS_SAMPLES/2+1] + a[TS_SAMPLES/2+2]) >> 2; */
            x = a[TS_SAMPLES/2];

            for(i = 0; i < TS_SAMPLES; i++) {
                a[i] = SSDR_P(ts->port);
            }
            /* Sort them (bubble) */
            for(j = TS_SAMPLES - 1; j > 0; j--) {
                for(i = 0; i < j; i++) {
                    if(a[i] > a[i + 1]) {
                        int tmp;
                        tmp    = a[i+1];
                        a[i+1] = a[i];
                        a[i]   = tmp;
                    }
                }
            }


            /* Take the average of the middle two */
            /* y = (a[TS_SAMPLES/2 - 1] + a[TS_SAMPLES/2] + a[TS_SAMPLES/2+1] + a[TS_SAMPLES/2+2]) >> 2; */
            y = a[TS_SAMPLES/2];
        } else {
            /* We have an error! Too many entries. */
            printk(KERN_ERR "TS: Expected %d entries. Got %d\n", TS_SAMPLES*2, number_of_entries_in_fifo);
            /* Try to clear the FIFO */
            while(number_of_entries_in_fifo--) {
                (void)SSDR_P(ts->port);
            }

            if (ts->pen_down())
                start_read(ts);

            return;
        }
    } else {
        /* Not ready yet. Come back later. */
        unsigned long inc = (poll_sample_time * HZ) / 1000;
        mod_timer(&ts->timer, jiffies + inc);
        return;
    }

    /*
     * Now we check to see if the pen is still down.  If it is, then call
     * start_read().
     */
    if (ts->pen_down())
    {
        /* Still down */
        if(oldx != x || oldy != y) {
            oldx = x;
            oldy = y;
            report_touchpanel(ts, 1, x, y);
        }
        start_read(ts);
    } else {
        /* Up */
        report_touchpanel(ts, 0, 0, 0);
        irqblock = 0;
        ts->state = STATE_WAIT_FOR_TOUCH;
        /* Re-enable pen down interrupt */
        enable_irq(ts->irq);
    }
}

static int pen_down(void)
{
 return ( asic3_get_gpio_status_a( &htcuniversal_asic3.dev ) & (1<<GPIOA_TOUCHSCREEN_N)) == 0 ;
}

static int
ts_probe (struct platform_device *dev)
{
    int retval;
    struct touchscreen_data *ts;
    struct tsc2046_mach_info *mach = dev->dev.platform_data;

    printk("htcuniversal: ts_probe\n");

    ts = ts_data = kmalloc (sizeof (*ts), GFP_KERNEL);
    if (ts == NULL) {
	printk( KERN_NOTICE "htcuniversal_ts: unable to allocate memory\n" );
	return -ENOMEM;
    }
    memset (ts, 0, sizeof (*ts));

    ts->input = input_allocate_device();
    if (ts->input == NULL) {
	printk( KERN_NOTICE "htcuniversal_ts: unable to allocation touchscreen input\n" );
	kfree(ts);
	return -ENOMEM;
    }
    ts->input->evbit[0]             = BIT(EV_ABS);
    ts->input->absbit[0]            = BIT(ABS_X) | BIT(ABS_Y) | BIT(ABS_PRESSURE);
    ts->input->absmin[ABS_X]        = 0;
    ts->input->absmax[ABS_X]        = 32767;
    ts->input->absmin[ABS_Y]        = 0;
    ts->input->absmax[ABS_Y]        = 32767;
    ts->input->absmin[ABS_PRESSURE] = 0;
    ts->input->absmax[ABS_PRESSURE] = 1;

    ts->input->name = "htcuniversal_ts";
    ts->input->phys =  "touchscreen/htcuniversal_ts";
    ts->input->private = ts;

    input_register_device(ts->input);

    ts->timer.function = ts_timer_callback;
    ts->timer.data = (unsigned long)ts;
    ts->state = STATE_WAIT_FOR_TOUCH;
    init_timer (&ts->timer);

    platform_set_drvdata(dev, ts);

    ts->port=-1;

    if (mach) {
        ts->port = mach->port;
        ts->clock = mach->clock;
        ts->pwrbit_X = mach->pwrbit_X;
        ts->pwrbit_Y = mach->pwrbit_Y;

        /* static irq */
        if (mach->irq)
         ts->irq = mach->irq;

        if (mach->pen_down)
         ts->pen_down=mach->pen_down;
    }

    if (ts->port == -1)
    {
     printk("tsc2046: your device is not supported by this driver\n");
     return -ENODEV;
    }

    /* *** Initialize the SSP interface *** */
    ssp_init(ts->port, ts->clock);

    while(!(SSSR_P(ts->port) & SSSR_TNF))
        ;
    SSDR_P(ts->port) = (TSC2046_SAMPLE_X|(ts->pwrbit_X))<<16;

    for(retval = 0; retval < 100; retval++) {
        if(SSSR_P(ts->port) & SSSR_RNE) {
            while(SSSR_P(ts->port) & SSSR_RNE) {
                (void)SSDR_P(ts->port);
            }
            break;
        }
        mdelay(1);
    }

    if (machine_is_htcuniversal() )
    {
     ts->irq = asic3_irq_base( &htcuniversal_asic3.dev ) + ASIC3_GPIOA_IRQ_BASE + GPIOA_TOUCHSCREEN_N;
     ts->pen_down=pen_down;
    }

    retval = request_irq(ts->irq, pen_isr, SA_INTERRUPT, "tsc2046_ts", ts);
    if(retval) {
        printk("Unable to get interrupt\n");
        input_unregister_device (ts->input);
        return -ENODEV;
    }
    set_irq_type(ts->irq, IRQ_TYPE_EDGE_FALLING);

    return 0;
}

static int
ts_remove (struct platform_device *dev)
{
    struct touchscreen_data *ts = platform_get_drvdata(dev);

    input_unregister_device (ts->input);
    del_timer_sync (&ts->timer);
    free_irq (ts->irq, ts);
    pxa_set_cken(ts->clock, 0);

    kfree(ts);
    return 0;
}

static int
ts_suspend (struct platform_device *dev, pm_message_t state)
{
    struct touchscreen_data *ts = platform_get_drvdata(dev);

    disable_irq(ts->irq);

    printk("htcuniversal_ts2_suspend: called.\n");
    return 0;
}

static int
ts_resume (struct platform_device *dev)
{
    struct touchscreen_data *ts = platform_get_drvdata(dev);

    ts->state = STATE_WAIT_FOR_TOUCH;
    ssp_init(ts->port, ts->clock);
    enable_irq(ts->irq);

    printk("htcuniversal_ts2_resume: called.\n");
    return 0;
}

static struct platform_driver ts_driver = {
    .probe    = ts_probe,
    .remove   = ts_remove,
    .suspend  = ts_suspend,
    .resume   = ts_resume,
    .driver = {
		.name = "htcuniversal_ts",
	},
};


static int
ts_module_init (void)
{
    printk(KERN_NOTICE "HTC Universal Touch Screen Driver\n");
    
    return platform_driver_register(&ts_driver);
}

static void
ts_module_cleanup (void)
{
    platform_driver_unregister (&ts_driver);
}

module_init(ts_module_init);
module_exit(ts_module_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Aric Blumer, SDG Systems, LLC");
MODULE_DESCRIPTION("HTC Universal Touch Screen Driver");

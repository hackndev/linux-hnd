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
#include <linux/input_pda.h>
#include <linux/platform_device.h>
#include <linux/battery.h>

#include <asm/arch/hardware.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>
#include <asm/io.h>
#include <asm/semaphore.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/hx4700-gpio.h>
#include <asm/arch/hx4700-asic.h>

enum touchscreen_state {
    STATE_WAIT_FOR_TOUCH,   /* Waiting for a PEN interrupt */
    STATE_SAMPLING          /* Actively sampling ADC */
};

struct touchscreen_data {
    struct work_struct serial_work;
    enum touchscreen_state state;
    struct timer_list      timer;
    int                    irq;
    struct input_dev       *input;
};

static unsigned long poll_sample_time   = 10; /* Sample every 10 milliseconds */

static struct touchscreen_data *ts_data;

#define TS_SAMPLES 5

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

static irqreturn_t
pen_isr(int irq, void *irq_desc)
{
    /* struct touchscreen_data *ts = dev_id->data; */
    struct touchscreen_data *ts = ts_data;


    if(irq == HX4700_IRQ(TOUCHPANEL_IRQ_N)) {

        if (ts->state == STATE_WAIT_FOR_TOUCH) {
            /*
             * There is ground bounce or noise or something going on here:
             * when you write to the SSP port to get the X and Y values, it
             * causes a TOUCHPANEL_IRQ_N interrupt to occur.  So if that
             * happens, we can check to make sure the pen is actually down and
             * disregard the interrupt if it's not.
             */
            if(GET_HX4700_GPIO(TOUCHPANEL_IRQ_N) == 0) {
                /*
                * Disable the pen interrupt.  It's reenabled when the user lifts the
                * pen.
                */
                disable_irq(HX4700_IRQ(TOUCHPANEL_IRQ_N));

                ts->state = STATE_SAMPLING;
                schedule_work(&ts->serial_work);
            }
        } else {
            /* Shouldn't happen */
            printk(KERN_ERR "Unexpected ts interrupt\n");
        }

    }
    return IRQ_HANDLED;
}

static void
ssp_init(void)
{

    pxa_set_cken(CKEN3_SSP2, 1);

    /* *** Set up the SPI Registers *** */
    SSCR0_P2 =
          SSCR0_EDSS     /* Extended Data Size Select */
        | (6 << 8)      /* Serial Clock Rate */
        | (0 << 7)      /* Synchronous Serial Enable (Disable for now) */
        | SSCR0_Motorola      /* Motorola SPI Interface */
        | SSCR0_DataSize(24 - 16)  /* Data Size Select  (24-bit) */
        ;
    SSCR1_P2 = 0;
    SSPSP_P2 = 0;

    /* Clear the Status */
    SSSR_P2  = SSSR_P2 & 0x00fcfffc;

    /* Now enable it */
    SSCR0_P2 =
          SSCR0_EDSS     /* Extended Data Size Select */
        | (6 << 8)      /* Serial Clock Rate */
        | SSCR0_SSE      /* Synchronous Serial Enable */
        | SSCR0_Motorola      /* Motorola SPI Interface */
        | SSCR0_DataSize(24 - 16)      /* Data Size Select  (24-bit) */
        ;
    /* enable_irq(HX4700_IRQ(TOUCHPANEL_IRQ_N)); */
}

DECLARE_MUTEX(serial_mutex);

static void
start_read(struct touchscreen_data *touch)
{
    unsigned long inc = (poll_sample_time * HZ) / 1000;
    int i;

    down(&serial_mutex);

    /* Write here to the serial port.
     * Then we have to wait for poll_sample_time before we read out the serial
     * port.  Then, when we read it out, we check to see if the pen is still
     * down.  If so, then we issue another request here.
     */

    for(i = 0; i < TS_SAMPLES; i++) {
        while(!(SSSR_P2 & SSSR_TNF))
            ;
        /* It's not full. Write the command for X */
        SSDR_P2 = 0xd00000;
        while(!(SSSR_P2 & SSSR_TNF))
            ;
        /* It's not full. Write the command for Y */
        SSDR_P2 = 0x900000;
    }

    /*
     * Enable the timer. We should get an interrupt, but we want keep a timer
     * to ensure that we can detect missing data
     */
    mod_timer(&touch->timer, jiffies + inc);
}

static void
start_read_task_func(struct work_struct *work)
{
    struct touchscreen_data *touch = container_of(work, struct touchscreen_data, serial_work);
    start_read(touch);
}


static int
do_delta_calc(int x1, int y1, int x2, int y2, int x3, int y3)
{
    /* This is based on Jamey Hicks' description on IRC. */
    int dx2_a, dy2_a, dx2_b, dy2_b;

    dx2_a = x2 - x1;
    dx2_a = dx2_a * dx2_a;    /* If dx2_a was negative, it's not now */
    dy2_a = y2 - y1;
    dy2_a = dy2_a * dy2_a;    /* If dy2_a was negative, it's not now */

    dx2_b = x3 - x2;
    dx2_b = dx2_b * dx2_b;    /* If dx2_b was negative, it's not now */
    dy2_b = y3 - y2;
    dy2_b = dy2_b * dy2_b;    /* If dy2_b was negative, it's not now */

#if 0
    /* This was described in the algorithm by Jamey, but it doesn't do much
     * good.
     */
    if(dx2_a + dy2_a < dx2_b + dy2_b) return 0;
#endif

    /* dx2_a + dy2_a is the distance squared */
    if(
           ((dx2_a + dy2_a) > 8000)
        || ((dx2_b + dy2_b) > 8000)
    ) {
        return 0;
    } else {
        return 1;
    }

    if((dx2_b + dy2_b) > 5000) {
        return 0;
    } else {
        return 1;
    }
}

static void
ts_timer_callback(unsigned long data)
{
    struct touchscreen_data *ts = (struct touchscreen_data *)data;
    int x, y;
    int ssrval;

    /*
     * Check here to see if there is anything in the SPI FIFO.  If so,
     * return it if there has been a change.  If not, then we have a
     * timeout.  Generate an erro somehow.
     */
    ssrval = SSSR_P2;
    if(ssrval & SSSR_RNE) { /* Look at Rx Not Empty bit */
        int number_of_entries_in_fifo;

        /* The FIFO is not emtpy. Good! Now make sure there are 
           requested number of samples for both X and Y. */

        number_of_entries_in_fifo = ((ssrval >> 12) & 0xf) + 1;

        if(number_of_entries_in_fifo < (TS_SAMPLES * 2)) {
            /* Not ready yet. Come back later. */
            unsigned long inc = (poll_sample_time * HZ) / 1000;
            mod_timer(&ts->timer, jiffies + inc);
            return;
        }

        if(number_of_entries_in_fifo == TS_SAMPLES * 2) {
            int i, result, keep;
            int X[TS_SAMPLES], Y[TS_SAMPLES];

            for(i = 0; i < TS_SAMPLES; i++) {
                X[i] = SSDR_P2;
                Y[i] = SSDR_P2;
            }

            up(&serial_mutex);

            keep = 0;
            x = y = 0;

            result = 0;
            if(do_delta_calc(X[0], Y[0], X[1], Y[1], X[2], Y[2])) {
                result |= (1 << 2);
            }
            if(do_delta_calc(X[1], Y[1], X[2], Y[2], X[3], Y[3])) {
                result |= (1 << 1);
            }
            if(do_delta_calc(X[2], Y[2], X[3], Y[3], X[4], Y[4])) {
                result |= (1 << 0);
            }
            switch(result) {
                case 0:
                    /* Take average of point 0 and point 3 */
                    X[2] = (X[1] + X[3]) / 2;
                    Y[2] = (Y[1] + Y[3]) / 2;
                    /* don't keep */
                    break;
                case 1:
                    /* Just ignore this one */
                    break;
                case 2:
                case 3:
                case 6:
                    /* keep this sample */
                    x = (X[1] + (2 * X[2]) + X[3]) / 4;
                    y = (Y[1] + (2 * Y[2]) + Y[3]) / 4;
                    keep = 1;
                    break;
                case 4:
                    X[1] = (X[0] + X[2]) / 2;
                    Y[1] = (Y[0] + Y[2]) / 2;
                    /* don't keep */
                    break;
                case 5:
                case 7:
                    x = (X[0] + (4 * X[1]) + (6 * X[2]) + (4 * X[3]) + X[4]) >> 4;
                    y = (Y[0] + (4 * Y[1]) + (6 * Y[2]) + (4 * Y[3]) + Y[4]) >> 4;
                    keep = 1;
                    break;
            }

            if(GET_HX4700_GPIO(TOUCHPANEL_IRQ_N) == 0) {
                /* Still down */
                if(keep) {
                    report_touchpanel(ts, 1, x, y);
                }
                start_read(ts);
            } else {
                /* Up */
                report_touchpanel(ts, 0, 0, 0);
                ts->state = STATE_WAIT_FOR_TOUCH;
                /* Re-enable pen down interrupt */
                enable_irq(HX4700_IRQ(TOUCHPANEL_IRQ_N));
            }

        } else {
            /* We have an error! Too many entries. */
            printk(KERN_ERR "TS: Expected %d entries. Got %d\n", 2, number_of_entries_in_fifo);
            /* Try to clear the FIFO */
            while(number_of_entries_in_fifo--) {
                (void)SSDR_P2;
            }
            up(&serial_mutex);
            if(GET_HX4700_GPIO(TOUCHPANEL_IRQ_N) == 0) {
                start_read(ts);
            }
            return;
        }
    } else {
        /* Not ready yet. Come back later. */
        unsigned long inc = (poll_sample_time * HZ) / 1000;
        mod_timer(&ts->timer, jiffies + inc);
        return;
    }
}

static int
ts_probe (struct platform_device *pdev)
{
    int retval;
    struct touchscreen_data *ts;
    unsigned int irq;
    int err;

    ts = ts_data = kmalloc (sizeof (*ts), GFP_KERNEL);
    if (ts == NULL) {
	printk( KERN_NOTICE "hx4700_ts: unable to allocate memory\n" );
	return -ENOMEM;
    }
    memset (ts, 0, sizeof (*ts));


    /* *** Set up the input subsystem stuff *** */
    // memset(ts->input, 0, sizeof(struct input_dev));
    ts->input = input_allocate_device();
    if (ts->input == NULL) {
	printk( KERN_NOTICE "hx4700_ts: unable to allocation touchscreen input\n" );
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

    ts->input->name = "hx4700_ts";
    ts->input->private = ts;

    input_register_device(ts->input);

    ts->timer.function = ts_timer_callback;
    ts->timer.data = (unsigned long)ts;
    ts->state = STATE_WAIT_FOR_TOUCH;
    init_timer (&ts->timer);

    INIT_WORK(&ts->serial_work, start_read_task_func);

    platform_set_drvdata(pdev, ts);

    /* *** Initialize the SSP interface *** */
    ssp_init();

    down(&serial_mutex);
    /* Make sure the device is in such a state that it can give pen
     * interrupts. */
    while(!(SSSR_P2 & SSSR_TNF))
        ;
    SSDR_P2 = 0xd00000;

    for(retval = 0; retval < 100; retval++) {
        if(SSSR_P2 & SSSR_RNE) {
            while(SSSR_P2 & SSSR_RNE) {
                (void)SSDR_P2;
            }
            break;
        }
        mdelay(1);
    }
    up(&serial_mutex);


    ts->irq = HX4700_IRQ( TOUCHPANEL_IRQ_N );
    retval = request_irq(ts->irq, pen_isr, SA_INTERRUPT, "hx4700_ts", ts);
    if(retval) {
        printk("Unable to get interrupt\n");
        input_unregister_device (ts->input);
        return -ENODEV;
    }
    set_irq_type(ts->irq, IRQT_FALLING);

    return 0;
}

static int
ts_remove (struct platform_device *pdev)
{
    struct touchscreen_data *ts = platform_get_drvdata(pdev);

    input_unregister_device (ts->input);
    del_timer_sync (&ts->timer);
    free_irq (ts->irq, ts);

    kfree(ts);
    pxa_set_cken(CKEN3_SSP2, 0);
    return 0;
}

static int
ts_suspend (struct platform_device *pdev, pm_message_t state)
{
    disable_irq(HX4700_IRQ(TOUCHPANEL_IRQ_N));
    return 0;
}

static int
ts_resume (struct platform_device *pdev)
{
    struct touchscreen_data *ts = platform_get_drvdata(pdev);

    ts->state = STATE_WAIT_FOR_TOUCH;
    ssp_init();
    enable_irq(HX4700_IRQ(TOUCHPANEL_IRQ_N));

    return 0;
}

static struct platform_driver ts_driver = {
    .probe    = ts_probe,
    .remove   = ts_remove,
    .suspend  = ts_suspend,
    .resume   = ts_resume,
    .driver = {
        .name = "hx4700-ts",
    },
};

static int tssim_init(void);

static int
ts_module_init (void)
{
    printk(KERN_NOTICE "hx4700 Touch Screen Driver\n");
    if(tssim_init()) {
        printk(KERN_NOTICE "  TS Simulator Not Installed\n");
    } else {
        printk(KERN_NOTICE "  TS Simulator Installed\n");
    }
    return platform_driver_register(&ts_driver);
}

static void tssim_exit(void);

static void
ts_module_cleanup (void)
{
    tssim_exit();
    platform_driver_unregister (&ts_driver);
}

/************* Code for Touch Screen Simulation for FBVNC Server **********/
static        dev_t     dev;
static struct cdev      *cdev;

static long a0 = -1122, a2 = 33588528, a4 = 1452, a5 = -2970720, a6 = 65536;

/* The input into the input subsystem is prior to correction from calibration.
 * So we have to undo the effects of the calibration.  It's actually a
 * complicated equation where the calibrated value of X depends on the
 * uncalibrated values of X and Y.  Fortunately, at least on the hx4700, the
 * multiplier for the Y value is zero, so I assume that here.  It is a shame
 * that the tslib does not allow multiple inputs.  Then we could do another
 * driver for this (as it was originally) that give input that does not
 * require calibration.
 */
static int
tssim_ioctl(struct inode *inode, struct file *fp, unsigned int ioctlnum, unsigned long parm)
{
    switch(ioctlnum) {
        case 0: a0 = parm; break;
        case 1: break;
        case 2: a2 = parm; break;
        case 3: break;
        case 4: a4 = parm; break;
        case 5: a5 = parm; break;
        case 6:
                a6 = parm;
                printk(KERN_DEBUG "a0 = %ld, a2 = %ld, a4 = %ld, a5 = %ld, a6 = %ld\n",
                    a0, a2, a4, a5, a6);
                break;
        default: return -ENOTTY;
    }
    return 0;
}

static int
tssim_open(struct inode *inode, struct file *fp)
{
    /* Nothing to do here */
    return 0;
}

static ssize_t
tssim_write(struct file *fp, const char __user *data, size_t bytes, loff_t *offset)
{
    unsigned long pressure;
    long y;
    long x;

    x        = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24); data += 4;
    y        = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24); data += 4;
    pressure = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24); data += 4;

    input_report_abs(ts_data->input, ABS_PRESSURE, pressure?1:0);
    input_report_abs(ts_data->input, ABS_X, ((x * a6) - a2)/a0);
    input_report_abs(ts_data->input, ABS_Y, ((y * a6) - a5)/a4);
    input_sync(ts_data->input);

    return bytes;
}

int tssim_close(struct inode *inode, struct file *fp)
{
    return 0;
}

struct file_operations fops = {
    THIS_MODULE,
    .write      = tssim_write,
    .open       = tssim_open,
    .release    = tssim_close,
    .ioctl      = tssim_ioctl,
};

static int battery_class;

static int get_min_voltage(struct battery *b)
{
    return 1000;
}
static int get_max_voltage(struct battery *b)
{
    return 1400; /* mV */
}
static int get_max_charge(struct battery *b)
{
    return 100;
}
static int get_voltage(struct battery *b)
{
    static int battery_sample;

    if(!down_interruptible(&serial_mutex)) {
        int i;
        int ssrval;

        while(!(SSSR_P2 & SSSR_TNF))
            ;
        SSDR_P2 = 0xe70000;
        while(!(SSSR_P2 & SSSR_TNF))
            ;
        SSDR_P2 = 0xe70000;
        while(!(SSSR_P2 & SSSR_TNF))
            ;
        SSDR_P2 = 0xd00000;     /* Dummy command to allow pen interrupts again */

        for(i = 0; i < 10; i++) {
            ssrval = SSSR_P2;
            if(ssrval & SSSR_RNE) { /* Look at Rx Not Empty bit */
                int number_of_entries_in_fifo;

                number_of_entries_in_fifo = ((ssrval >> 12) & 0xf) + 1;
                if(number_of_entries_in_fifo == 3) {
                    break;
                }
            }
            msleep(1);
        }

        if(i < 1000) {
            (void) SSDR_P2;
            battery_sample = SSDR_P2 & 0xfff;
            (void) SSDR_P2;
        } else {
            /* Make sure the FIFO is empty */
            while(SSSR_P2 & SSSR_RNE) {
                (void) SSDR_P2;
            }
            battery_sample = -1;
        }
        up(&serial_mutex);
    }

    return battery_sample;
}
static int get_charge(struct battery *b)
{
    return 100;
}
static int get_status(struct battery *b)
{
    return 1;
}

static struct battery hx4700_power = {
    .name               = "hx4700_backup",
    .id                 = "backup",
    .get_min_voltage    = get_min_voltage,
    .get_min_current    = 0,
    .get_min_charge     = 0,
    .get_max_voltage    = get_max_voltage,
    .get_max_current    = 0,
    .get_max_charge     = get_max_charge,
    .get_temp           = 0,
    .get_voltage        = get_voltage,
    .get_current        = 0,
    .get_charge         = get_charge,
    .get_status         = get_status,
};

static int
tssim_init(void)
{
    int retval;

    retval = alloc_chrdev_region(&dev, 0, 1, "tssim");
    if(retval) {
        printk(KERN_ERR "TSSIM Unable to allocate device numbers\n");
        return retval;
    }

    cdev = cdev_alloc();
    cdev->owner = THIS_MODULE;
    cdev->ops = &fops;
    retval = cdev_add(cdev, dev, 1);
    if(retval) {
        printk(KERN_ERR "Unable to add cdev\n");
        unregister_chrdev_region(dev, 1);
        return retval;
    }

    battery_class = 0;
    if(battery_class_register(&hx4700_power)) {
        printk(KERN_ERR "hx4700_ts: Could not register battery class\n");
    } else {
        battery_class = 1;
    }

    return 0;
}

static void
tssim_exit(void)
{
    cdev_del(cdev);
    unregister_chrdev_region(dev, 1);
    if(battery_class)  {
        battery_class_unregister(&hx4700_power);
    }
    return;
}

module_init(ts_module_init);
module_exit(ts_module_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Aric Blumer, SDG Systems, LLC");
MODULE_DESCRIPTION("hx4700 Touch Screen Driver");


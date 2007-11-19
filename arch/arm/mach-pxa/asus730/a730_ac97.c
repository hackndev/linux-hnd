/*
 * Touchscreen/battery driver for ASUS A730(W). WM9712 AC97 codec.
 *	based on:
 *		linux/arch/arm/mach-pxa/palmld/palmld_ac97.c &&
 *		linux/arch/arm/mach-pxa/palmld/palmtx_ac97.c &&
 *		touchscreen driver by Liam Girdwood
 */
 
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/battery.h>
#include <linux/irq.h>
#include <linux/platform_device.h>

#include <asm/delay.h>
#include <asm/arch/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/irqs.h>
#include <asm/arch/asus730-gpio.h>
#include <asm/arch/dma.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/ac97_codec.h>
#include <sound/wm9712.h>

/********************************************************/
#define RC_AGAIN	0x00000001
#define RC_VALID	0x00000002
#define RC_PENUP	0x00000004
#define RC_PENDOWN	0x00000008

#define AC97_LINK_FRAME	21
#define MAX_PRESSURE	150

/********************************************************/
static DECLARE_MUTEX(pendown_mutex);
static DECLARE_MUTEX(jack_mutex);
static DECLARE_MUTEX(battery_update_mutex);
static DECLARE_MUTEX(reg_rw_mutex);
static DECLARE_MUTEX(jack_delay_mutex);
static DECLARE_MUTEX(input_mutex);

/********************************************************/
static struct workqueue_struct *pendown_workqueue;
static struct work_struct pendown_task;
static struct workqueue_struct *phonejack_workqueue;
static struct work_struct phonejack_task;
static struct workqueue_struct *wm9712_phonejack_delayqueue;
static struct work_struct wm9712_phonejack_delay_task;

/********************************************************/
static struct snd_ac97 *ac97 = NULL;
static struct input_dev *a730_ts_input = NULL;

/********************************************************/
static int pen_is_down = 0;

/********************************************************/
#define PJ_IN 1
#define PJ_OUT 0
static int phonejack_state = 1;//jack state (0=plugged out; 1=plugged in)

/********************************************************/
static u64 last_batt_jiff = 0;
static int last_voltage = 0;
static int cur_voltage = 0;

static u16 wm_regs[128];

/********************************************************/
static inline u16 wm9712_read(int reg)
{
	down(&reg_rw_mutex);
	wm_regs[reg] = ac97->bus->ops->read(ac97, reg);
	udelay(10);
	up(&reg_rw_mutex);
	return wm_regs[reg];
}

static inline void wm9712_write(int reg, u16 data)
{
	down(&reg_rw_mutex);
	wm_regs[reg] = data;
	ac97->bus->ops->write(ac97, reg, data);
	udelay(10);
	up(&reg_rw_mutex);
}

static void wm97xx_gpio_func(int gpio, int func)
{
	int GEn;
	GEn = wm9712_read(AC97_MISC_AFE);
	if (func) GEn |= gpio;
	else GEn &= ~gpio;
	wm9712_write(AC97_MISC_AFE, GEn);
}
	
static void wm97xx_gpio_mode(int gpio, int config, int polarity, int sticky, int wakeup)
{
	int GCn, GPn, GSn, GWn;
	GCn = wm9712_read(AC97_GPIO_CFG);
	GPn = wm9712_read(AC97_GPIO_POLARITY);
	GSn = wm9712_read(AC97_GPIO_STICKY);
	GWn = wm9712_read(AC97_GPIO_WAKEUP);
	
	if (config) GCn |= gpio;
	else GCn &= ~gpio;
	
	if (polarity) GPn |= gpio;
	else GPn &= ~gpio;
	
	if (sticky) GSn |= gpio;
	else GSn &= ~gpio;
	
	if (wakeup) GWn |= gpio;
	else GWn &= ~gpio;
	
	wm9712_write(AC97_GPIO_CFG, GCn);
	wm9712_write(AC97_GPIO_POLARITY, GPn);
	wm9712_write(AC97_GPIO_STICKY, GSn);
	wm9712_write(AC97_GPIO_WAKEUP, GWn);
}

/***********************************************/
/******************* BATTERY *******************/
/***********************************************/
//delay 1 ms
#define DELAY	6
//rate 0=93Hz, 3=750Hz
#define RATE	0
static void a730_battery_update_data(int force)
{
	u16 i, d2, d1, dig2, dig1;
	int reading = -1, rsum = 0, rcnt = 0;

	if(!force && ((last_batt_jiff + HZ * 10) > jiffies)) return;

	if(down_trylock(&battery_update_mutex) != 0) return;
	
	down(&pendown_mutex);
	
	while (MISR & 4) MODR;
	
	dig2 = d2 = wm_regs[AC97_WM97XX_DIGITISER2];
	dig1 = d1 = wm_regs[AC97_WM97XX_DIGITISER1];
	
        dig1 &= ~(WM97XX_CM_RATE_MASK | WM97XX_ADCSEL_MASK | WM97XX_DELAY_MASK | WM97XX_SLT_MASK | WM97XX_COO);
        dig1 |= WM97XX_CTC | WM97XX_SLEN | WM97XX_DELAY(DELAY) | WM97XX_SLT(5) | WM97XX_RATE(RATE) | WM97XX_ADCSEL_BMON;
        dig2 &= ~WM9712_PDEN;
        dig2 = WM97XX_PRP_DET_DIG;
	
	wm9712_write(AC97_WM97XX_DIGITISER1, dig1);
	wm9712_write(AC97_WM97XX_DIGITISER2, dig2);
	wm9712_read(AC97_WM97XX_DIGITISER_RD);
	
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(1);

	printk("---batt---\n");
	for(i = 0; i < 32; i++)
	{
	    reading = MODR;
	    if ((reading & WM97XX_ADCSEL_MASK) == WM97XX_ADCSEL_BMON)
	    {
		rsum += (reading & 0xfff);
		rcnt++;
	    }
	}
	
	printk("have %d readings, sum is %d\n", rcnt, rsum);
	
	wm9712_write(AC97_WM97XX_DIGITISER1, d1);
	wm9712_write(AC97_WM97XX_DIGITISER2, d2);
	
	if (rcnt > 0)
	{
		last_voltage = cur_voltage;
		cur_voltage = rsum / rcnt;
		printk("Battery: %d\n", cur_voltage);
		last_batt_jiff = jiffies;
	}
	else printk("Battery read error.\n");
	
	up(&pendown_mutex);
	up(&battery_update_mutex);
}

static int a730_battery_get_min_voltage(struct battery *b)
{
	return 0;
}
static int a730_battery_get_min_charge(struct battery *b)
{
	return 0;
}
static int a730_battery_get_max_voltage(struct battery *b)
{
	return 4750;//mV
}
static int a730_battery_get_max_charge(struct battery *b)
{
	return 1;
}
static int a730_battery_get_voltage(struct battery *b)
{
	a730_battery_update_data(1);
	return cur_voltage * 3;// * 80586 / 100000;
}
static int a730_battery_get_charge(struct battery *b)
{
	return 0;
}
static int a730_battery_get_status(struct battery *b)
{
	return 0;
}

/*
static int a730_battery_class_hotplug(struct class_device *dev, char **envp, int num_envp, char *buffer, int buffer_size)
{
        return 0;
}
*/

static void a730_battery_class_release(struct class_device *dev)
{
}

static void a730_battery_class_class_release(struct class *class)
{
}

static struct battery a730_battery = {
    .name               = "a730-ac97",
    .id                 = "battery0",
    .get_min_voltage    = a730_battery_get_min_voltage,
    .get_min_current    = NULL,
    .get_min_charge     = a730_battery_get_min_charge,
    .get_max_voltage    = a730_battery_get_max_voltage,
    .get_max_current    = NULL,
    .get_max_charge     = a730_battery_get_max_charge,
    .get_temp           = NULL,
    .get_voltage        = a730_battery_get_voltage,
    .get_current        = NULL,
    .get_charge         = a730_battery_get_charge,
    .get_status         = a730_battery_get_status,
};

/***********************************************/
/****************** PHONEJACK ******************/
/***********************************************/

static void set_routing(struct snd_ac97 *ac97, int allow_phones, int allow_speaker)
{
	int tmp = 0, reg2 = 0, reg4 = 0, reg18 = 0, reg24 = 0, reg26 = 0;

	if (allow_phones) tmp &= ~(1 << 15);
	else tmp |= (1 << 15);
	if (allow_speaker) tmp &= ~(1 << 14);
	else tmp |= (1 << 14);
	
	reg2 = wm9712_read(AC97_MASTER);
	reg4 = wm9712_read(AC97_HEADPHONE);
	reg18 = wm9712_read(AC97_PCM);
	reg24 = wm9712_read(AC97_INT_PAGING);
	reg26 = wm9712_read(AC97_POWERDOWN);
	
	if (phonejack_state == PJ_OUT)
        {
        	printk("set_audio_out: p=%d s=%d\n", allow_phones, allow_speaker);
                reg2 |= (1 << 6);
                reg18 |= (1 << 13);
                reg4 |= (1 << 15);
                reg24 = 0x1F77;//speaker
                reg26 |= (1 << 8);
                reg26 &= ~(1 << 9);//DAC on
                reg26 |= (1 << 14);
        }
        else
        {
        	printk("set_audio_out: p=%d s=%d\n", allow_phones, allow_speaker);
                reg2 &= ~(1 << 6);
                reg18 &= ~(1 << 13);
                reg4 &= ~(0x01 << 15);
                reg24 = 0x1CEF;//hphones
                reg26 |= (1 << 8);
                reg26 &= ~(1 << 9);//DAC on
                reg26 &= ~(1 << 14);
        }
	
	snd_ac97_update(ac97, AC97_MASTER, reg2);
        snd_ac97_update(ac97, AC97_HEADPHONE, reg4);
        snd_ac97_update(ac97, AC97_PCM, reg18);
        snd_ac97_update(ac97, AC97_INT_PAGING, reg24);
        snd_ac97_update(ac97, AC97_POWERDOWN, reg26);
}

DECLARE_WAIT_QUEUE_HEAD(wait_pj_q);
static int pj_irq = 0;
static int stop_jackwork = 0;

static void wm9712_phonejack_switch_thread(void *data)
{
	msleep(200);
	set_routing(ac97, phonejack_state != PJ_OUT, phonejack_state == PJ_OUT);
	up(&jack_delay_mutex);
}

static void phonejack_handler(void *data)
{
	u16 status = 0, polarity = 0;
		
	status = wm9712_read(AC97_GPIO_STATUS);
	//udelay(10);
	polarity = wm9712_read(AC97_GPIO_POLARITY);

	if (status & WM97XX_GPIO_1)
	{
		if (polarity & WM97XX_GPIO_1)
		{
			phonejack_state = PJ_OUT;//plugged out
			polarity &= ~WM97XX_GPIO_1;
		}
		else
		{
			phonejack_state = PJ_IN;//plugged in
			polarity |= WM97XX_GPIO_1;
		}
		if (!down_trylock(&jack_delay_mutex))
                {
                        queue_work(wm9712_phonejack_delayqueue, &wm9712_phonejack_delay_task);
                }
                status &= ~WM97XX_GPIO_1;
	}
	
	wm9712_write(AC97_GPIO_POLARITY, polarity);

	wm9712_write(AC97_GPIO_STATUS, status);
	udelay(100);
	up(&jack_mutex);
	enable_irq(A730_IRQ(JACK_IRQ));
}

static irqreturn_t jack_irq_handler(int irq, void *dev_id)
{
	if (!down_trylock(&jack_mutex))
	{
		disable_irq(A730_IRQ(JACK_IRQ));
		queue_work(phonejack_workqueue, &phonejack_task);
	}
	return IRQ_HANDLED;
}

/***********************************************/
/**************** TOUCHSCREEN ******************/
/***********************************************/
static void set_dig_power(int power)
{
	u16 dig1, dig2;

	dig1 = wm9712_read(AC97_WM97XX_DIGITISER1);
	dig2 = wm9712_read(AC97_WM97XX_DIGITISER2);

	if (power)
	{
		// continous mode, x-y only
		dig1 &= ~(WM97XX_CM_RATE_MASK | WM97XX_ADCSEL_MASK | WM97XX_DELAY_MASK | WM97XX_SLT_MASK);
		dig1 |= WM97XX_CTC | WM97XX_COO | WM97XX_SLEN | WM97XX_DELAY(DELAY) | WM97XX_SLT(5) | WM97XX_RATE(RATE);
		dig2 |= WM9712_PDEN;
	}
	else
	{
		dig1 &= ~(WM97XX_CTC | WM97XX_COO | WM97XX_SLEN);
		dig2 &= ~WM9712_PDEN;
	}

	wm9712_write(AC97_WM97XX_DIGITISER1, dig1);
	wm9712_write(AC97_WM97XX_DIGITISER2, dig2);
}

DECLARE_WAIT_QUEUE_HEAD(wait_q);

struct ts_data {
    u16 x;
    u16 y;
    u16 p;
    u16 t;
};

static u16 last_x = 0;

static inline int a730_ts_read_sample(struct ts_data *data)
{
    u16 x = 0, y = 0, tmp;
    int rc = RC_AGAIN;

    /* data is never immediately available after pen down irq */
    set_current_state(TASK_INTERRUPTIBLE);
    schedule_timeout(1);

    x = y = 0;
    //read x-y pair
    while (pen_is_down && (!x || !y) && (MISR & 4))
    {
        tmp = MODR;
        switch (tmp & WM97XX_ADCSEL_MASK)
        {
            case WM97XX_ADCSEL_X: x = tmp; break;
            case WM97XX_ADCSEL_Y: y = tmp; break;
        }
    }
    if (!x || !y || x == last_x) return RC_AGAIN;
    last_x = x;

    data->t = (x & 0x8000) && (y & 0x8000);
    data->x = x &= 0xfff;
    data->y = y &= 0xfff;

    if (x && y) rc = RC_VALID;

    return rc;
}

struct a730_ts_state
{
    int sleep_time;
    int min_sleep_time;
};

static int stop_penwork = 0;

static void pendown_handler(void *data)
{
        struct a730_ts_state state;
        struct ts_data ts_data;
        int rc, pendown;

        state.min_sleep_time = HZ >= 100 ? HZ / 100 : 1;
        if (state.min_sleep_time < 1) state.min_sleep_time = 1;
        state.sleep_time = state.min_sleep_time;

        set_dig_power(1);

        while(!stop_penwork)
        {
            ts_data.x = ts_data.y = ts_data.p = ts_data.t = 0;
            try_to_freeze();
            down(&pendown_mutex);
            rc = a730_ts_read_sample(&ts_data);
            up(&pendown_mutex);

            pendown = (GET_A730_GPIO(PENDOWN_IRQ) != 0);

            if (rc == RC_VALID)
            {
                input_report_abs(a730_ts_input, ABS_X, ts_data.x);
                input_report_abs(a730_ts_input, ABS_Y, ts_data.y);
                input_report_abs(a730_ts_input, ABS_PRESSURE, MAX_PRESSURE);
                input_sync(a730_ts_input);
            }

            if (pen_is_down)
            {
            }
            else
            {
                printk("Zzzzz...\n");
                input_report_abs(a730_ts_input, ABS_PRESSURE, 0);
                input_sync(a730_ts_input);
                //make sure old samples are gone
                while (MISR & 4) MODR;
                ts_data.x = ts_data.y = ts_data.p = ts_data.t = 0;
                wait_event_interruptible(wait_q, (pen_is_down != 0));
            }
        }

        set_dig_power(0);

        //read tail. do we need this ?
        if (!pen_is_down)
        {
                set_current_state(TASK_INTERRUPTIBLE);
                schedule_timeout(1);
                while (MISR & 4) MODR;
        }
}

static irqreturn_t pendown_irq_handler(int irq, void *dev_id)
{
	if ((pen_is_down = (GET_A730_GPIO(PENDOWN_IRQ) != 0))) wake_up_interruptible(&wait_q);
	return IRQ_HANDLED;
}

static int suspended = 0;

static int a730_ts_setup(void)
{
	u16 tmp, dig1 = 0, dig2 = WM97XX_RPR | WM9712_RPU(1), mask = 0;//2;
	
	/* turn off irq gpio inverting, turn on jack insert detect */
	tmp = wm9712_read(AC97_ADD_FUNC);//0x58
        tmp &= ~1;//no invert irq
        tmp |= 2;//wake ena
        tmp |= (1 << 12);//phonejack detect
        wm9712_write(AC97_ADD_FUNC, tmp);
        
        wm9712_write(AC97_GPIO_STICKY, 0);
	
	/* enable interrupts on codec's gpio 2 (connected to cpu gpio 27) */
	wm97xx_gpio_mode(WM97XX_GPIO_2, WM97XX_GPIO_OUT, WM97XX_GPIO_POL_HIGH, WM97XX_GPIO_NOTSTICKY, WM97XX_GPIO_NOWAKE);
	wm97xx_gpio_func(WM97XX_GPIO_2, 0);

	/* enable interrupts on codec's gpio 3 (connected to cpu gpio 25) */
	wm97xx_gpio_mode(WM97XX_GPIO_3, WM97XX_GPIO_OUT, WM97XX_GPIO_POL_HIGH, WM97XX_GPIO_NOTSTICKY, WM97XX_GPIO_NOWAKE);
	wm97xx_gpio_func(WM97XX_GPIO_3, 0);

	wm97xx_gpio_mode(WM97XX_GPIO_1, WM97XX_GPIO_IN, WM97XX_GPIO_POL_HIGH, WM97XX_GPIO_STICKY, WM97XX_GPIO_WAKE);
	wm97xx_gpio_func(WM97XX_GPIO_1, 1);
	//do not generate interrupts from here
	wm97xx_gpio_mode(WM97XX_GPIO_13, WM97XX_GPIO_IN, WM97XX_GPIO_POL_HIGH, WM97XX_GPIO_NOTSTICKY, WM97XX_GPIO_NOWAKE);
	wm97xx_gpio_mode(WM97XX_GPIO_4, WM97XX_GPIO_IN, WM97XX_GPIO_POL_LOW, WM97XX_GPIO_NOTSTICKY, WM97XX_GPIO_WAKE);
	wm97xx_gpio_func(WM97XX_GPIO_4, 0);
	
	//dig1 &= 0xff0f;
        dig1 |= WM97XX_DELAY(DELAY);

        //pendown detect, digitiser off, wakep link
	dig2 |= WM97XX_PRP_DETW;
        dig2 |= ((mask & 0x3) << 6);

        wm9712_write(AC97_WM97XX_DIGITISER1, dig1);
        wm9712_write(AC97_WM97XX_DIGITISER2, dig2);
	
	return 0;
}

static int a730_ts_suspend(struct device *dev, pm_message_t state)
{
	//disable_irq(A730_IRQ(PENDOWN_IRQ));
	//disable_irq(A730_IRQ(JACK_IRQ));
	suspended = 1;
	return 0;
}

static int a730_ts_resume(struct device *dev)
{
	wm9712_write(AC97_WM97XX_DIGITISER1, wm_regs[AC97_WM97XX_DIGITISER1]);
        wm9712_write(AC97_WM97XX_DIGITISER2, wm_regs[AC97_WM97XX_DIGITISER2]);

        wm9712_write(AC97_GPIO_CFG, wm_regs[AC97_GPIO_CFG]);
        wm9712_write(AC97_GPIO_POLARITY, wm_regs[AC97_GPIO_POLARITY]);
        wm9712_write(AC97_GPIO_STICKY, wm_regs[AC97_GPIO_STICKY]);
        wm9712_write(AC97_GPIO_WAKEUP, wm_regs[AC97_GPIO_WAKEUP]);
        wm9712_write(AC97_GPIO_STATUS, wm_regs[AC97_GPIO_STATUS]);
        wm9712_write(AC97_MISC_AFE, wm_regs[AC97_MISC_AFE]);
        
	suspended = 0;
	wm9712_write(AC97_POWERDOWN, 0);
	udelay(10);
	wm9712_write(AC97_GPIO_STATUS, 0);
	
	return 0;
}

static int ts_input_usage = 0;

static int a730_ts_input_open(struct input_dev *idev)
{
	down(&input_mutex);
	//if first opening
	if (!ts_input_usage++)
	{
		//start digitiser
	}
	up(&input_mutex);
	return 0;
}

static void a730_ts_input_close(struct input_dev *idev)
{
	down(&input_mutex);
	//if last closing
	if (!--ts_input_usage)
	{
		//stop digitiser	
	}
	up(&input_mutex);
}

////////////////////////////////////////////////////

static int __init a730_ts_probe(struct device *dev)
{
	int err;
	int irqflag = IRQF_DISABLED;
#ifdef CONFIG_PREEMPT_RT
	irqflag |= IRQF_NODELAY;
#endif
	printk("%s\n", __FUNCTION__);
	ac97 = to_ac97_t(dev);
	
	/* setup the input device */
	a730_ts_input = input_allocate_device();
	a730_ts_input->name = "Asus A730(W) touchscreen";
	a730_ts_input->phys = "touchscreen/A730";
	a730_ts_input->evbit[0] = BIT(EV_ABS);
	//input_set_abs_params(a730_ts_input, ABS_X, 190, 3900, 0, 0);
	//input_set_abs_params(a730_ts_input, ABS_Y, 190, 3900, 0, 0);
	input_set_abs_params(a730_ts_input, ABS_X, 350, 3900, 5, 0);
	input_set_abs_params(a730_ts_input, ABS_Y, 320, 3750, 40, 0);
	input_set_abs_params(a730_ts_input, ABS_PRESSURE, 0, MAX_PRESSURE, 0, 0);
	a730_ts_input->id.bustype = BUS_HOST;
	a730_ts_input->open = a730_ts_input_open,
	a730_ts_input->close = a730_ts_input_close,
	input_register_device(a730_ts_input);
	
	/* register battery */
	if(battery_class_register(&a730_battery))
	{
		printk(KERN_ERR "a730_ac97_probe: Could not register battery class\n");
	}
	else
	{
		a730_battery.class_dev.class->release       = a730_battery_class_release;
		//battery.class_dev.class->hotplug       = battery_class_hotplug;
		a730_battery.class_dev.class->class_release = a730_battery_class_class_release;
	}
	
	pendown_workqueue = create_singlethread_workqueue("pendownd");
	INIT_WORK(&pendown_task, pendown_handler, dev);
	phonejack_workqueue = create_singlethread_workqueue("phonejackd");
	INIT_WORK(&phonejack_task, phonejack_handler, dev);
	wm9712_phonejack_delayqueue = create_singlethread_workqueue("phonejackdd");
        INIT_WORK(&wm9712_phonejack_delay_task, wm9712_phonejack_switch_thread, dev);
	
	queue_work(pendown_workqueue, &pendown_task);

	/* setup irq */
	set_irq_type(A730_IRQ(PENDOWN_IRQ), IRQF_TRIGGER_HIGH | IRQF_TRIGGER_LOW | IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING);
	err = request_irq(A730_IRQ(PENDOWN_IRQ), pendown_irq_handler, irqflag, "WM9712 GPIO2", NULL);
	if (err)
	{
		printk("%s: cannot request pen down IRQ\n", __FUNCTION__);
		return err;
	}

	set_irq_type(A730_IRQ(JACK_IRQ), IRQF_TRIGGER_RISING);
	err = request_irq(A730_IRQ(JACK_IRQ), jack_irq_handler, irqflag, "WM9712 GPIO3", NULL);
	if (err)
	{
		printk("%s: cannot request jack detect IRQ\n", __FUNCTION__);
		return err;
	}
	
	wm9712_write(AC97_POWERDOWN, 0);
	a730_ts_setup();
	wm9712_write(AC97_GPIO_STATUS, 0);
	
	return 0;
}

static int a730_ts_remove(struct device *dev)
{
	disable_irq(A730_IRQ(PENDOWN_IRQ));
	disable_irq(A730_IRQ(JACK_IRQ));
	
	wm9712_write(AC97_WM97XX_DIGITISER1, 0);
        wm9712_write(AC97_WM97XX_DIGITISER2, 0);

       	stop_penwork = 1;
        pen_is_down = 1;
        wake_up_interruptible(&wait_q);
        down(&pendown_mutex);
        destroy_workqueue(pendown_workqueue);
        
        stop_jackwork = 1;
        pj_irq = 1;
        wake_up_interruptible(&wait_pj_q);
        down(&jack_mutex);
	destroy_workqueue(phonejack_workqueue);

        battery_class_unregister(&a730_battery);
	input_unregister_device(a730_ts_input);

	free_irq(A730_IRQ(PENDOWN_IRQ), NULL);
	free_irq(A730_IRQ(JACK_IRQ), NULL);
	
	return 0;
}

static struct device_driver a730_ts_driver = {
	.name           = "WM9712",
	.bus            = &ac97_bus_type,
	.probe          = a730_ts_probe,
        .remove		= a730_ts_remove,
#ifdef CONFIG_PM
	.suspend        = a730_ts_suspend,
	.resume         = a730_ts_resume,
#endif
};

static int __init a730_ts_init(void)
{
	if (!machine_is_a730()) return -ENODEV;
	return driver_register(&a730_ts_driver);
}

static void __exit a730_ts_exit(void)
{
	driver_unregister(&a730_ts_driver);
}

module_init(a730_ts_init);
module_exit(a730_ts_exit);

MODULE_AUTHOR("Serge Nikolaenko <mypal_hh@utl.ru>");
MODULE_DESCRIPTION("Touchscreen driver for Asus MyPal A730(W)");
MODULE_LICENSE("GPL");

/*
 * linux/arch/arm/mach-pxa/palmtc/ptc_ucb1400.c
 *
 *  Driver for Palm Tungsten C AC97 codec ucb1400
 *  this gives support for 
 *	-touchscreen
 *	-adc (battery)
 *	-gpio (led, vibra-alarm, etc)
 *
 *  Author: Holger Bocklet <bitz.email@gmx.net>
 *
 * There's noting new under the sun,
 * So this piece of software ist derived of 
 * - the ucb1x00-ts by Russell King, Pavel Machek and
 * - patches for the ucb1400 by Nicols Pitre and
 * - the wm97xx-driver by Liam Girdwood
 */
 
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/jiffies.h>
#include <linux/moduleparam.h>
#include <linux/wait.h>
#include <linux/errno.h>
#include <linux/kthread.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/freezer.h>

#include <asm/delay.h>
#include <asm/arch/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/irqs.h>
#include <asm/arch/ptc_ucb1400.h>
#include <asm/apm.h>

#include <sound/driver.h>
#include <sound/core.h>
#include <sound/ac97_codec.h>

#define PALMTC_UCB_DEBUG

#ifdef PALMTC_UCB_DEBUG
#define DBG(x...) \
	printk(KERN_INFO "PTC_UCB: " x)
#else
#define DBG(x...)	do { } while (0)
#endif

#define TRUE	1
#define FALSE	0


struct ucb1400 {
	struct device		*dev;
	ac97_t 			*ac97;
	struct input_dev	*idev;
	struct task_struct	*thread;
	//wait_queue_head_t 	pen_irq_wait;
	//wait_queue_head_t 	adc_irq_wait;
	int 			pen_irq;
	int 			adc_irq;
	struct workqueue_struct *palmtc_ucb1400_workqueue;
	struct work_struct 	palmtc_ucb1400_work;
	struct completion	thr_init;
	struct completion	thr_exit;
	struct completion	pen_irq_wait;
	struct completion	adc_irq_wait;
	struct mutex		adc_mutex;
	struct mutex		ucb_reg_mutex;
	u8 last_status;
};

#ifdef CONFIG_APM
#define APM_MIN_INTERVAL	1000
    struct ucb1400 *ucb_static_copy;
    struct mutex	apm_mutex;
    struct apm_power_info info_cache;
    unsigned long cache_tstamp;
#endif

//static struct timeval t;

struct ts_reading {
    u16 p;
    u16 x;
    u16 y;
};

//static spinlock_t kbd_lock = SPIN_LOCK_UNLOCKED;



// Pen sampling frequency (Hz) while touched.
// more is not possible cause jiffies are at 100Hz in mach-pxa (why ???)
static int cont_rate = 80;
module_param(cont_rate, int, 0);
MODULE_PARM_DESC(cont_rate, "Sampling rate while pen down (Hz). Not more than ~100Hz on T|C");

/* codec AC97 IO access */
int ucb1400_reg_read(struct ucb1400 *ucb, u16 reg) 
{    
	if (ucb->ac97)
		return ucb->ac97->bus->ops->read(ucb->ac97, reg);
	else
		return -1;
}

void ucb1400_reg_write(struct ucb1400 *ucb, u16 reg, u16 val) 
{
	if (ucb->ac97)
		ucb->ac97->bus->ops->write(ucb->ac97, reg, val);
}

/**
 *	ucb1x00_adc_enable - enable the ADC converter
 *	@ucb: UCB1x00 structure describing chip
 *
 *	Enable the ucb1x00 and ADC converter on the UCB1x00 for use.
 *	Any code wishing to use the ADC converter must call this
 *	function prior to using it.
 *	This function takes the ADC semaphore to prevent two or more
 *	concurrent uses, and therefore may sleep.  As a result, it
 *	can only be called from process context, not interrupt
 *	context.
 */
int ucb1400_adc_enable(struct ucb1400 *ucb)
{
	u16 val=0;

	if (unlikely(mutex_lock_interruptible(&ucb->adc_mutex)))
	    return -ERESTARTSYS;
	ucb1400_reg_write(ucb, UCB_ADC_CR, UCB_ADC_ENA);
	
	mutex_lock(&ucb->ucb_reg_mutex);
	val=ucb1400_reg_read(ucb, UCB_IE_RIS);
	ucb1400_reg_write(ucb, UCB_IE_RIS, val | UCB_IE_ADC);
	val=ucb1400_reg_read(ucb, UCB_IE_FAL);
	ucb1400_reg_write(ucb, UCB_IE_FAL, val | UCB_IE_ADC);
	mutex_unlock(&ucb->ucb_reg_mutex);
	
	return 0;
}


/**
 *	ucb1x00_adc_disable - disable the ADC converter
 *	@ucb: UCB1x00 structure describing chip
 *
 *	Disable the ADC converter and release the ADC semaphore.
 */
void ucb1400_adc_disable(struct ucb1400 *ucb)
{	
	u16 val=0;

	mutex_lock(&ucb->ucb_reg_mutex);
	val &= ~UCB_ADC_ENA;
	ucb1400_reg_write(ucb, UCB_ADC_CR, val);
	val=ucb1400_reg_read(ucb, UCB_IE_RIS);
	ucb1400_reg_write(ucb, UCB_IE_RIS, val & (~UCB_IE_ADC));
	val=ucb1400_reg_read(ucb, UCB_IE_FAL);
	ucb1400_reg_write(ucb, UCB_IE_FAL, val & (~UCB_IE_ADC));
	mutex_unlock(&ucb->ucb_reg_mutex);

	mutex_unlock(&ucb->adc_mutex);
}


/**
 *	ucb1x00_adc_read - read the specified ADC channel
 *	@ucb: UCB1x00 structure describing chip
 *	@adc_channel: ADC channel mask
 *	@sync: wait for syncronisation pulse.
 *
 *	Start an ADC conversion and wait for the result. 
 *	This function currently sleeps waiting for the conversion to
 *	complete (2 frames max without sync).
 */
unsigned int ucb1400_adc_read(struct ucb1400 *ucb, u16 adc_channel)
{
	unsigned int  val;
	//unsigned long tstamp, sleep_time;

	adc_channel |= UCB_ADC_ENA;

	ucb1400_reg_write(ucb, UCB_ADC_CR, adc_channel);
	ucb1400_reg_write(ucb, UCB_ADC_CR, adc_channel | UCB_ADC_START);

    	ucb->adc_irq=FALSE;
	val = ucb1400_reg_read(ucb, UCB_ADC_DATA);
	if ( !(val & UCB_ADC_DATA_VALID)) {
	    wait_for_completion_interruptible_timeout(&ucb->adc_irq_wait,UCB_ADC_TIMEOUT);
	    val = ucb1400_reg_read(ucb, UCB_ADC_DATA);

/*    	ucb->adc_irq=FALSE;
    	wait_event_interruptible( ucb->adc_irq_wait, ucb->adc_irq);
	sleep_time=UCB_ADC_TIMEOUT;
	tstamp=jiffies;
	wait_event_interruptible_timeout( ucb->adc_irq_wait, (ucb->adc_irq) || 
				(((unsigned long)((long)jiffies - (long)tstamp)) >= sleep_time) , sleep_time);
*/
	    if (likely(ucb->adc_irq)) {
    		ucb->adc_irq=FALSE;
	    } else {
		//do_gettimeofday(&t);
		//DBG("adc read timeout: %ld %ld\n",t.tv_sec,t.tv_usec);
		printk(KERN_ERR "%s: read error (timeout on adc%1u), value:0x%x\n",
					__FUNCTION__,(adc_channel&0x0c)>>2, val);

	    }
	
	    if (unlikely ( !(val & UCB_ADC_DATA_VALID) ) ){
		printk(KERN_ERR "%s: read error (data invalid on adc%1u), value:0x%x\n",
					__FUNCTION__,(adc_channel&0x0c)>>2, val);
	    }
	}
return val & UCB_ADC_DATA_MASK; 
}



// create sysfs-entries for adc+gpio
#define UCB1400_SYSFS_ATTR_SHOW(name,input); \
static ssize_t ucb1400_sysfs_##name##_show(struct device *dev, struct device_attribute *attr, char *buf)   \
{ \
	u16 val; \
	struct ucb1400 *ucb = dev_get_drvdata(dev); \
	ucb1400_adc_enable(ucb);\
	val=ucb1400_adc_read(ucb, input); \
	ucb1400_adc_disable(ucb); \
	return sprintf(buf, "%u\n", val); \
}\
static DEVICE_ATTR(name, 0444, ucb1400_sysfs_##name##_show, NULL);

UCB1400_SYSFS_ATTR_SHOW(adc0,UCB_ADC_INP_AD0);
UCB1400_SYSFS_ATTR_SHOW(adc1,UCB_ADC_INP_AD1);
UCB1400_SYSFS_ATTR_SHOW(adc2,UCB_ADC_INP_AD2);
UCB1400_SYSFS_ATTR_SHOW(adc3,UCB_ADC_INP_AD3);

static ssize_t ucb1400_sysfs_io_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ucb1400 *ucb = dev_get_drvdata(dev);
	return sprintf(buf, "0x%x\n", ucb1400_reg_read(ucb, UCB_IO_DATA));
}

static ssize_t ucb1400_sysfs_io_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	u16 val,val1,val2;
	long io_nr;
	struct ucb1400 *ucb = dev_get_drvdata(dev);

	val1=ucb1400_reg_read(ucb, UCB_IO_DIR);
	val2=ucb1400_reg_read(ucb, UCB_IO_DATA);
	//DBG("gpio: dir: %u data: %u buf:%d =>%s<=\n",val1,val2, strlen(buf), buf);

	if(strncmp(buf,"-",1)==0) {
	    if (strlen(buf)>3) 
		return -EINVAL;
	    if (!(isdigit(buf[1])))
		return -EINVAL;
	    io_nr=simple_strtol(buf+1, NULL, 10);

	    mutex_lock(&ucb->ucb_reg_mutex);
	    val=ucb1400_reg_read(ucb, UCB_IO_DIR);
	    ucb1400_reg_write(ucb, UCB_IO_DIR, val | (1 << io_nr)); // set output
	    val=ucb1400_reg_read(ucb, UCB_IO_DATA);
	    ucb1400_reg_write(ucb, UCB_IO_DATA, val & (~(1 << io_nr))); // unset bit
	    mutex_unlock(&ucb->ucb_reg_mutex);
	} else {
	    if (strlen(buf)>2) 
		return -EINVAL;
	    if (!(isdigit(buf[0])))
		return -EINVAL;
	    io_nr=simple_strtol(buf, NULL, 10);
	    mutex_lock(&ucb->ucb_reg_mutex);
	    val=ucb1400_reg_read(ucb, UCB_IO_DIR);
	    ucb1400_reg_write(ucb, UCB_IO_DIR, val | (1 << io_nr)); // set output
	    val=ucb1400_reg_read(ucb, UCB_IO_DATA);
	    ucb1400_reg_write(ucb, UCB_IO_DATA, val | (1 << io_nr)); // set bit
	    mutex_unlock(&ucb->ucb_reg_mutex);
	}

	return 2;
}
static DEVICE_ATTR(gpio, 0664, ucb1400_sysfs_io_show, ucb1400_sysfs_io_store);

#ifdef CONFIG_APM
static void palmtc_get_power_status(struct apm_power_info *info)
{
        struct ucb1400 *ucb=ucb_static_copy;
	u16 battery, a,b,c;
	int i;

	if (mutex_lock_interruptible(&apm_mutex))
	    return;
	//do_gettimeofday(&t);
	//DBG("apm-call sem-down: %ld %ld\n",t.tv_sec,t.tv_usec);

	// if last call with hardware-read is < 5 Sek just return cached info
	if ( ((unsigned long)((long)jiffies - (long)cache_tstamp) ) < APM_MIN_INTERVAL ) {
	    info->ac_line_status = info_cache.ac_line_status;
	    info->battery_status = info_cache.battery_status;
	    info->battery_flag = info_cache.battery_flag;
	    info->battery_life = info_cache.battery_life;
	    info->time = info_cache.time;
	    mutex_unlock(&apm_mutex);
	    return;
	}
	
	//DBG("apm-call read\n");
	info->battery_flag = 0;
	battery=ucb1400_reg_read(ucb, UCB_IO_DATA);
	if (battery & UCB_IO_0) {
		info->ac_line_status = APM_AC_ONLINE;
		info->battery_status = APM_BATTERY_STATUS_CHARGING;
		info->battery_flag |= APM_BATTERY_FLAG_CHARGING;
	} else {
		info->ac_line_status = APM_AC_OFFLINE;
	}
	
	if (ucb1400_adc_enable(ucb))
	    return;
	    
	battery=ucb1400_adc_read(ucb, UCB_ADC_INP_AD0); 
	for (i=0;(i<3);i++) {
	    a=ucb1400_adc_read(ucb, UCB_ADC_INP_AD0); 
	    b=ucb1400_adc_read(ucb, UCB_ADC_INP_AD0); 
	    c=ucb1400_adc_read(ucb, UCB_ADC_INP_AD0); 
	    if ((a==b) && (b==c)) {
		battery=c;
		break;
	    } else {
		battery=(a+b+c+battery)/4;
	    }
	}
	ucb1400_adc_disable(ucb);
	
	if (battery > UCB_BATT_HIGH) {
		info->battery_flag |= APM_BATTERY_FLAG_HIGH;
		info->battery_status = APM_BATTERY_STATUS_HIGH;
	} else {
		if (battery > UCB_BATT_CRITICAL) {
		    if (battery < UCB_BATT_LOW) {
			info->battery_flag |= APM_BATTERY_FLAG_LOW;
			info->battery_status = APM_BATTERY_STATUS_LOW;
		    }
		} else {
		    info->battery_flag |= APM_BATTERY_FLAG_CRITICAL;
		    info->battery_status = APM_BATTERY_STATUS_CRITICAL;
		}
	}
	info->battery_life = ((battery - UCB_BATT_CRITICAL) / (UCB_BATT_HIGH-UCB_BATT_CRITICAL) ) * 100;  //battery_life = %
	if (info->battery_life>100) {
	    info->battery_life=100;
	} else {
	    if (info->battery_life<0) {
		info->battery_life=0;
	    }
	}
	info->time = info->battery_life * UCB_BATT_DURATION / 100; 
	info->units = APM_UNITS_MINS;
	
	info_cache.ac_line_status=info->ac_line_status;
	info_cache.battery_status=info->battery_status;
	info_cache.battery_flag=info->battery_flag;
	info_cache.battery_life=info->battery_life;
	info_cache.time=info->time;
	cache_tstamp=jiffies;
	
	mutex_unlock(&apm_mutex);
}
#endif


// ts - handling routines
static inline void ucb1400_ts_report_event(struct ucb1400 *ucb, struct ts_reading ts)
{
	input_report_abs(ucb->idev, ABS_X, ts.x);
	input_report_abs(ucb->idev, ABS_Y, ts.y);
	input_report_abs(ucb->idev, ABS_PRESSURE, ts.p);
	input_sync(ucb->idev);
}


static inline void ucb1400_ts_event_pen_up(struct ucb1400 *ucb)
{
	input_report_abs(ucb->idev, ABS_PRESSURE, 0);
	input_sync(ucb->idev);
}


/*
 * Switch touchscreen to given interrupt mode.
* param = UCB_IE_RIS || UCB_IE_FAL switch ts-mod to int on edge in param
* param != 0 leave edge as it is
* param == 0 switch ts - int off
 */
static inline void ucb1400_ts_mode_int(struct ucb1400 *ucb, u16 param)
{ 
	u16 val;
	
	// clear remaining ints 
	mutex_lock(&ucb->ucb_reg_mutex);

	val=ucb1400_reg_read(ucb, UCB_IE_STATUS);
	ucb1400_reg_write(ucb, UCB_IE_CLEAR, val);
	
	if (param==0) { // switch off int

		ucb1400_reg_write(ucb, UCB_TS_CR,
			UCB_TS_CR_TSMX_POW | UCB_TS_CR_TSPX_POW |
			UCB_TS_CR_TSMY_GND | UCB_TS_CR_TSPY_GND );
		
		val=ucb1400_reg_read(ucb, UCB_IE_FAL);
		ucb1400_reg_write(ucb, UCB_IE_FAL, val & (~UCB_IE_TSPX) & (~UCB_IE_TSMX));
		//ucb1400_reg_write(ucb, UCB_IE_FAL, val & (~UCB_IE_TSPX));
		val=ucb1400_reg_read(ucb, UCB_IE_RIS);
		ucb1400_reg_write(ucb, UCB_IE_RIS, val & (~UCB_IE_TSPX) & (~UCB_IE_TSMX));
		//ucb1400_reg_write(ucb, UCB_IE_RIS, val & (~UCB_IE_TSPX));
		//DBG("ts irq-off");
		
	} else {

		switch(param) {
			case (UCB_IE_RIS):
				val=ucb1400_reg_read(ucb, UCB_IE_RIS);
				ucb1400_reg_write(ucb,  UCB_IE_RIS, val | UCB_IE_TSPX);
				val=ucb1400_reg_read(ucb, UCB_IE_FAL);
				ucb1400_reg_write(ucb,  UCB_IE_FAL, val & (~UCB_IE_TSPX) );
				//DBG(" ucb1400_intmode_Ris\n");
				break;
			case (UCB_IE_FAL):
				val=ucb1400_reg_read(ucb, UCB_IE_FAL);
				ucb1400_reg_write(ucb,  UCB_IE_FAL, val | UCB_IE_TSPX);
				val=ucb1400_reg_read(ucb, UCB_IE_RIS);
				ucb1400_reg_write(ucb,  UCB_IE_RIS, val & (~UCB_IE_TSPX) );
				break;
		}
		ucb1400_reg_write(ucb, UCB_TS_CR,
			UCB_TS_CR_TSMX_POW | UCB_TS_CR_TSPX_POW |
			UCB_TS_CR_TSMY_GND | UCB_TS_CR_TSPY_GND |
			UCB_TS_CR_MODE_INT);
	}
	mutex_unlock(&ucb->ucb_reg_mutex);
	//DBG("ts irqcontrol: fal: 0x%x ris: 0x%x\n",ucb1400_reg_read(ucb, UCB_IE_FAL),ucb1400_reg_read(ucb, UCB_IE_RIS));
}


/*
 * Switch to pressure mode, and read pressure.  We don't need to wait
 * here, since both plates are being driven.
 */
static inline unsigned int ucb1400_ts_read_pressure(struct ucb1400 *ucb)
{
	ucb1400_reg_write(ucb, UCB_TS_CR,
			UCB_TS_CR_TSMX_POW | UCB_TS_CR_TSPX_POW |
			UCB_TS_CR_TSMY_GND | UCB_TS_CR_TSPY_GND |
			UCB_TS_CR_MODE_PRES | UCB_TS_CR_BIAS_ENA);

	return ucb1400_adc_read(ucb, UCB_ADC_INP_TSPY);
}

/*
 * Switch to X position mode and measure Y plate.  We switch the plate
 * configuration in pressure mode, then switch to position mode.  This
 * gives a faster response time.  Even so, we need to wait about 55us
 * for things to stabilise.
 */
static inline unsigned int ucb1400_ts_read_xpos(struct ucb1400 *ucb)
{
	ucb1400_reg_write(ucb, UCB_TS_CR,
			  UCB_TS_CR_TSMX_GND | UCB_TS_CR_TSPX_POW |
			  UCB_TS_CR_MODE_PRES | UCB_TS_CR_BIAS_ENA);
	ucb1400_reg_write(ucb, UCB_TS_CR,
			  UCB_TS_CR_TSMX_GND | UCB_TS_CR_TSPX_POW |
			  UCB_TS_CR_MODE_PRES | UCB_TS_CR_BIAS_ENA);

	ucb1400_reg_write(ucb, UCB_TS_CR,
			UCB_TS_CR_TSMX_GND | UCB_TS_CR_TSPX_POW |
			UCB_TS_CR_MODE_POS | UCB_TS_CR_BIAS_ENA);

	udelay(55);

	return ucb1400_adc_read(ucb, UCB_ADC_INP_TSPY);
}

/*
 * Switch to Y position mode and measure X plate.  We switch the plate
 * configuration in pressure mode, then switch to position mode.  This
 * gives a faster response time.  Even so, we need to wait about 55us
 * for things to stabilise.
 */
static inline unsigned int ucb1400_ts_read_ypos(struct ucb1400 *ucb)
{
	ucb1400_reg_write(ucb, UCB_TS_CR,
			  UCB_TS_CR_TSMY_GND | UCB_TS_CR_TSPY_POW |
			  UCB_TS_CR_MODE_PRES | UCB_TS_CR_BIAS_ENA);
	ucb1400_reg_write(ucb, UCB_TS_CR,
			  UCB_TS_CR_TSMY_GND | UCB_TS_CR_TSPY_POW |
			  UCB_TS_CR_MODE_PRES | UCB_TS_CR_BIAS_ENA);

	ucb1400_reg_write(ucb, UCB_TS_CR,
			UCB_TS_CR_TSMY_GND | UCB_TS_CR_TSPY_POW |
			UCB_TS_CR_MODE_POS | UCB_TS_CR_BIAS_ENA);

	udelay(55);

	return ucb1400_adc_read(ucb, UCB_ADC_INP_TSPX);
}


static int ucb1400_ts_take_reading(struct ucb1400 *ucb, struct ts_reading *ts)
{
	//ucb1400_adc_enable(ucb);
	if (ucb1400_adc_enable(ucb))
	    return -ERESTARTSYS;

	ts->x = ucb1400_ts_read_xpos(ucb);
	ts->y = ucb1400_ts_read_ypos(ucb);
	ts->p = ucb1400_ts_read_pressure(ucb);

	ucb1400_adc_disable(ucb);
	return 0;
}

// ISR for ac97, shared with pxa2xx-ac97, and later with ucb1400-gpio and ucb1400-adc
static irqreturn_t ucb1400_irq(int irqnr, void *devid)
{
    u32 status;    
    struct ucb1400 *ucb = (struct ucb1400 *) devid;

	status=GSR;
	if (likely(status & GSR_GSCI)) {
		GSR&=GSR_GSCI;

		queue_work(ucb->palmtc_ucb1400_workqueue, &ucb->palmtc_ucb1400_work);

		return IRQ_HANDLED;
	} else {
		return IRQ_NONE;
	}
}


static void palmtc_ucb1400_irq_queuework(struct work_struct *data)
{
    struct ucb1400 *ucb = (struct ucb1400 *) data;
    u16 val,val1,val2;

    //mutex_trylock(&ucb->ucb_reg_mutex); don't: this delays irqs !
    // check which irq and clear
    val=ucb1400_reg_read(ucb, UCB_IE_STATUS);
    ucb1400_reg_write(ucb, UCB_IE_CLEAR, val);
    val1=ucb1400_reg_read(ucb, UCB_IE_EXTRA);
    ucb1400_reg_write(ucb, UCB_IE_EXTRA, val1);
    //mutex_unlock(&ucb->ucb_reg_mutex);
    
    //if ((val & UCB_IE_ADC) && (ucb->adc_irq==FALSE)) {
    if ((val & UCB_IE_ADC) ) {
	ucb->adc_irq=TRUE;
	complete(&ucb->adc_irq_wait);
	//wake_up_interruptible(&ucb->adc_irq_wait);
    }
    //if ((val & UCB_IE_TSPX) && (ucb->pen_irq==FALSE)) {
    if ((val & UCB_IE_TSPX)) {
	ucb->pen_irq=TRUE;
	complete(&ucb->pen_irq_wait);
	//do_gettimeofday(&t);
	//DBG("irq_wq_TS: %ld %ld\n",t.tv_sec,t.tv_usec);
	//wake_up_interruptible(&ucb->pen_irq_wait);
    }
    if (val & UCB_IE_IO) {
	//show it
	val1=ucb1400_reg_read(ucb, UCB_IO_DIR);
	val2=ucb1400_reg_read(ucb, UCB_IO_DATA);
	DBG("gpio_irq: irq: 0x%u dir: %u data: %u\n",val,val1,val2);
    }
}


/*
* The touchscreen reader thread.
*/ 
static int ucb1400_ts_read_thread(void *data) 
{
	unsigned long  sleep_time; 
	int pen_was_down=FALSE;
	//unsigned long tstamp;  // timestamp (jiffies)
	struct ts_reading ts_values;
	struct ucb1400 *ucb = (struct ucb1400 *) data; 

	/* set up thread context */ 
	ucb->thread = current;

	daemonize("kpalmtcucbd");

	if (ucb->ac97 == NULL) {
		ucb->thread = NULL;
		printk(KERN_ERR "ac97-codec is NULL in read-thread, bailing out\n"); 
	}

	// read-rate in hertz => sleeptime in jiffies
	sleep_time = msecs_to_jiffies(1000/cont_rate);

	ucb1400_ts_mode_int(ucb, 0); 
	ucb1400_ts_mode_int(ucb, UCB_IE_FAL);
	complete(&ucb->thr_init); 
	
	/* touch reader loop */ 
	while (ucb->thread) {
    		ucb->pen_irq=FALSE;
		//do_gettimeofday(&t);
		//DBG("kthread-1: %ld %ld\n",t.tv_sec,t.tv_usec);
    		//wait_event_interruptible( ucb->pen_irq_wait, ucb->pen_irq);
		wait_for_completion_interruptible(&ucb->pen_irq_wait);
		if (ucb->thread) { // are we still alive?
			do {
				ucb1400_ts_mode_int(ucb, 0); // int off / clear int
				if (! (ucb1400_ts_take_reading(ucb, &ts_values))) {

				ucb1400_ts_mode_int(ucb, UCB_IE_RIS); //int on rising edge=wait for penup
				ucb1400_ts_report_event(ucb, ts_values);
				pen_was_down=TRUE;
				//DBG("kthread x: %d y: %d p: %d\n",ts_values.x,ts_values.y,ts_values.p);
				//tstamp=jiffies;
				// This marco needs a TRUE condition to return, Timeout alone without TRUE does NOT return, 
				// so make up a TRUE when timeout. A bit bloated though ....
    				ucb->pen_irq=FALSE;
				wait_for_completion_interruptible_timeout(&ucb->pen_irq_wait,sleep_time);
				//wait_event_interruptible_timeout( ucb->pen_irq_wait, (ucb->pen_irq) || 
				//	(((unsigned long)((long)jiffies - (long)tstamp)) >= sleep_time) , sleep_time);
				}
			} while(! ucb->pen_irq);
			
			if (ucb->thread)
			    ucb1400_ts_mode_int(ucb, UCB_IE_FAL); //int on falling edge=wait for pendwon
			if (pen_was_down) {
				ucb1400_ts_event_pen_up(ucb); // do pen-up for input
				pen_was_down=FALSE;
			} else {
				printk(KERN_ERR "Unexpected Programmer Failure: Pendown-IRQ without pendown ???\n");
			}
		}
		try_to_freeze();
	}
	complete_and_exit(&ucb->thr_exit, 0); 
}

static int __init ptc_ucb1400_probe(struct device *dev)
{
	u16 val;
	int err;	
	struct ucb1400 *ucb;
	int ret = -ENODEV;

	ucb = kmalloc(sizeof(struct ucb1400), GFP_KERNEL);
	if (!ucb) {
		printk(KERN_ERR "Cant allocate memory for device struct\n");
		ret=-ENOMEM;
		goto errout;
	}
	memset(ucb, 0, sizeof(struct ucb1400));
 	ucb->ac97 = to_ac97_t(dev);
	dev->driver_data = ucb;
	val = ucb1400_reg_read(ucb, AC97_VENDOR_ID2);
	if (val != UCB_ID_1400) {
		printk(KERN_ERR "no ucb1400 Chip ID (%04x) found: %04x\n",UCB_ID_1400, val);
		ret=-ENODEV;
		goto rollback1;
	}

	printk(KERN_INFO "detected a ucb1400 codec on the ac97-bus, enabling touchscreen-driver\n");

	/* setup the input device */
	ucb->idev = input_allocate_device();
	if (ucb->idev==NULL) {
		printk(KERN_ERR "cannot allocate input-device for ucb1400-touchscreen\n");
		ret= -ENOMEM;
		goto rollback1;
	}

	ucb->idev->evbit[0] = BIT(EV_KEY) | BIT(EV_ABS);
	ucb->idev->keybit[LONG(BTN_TOUCH)] = BIT(BTN_TOUCH);
	input_set_abs_params(ucb->idev, ABS_X, UCB_TS_X_MIN, UCB_TS_X_MAX, UCB_TS_FUZZ,  UCB_TS_FUZZ);
	input_set_abs_params(ucb->idev, ABS_Y, UCB_TS_Y_MIN, UCB_TS_Y_MAX, UCB_TS_FUZZ,  UCB_TS_FUZZ);
	input_set_abs_params(ucb->idev, ABS_PRESSURE, UCB_TS_PRESSURE_MIN, UCB_TS_PRESSURE_MAX, 1, 1);

	ucb->idev->name = "Palm Tungsten C touchscreen (ucb1400)";
/*	ucb->idev->dev = dev; */
	input_register_device(ucb->idev);

	// clear ucb1400-ints if there are
	ucb1400_reg_write(ucb, UCB_IE_RIS, 0);
	ucb1400_reg_write(ucb, UCB_IE_FAL, 0);

	val=ucb1400_reg_read(ucb, UCB_IE_STATUS);
	ucb1400_reg_write(ucb, UCB_IE_CLEAR, val);
	val=ucb1400_reg_read(ucb, UCB_IE_EXTRA);
	ucb1400_reg_write(ucb, UCB_IE_EXTRA, val);

	// start reader-thread for touchscreen
	ucb->pen_irq=FALSE;
	ucb->adc_irq=FALSE;
	init_completion(&ucb->thr_init);
	init_completion(&ucb->thr_exit);
	init_completion(&ucb->pen_irq_wait);
	mutex_init(&ucb->ucb_reg_mutex);
	//init_MUTEX(&ucb->ucb_reg_sem);
	//init_waitqueue_head(&ucb->pen_irq_wait);
	//ucb1400_ts_mode_int(ucb, 0);
	ret = kernel_thread(ucb1400_ts_read_thread, ucb, CLONE_KERNEL);
	if (ret > 0) {
		wait_for_completion(&ucb->thr_init); 
		if (ucb->thread == NULL)  {
			printk(KERN_ERR "cannot start kernel-thread for ucb1400-touchscreen\n");
			ret=-EINVAL;
			goto rollback2;
		}
	} 
	// init workqueue for irq-handling
	ucb->palmtc_ucb1400_workqueue = create_workqueue("kpalmtcucbw");
	INIT_WORK(&ucb->palmtc_ucb1400_work, palmtc_ucb1400_irq_queuework);
	
	//adc
	val=ucb1400_reg_read(ucb, UCB_CSR2);
	ucb1400_reg_write(ucb, UCB_CSR2, val | UCB_ADC_FILTER_ENA);
	
	//init_waitqueue_head(&ucb->adc_irq_wait);
	init_completion(&ucb->adc_irq_wait);
	mutex_init(&ucb->adc_mutex);
	//init_MUTEX(&ucb->adc_sem);
	//sema_init(&ucb->adc_sem, 1);

	// get our share of AC97-Bus interrupt
	err = request_irq(IRQ_AC97, ucb1400_irq, SA_SHIRQ, "ucb1400", ucb);
	if (err) {
		printk(KERN_ERR "ucb1400: IRQ request failed\n");
		ret=-ENODEV;
		goto rollback3;
	}
	
	//enable gpio-irqs
	val=ucb1400_reg_read(ucb, UCB_IE_RIS);
	ucb1400_reg_write(ucb, UCB_IE_RIS, val | UCB_IE_IO);
	val=ucb1400_reg_read(ucb, UCB_IE_FAL);
	ucb1400_reg_write(ucb, UCB_IE_FAL, val | UCB_IE_IO);
	
	ucb->last_status=0;
	val= ucb1400_reg_read(ucb, UCB_CSR1); // remeber old int-status and set ints to ON 
	if (val & UCB_CS_GPIO_AC97_INT) {
		ucb->last_status&=UCB_CS_GPIO_AC97_INT;
	} else {
		ucb1400_reg_write(ucb, UCB_CSR1, val | UCB_CS_GPIO_AC97_INT); // enable "gpio"-interrupt on ucb1400
	}
	val=GCR;
	if (val & GCR_GIE) {
		ucb->last_status&=GCR_GIE;
	} else {
		GCR|=GCR_GIE; // enable interrupt on pxa-ac97
	}

	//sysfs
	device_create_file(dev, &dev_attr_adc0);    
	device_create_file(dev, &dev_attr_adc1);    
	device_create_file(dev, &dev_attr_adc2);    
	device_create_file(dev, &dev_attr_adc3);    
	device_create_file(dev, &dev_attr_gpio);    

#ifdef CONFIG_APM
	//sema_init(&apm_sem, 1);
	info_cache.ac_line_status=APM_AC_UNKNOWN;
	info_cache.battery_status=APM_BATTERY_STATUS_UNKNOWN;
	info_cache.battery_flag=APM_BATTERY_FLAG_UNKNOWN;
	info_cache.battery_life=-1;
	info_cache.time=-1;
	info_cache.units=APM_UNITS_MINS;
	cache_tstamp=0;
	//init_MUTEX(&apm_sem);
	mutex_init(&apm_mutex);
	ucb_static_copy=ucb; //sorry for this, apm-power-function doesnt have a parm to pass dev...
	apm_get_power_status = palmtc_get_power_status;
#endif

	
    return 0;

rollback3:
	if (ucb->thread) {
		ucb->thread = NULL; 
		ucb->pen_irq = TRUE;
		complete_all(&ucb->pen_irq_wait);
		complete_all(&ucb->adc_irq_wait);
		//wake_up_interruptible(&ucb->pen_irq_wait);
		wait_for_completion(&ucb->thr_exit); 
	}
rollback2:	
	input_unregister_device(ucb->idev);
rollback1:
	kfree(ucb);
errout:
	return ret;	
}

static int ptc_ucb1400_remove(struct device *dev)
{
	struct ucb1400 *ucb = dev_get_drvdata(dev);
	u16 val;

	//sysfs
	device_remove_file(dev, &dev_attr_adc0);    
	device_remove_file(dev, &dev_attr_adc1);    
	device_remove_file(dev, &dev_attr_adc2);    
	device_remove_file(dev, &dev_attr_adc3);    
	device_remove_file(dev, &dev_attr_gpio);    

#ifdef CONFIG_APM
	apm_get_power_status = NULL;;
#endif
	ucb->adc_irq = TRUE;
	complete_all(&ucb->adc_irq_wait);
	//wake_up_interruptible(&ucb->adc_irq_wait);

	// disable "GPIO"-int in pxa-ac97 if we found it disabled
	if ((ucb->last_status & GCR_GIE) == 0)
		GCR&=~GCR_GIE; 
	if ((ucb->last_status & UCB_CS_GPIO_AC97_INT) == 0){
		val=ucb1400_reg_read(ucb, UCB_CSR1);
		val&=~UCB_CS_GPIO_AC97_INT;
		ucb1400_reg_write(ucb, UCB_CSR1, val); 
	}

	//stop kthread
	if (ucb->thread) {
		ucb->thread = NULL; 
		ucb->pen_irq = TRUE;
		complete_all(&ucb->pen_irq_wait);
		//wake_up_interruptible(&ucb->pen_irq_wait);
		wait_for_completion(&ucb->thr_exit); 
	}

	ucb1400_ts_mode_int(ucb, 0);
	
	input_unregister_device(ucb->idev);
	free_irq(IRQ_AC97, ucb);
	kfree(ucb);
	return 0;
}

static struct device_driver ptc_ucb1400_driver = {
	.name           = "ucb1400",
	.bus            = &ac97_bus_type,
	.probe          = ptc_ucb1400_probe,
        .remove		= ptc_ucb1400_remove,
#ifdef CONFIG_PM
	.suspend        = NULL, //FIXME do resume and suspend 
	.resume         = NULL,
#endif
};

static int __init ptc_ucb1400_init(void)
{
	
	return driver_register(&ptc_ucb1400_driver);
}

static void ptc_ucb1400_exit(void)
{
	driver_unregister(&ptc_ucb1400_driver);
}

module_init(ptc_ucb1400_init);
module_exit(ptc_ucb1400_exit);

MODULE_AUTHOR ("Holger Bocklet <bitz.email@gmx.net>");
MODULE_DESCRIPTION ("UCB1400 support for Palm Tungsten C");
MODULE_LICENSE ("GPL");

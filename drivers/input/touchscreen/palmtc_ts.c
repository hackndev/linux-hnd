/*
 *  Philips UCB1400 touchscreen driver modified for PalmTC
 *
 * Author: Marek Vasut
 * Heavily based on ucb1400 driver by Nicolas Pitre
 *
 * PS. I should really get rid of this wicked driver, but for now ...
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This code is heavily based on ucb1x00-*.c copyrighted by Russell King
 * covering the UCB1100, UCB1200 and UCB1300..  Support for the UCB1400 has
 * been made separate from ucb1x00-core/ucb1x00-ts on Russell's request.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/ucb1400.h>
#include <linux/ctype.h>
#include <linux/power_supply.h>
#include <linux/leds.h>
#include <linux/apm-emulation.h>

#include <asm/arch/palmtc-gpio.h>
#include <asm/arch/gpio.h>

#include <sound/driver.h>
#include <sound/core.h>
#include <sound/ac97_codec.h>

/*
 * Interesting UCB1400 AC-link registers
 */

struct ucb1400 {
	struct snd_ac97		*ac97;
	struct input_dev	*ts_idev;

	int			irq;

	wait_queue_head_t	ts_wait;
	struct task_struct	*ts_task;

	unsigned int		irq_pending;	/* not bit field shared */
	unsigned int		ts_restart:1;
	unsigned int		adcsync:1;
	struct mutex		ucb_reg_mutex;
};
struct ucb1400_battery_dev
{
    struct ucb1400 * ucb;
    int battery_registered;
    int current_voltage;
    int previous_voltage;
    u32 last_battery_update;
};
	  
struct ucb1400_battery_dev bat;

#ifdef CONFIG_APM_EMULATION
#define APM_MIN_INTERVAL        1000
    struct ucb1400		*ucb_static_copy;
    struct mutex		apm_mutex;
    struct apm_power_info	pwr_info;
    unsigned long		cache_tstamp;
#endif

static int adcsync;

static inline u16 ucb1400_reg_read(struct ucb1400 *ucb, u16 reg)
{
	if (ucb->ac97)
	    return ucb->ac97->bus->ops->read(ucb->ac97, reg);
	else
	    return -1;
}

static inline void ucb1400_reg_write(struct ucb1400 *ucb, u16 reg, u16 val)
{
	if (ucb->ac97)
	    ucb->ac97->bus->ops->write(ucb->ac97, reg, val);
}

static inline void ucb1400_adc_enable(struct ucb1400 *ucb)
{
        u16 val=0;

        ucb1400_reg_write(ucb, UCB_ADC_CR, UCB_ADC_ENA);
        val=ucb1400_reg_read(ucb, UCB_IE_RIS);
        ucb1400_reg_write(ucb, UCB_IE_RIS, val | UCB_IE_ADC);
        val=ucb1400_reg_read(ucb, UCB_IE_FAL);
        ucb1400_reg_write(ucb, UCB_IE_FAL, val | UCB_IE_ADC);

/*	ucb1400_reg_write(ucb, UCB_ADC_CR, UCB_ADC_ENA); */
}

static unsigned int ucb1400_adc_read(struct ucb1400 *ucb, u16 adc_channel)
{
	unsigned int val;

	if (ucb->adcsync)
		adc_channel |= UCB_ADC_SYNC_ENA;

	ucb1400_reg_write(ucb, UCB_ADC_CR, UCB_ADC_ENA | adc_channel);
	ucb1400_reg_write(ucb, UCB_ADC_CR, UCB_ADC_ENA | adc_channel | UCB_ADC_START);

	for (;;) {
		val = ucb1400_reg_read(ucb, UCB_ADC_DATA);
		if (val & UCB_ADC_DAT_VALID)
			break;
		/* yield to other processes */
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(1);
	}

	return UCB_ADC_DAT_VALUE(val);
}

static inline void ucb1400_adc_disable(struct ucb1400 *ucb)
{
        u16 val=0;

        val &= ~UCB_ADC_ENA;
        ucb1400_reg_write(ucb, UCB_ADC_CR, val);
        val=ucb1400_reg_read(ucb, UCB_IE_RIS);
        ucb1400_reg_write(ucb, UCB_IE_RIS, val & (~UCB_IE_ADC));
        val=ucb1400_reg_read(ucb, UCB_IE_FAL);
        ucb1400_reg_write(ucb, UCB_IE_FAL, val & (~UCB_IE_ADC));
/*	ucb1400_reg_write(ucb, UCB_ADC_CR, 0);
*/
}

/* Switch to interrupt mode. */
static inline void ucb1400_ts_mode_int(struct ucb1400 *ucb)
{
	ucb1400_reg_write(ucb, UCB_TS_CR,
			UCB_TS_CR_TSMX_POW | UCB_TS_CR_TSPX_POW |
			UCB_TS_CR_TSMY_GND | UCB_TS_CR_TSPY_GND |
			UCB_TS_CR_MODE_INT);
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

/*
 * Switch to X plate resistance mode.  Set MX to ground, PX to
 * supply.  Measure current.
 */
static inline unsigned int ucb1400_ts_read_xres(struct ucb1400 *ucb)
{
	ucb1400_reg_write(ucb, UCB_TS_CR,
			UCB_TS_CR_TSMX_GND | UCB_TS_CR_TSPX_POW |
			UCB_TS_CR_MODE_PRES | UCB_TS_CR_BIAS_ENA);
	return ucb1400_adc_read(ucb, 0);
}

/*
 * Switch to Y plate resistance mode.  Set MY to ground, PY to
 * supply.  Measure current.
 */
static inline unsigned int ucb1400_ts_read_yres(struct ucb1400 *ucb)
{
	ucb1400_reg_write(ucb, UCB_TS_CR,
			UCB_TS_CR_TSMY_GND | UCB_TS_CR_TSPY_POW |
			UCB_TS_CR_MODE_PRES | UCB_TS_CR_BIAS_ENA);
	return ucb1400_adc_read(ucb, 0);
}

static inline int ucb1400_ts_pen_down(struct ucb1400 *ucb)
{
	unsigned short val = ucb1400_reg_read(ucb, UCB_TS_CR);
	return (val & (UCB_TS_CR_TSPX_LOW | UCB_TS_CR_TSMX_LOW));
}

static inline void ucb1400_ts_irq_enable(struct ucb1400 *ucb)
{
	ucb1400_reg_write(ucb, UCB_IE_CLEAR, UCB_IE_TSPX);
	ucb1400_reg_write(ucb, UCB_IE_CLEAR, 0);
	ucb1400_reg_write(ucb, UCB_IE_FAL, UCB_IE_TSPX);
}

static inline void ucb1400_ts_irq_disable(struct ucb1400 *ucb)
{
	ucb1400_reg_write(ucb, UCB_IE_FAL, 0);
}

static void ucb1400_ts_evt_add(struct input_dev *idev, u16 pressure, u16 x, u16 y)
{
	input_report_abs(idev, ABS_X, x);
	input_report_abs(idev, ABS_Y, y);
	input_report_abs(idev, ABS_PRESSURE, pressure);
	input_sync(idev);
}

static void ucb1400_ts_event_release(struct input_dev *idev)
{
	input_report_abs(idev, ABS_PRESSURE, 0);
	input_sync(idev);
}

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

static int ucb1400_get_gpio_val(int gpio)
{
        struct ucb1400 *ucb=ucb_static_copy;
	return ((((ucb1400_reg_read(ucb, UCB_IO_DATA)) & (1 << gpio)))?1:0);
}

static int ucb1400_get_gpio_dir(int gpio)
{
        struct ucb1400 *ucb=ucb_static_copy;
	return ((((ucb1400_reg_read(ucb, UCB_IO_DIR)) & (1 << gpio)))?1:0);
}

static void ucb1400_set_gpio_val(int gpio, int set)
{
        struct ucb1400 *ucb=ucb_static_copy;
	u16 val;

        val=ucb1400_reg_read(ucb, UCB_IO_DATA);

	if(set)
            ucb1400_reg_write(ucb, UCB_IO_DATA, val | (1 << gpio)); // set bit
	else
            ucb1400_reg_write(ucb, UCB_IO_DATA, val & ~(1 << gpio)); // unset bit
}

static void ucb1400_set_gpio_dir(int gpio, int dir_in)
{
        struct ucb1400 *ucb=ucb_static_copy;
	u16 val;

        val=ucb1400_reg_read(ucb, UCB_IO_DIR);

	if(dir_in)
            ucb1400_reg_write(ucb, UCB_IO_DIR, val & ~(1 << gpio)); // set input
	else
            ucb1400_reg_write(ucb, UCB_IO_DIR, val | (1 << gpio)); // set output
}

static ssize_t ucb1400_sysfs_io_show(struct device *dev, struct device_attribute *attr, char *buf)
{
//        struct ucb1400 *ucb = dev_get_drvdata(dev);

	return sprintf(buf,"GPIO  VAL  DIR\n"
	"  0    %i    %i\n  1    %i    %i\n"
	"  2    %i    %i\n  3    %i    %i\n"
	"  4    %i    %i\n  5    %i    %i\n"
	"  6    %i    %i\n  7    %i    %i\n"
	"  8    %i    %i\n  9    %i    %i\n",
	ucb1400_get_gpio_val(0),ucb1400_get_gpio_dir(0),
	ucb1400_get_gpio_val(1),ucb1400_get_gpio_dir(1),
	ucb1400_get_gpio_val(2),ucb1400_get_gpio_dir(2),
	ucb1400_get_gpio_val(3),ucb1400_get_gpio_dir(3),
	ucb1400_get_gpio_val(4),ucb1400_get_gpio_dir(4),
	ucb1400_get_gpio_val(5),ucb1400_get_gpio_dir(5),
	ucb1400_get_gpio_val(6),ucb1400_get_gpio_dir(6),
	ucb1400_get_gpio_val(7),ucb1400_get_gpio_dir(7),
	ucb1400_get_gpio_val(8),ucb1400_get_gpio_dir(8),
	ucb1400_get_gpio_val(9),ucb1400_get_gpio_dir(9));	
}

static ssize_t ucb1400_sysfs_io_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
        unsigned long ugpio, ugpio_val;
//        struct ucb1400 *ucb = dev_get_drvdata(dev);

/*	if(strncmp(buf,"i",1) && strncmp(buf,"o",1) && strncmp(buf,"w",1)) {
	    printk("Unknown argument, usage:\n"
		    "iX - to set gpio X as input\n"
		    "oX - to set gpio X as output\n"
		    "wXY - to set gpio X to value Y (output only)\n");
	return -EINVAL;
	}
*/	
        if ((strlen(buf)>3) && strncmp(buf,"w",1))
	{
            return -EINVAL;
	}
        if ((strlen(buf)>4) && !(strncmp(buf,"w",1)))
	{
            return -EINVAL;
	}
        if (!(isdigit(buf[1])))
            return -EINVAL;

        ugpio=simple_strtol(buf+1, NULL, 10);

        if(strncmp(buf,"i",1)==0) {
	    printk("setting gpio %li as input\n",ugpio);
	    ucb1400_set_gpio_dir(ugpio,1);

	} else if(strncmp(buf,"o",1)==0) {
	    printk("setting gpio %li as output\n",ugpio);	    
	    ucb1400_set_gpio_dir(ugpio,0);

	} else if(strncmp(buf,"w",1)==0) {
	    ugpio_val=simple_strtol(buf+2, NULL, 10);
	    ugpio=((ugpio - ugpio_val)/10);
	    printk("setting gpio %li to %li\n",ugpio,ugpio_val);	    
	    ucb1400_set_gpio_dir(ugpio,0);
	    ucb1400_set_gpio_val(ugpio,ugpio_val);
	}

        return -EINVAL;
}
static DEVICE_ATTR(gpio, 0664, ucb1400_sysfs_io_show, ucb1400_sysfs_io_store);

static void ucb1400_vibra_set(struct led_classdev *led_cdev, enum led_brightness value)
{
	ucb1400_set_gpio_dir(5,0);
        if (value)
            ucb1400_set_gpio_val(5,1);
        else
            ucb1400_set_gpio_val(5,0);
}

static void ucb1400_led_set(struct led_classdev *led_cdev, enum led_brightness value)
{
	ucb1400_set_gpio_dir(7,0);
        if (value)
            ucb1400_set_gpio_val(7,1);
        else
            ucb1400_set_gpio_val(7,0);
}

static struct led_classdev ucb1400_gpio_vibra = {
        .name                   = "ucb1400:vibra",
        .brightness_set         = ucb1400_vibra_set,
};

static struct led_classdev ucb1400_gpio_led = {
        .name                   = "ucb1400:led",
        .brightness_set         = ucb1400_led_set,
};


int ucb1400_battery_min_voltage(struct power_supply *b)
{
    return UCB_BATT_MIN_VOLTAGE;
}

int ucb1400_battery_max_voltage(struct power_supply *b)
{
    return UCB_BATT_MAX_VOLTAGE;
}

int ucb1400_battery_get_voltage(struct power_supply *b)
{
    if (bat.battery_registered){
	bat.previous_voltage = bat.current_voltage;
        ucb1400_adc_enable(bat.ucb);
	bat.current_voltage = ucb1400_adc_read(bat.ucb, UCB_ADC_INP_AD0);
        ucb1400_adc_disable(bat.ucb);
	bat.last_battery_update = jiffies;
	return bat.current_voltage * 20;
    } else {
	printk(	"ucb1400_battery: cannot get voltage -> "
		"battery driver unregistered\n");
	return 0;
    }
}

int ucb1400_battery_get_capacity(struct power_supply *b)
{
    if (bat.battery_registered){
        return (((ucb1400_battery_get_voltage(b)-ucb1400_battery_min_voltage(b))
        /(ucb1400_battery_max_voltage(b)-ucb1400_battery_min_voltage(b)))*100);
    }
    else{
        printk( "ucb1400_battery: cannot get capacity -> "
	        "battery driver unregistered\n");
        return 0;
    }
}

int ucb1400_battery_get_status(struct power_supply *b)
{
    int ac_connected  = !(gpio_get_value(GPIO_NR_PALMTC_CRADLE_DETECT_N));
    int usb_connected = gpio_get_value(GPIO_NR_PALMTC_USB_DETECT);

    if ( (ac_connected || usb_connected) &&
       ( ( bat.current_voltage > bat.previous_voltage ) ||
         ( bat.current_voltage <= UCB_BATT_MAX_VOLTAGE ) ) )
	    return POWER_SUPPLY_STATUS_CHARGING;
    else
	    return POWER_SUPPLY_STATUS_NOT_CHARGING;
}

int tmp;

static int ucb1400_battery_get_property(struct power_supply *b,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
        switch (psp) {
        case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
                val->intval = ucb1400_battery_max_voltage(b);
		break;
        case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
                val->intval = ucb1400_battery_min_voltage(b);
		break;
        case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
                val->intval = 100;
		break;
        case POWER_SUPPLY_PROP_CHARGE_EMPTY_DESIGN:
                val->intval = 0;
		break;
        case POWER_SUPPLY_PROP_CHARGE_NOW:
                val->intval = ucb1400_battery_get_capacity(b);
		break;
        case POWER_SUPPLY_PROP_VOLTAGE_NOW:
                val->intval = ucb1400_battery_get_voltage(b);
		break;
        case POWER_SUPPLY_PROP_STATUS:
                val->intval = ucb1400_battery_get_status(b);
		break;
        default:
		break;
        };

        return 0;
}

static enum power_supply_property ucb1400_battery_props[] = {
        POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
        POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
        POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
        POWER_SUPPLY_PROP_CHARGE_EMPTY_DESIGN,
        POWER_SUPPLY_PROP_CHARGE_NOW,
        POWER_SUPPLY_PROP_VOLTAGE_NOW,
        POWER_SUPPLY_PROP_STATUS,
};

struct power_supply ucb1400_battery = {
    .name		= "ucb1400_battery",
    .get_property	= ucb1400_battery_get_property,
    .properties		= ucb1400_battery_props,
    .num_properties	= ARRAY_SIZE(ucb1400_battery_props),
};
						
#if defined(CONFIG_APM_EMULATION) || defined(CONFIG_APM_MODULE)
static void ucb1400_get_power_status(struct apm_power_info *info)
{
        struct ucb1400 *ucb=ucb_static_copy;
        u16 battery, a,b,c;
        int i;
printk("APM: getting battery state\n");

//        if (mutex_lock_interruptible(&apm_mutex))
//            return;
        // if last call with hardware-read is < 5 Sec just return cached info
        if ( ((unsigned long)((long)jiffies - (long)cache_tstamp) ) < APM_MIN_INTERVAL ) {
            info->ac_line_status = pwr_info.ac_line_status;
            info->battery_status = pwr_info.battery_status;
            info->battery_flag = pwr_info.battery_flag;
            info->battery_life = pwr_info.battery_life;
            info->time = pwr_info.time;
//            mutex_unlock(&apm_mutex);
            return;
        }

        info->battery_flag = 0;
        battery=ucb1400_reg_read(ucb, UCB_IO_DATA);
        if (battery & UCB_IO_0) {
                info->ac_line_status = APM_AC_ONLINE;
                info->battery_status = APM_BATTERY_STATUS_CHARGING;
                info->battery_flag |= APM_BATTERY_FLAG_CHARGING;
        } else {
                info->ac_line_status = APM_AC_OFFLINE;
        }

        ucb1400_adc_enable(ucb);

        battery=ucb1400_adc_read(ucb, UCB_ADC_INP_AD0);
	printk("battery: %i\n",battery);
        for (i=0;(i<3);i++) {
            a=ucb1400_adc_read(ucb, UCB_ADC_INP_AD0);
	printk("battery-a: %i\n",a);
            b=ucb1400_adc_read(ucb, UCB_ADC_INP_AD0);
	printk("battery-b: %i\n",b);
            c=ucb1400_adc_read(ucb, UCB_ADC_INP_AD0);
	printk("battery-c: %i\n------------------\n",c);
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
printk("WRITING INFO\n");
printk("%04x %04x %04x %04x %04x %04x | %i\n",info->ac_line_status,info->battery_status,
	info->battery_flag,info->battery_life,info->time,info->units,battery);

/*----------------*/
        ucb1400_adc_enable(ucb);
printk("ADC %i %i %i %i\n",ucb1400_adc_read(ucb, UCB_ADC_INP_AD0),
ucb1400_adc_read(ucb, UCB_ADC_INP_AD1),ucb1400_adc_read(ucb, UCB_ADC_INP_AD2),
ucb1400_adc_read(ucb, UCB_ADC_INP_AD3));
        ucb1400_adc_disable(ucb);
/*----------------*/
	
        pwr_info.ac_line_status=info->ac_line_status;
        pwr_info.battery_status=info->battery_status;
        pwr_info.battery_flag=info->battery_flag;
        pwr_info.battery_life=info->battery_life;
        pwr_info.time=info->time;
        pwr_info.units=info->units;
        cache_tstamp=jiffies;

//        mutex_unlock(&apm_mutex);
}
#endif

static void ucb1400_handle_pending_irq(struct ucb1400 *ucb)
{
	unsigned int isr;

	isr = ucb1400_reg_read(ucb, UCB_IE_STATUS);
	ucb1400_reg_write(ucb, UCB_IE_CLEAR, isr);
	ucb1400_reg_write(ucb, UCB_IE_CLEAR, 0);

	if (isr & UCB_IE_TSPX)
		ucb1400_ts_irq_disable(ucb);
	else
		printk(KERN_ERR "ucb1400: unexpected IE_STATUS = %#x\n", isr);

	enable_irq(ucb->irq);
}

static int ucb1400_ts_thread(void *_ucb)
{
	struct ucb1400 *ucb = _ucb;
	struct task_struct *tsk = current;
	int valid = 0;

	tsk->policy = SCHED_FIFO;
	tsk->rt_priority = 1;

	while (!kthread_should_stop()) {
		unsigned int x, y, p;
		long timeout;

		ucb->ts_restart = 0;

		if (ucb->irq_pending) {
			ucb->irq_pending = 0;
			ucb1400_handle_pending_irq(ucb);
		}

		ucb1400_adc_enable(ucb);
		x = ucb1400_ts_read_xpos(ucb);
		y = ucb1400_ts_read_ypos(ucb);
		p = ucb1400_ts_read_pressure(ucb);
		ucb1400_adc_disable(ucb);

		/* Switch back to interrupt mode. */
		ucb1400_ts_mode_int(ucb);

		msleep(10);

		if (ucb1400_ts_pen_down(ucb)) {
			ucb1400_ts_irq_enable(ucb);

			/*
			 * If we spat out a valid sample set last time,
			 * spit out a "pen off" sample here.
			 */
			if (valid) {
				ucb1400_ts_event_release(ucb->ts_idev);
				valid = 0;
			}

			timeout = MAX_SCHEDULE_TIMEOUT;
		} else {
			valid = 1;
			ucb1400_ts_evt_add(ucb->ts_idev, p, x, y);
			timeout = msecs_to_jiffies(10);
		}

		wait_event_interruptible_timeout(ucb->ts_wait,
			ucb->irq_pending || ucb->ts_restart || kthread_should_stop(),
			timeout);
		try_to_freeze();
	}

	/* Send the "pen off" if we are stopping with the pen still active */
	if (valid)
		ucb1400_ts_event_release(ucb->ts_idev);

	ucb->ts_task = NULL;
	return 0;
}

/*
 * A restriction with interrupts exists when using the ucb1400, as
 * the codec read/write routines may sleep while waiting for codec
 * access completion and uses semaphores for access control to the
 * AC97 bus.  A complete codec read cycle could take  anywhere from
 * 60 to 100uSec so we *definitely* don't want to spin inside the
 * interrupt handler waiting for codec access.  So, we handle the
 * interrupt by scheduling a RT kernel thread to run in process
 * context instead of interrupt context.
 */
static irqreturn_t ucb1400_hard_irq(int irqnr, void *devid)
{
	struct ucb1400 *ucb = devid;

	if (irqnr == ucb->irq) {
		disable_irq(ucb->irq);
		ucb->irq_pending = 1;
		wake_up(&ucb->ts_wait);
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}

static int ucb1400_ts_open(struct input_dev *idev)
{
	struct ucb1400 *ucb = idev->private;
	int ret = 0;

	BUG_ON(ucb->ts_task);

	ucb->ts_task = kthread_run(ucb1400_ts_thread, ucb, "UCB1400_ts");
	if (IS_ERR(ucb->ts_task)) {
		ret = PTR_ERR(ucb->ts_task);
		ucb->ts_task = NULL;
	}

	return ret;
}

static void ucb1400_ts_close(struct input_dev *idev)
{
	struct ucb1400 *ucb = idev->private;

	if (ucb->ts_task)
		kthread_stop(ucb->ts_task);

	ucb1400_ts_irq_disable(ucb);
	ucb1400_reg_write(ucb, UCB_TS_CR, 0);
}

#ifdef CONFIG_PM
static int ucb1400_ts_resume(struct device *dev)
{
	struct ucb1400 *ucb = dev_get_drvdata(dev);

	if (ucb->ts_task) {
		/*
		 * Restart the TS thread to ensure the
		 * TS interrupt mode is set up again
		 * after sleep.
		 */
		ucb->ts_restart = 1;
		wake_up(&ucb->ts_wait);
	}
	return 0;
}
#else
#define ucb1400_ts_resume NULL
#endif

#ifndef NO_IRQ
#define NO_IRQ	0
#endif

/*
 * Try to probe our interrupt, rather than relying on lots of
 * hard-coded machine dependencies.
 */
static int ucb1400_detect_irq(struct ucb1400 *ucb)
{
	unsigned long mask, timeout;

	mask = probe_irq_on();
	if (!mask) {
		probe_irq_off(mask);
		return -EBUSY;
	}

	/* Enable the ADC interrupt. */
	ucb1400_reg_write(ucb, UCB_IE_RIS, UCB_IE_ADC);
	ucb1400_reg_write(ucb, UCB_IE_FAL, UCB_IE_ADC);
	ucb1400_reg_write(ucb, UCB_IE_CLEAR, 0xffff);
	ucb1400_reg_write(ucb, UCB_IE_CLEAR, 0);

	/* Cause an ADC interrupt. */
	ucb1400_reg_write(ucb, UCB_ADC_CR, UCB_ADC_ENA);
	ucb1400_reg_write(ucb, UCB_ADC_CR, UCB_ADC_ENA | UCB_ADC_START);

	/* Wait for the conversion to complete. */
	timeout = jiffies + HZ/2;
	while (!(ucb1400_reg_read(ucb, UCB_ADC_DATA) & UCB_ADC_DAT_VALID)) {
		cpu_relax();
		if (time_after(jiffies, timeout)) {
			printk(KERN_ERR "ucb1400: timed out in IRQ probe\n");
			probe_irq_off(mask);
			return -ENODEV;
		}
	}
	ucb1400_reg_write(ucb, UCB_ADC_CR, 0);

	/* Disable and clear interrupt. */
	ucb1400_reg_write(ucb, UCB_IE_RIS, 0);
	ucb1400_reg_write(ucb, UCB_IE_FAL, 0);
	ucb1400_reg_write(ucb, UCB_IE_CLEAR, 0xffff);
	ucb1400_reg_write(ucb, UCB_IE_CLEAR, 0);

	/* Read triggered interrupt. */
	ucb->irq = probe_irq_off(mask);
	if (ucb->irq < 0 || ucb->irq == NO_IRQ)
		return -ENODEV;

	return 0;
}

static int ucb1400_ts_probe(struct device *dev)
{
	struct ucb1400 *ucb;
	struct input_dev *idev;
	int error, id, x_res, y_res;
	int sysfs_ret;

	ucb = kzalloc(sizeof(struct ucb1400), GFP_KERNEL);
	idev = input_allocate_device();
	if (!ucb || !idev) {
		error = -ENOMEM;
		goto err_free_devs;
	}

	ucb->ts_idev = idev;
	ucb->adcsync = adcsync;
	ucb->ac97 = to_ac97_t(dev);
	init_waitqueue_head(&ucb->ts_wait);

	id = ucb1400_reg_read(ucb, UCB_ID);
	if (id != UCB_ID_1400) {
		error = -ENODEV;
		goto err_free_devs;
	}

	error = ucb1400_detect_irq(ucb);
	if (error) {
		printk(KERN_ERR "UCB1400: IRQ probe failed\n");
		goto err_free_devs;
	}

	error = request_irq(ucb->irq, ucb1400_hard_irq, IRQF_TRIGGER_RISING,
				"UCB1400", ucb);
	if (error) {
		printk(KERN_ERR "ucb1400: unable to grab irq%d: %d\n",
				ucb->irq, error);
		goto err_free_devs;
	}
	printk(KERN_DEBUG "UCB1400: found IRQ %d\n", ucb->irq);

	idev->private		= ucb;
	idev->cdev.dev		= dev;
	idev->name		= "UCB1400 touchscreen interface";
	idev->id.vendor		= ucb1400_reg_read(ucb, AC97_VENDOR_ID1);
	idev->id.product	= id;
	idev->open		= ucb1400_ts_open;
	idev->close		= ucb1400_ts_close;
	idev->evbit[0]		= BIT(EV_ABS);

	ucb1400_adc_enable(ucb);
	x_res = ucb1400_ts_read_xres(ucb);
	y_res = ucb1400_ts_read_yres(ucb);
	ucb1400_adc_disable(ucb);
	printk(KERN_DEBUG "UCB1400: x/y = %d/%d\n", x_res, y_res);

	input_set_abs_params(idev, ABS_X, 0, x_res, 0, 0);
	input_set_abs_params(idev, ABS_Y, 0, y_res, 0, 0);
	input_set_abs_params(idev, ABS_PRESSURE, 0, 0, 0, 0);

	error = input_register_device(idev);
	if (error)
		goto err_free_irq;

	dev_set_drvdata(dev, ucb);

	/* Battery */
	bat.ucb=ucb;

	/* sysfs */
	sysfs_ret = device_create_file(dev, &dev_attr_adc0);
	sysfs_ret = device_create_file(dev, &dev_attr_adc1);
	sysfs_ret = device_create_file(dev, &dev_attr_adc2);
	sysfs_ret = device_create_file(dev, &dev_attr_adc3);
	sysfs_ret = device_create_file(dev, &dev_attr_gpio);
						
	/* register battery to APM layer */
	bat.battery_registered = 0;
	if(power_supply_register(NULL, &ucb1400_battery)) {
	    printk(KERN_ERR "ucb1400_ts: could not register battery class\n");
	} else {
	    bat.battery_registered = 1;
	    printk("Battery registered\n");
	}

	if(led_classdev_register(dev, &ucb1400_gpio_led)) {
	    printk("ucb1400_ts: could not register led class\n");
	} else {
	    printk("LED registered\n");
	}

	if(led_classdev_register(dev, &ucb1400_gpio_vibra)) {
	    printk("ucb1400_ts: could not register led/vibra class\n");
	} else {
	    printk("Vibra registered\n");
	}

        ucb_static_copy=ucb; //sorry for this, apm-power-function doesnt have a parm to pass dev...

#ifdef CONFIG_APM_EMULATION
	printk("PROBING BATTERY\n");
        pwr_info.ac_line_status=APM_AC_UNKNOWN;
        pwr_info.battery_status=APM_BATTERY_STATUS_UNKNOWN;
        pwr_info.battery_flag=APM_BATTERY_FLAG_UNKNOWN;
        pwr_info.battery_life=-1;
        pwr_info.time=-1;
        pwr_info.units=APM_UNITS_MINS;
        cache_tstamp=0;
//        mutex_init(&apm_mutex);
        apm_get_power_status = ucb1400_get_power_status;
#endif
	return 0;

 err_free_irq:
	free_irq(ucb->irq, ucb);
 err_free_devs:
	input_free_device(idev);
	kfree(ucb);
	return error;
}

static int ucb1400_ts_remove(struct device *dev)
{
	struct ucb1400 *ucb = dev_get_drvdata(dev);

	/* sysfs */
	device_remove_file(dev, &dev_attr_adc0);
	device_remove_file(dev, &dev_attr_adc1);
	device_remove_file(dev, &dev_attr_adc2);
	device_remove_file(dev, &dev_attr_adc3);
	device_remove_file(dev, &dev_attr_gpio);

        led_classdev_unregister(&ucb1400_gpio_vibra);
        led_classdev_unregister(&ucb1400_gpio_led);
	power_supply_unregister(&ucb1400_battery);
	
	free_irq(ucb->irq, ucb);
	input_unregister_device(ucb->ts_idev);
	dev_set_drvdata(dev, NULL);
	kfree(ucb);
	return 0;
}

static struct device_driver ucb1400_ts_driver = {
	.name           = "palmtc_ts",
	.owner		= THIS_MODULE,
	.bus		= &ac97_bus_type,
	.probe		= ucb1400_ts_probe,
	.remove		= ucb1400_ts_remove,
	.resume		= ucb1400_ts_resume,
};

static int __init ucb1400_ts_init(void)
{
	return driver_register(&ucb1400_ts_driver);
}

static void __exit ucb1400_ts_exit(void)
{
	driver_unregister(&ucb1400_ts_driver);
}

module_param(adcsync, int, 0444);

module_init(ucb1400_ts_init);
module_exit(ucb1400_ts_exit);

MODULE_DESCRIPTION("Philips UCB1400 touchscreen driver");
MODULE_LICENSE("GPL");

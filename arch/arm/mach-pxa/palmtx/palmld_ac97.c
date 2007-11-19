/*
 * linux/arch/arm/mach-pxa/palmtx/palmld_ac97.c
 *
 *  Touchscreen/battery driver for Palm TX' WM9712 AC97 codec
 *
 *  Based on palmld_ac97.c code from Alex Osborne
 *  
 */

 
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/battery.h>
#include <linux/irq.h>

#include <asm/apm.h>
#include <asm/delay.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/arch/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/irqs.h>

#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/ac97_codec.h>
#include <sound/wm9712.h>

#include <asm/arch/palmtx-gpio.h>
#include <asm/arch/palmtx-init.h>


#define X_AXIS_MAX		  3900
#define X_AXIS_MIN		  350
#define Y_AXIS_MAX		  3750
#define Y_AXIS_MIN		  320
#define PRESSURE_MIN		  0
#define PRESSURE_MAX		  150

#define DIG2_INIT	  	  0x0001 /* initial value for digitiser2 reg */

#define DEFAULT_PRESSURE_TRESHOLD 45160  /* default pressure treshold for pen up/down */ 
#define DEFAULT_X_AXIS_FUZZ	  5	 /* default x axis noise treshold   */
#define DEFAULT_Y_AXIS_FUZZ	  35	 /* default y axis noise treshold   */
#define PRESSURE_FUZZ		  5      /* default pressure noise treshold */

#define palmld_ac97_WORK_QUEUE_NAME	"palmld_ac97_workqueue"

/* module parameters */

static int ptrsh = DEFAULT_PRESSURE_TRESHOLD;
module_param(ptrsh, int, 0);
MODULE_PARM_DESC(ptrsh, "pressure treshold for pen up/down");

static int dbglvl = 0; 		// debug disabled
module_param(dbglvl, int, 0);
MODULE_PARM_DESC(dbglvl, "debug level (0 is disabled)");

static int xdjtrsh = DEFAULT_X_AXIS_FUZZ;
module_param(xdjtrsh, int, 0);
MODULE_PARM_DESC(xdjtrsh, "treshold for x axis jitter");

static int ydjtrsh = DEFAULT_Y_AXIS_FUZZ;
module_param(ydjtrsh, int, 0);
MODULE_PARM_DESC(ydjtrsh, "treshold for y axis jitter");


static DECLARE_MUTEX_LOCKED(queue_sem);
static DECLARE_MUTEX(digitiser_sem);
static DECLARE_MUTEX(battery_update_mutex);

static struct workqueue_struct *palmld_ac97_workqueue;
static struct work_struct palmld_ac97_irq_task;

struct input_dev *palmld_ac97_input;
struct device *palmld_ac97_dev;

static ac97_t *ac97;

static int battery_registered = 0;
static unsigned long last_battery_update = 0;
static int current_voltage;
static int previous_voltage;
static u16 d2base;

/*
 * ac97 codec
 */
 
void wm97xx_gpio_func(int gpio, int func)
{
	int GEn;
	GEn = ac97->bus->ops->read(ac97, 0x56);
	if(func)
		GEn |= gpio;
	else
		GEn &= ~gpio;
	ac97->bus->ops->write(ac97, 0x56, GEn);
}


void wm97xx_gpio_mode(int gpio, int config, int polarity, int sticky, int wakeup)
{
	int GCn, GPn, GSn, GWn;
	GCn = ac97->bus->ops->read(ac97, 0x4c);
	GPn = ac97->bus->ops->read(ac97, 0x4e);
	GSn = ac97->bus->ops->read(ac97, 0x50);
	GWn = ac97->bus->ops->read(ac97, 0x52);
	
	if(config)
		GCn |= gpio;
	else
		GCn &= ~gpio;
	
	if(polarity)
		GPn |= gpio;
	else
		GPn &= ~gpio;
	
	if(sticky)
		GSn |= gpio;
	else
		GSn &= ~gpio;
	
	if(wakeup)
		GWn |= gpio;
	else
		GWn &= ~gpio;
	
	ac97->bus->ops->write(ac97, 0x4c, GCn);
	ac97->bus->ops->write(ac97, 0x4e, GPn);
	ac97->bus->ops->write(ac97, 0x50, GSn);
	ac97->bus->ops->write(ac97, 0x52, GWn);
}


static void wm97xx_set_digitiser_power(int value)
{
	ac97->bus->ops->write(ac97, AC97_WM97XX_DIGITISER2, d2base | value);
}


/*
 * note: for the TX there's some code that gets enabled in linux/sound/pxa2xx-ac97.c
 * (ifdef CONFIG_MACH_PALMTX) that tries to implement some recommended procedure for
 * reading/writing reg 0x54 from a Intel's document
 * (PXA27x Specification Update: 28007109.pdf sec.: E54)
 */

static int palmld_ac97_take_reading(int adcsel)
{
	int timeout = 15;
	u16 r76 = 0;
	u16 r7a;

	r76 |= adcsel;		        /* set ADCSEL (ADC source)      */
	r76 |= WM97XX_DELAY(3);		/* set settling time delay 	*/
	r76 &= ~(1<<11);	        /* COO = 0 (single measurement) */
	r76 &= ~(1<<10);	        /* CTC = 0 (polling mode)       */
	r76 |=	(1<<15);	        /* initiate measurement (POLL)  */
	
	ac97->bus->ops->write(ac97, 0x76, r76);
	
	// wait settling time
	udelay ((3 * AC97_LINK_FRAME) + 167);
	
	/* wait for POLL to go low */
	while((ac97->bus->ops->read(ac97, 0x76) & 0x8000) && timeout){
		udelay(AC97_LINK_FRAME);
		timeout--;	
	}

	if (timeout == 0){
		printk("palmld_ac97: discarding reading due to POLL wait timout on 0x76\n");	
		return 0;
	}

	r7a = ac97->bus->ops->read(ac97, 0x7a);
	
	if ((r7a & WM97XX_ADCSEL_MASK) != adcsel){
		printk("palmld_ac97: discarding reading -> wrong ADC source\n");
		return 0;
	}
			
	return (int) r7a;
}


static void palmld_ac97_pendown(void)
{
	int xread, yread, pressure;
	int valid_coords=0, btn_pressed = 0;

	/* take readings until the pen goes up */
	do {
		/* take readings */
		xread = palmld_ac97_take_reading(WM97XX_ADCSEL_X);
		yread = palmld_ac97_take_reading(WM97XX_ADCSEL_Y);
		pressure = palmld_ac97_take_reading(WM97XX_ADCSEL_PRES);

		valid_coords =  (xread & 0xfff) && (yread & 0xfff) && (pressure & 0xfff);
		
		if(valid_coords && (pressure < ptrsh)) {
			btn_pressed = 1;
			input_report_key(palmld_ac97_input, BTN_TOUCH, 1);		
			input_report_abs(palmld_ac97_input, ABS_X, xread & 0xfff);
			input_report_abs(palmld_ac97_input, ABS_Y, yread & 0xfff);
			input_report_abs(palmld_ac97_input, ABS_PRESSURE, pressure & 0xfff);
			input_sync(palmld_ac97_input);
			
			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(HZ/100);
			set_current_state(TASK_RUNNING);
		}
	} while( valid_coords );
	

	if (btn_pressed) {
		input_report_key(palmld_ac97_input, BTN_TOUCH, 0);
		input_report_abs(palmld_ac97_input, ABS_X, 0);
		input_report_abs(palmld_ac97_input, ABS_Y, 0);	
		input_report_abs(palmld_ac97_input, ABS_PRESSURE, 0);
		input_sync(palmld_ac97_input);
	}
	
}


static void palmld_ac97_irq_work(void *data)
{
	//struct device *dev = data;
	//ac97_t *ac97 = dev->platform_data;
	u16 levels;
	u16 polarity;
	
	levels = ac97->bus->ops->read(ac97, 0x54);
	polarity = ac97->bus->ops->read(ac97, 0x4e);
	
	if(polarity & levels & WM97XX_GPIO_13) {
		// power up digitiser: 
		down(&digitiser_sem);
	        wm97xx_set_digitiser_power(WM97XX_PRP_DET_DIG);

		palmld_ac97_pendown();
		
		/* power down digitiser to conserve power */
		wm97xx_set_digitiser_power(WM97XX_PRP_DET);
		ac97->bus->ops->write(ac97, 0x4e, polarity & ~WM97XX_GPIO_13);
		up(&digitiser_sem);
	}
	else {
		ac97->bus->ops->write(ac97, 0x4e, polarity | WM97XX_GPIO_13);
	}
		
	ac97->bus->ops->write(ac97, 0x54, levels & ~WM97XX_GPIO_13);

	udelay(1);
	up(&queue_sem);
	enable_irq(IRQ_GPIO_PALMTX_WM9712_IRQ);
}


static irqreturn_t palmld_ac97_irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	if (down_trylock(&queue_sem) == 0){
		disable_irq(IRQ_GPIO_PALMTX_WM9712_IRQ);
		queue_work(palmld_ac97_workqueue, &palmld_ac97_irq_task);
	}

	return IRQ_HANDLED;
}

/* battery */


void palmld_battery_read_adc(int force)
{
	u16 vread;
	
	if(!force && ((last_battery_update + 10 *HZ) > jiffies))
		return;
	
	if(down_trylock(&battery_update_mutex))
		return;
	
	down(&digitiser_sem);
	wm97xx_set_digitiser_power(WM97XX_PRP_DET_DIG);
	vread = palmld_ac97_take_reading(WM97XX_ADCSEL_BMON);
	wm97xx_set_digitiser_power(WM97XX_PRP_DET);
	up(&digitiser_sem);

	previous_voltage = current_voltage;
	current_voltage = vread & 0xfff;
	last_battery_update = jiffies;

	up(&battery_update_mutex);
}


int palmld_battery_min_voltage(struct battery *b)
{
    return PALMTX_BAT_MIN_VOLTAGE;
}


int palmld_battery_max_voltage(struct battery *b)
{
    return PALMTX_BAT_MAX_VOLTAGE; /* mV */
}


// let's suppose AVDD=+3.3v so battV = intV * 3 * 0.80586
// note: 0.80586 = 3.3/4095
int palmld_battery_get_voltage(struct battery *b)
{
    if (battery_registered){
    	palmld_battery_read_adc(0);
    	return current_voltage * 3 *  80586 / 100000;
    }
    else{
    	printk("palmld_battery: cannot get voltage -> battery driver unregistered\n");
    	return 0;
    }
}


int palmld_battery_get_status(struct battery *b)
{
	int ac_connected  = GET_GPIO(GPIO_NR_PALMTX_POWER_DETECT);
	int usb_connected = !GET_GPIO(GPIO_NR_PALMTX_USB_DETECT);
	
	if (current_voltage <= 0)
		return BATTERY_STATUS_UNKNOWN;
	
	if (ac_connected || usb_connected){
		// TODO: ok maybe this is too stupid ... to be reviewed
		if ( ( current_voltage > previous_voltage ) || (current_voltage <= PALMTX_BAT_MAX_VOLTAGE) )
			return BATTERY_STATUS_CHARGING;	
		return BATTERY_STATUS_NOT_CHARGING;	
	}	
	else
		return BATTERY_STATUS_DISCHARGING;
}


struct battery palmtx_battery = {
	.name             = "palmtx-battery",
	.id               = "battery0",
	.get_min_voltage  = palmld_battery_min_voltage,
	.get_max_voltage  = palmld_battery_max_voltage,
	.get_voltage	  = palmld_battery_get_voltage,
	.get_status	  = palmld_battery_get_status,
};




static int __init palmld_ac97_probe(struct device *dev)
{
	int err;
	u16 d2 = DIG2_INIT; // init d1 too?
	
	if(!machine_is_xscale_palmtx())
		return -ENODEV;
	
	ac97 = to_ac97_t(dev);
	
	set_irq_type(IRQ_GPIO_PALMTX_WM9712_IRQ, IRQT_RISING);
	
	err = request_irq(IRQ_GPIO_PALMTX_WM9712_IRQ, palmld_ac97_irq_handler,
		SA_INTERRUPT, "WM9712 pendown IRQ", dev);
	
	if(err) {
		printk(KERN_ERR "palmld_ac97_probe: cannot request pen down IRQ\n");
		return -1;
	}
	
	/* reset levels */
	ac97->bus->ops->write(ac97, 0x54, 0);
	
	/* disable digitiser to save power, enable pen-down detect */
	d2 |= WM97XX_PRP_DET;
	d2base = d2;
	ac97->bus->ops->write(ac97, AC97_WM97XX_DIGITISER2, d2base);
	
	/* enable interrupts on codec's gpio 2 (connected to cpu gpio 27) */
	wm97xx_gpio_mode(WM97XX_GPIO_2, WM97XX_GPIO_OUT, WM97XX_GPIO_POL_HIGH, 
		WM97XX_GPIO_NOTSTICKY, WM97XX_GPIO_NOWAKE);
	wm97xx_gpio_func(WM97XX_GPIO_2, 0);

	/* enable pen detect interrupt */
	wm97xx_gpio_mode(WM97XX_GPIO_13, WM97XX_GPIO_OUT, WM97XX_GPIO_POL_HIGH, 
		WM97XX_GPIO_STICKY, WM97XX_GPIO_WAKE);
		
	/* turn off irq gpio inverting */	
	ac97->bus->ops->write(ac97, 0x58, ac97->bus->ops->read(ac97, 0x58)&~1);

	/* turn on the digitiser and pen down detector */
	ac97->bus->ops->write(ac97, AC97_WM97XX_DIGITISER2, d2base | WM97XX_PRP_DETW);
	
	/* setup the input device */
	palmld_ac97_input = input_allocate_device();
	if (palmld_ac97_input == NULL){
		printk ("palmld_ac97_probe: cannot allocate input device\n");
		return -ENOMEM;
	}
	
	palmld_ac97_input->evbit[0] = BIT(EV_KEY) | BIT(EV_ABS);
	
	palmld_ac97_input->keybit[LONG(BTN_TOUCH)] = BIT(BTN_TOUCH);
	input_set_abs_params(palmld_ac97_input, ABS_X, X_AXIS_MIN, X_AXIS_MAX, xdjtrsh, 0);
	input_set_abs_params(palmld_ac97_input, ABS_Y, Y_AXIS_MIN, Y_AXIS_MAX, ydjtrsh, 0);
	input_set_abs_params(palmld_ac97_input, ABS_PRESSURE, PRESSURE_MIN, PRESSURE_MAX, PRESSURE_FUZZ, 0);

	palmld_ac97_input->name = "palmtx touchscreen";
	palmld_ac97_input->dev = dev;
	palmld_ac97_input->id.bustype = BUS_HOST;
	input_register_device(palmld_ac97_input);
	
	/* register battery */
	
	if(battery_class_register(&palmtx_battery)) {
		printk(KERN_ERR "palmld_ac97_probe: could not register battery class\n");
	}
	else{
		battery_registered = 1;
	}
	
	/* setup work queue */	
	palmld_ac97_workqueue = create_workqueue(palmld_ac97_WORK_QUEUE_NAME);
	INIT_WORK(&palmld_ac97_irq_task, palmld_ac97_irq_work);
	
	up(&queue_sem);
	return 0;
}


static int palmld_ac97_remove (struct device *dev)
{
	// TODO: stop running tasks if any?
	
	battery_class_unregister(&palmtx_battery);
	ac97 = NULL;
	input_unregister_device(palmld_ac97_input);
	return 0;
}


static struct device_driver palmld_ac97_driver = {
	.name           = "palmld_ac97 (WM9712)",
	.bus            = &ac97_bus_type,
	.owner = 	  THIS_MODULE,
	.probe          = palmld_ac97_probe,
        .remove		= palmld_ac97_remove,

#ifdef CONFIG_PM
	.suspend        = NULL,
	.resume         = NULL,
#endif
};


static int __init palmld_ac97_init(void)
{
	if(!machine_is_xscale_palmtx())
		return -ENODEV;
	
	return driver_register(&palmld_ac97_driver);
}


static void __exit palmld_ac97_exit(void)
{
	driver_unregister(&palmld_ac97_driver);
}


module_init(palmld_ac97_init);
module_exit(palmld_ac97_exit);

MODULE_AUTHOR ("Alex Osborne <bobofdoom@gmail.com>");
MODULE_DESCRIPTION ("WM9712 AC97 codec support for Palm TX");
MODULE_LICENSE ("GPL");

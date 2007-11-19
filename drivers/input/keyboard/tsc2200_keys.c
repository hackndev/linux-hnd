/*
 * LED interface for Himalaya, the HTC PocketPC.
 *
 * License: GPL
 *
 * Author: Luke Kenneth Casson Leighton, Copyright(C) 2004
 *
 * Copyright(C) 2004, Luke Kenneth Casson Leighton.
 *
 * History:
 *
 * 2004-02-19	Luke Kenneth Casson Leighton	created.
 *
 */

#include <linux/init.h>                                                                                          
                                                                                                                 
 
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/interrupt.h>

#include <asm/hardware/ipaq-asic3.h>
#include <asm/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/mach-types.h>

#include <linux/mfd/tsc2200.h>

#ifdef DEBUG
#define dprintk(x...) printk(x)
#else
#define dprintk(x...)
#endif

#define KP_POLL_TIME 30 /* milliseconds */

static void
tsc2200_buttons_check_keys( struct tsc2200_buttons_data *devdata, int new)
{
	struct tsc2200_buttons_platform_info *platform_info = devdata->platform_info;
		
	int i, pressed, xor;
	struct tsc2200_key *k;

	xor = new ^ devdata->keydata;
	for (i = 0 ; i < platform_info->num_keys ; i++) {
		k = &platform_info->keys[i];
		if ((xor & (1 << k->key_index))) 
		{
			pressed = new & (1 << k->key_index);
			input_report_key(devdata->input_dev, k->keycode, pressed);
			input_sync(devdata->input_dev);

			if (pressed)
				printk("%s pressed\n", k->name);
			else
				printk("%s released\n", k->name);

		}
	}
	devdata->keydata = new;
}

static void
tsc2200_buttons_getkey( struct tsc2200_buttons_data *devdata ) 
{
	struct tsc2200_buttons_platform_info *platform_info = devdata->platform_info;

	int status, keys;

//	tsc2200_lock( devdata->tsc2200_dev );	
	status = tsc2200_read( devdata->tsc2200_dev, TSC2200_CTRLREG_KEY);
	
	if (status & 0x8000) {
		keys = tsc2200_read( devdata->tsc2200_dev, TSC2200_DATAREG_KPDATA );
	} else {
		keys = 0;
	}
	
//	tsc2200_unlock( devdata->tsc2200_dev );	

	tsc2200_buttons_check_keys(devdata, keys);

	if (keys) {
		mod_timer (&devdata->timer, jiffies + (KP_POLL_TIME * HZ) / 1000);
	} else {
		enable_irq(platform_info->irq);
	}
	
	return;
}

static void
tsc2200_buttons_timer(unsigned long data)
{
        struct tsc2200_buttons_data *devdata = (struct tsc2200_buttons_data *)data; 

	printk("t");
	tsc2200_buttons_getkey(devdata);
}

static irqreturn_t tsc2200_buttons_irq(int irq, void *data)
{
        struct tsc2200_buttons_data *devdata = data; 
	struct tsc2200_buttons_platform_info *platform_info = devdata->platform_info;

	printk("i");

	disable_irq( platform_info->irq );

	tsc2200_buttons_getkey(devdata);

        return IRQ_HANDLED;
}

static int tsc2200_buttons_probe (struct device *dev )
{
        struct tsc2200_buttons_data *devdata = dev->platform_data; 
	struct tsc2200_buttons_platform_info *platform_info = devdata->platform_info;
	                
	int i;
	struct tsc2200_key *k;
	                                                                                                 
	printk("%x", platform_info->num_keys);	
	dprintk("%s\n", __FUNCTION__);

	// Allocate input device
	devdata->input_dev = input_allocate_device();
        set_bit(EV_KEY, devdata->input_dev->evbit);

        //devdata->input_dev->evbit[0] = BIT(EV_KEY) | BIT(EV_REP);

	for (i = 0 ; i < platform_info->num_keys ; i++) {
		k = &platform_info->keys[i];
		set_bit(k->keycode, devdata->input_dev->keybit);
	}

        devdata->input_dev->name = "tsc2200-keys";
        input_register_device(devdata->input_dev);

//	tsc2200_write(devdata->tsc2200_dev,                                                                                                  
//	                  TSC2200_CTRLREG_ADC,                                                                                                         
//	                  TSC2200_CTRLREG_ADC_AD3 |                                                                                                    
//	                  TSC2200_CTRLREG_ADC_AD1 |                                                                                                    
//	                  TSC2200_CTRLREG_ADC_AD0 |                                                                                                    
//	                  TSC2200_CTRLREG_ADC_RES (TSC2200_CTRLREG_ADC_RES_12BIT) |                                                                    
//		          TSC2200_CTRLREG_ADC_AVG (TSC2200_CTRLREG_ADC_16AVG) |                                                                        
//		          TSC2200_CTRLREG_ADC_CL  (TSC2200_CTRLREG_ADC_CL_4MHZ_10BIT) |                                                                
//		          TSC2200_CTRLREG_ADC_PV  (TSC2200_CTRLREG_ADC_PV_1mS) );

	// Wait for keypress interrupt from the tsc2200
	request_irq( platform_info->irq, tsc2200_buttons_irq, SA_SAMPLE_RANDOM, "tsc2200-keys", devdata);
	set_irq_type( platform_info->irq, IRQF_TRIGGER_FALLING);

	// Initialize timer for buttons
	init_timer(&devdata->timer);
	devdata->timer.function = tsc2200_buttons_timer;
	devdata->timer.data = (unsigned long)devdata;

	return 0;
}

static int tsc2200_buttons_suspend( struct device *dev, pm_message_t state) {
        struct tsc2200_buttons_data *devdata = dev->platform_data; 
	struct tsc2200_buttons_platform_info *platform_info = devdata->platform_info;

	disable_irq(platform_info->irq);

	return 0;
}

static int tsc2200_buttons_resume ( struct device *dev ) {
        struct tsc2200_buttons_data *devdata = dev->platform_data; 
	struct tsc2200_buttons_platform_info *platform_info = devdata->platform_info;

	enable_irq(platform_info->irq);

	return 0;
}

static int tsc2200_buttons_remove (struct device *dev)
{
        struct tsc2200_buttons_data *devdata = dev->platform_data; 
	struct tsc2200_buttons_platform_info *platform_info = devdata->platform_info;

	dprintk("%s\n", __FUNCTION__);
	input_unregister_device(devdata->input_dev);

	disable_irq( platform_info->irq );
	free_irq( platform_info->irq, NULL);

	del_timer_sync (&devdata->timer);
	return 0;
}

static struct platform_driver tsc2200_buttons = {
	.driver = {
		.name		= "tsc2200-keys",
		.probe		= tsc2200_buttons_probe,
		.remove		= tsc2200_buttons_remove,
		.suspend	= tsc2200_buttons_suspend,
		.resume		= tsc2200_buttons_resume,
	}
};

static int tsc2200_buttons_init(void) {
	printk("Registering tsc2200 buttons driver\n");
	return platform_driver_register(&tsc2200_buttons);
}

static void tsc2200_buttons_exit(void) {
	printk("Deregistering tsc2200 buttons driver\n");
	platform_driver_unregister(&tsc2200_buttons);
}


module_init (tsc2200_buttons_init);
module_exit (tsc2200_buttons_exit);

MODULE_LICENSE("GPL");

#include <linux/mfd/tsc2101.h>
#include <linux/module.h>

static struct tsc2101_driver tsc2101_meas_driver;
static struct tsc2101_meas tsc2101_meas;

static int tsc2101_meas_suspend(pm_message_t state)
{
	return 0;
}

static int tsc2101_meas_resume(void)
{
	return 0;
}

static void tsc2101_get_miscdata(struct tsc2101 *devdata)
{
	static int i=0;
	unsigned long flags;

	/* FIXME: is touchscreen reading running? - is really necessary to disable reading while touchscreen processing? */
	if ((!devdata->ts) || (((struct tsc2101_ts *)devdata->ts->priv)->pendown == 0)) { 
		i++;
		/* FIXME: is spinlock sufficient? */
		spin_lock_irqsave(&tsc2101_meas_driver.tsc->lock, flags);
		printk(KERN_DEBUG "tsc2101_meas: tsc2101_get_misc with i=%d\n",i);
		if (i==1) 
			tsc2101_regwrite(devdata, TSC2101_REG_ADC, TSC2101_ADC_DEFAULT | TSC2101_ADC_ADMODE(0x6));
		else if (i==2) 
			tsc2101_regwrite(devdata, TSC2101_REG_ADC, TSC2101_ADC_DEFAULT | TSC2101_ADC_ADMODE(0x7));
		else if (i==3)
			tsc2101_regwrite(devdata, TSC2101_REG_ADC, TSC2101_ADC_DEFAULT | TSC2101_ADC_ADMODE(0x8));
		else if (i==4)
			tsc2101_regwrite(devdata, TSC2101_REG_ADC, TSC2101_ADC_DEFAULT | TSC2101_ADC_ADMODE(0xa));
		else if (i>=5) {
			tsc2101_regwrite(devdata, TSC2101_REG_ADC, TSC2101_ADC_DEFAULT | TSC2101_ADC_ADMODE(0xc));
			i=0;
		}
		spin_unlock_irqrestore(&tsc2101_meas_driver.tsc->lock, flags);
		/* FIXME: I don't need to read data so often */
		tsc2101_readdata();
	} 
}


static void tsc2101_misc_timer(unsigned long data)
{
	tsc2101_get_miscdata(tsc2101_meas_driver.tsc);
	mod_timer(&(tsc2101_meas.misc_timer), jiffies + HZ);	
}

static u8 tsc2101_meas_readdata(u32 status)
{
	u32 values[4];
	u8 fixadc=0;
	printk(KERN_DEBUG "tsc2101_meas: tsc2101_meas_readdata enter\n");	
	if (status & TSC2101_STATUS_BSTAT) {
		tsc2101_meas_driver.tsc->platform->send(TSC2101_READ, TSC2101_REG_BAT, &values[0], 1);
		printk(KERN_DEBUG "tsc2101_meas: read battery value: %d\n",values[0]);
		tsc2101_meas.bat=values[0];
		fixadc=1;
	}	

	if (status & TSC2101_STATUS_AX1STAT) {
		tsc2101_meas_driver.tsc->platform->send(TSC2101_READ, TSC2101_REG_AUX1, &values[0], 1);
		tsc2101_meas.aux1=values[0];
		printk(KERN_DEBUG "tsc2101_meas: read aux1 value: %d\n",values[0]);
		fixadc=1;
	}

	if (status & TSC2101_STATUS_AX2STAT) {
		tsc2101_meas_driver.tsc->platform->send(TSC2101_READ, TSC2101_REG_AUX2, &values[0], 1);
		tsc2101_meas.aux2=values[0];
		printk(KERN_DEBUG "tsc2101_meas: read aux2 value: %d\n",values[0]);
		fixadc=1;
	}

	if (status & TSC2101_STATUS_T1STAT) {
		tsc2101_meas_driver.tsc->platform->send(TSC2101_READ, TSC2101_REG_TEMP1, &values[0], 1);
		tsc2101_meas.temp1=values[0];
		printk(KERN_DEBUG "tsc2101_meas: read temp1 value: %d\n",values[0]);
		fixadc=1;
	}

	if (status & TSC2101_STATUS_T2STAT) {
		tsc2101_meas_driver.tsc->platform->send(TSC2101_READ, TSC2101_REG_TEMP2, &values[0], 1);
		tsc2101_meas.temp2=values[0];
		printk(KERN_DEBUG "tsc2101_meas: read temp2 value: %d\n",values[0]);
		fixadc=1;
	}
	return fixadc;
}


static int tsc2101_meas_probe(struct tsc2101 *tsc)
{

	tsc2101_meas_driver.tsc = tsc;
	tsc2101_meas_driver.priv = (void *) &tsc2101_meas;

	init_timer(&tsc2101_meas.misc_timer);
	tsc2101_meas.misc_timer.data = (unsigned long) &tsc2101_meas;
	tsc2101_meas.misc_timer.function = tsc2101_misc_timer;

	mod_timer(&tsc2101_meas.misc_timer, jiffies + HZ);

	tsc2101_readdata();	
	printk(KERN_INFO "tsc2101_meas: measurement driver initialized\n");

	return 0;
}

static int tsc2101_meas_remove(void)
{
	del_timer_sync(&tsc2101_meas.misc_timer);
	printk(KERN_INFO "tsc2101_meas: measurement driver removed\n");
	return 0;
}

static int __init tsc2101_meas_init(void)
{
	return tsc2101_register(&tsc2101_meas_driver,TSC2101_DEV_MEAS);
}

static void __exit tsc2101_meas_exit(void)
{
	tsc2101_unregister(&tsc2101_meas_driver,TSC2101_DEV_MEAS);
}

static struct tsc2101_driver tsc2101_meas_driver = {
	.suspend	= tsc2101_meas_suspend,
	.resume		= tsc2101_meas_resume,
	.probe		= tsc2101_meas_probe,
	.remove		= tsc2101_meas_remove,
	.readdata	= tsc2101_meas_readdata
};

module_init(tsc2101_meas_init);
module_exit(tsc2101_meas_exit);

MODULE_LICENSE("GPL");

/*
 * HP iPAQ hx2750 Test Development Code
 *
 * Copyright 2005 Openedhand Ltd.
 *
 * Author: Richard Purdie <richard@o-hand.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/interrupt.h>

#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/mach/arch.h>

#include <asm/arch/hx2750.h>
#include <asm/arch/pxa-regs.h>
 

static int prodval;

/*    
 * Sysfs functions       
 */   
static ssize_t test1_show(struct device *dev, char *buf) 
{
	unsigned long rp;

	asm ("mrc	p15, 0, %0, cr1, cr0;\n" :"=r"(rp) ); 
	printk("%lx\n",rp);
	
	asm ("mrc	p15, 0, %0, cr1, cr1;\n" :"=r"(rp) ); 
	printk("%lx\n",rp);

	asm ("mrc	p15, 0, %0, cr2, cr0;\n" :"=r"(rp) ); 
	printk("%lx\n",rp);

	asm ("mrc	p15, 0, %0, cr3, cr0;\n" :"=r"(rp) ); 
	printk("%lx\n",rp);

	asm ("mrc	p15, 0, %0, cr13, cr0;\n" :"=r"(rp) ); 
	printk("%lx\n",rp);
		
	return sprintf(buf, "%d\n",prodval); 
}
extern void tsc2101_print_miscdata(struct device *dev);
extern struct platform_device tsc2101_device;
 
static ssize_t test1_store(struct device *dev, const char *buf, size_t count)
{
	prodval = simple_strtoul(buf, NULL, 10);  
 
 	tsc2101_print_miscdata(&tsc2101_device.dev);
 
	return count;
}
 
static DEVICE_ATTR(test1, 0644, test1_show, test1_store);

static ssize_t test2_show(struct device *dev, char *buf) 
{
	hx2750_ssp_init2();
 
	return sprintf(buf, "%d\n",0); 
} 

static DEVICE_ATTR(test2, 0444, test2_show, NULL);

extern unsigned int hx2750_egpio_current;

static ssize_t setegpio_show(struct device *dev, char *buf) 
{
	return sprintf(buf, "%d\n",hx2750_egpio_current); 
} 

static ssize_t setegpio_store(struct device *dev, const char *buf, size_t count)
{
	unsigned int val = simple_strtoul(buf, NULL, 16);

	hx2750_set_egpio(val);

	return count; 
} 

static DEVICE_ATTR(setegpio, 0644, setegpio_show, setegpio_store);

static ssize_t clregpio_show(struct device *dev, char *buf) 
{
	return sprintf(buf, "%d\n",hx2750_egpio_current); 
} 

static ssize_t clregpio_store(struct device *dev, const char *buf, size_t count)
{
	unsigned int val = simple_strtoul(buf, NULL, 16);

	hx2750_clear_egpio(val);

	return count; 
} 

static DEVICE_ATTR(clregpio, 0644, clregpio_show, clregpio_store);


static ssize_t gpioclr_store(struct device *dev, const char *buf, size_t count)
{
	int prod;
	prod = simple_strtoul(buf, NULL, 10);  

	GPCR(prod) = GPIO_bit(prod);
 
	return count;
}
 
static DEVICE_ATTR(gpioclr, 0200, NULL, gpioclr_store);

static ssize_t gpioset_store(struct device *dev, const char *buf, size_t count)
{
	int prod;
	prod = simple_strtoul(buf, NULL, 10);  

	GPSR(prod) = GPIO_bit(prod);
 
	return count;
}
 
static DEVICE_ATTR(gpioset, 0200, NULL, gpioset_store);




static ssize_t sspr_store(struct device *dev, const char *buf, size_t count)
{        
	unsigned long val,ret;        
	val = simple_strtoul(buf, NULL, 0);   
	
	hx2750_tsc2101_send(1<<15,val,&ret,1);

	printk("Response: %x\n",ret);  
	
	return count;
}   

static DEVICE_ATTR(sspr, 0200, NULL, sspr_store);
 
static ssize_t sspw_store(struct device *dev, const char *buf, size_t count)
{        
	int val,ret;        
	sscanf(buf, "%lx %lx", &val, &ret);	
	
	hx2750_tsc2101_send(0,val,&ret,1);

	return count;
}   

static DEVICE_ATTR(sspw, 0200, NULL, sspw_store);


extern struct pm_ops pxa_pm_ops;
extern void pxa_cpu_resume(void);
extern unsigned long pxa_pm_pspr_value; 

static int (*pxa_pm_enter_orig)(suspend_state_t state);

//static struct {
//	u32 ffier, fflcr, ffmcr, ffspr, ffisr, ffdll, ffdlh;
//} sys_ctx;

u32 resstruct[20];

static int hx2750_pxa_pm_enter(suspend_state_t state)
{
	int i;
	u32 save[10];

	PWER = 0xC0000003;// | PWER_RTC;
	PFER = 0x00000003;
	PRER = 0x00000003;
	
	PGSR0=0x00000018;
	PGSR1=0x00000380;
	PGSR2=0x00800000;
	PGSR3=0x00500400;
		
	//PVCR=0x494; or 0x14;
	//PVCR=0x14;
	//PCMD0=0x41f;
	//i=PISR;
	//PICR=0x00000062;
	
	//PCFR=0x457;
	//PCFR=0x70; // Does not resume from sleep
	PCFR=0x077; // was 0x477
	PSLR=0xff100004;
	
	resstruct[0]=0x0000b4e6;
	resstruct[1]=0x00000001;
	resstruct[2]=virt_to_phys(pxa_cpu_resume);
	resstruct[3]=0xffffffff; //value for r0

	resstruct[14]=0x00000078;  //mcr	15, 0, r0, cr1, cr0, {0}
	resstruct[15]=0x00000000;  //mcr	15, 0, r0, cr1, cr1, {0} 0xffffffff
	resstruct[16]=0xa0000000;  //mcr	15, 0, r0, cr2, cr0, {0} 0xa0748000
	resstruct[17]=0x00000001;  //mcr	15, 0, r0, cr3, cr0, {0} 0x00000015
	resstruct[18]=0x00000000;  //mcr	15, 0, r0, cr13, cr0, {0} 0x00000000

	pxa_pm_pspr_value=virt_to_phys(&resstruct[0]);
	
	hx2750_send_egpio(3);
	
	pxa_gpio_mode(87 | GPIO_OUT  | GPIO_DFLT_HIGH);

	//sys_ctx.ffier = FFIER;
	//sys_ctx.fflcr = FFLCR;
	//sys_ctx.ffmcr = FFMCR;
	//sys_ctx.ffspr = FFSPR;
	//sys_ctx.ffisr = FFISR;
	//FFLCR |= 0x80;
	//sys_ctx.ffdll = FFDLL;
	//sys_ctx.ffdlh = FFDLH;
	//FFLCR &= 0xef;

	pxa_pm_enter_orig(state);
	
	//FFMCR = sys_ctx.ffmcr;
	//FFSPR = sys_ctx.ffspr;
	//FFLCR = sys_ctx.fflcr;
	//FFLCR |= 0x80;
	//FFDLH = sys_ctx.ffdlh;
	//FFDLL = sys_ctx.ffdll;
	//FFLCR = sys_ctx.fflcr;
	//FFISR = sys_ctx.ffisr;
	//FFLCR = 0x07;
	//FFIER = sys_ctx.ffier;

	return 0;
}

static irqreturn_t hx2750_charge_int(int irq, void *dev_id, struct pt_regs *regs)
{
	if ((GPLR(HX2750_GPIO_EXTPWR) & GPIO_bit(HX2750_GPIO_EXTPWR)) == 0) {
		printk("Charging On\n");
		GPCR(HX2750_GPIO_CHARGE);
	} else {
		printk("Charging Off\n");
		GPSR(HX2750_GPIO_CHARGE);
	}
		
	return IRQ_HANDLED;
}




static irqreturn_t hx2750_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
    printk("Input %d changed.\n", irq-32);

	return IRQ_HANDLED;
}


static int __init hx2750_test_probe(struct device *dev)
{
	pxa_gpio_mode(21 | GPIO_OUT | GPIO_DFLT_HIGH);
	pxa_gpio_mode(HX2750_GPIO_CHARGE | GPIO_OUT | GPIO_DFLT_HIGH);
	pxa_gpio_mode(83 | GPIO_OUT | GPIO_DFLT_HIGH);
	pxa_gpio_mode(HX2750_GPIO_BIOPWR | GPIO_OUT | GPIO_DFLT_HIGH);
	pxa_gpio_mode(116 | GPIO_OUT | GPIO_DFLT_HIGH);
	pxa_gpio_mode(118 | GPIO_OUT | GPIO_DFLT_HIGH);


	//pxa_gpio_mode(HX2750_GPIO_CF_PWR | GPIO_OUT | GPIO_DFLT_LOW);
	pxa_gpio_mode(HX2750_GPIO_CF_PWR | GPIO_OUT | GPIO_DFLT_HIGH);
	pxa_gpio_mode(HX2750_GPIO_BTPWR | GPIO_OUT | GPIO_DFLT_LOW);
	pxa_gpio_mode(79 | GPIO_OUT | GPIO_DFLT_LOW);
	pxa_gpio_mode(85 | GPIO_OUT | GPIO_DFLT_LOW);
	pxa_gpio_mode(HX2750_GPIO_LEDMAIL | GPIO_OUT | GPIO_DFLT_LOW);
	pxa_gpio_mode(107 | GPIO_OUT | GPIO_DFLT_LOW);
	pxa_gpio_mode(114 | GPIO_OUT | GPIO_DFLT_LOW);	
	
	pxa_gpio_mode(HX2750_GPIO_BATTCOVER1 | GPIO_IN);
	pxa_gpio_mode(HX2750_GPIO_BATTCOVER2 | GPIO_IN);
	pxa_gpio_mode(HX2750_GPIO_CF_IRQ | GPIO_IN);
	pxa_gpio_mode(HX2750_GPIO_USBCONNECT | GPIO_IN);
	pxa_gpio_mode(HX2750_GPIO_CF_DETECT | GPIO_IN);
	pxa_gpio_mode(HX2750_GPIO_EXTPWR | GPIO_IN);
	
	pxa_gpio_mode(HX2750_GPIO_BATLVL | GPIO_IN);
	//pxa_gpio_mode(HX2750_GPIO_CF_WIFIIRQ | GPIO_IN);
	pxa_gpio_mode(80 | GPIO_IN);
	pxa_gpio_mode(HX2750_GPIO_HP_JACK | GPIO_IN);
	pxa_gpio_mode(115 | GPIO_IN);
	pxa_gpio_mode(119 | GPIO_IN);	

	request_irq(IRQ_GPIO(HX2750_GPIO_BATLVL), hx2750_interrupt, SA_INTERRUPT, "hx2750test", NULL);
	//request_irq(IRQ_GPIO(HX2750_GPIO_CF_WIFIIRQ), hx2750_interrupt, SA_INTERRUPT, "hx2750test", NULL);
	request_irq(IRQ_GPIO(80), hx2750_interrupt, SA_INTERRUPT, "hx2750test", NULL);
	request_irq(IRQ_GPIO(115), hx2750_interrupt, SA_INTERRUPT, "hx2750test", NULL);
	request_irq(IRQ_GPIO(119), hx2750_interrupt, SA_INTERRUPT, "hx2750test", NULL);  
	request_irq(IRQ_GPIO(HX2750_GPIO_SR_CLK2), hx2750_interrupt, SA_INTERRUPT, "hx2750test", NULL);    

	//request_irq(IRQ_GPIO(10), hx2750_interrupt, SA_INTERRUPT, "hx2750test", NULL);    

	set_irq_type(IRQ_GPIO(HX2750_GPIO_BATLVL),IRQT_BOTHEDGE);
	//set_irq_type(IRQ_GPIO(HX2750_GPIO_CF_WIFIIRQ),IRQT_BOTHEDGE);
	set_irq_type(IRQ_GPIO(80),IRQT_BOTHEDGE);
	set_irq_type(IRQ_GPIO(115),IRQT_BOTHEDGE);
	set_irq_type(IRQ_GPIO(119),IRQT_BOTHEDGE);
	set_irq_type(IRQ_GPIO(HX2750_GPIO_SR_CLK2),IRQT_BOTHEDGE);
	
	//set_irq_type(IRQ_GPIO(10),IRQT_BOTHEDGE);

	printk("hx2750 Test Code Initialized.\n");
	device_create_file(dev, &dev_attr_test1);
	device_create_file(dev, &dev_attr_test2);
	device_create_file(dev, &dev_attr_setegpio);
	device_create_file(dev, &dev_attr_clregpio);
	device_create_file(dev, &dev_attr_gpioset);
	device_create_file(dev, &dev_attr_gpioclr);
	device_create_file(dev, &dev_attr_sspr);
	device_create_file(dev, &dev_attr_sspw);

	request_irq(HX2750_IRQ_GPIO_EXTPWR, hx2750_charge_int, SA_INTERRUPT, "hx2750_extpwr", NULL);
	set_irq_type(HX2750_IRQ_GPIO_EXTPWR, IRQT_BOTHEDGE);
	
	pxa_pm_enter_orig=pxa_pm_ops.enter;
	pxa_pm_ops.enter=hx2750_pxa_pm_enter;
	
	return 0;
}

static struct device_driver hx2750_test_driver = {
	.name		= "hx2750-test",
	.bus		= &platform_bus_type,
	.probe		= hx2750_test_probe,
//	.remove		= hx2750_bl_remove,
//	.suspend	= hx2750_bl_suspend,
//	.resume		= hx2750_bl_resume,
}; 
 
 
static int __init hx2750_test_init(void)
{
	return driver_register(&hx2750_test_driver);
}


static void __exit hx2750_test_exit(void)
{
 	driver_unregister(&hx2750_test_driver);
}

module_init(hx2750_test_init);
module_exit(hx2750_test_exit);

MODULE_AUTHOR("Richard Purdie <richard@o-hand.com>");
MODULE_DESCRIPTION("iPAQ hx2750 Backlight Driver");
MODULE_LICENSE("GPLv2");

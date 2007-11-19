/*
 * Blueangel resume with the native HTC bootloader.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License.
 *
 */

#include <linux/init.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/irq.h>

#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/mach/arch.h>
#include <asm/arch/pxa-pm_ll.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/htcblueangel-gpio.h>

extern void pxa_cpu_resume(void);

static unsigned long calc_csum(u32 *buf) {
	int i,sum=0,val;

	for(i = 0 ; i < 48 ; i++) {
		val = ((*buf++) & 1) << 31;
		val |= (val >> 1);
		sum+=val;
	}
	return sum;
}

static u32 save_buffer[6];
static u32 memstart_buffer[48];
static u32 memgpio_buffer[12];

static void blueangel_pm_suspend(unsigned long resume_addr)
{
	u32 *memstart=phys_to_virt(0xa0000000);
	u32 *memgpio =phys_to_virt(0xa0000c00);
	u32 *mem_save=phys_to_virt(0xa0038000);
	int i;

	// Save all the memory this function damages
	for(i=0; i<48; i++)
		memstart_buffer[i] = memstart[i];
		
	for(i=0; i<12; i++)
		memgpio_buffer[i] = memgpio[i];
	

	memstart[0]=resume_addr;
	memstart[1]=0;
	memstart[2]=0;
	memstart[3]=0;
	memstart[4]=0xe59f700c;  /* ldr	r7, [pc, #12] ; +0x14 =resume_addr */
        memstart[5]=0xe1a0f007;  /* mov	pc, r7 */
        memstart[6]=0xe1a00000;  /* nop */
        memstart[7]=0xe1a00000;  /* nop */
        memstart[8]=0xe1a00000;  /* nop */
        memstart[9]=resume_addr;
	memgpio[0]=GAFR0_L;
	memgpio[1]=GAFR0_U;
	memgpio[2]=GAFR1_L;
	memgpio[3]=GAFR1_U;
	memgpio[4]=GAFR2_L;
	memgpio[5]=GAFR2_U;
	memgpio[6]=PGSR0;
	memgpio[7]=PGSR1;
	memgpio[8]=PGSR2;
	memgpio[9]=GPDR0;
	memgpio[10]=GPDR1;
	memgpio[11]=GPDR2;
	
	for (i = 0 ; i < 6 ; i++)
		save_buffer[i]=mem_save[i];

	PSPR = calc_csum(memstart);

	/* Wake up enable. */
	PWER =    PWER_GPIO0 
		| PWER_GPIO1 /* reset */
		| PWER_GPIO2 /* keyboard */
		| PWER_GPIO3 /* asic3 mux */
		| PWER_GPIO4 /* ATI ?*/
		| PWER_GPIO6 /* BB_ALERT */
		| PWER_GPIO7 /* unknown */
		| PWER_GPIO8 /* USB */
		| PWER_GPIO9 /* serial */
		| PWER_GPIO10 /* remote */
		| PWER_RTC;
	/* Wake up on falling edge. */
	PFER =    PWER_GPIO0 
		| PWER_GPIO1
		| PWER_GPIO2
		| PWER_GPIO3
		| PWER_GPIO6
		| PWER_GPIO7
		| PWER_GPIO8
		| PWER_GPIO9
		| PWER_GPIO10;

	/* Wake up on rising edge. */
	PRER =    PWER_GPIO0 
		| PWER_GPIO1
		| PWER_GPIO2
		| PWER_GPIO6
		| PWER_GPIO7
		| PWER_GPIO8
		| PWER_GPIO9
		| PWER_GPIO10;

	PCFR  = PCFR_OPDE;

#if 0
        PGSR0 = 0x40c58f07;
        PGSR1 = 0x30b3426e;
        PGSR2 = 0x003ff7ff;
#endif
        
	printk("ll_pm_suspend resume_addr=0x%lx PSPR=0x%x\n", resume_addr, PSPR);
	return;
}

static void blueangel_pm_resume(void) {
	int i;
	u32 *memstart=phys_to_virt(0xa0000000);
	u32 *memgpio =phys_to_virt(0xa0000c00);
	u32 *mem_save=phys_to_virt(0xa0038000);

	for (i = 0 ; i < 6 ; i++)
		mem_save[i] = save_buffer[i];
	
	for(i=0; i<48; i++)
		memstart[i] = memstart_buffer[i];

	for(i=0; i<12; i++)
		memgpio [i] = memgpio_buffer[i];

}

static struct pxa_ll_pm_ops blueangel_ll_pm_ops = {
	.suspend = blueangel_pm_suspend,
	.resume = blueangel_pm_resume,
};

static irqreturn_t
gsm_alerting(int irq, void *data)
{
	printk("gsm_alerting irq\n");

        return IRQ_HANDLED;
}

static int __init blueangel_pm_init(void)
{
	int retval;

	printk("blueangel_pm_init\n");
	pxa_pm_set_ll_ops(&blueangel_ll_pm_ops);

	retval=request_irq(IRQ_NR_BLUEANGEL_GSM_ALERTING, gsm_alerting, SA_INTERRUPT, "gsm_alerting", NULL);
	set_irq_type(IRQ_NR_BLUEANGEL_GSM_ALERTING, IRQ_TYPE_EDGE_FALLING);
	printk("retval=%d\n", retval);

	return 0;
}

static void blueangel_pm_exit(void)
{
	printk("blueangel_pm_exit\n");
	free_irq(IRQ_NR_BLUEANGEL_GSM_ALERTING, NULL);
	pxa_pm_set_ll_ops(NULL);
}

module_init(blueangel_pm_init);
module_exit(blueangel_pm_exit);

MODULE_LICENSE("GPL");

/*
 *  arch/arm/mach-pxa/palmt650/palmt650_pm.c
 *
 *  Suspend/resume support with unmodified Treo 650 bootloader.
 *
 *  Copyright (C) 2007 Alex Osborne
 *
 *
 *  The original bootloader upon waking from sleep mode will
 *  check PSPR and if it's non-zero it will jump to 0xa0000000.
 *
 *  This code modifies 0xa0000000 just before going to sleep, inserting a jump
 *  to PSPR which will be holding the address of the Linux resume function.
 */

#include <linux/kernel.h>
#include <asm/arch/pxa-pm_ll.h>
#include <asm/arch/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/palmt650-gpio.h>

#ifdef CONFIG_PM

#define RESUME_VECTOR_PHYS 0xa0000000

static u32 *resume_vector;
static u32 resume_vector_save[3];

static void palmt650_pxa_ll_pm_suspend(unsigned long resume_addr)
{
	resume_vector_save[0] = resume_vector[0];
	resume_vector_save[1] = resume_vector[1];
	resume_vector_save[2] = resume_vector[2];

	/* write jump to PSPR */
	resume_vector[0] = 0xe3a00121; /* mov     r0, #0x40000008   */
	resume_vector[1] = 0xe280060f; /* add     r0, r0, #0xf00000 */
	resume_vector[2] = 0xe590f000; /* ldr     pc, [r0]	  */

	/* wake-up enable */
	PWER 	= PWER_RTC
		| PWER_GPIO0  /* AC adapter */
		| PWER_GPIO1  /* reset */
		| (1 << 20);  /* SD detect */
	
	/* falling-edge wake */
	PFER 	= PWER_GPIO0
		| PWER_GPIO1
		| (1 << 20);  /* SD detect */
	
	/* rising-edge wake */
	PRER 	= PWER_GPIO0
		| PWER_GPIO1
		| (1 << 20);  /* SD detect */

	/* wake-up by keypad  */
	//PKWR = PKWR_MKIN0 | PKWR_MKIN1 | PKWR_MKIN2 | PKWR_MKIN3 |
	//       PKWR_MKIN4 | PKWR_MKIN5 | PKWR_MKIN6 | PKWR_MKIN7;
	
	/* disable all inputs and outputs except in 5 and out 0.
	 * this means only the power button will wake us up;
	 * not any key.
	 */
	PKWR = PKWR_MKIN5;
	pxa_gpio_mode(GPIO_NR_PALMT650_KP_MKOUT1 | GPIO_OUT);
	pxa_gpio_mode(GPIO_NR_PALMT650_KP_MKOUT2 | GPIO_OUT);
	pxa_gpio_mode(GPIO_NR_PALMT650_KP_MKOUT3 | GPIO_OUT);
	pxa_gpio_mode(GPIO_NR_PALMT650_KP_MKOUT4 | GPIO_OUT);
	pxa_gpio_mode(GPIO_NR_PALMT650_KP_MKOUT5 | GPIO_OUT);
	pxa_gpio_mode(GPIO_NR_PALMT650_KP_MKOUT6 | GPIO_OUT);
	SET_PALMT650_GPIO(KP_MKOUT1, 0); 
	SET_PALMT650_GPIO(KP_MKOUT2, 0); 
	SET_PALMT650_GPIO(KP_MKOUT3, 0); 
	SET_PALMT650_GPIO(KP_MKOUT4, 0); 
	SET_PALMT650_GPIO(KP_MKOUT5, 0); 
	SET_PALMT650_GPIO(KP_MKOUT6, 0); 

	KPC &= ~KPC_ASACT;
	KPC |= KPC_AS;
	KPC &= ~KPC_MIE;         /* matrix keypad interrupt disabled */
	PKSR = 0xffffffff; /* clear */

	/* ensure USB connection is broken */
	SET_PALMT650_GPIO(USB_PULLUP, 0); 

	/* hold current GPIO levels for now
	 * TODO: find out what we can disable.
	 */
	PGSR0 = GPLR0;
	PGSR1 = GPLR1;
	PGSR2 = GPLR2;
	PGSR3 = GPLR3;
	
	PCFR |= PCFR_OPDE; /* low power: disable oscillator */

	/* ensure oscillator is stable (see 3.6.8.1) */
	while(!(OSCC & OSCC_OOK));

	PCFR |= PCFR_GPROD; /* disable gpio reset */
	return;
}

static void palmt650_pxa_ll_pm_resume(void)
{
	/* re-enable GPIO reset */
	PCFR |= PCFR_GPR_EN;

	resume_vector[0] = resume_vector_save[0];
	resume_vector[1] = resume_vector_save[1];
	resume_vector[2] = resume_vector_save[2];
}

struct pxa_ll_pm_ops palmt650_ll_pm_ops = {
	.suspend = palmt650_pxa_ll_pm_suspend,
	.resume = palmt650_pxa_ll_pm_resume,
};

void palmt650_pxa_ll_pm_init(void)
{
	resume_vector = phys_to_virt(0xa0000000);
	pxa_pm_set_ll_ops(&palmt650_ll_pm_ops);
}
#endif

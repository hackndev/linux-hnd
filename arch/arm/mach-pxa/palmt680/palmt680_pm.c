/*
 *  arch/arm/mach-pxa/palmt680/palmt680_pm.c
 *
 *  Suspend/resume support with unmodified Treo 680 bootloader.
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
#include <asm/arch/palmt680-gpio.h>
#include <asm/arch/palmt680-init.h>

#ifdef CONFIG_PM

#define RESUME_VECTOR_PHYS 0xa0000000

#define PUCR		__REG(0x40F0004C)	/*Power Manager USIM Card Control/Status Register */

static u32 *resume_vector;
static u32 resume_vector_save[3];

static void palmt680_pxa_ll_pm_suspend(unsigned long resume_addr)
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
		| PWER_GPIO0		/* AC adapter */
		| PWER_GPIO1		/* reset */
		| PWER_GPIO9		/* not known yet - GSM HOST WAKE? */
//		| PWER_GPIO14		/* not known yet */
		| PWER_GPIO15		/* silent switch */
		| PWER_GPIO(20)		/* SD detect */
		| PWER_GPIO(23)		/* headphones detect */
		| PWER_RTC;		/* Real time clock alarm */
	
	/* falling-edge wake */
	PFER 	= PWER_GPIO0
		| PWER_GPIO1
		| PWER_GPIO9		/* not known yet */
		| PWER_GPIO15;		/* silent switch */
//		| PWER_GPIO(20);  /* SD detect */
	
	/* rising-edge wake */
	PRER 	= PWER_GPIO0
		| PWER_GPIO1
//		| PWER_GPIO14		/* not known yet - BT wake? */
		| PWER_GPIO15;		/* silent switch */
//		| (1 << 20);  /* SD detect */

	PEDR	= PWER_RTC;

	PCFR	= PCFR_FVC | PCFR_PI2CEN;

	/* wake-up by keypad  */
	PKWR = PKWR_MKIN0 | /* PKWR_MKIN1 | */ PKWR_MKIN2 | PKWR_MKIN3 |
	      PKWR_MKIN4 | PKWR_MKIN5 | PKWR_MKIN6 | PKWR_MKIN7 | PKWR_DKIN0;
	
	/* disable all inputs and outputs except in 0 and out 0.
	 * this means only the power button will wake us up;
	 * not any key.
	 */

	/* temporary enabling everything - I'll be happy if it will resume! ;) */

//	PKWR = PKWR_MKIN0;
//	pxa_gpio_mode(GPIO_NR_PALMT680_KP_MKOUT0 | GPIO_OUT);
/*	pxa_gpio_mode(GPIO_NR_PALMT680_KP_MKOUT1 | GPIO_OUT);
	pxa_gpio_mode(GPIO_NR_PALMT680_KP_MKOUT2 | GPIO_OUT);
	pxa_gpio_mode(GPIO_NR_PALMT680_KP_MKOUT3 | GPIO_OUT);
	pxa_gpio_mode(GPIO_NR_PALMT680_KP_MKOUT4 | GPIO_OUT);
	pxa_gpio_mode(GPIO_NR_PALMT680_KP_MKOUT5 | GPIO_OUT);
	pxa_gpio_mode(GPIO_NR_PALMT680_KP_MKOUT6 | GPIO_OUT);*/
//	SET_PALMT680_GPIO(KP_MKOUT0, 0);
// 	SET_PALMT680_GPIO(KP_MKOUT1, 0);
// 	SET_PALMT680_GPIO(KP_MKOUT2, 0); 
// 	SET_PALMT680_GPIO(KP_MKOUT3, 0); 
// 	SET_PALMT680_GPIO(KP_MKOUT4, 0); 
// 	SET_PALMT680_GPIO(KP_MKOUT5, 0); 
// 	SET_PALMT680_GPIO(KP_MKOUT6, 0); 

	KPC &= ~KPC_ASACT;
	KPC |= KPC_AS;
	KPC &= ~KPC_MIE;         /* matrix keypad interrupt disabled */
	PKSR = 0xffffffff; /* clear */

	/* ensure USB connection is broken */
	SET_PALMT680_GPIO(USB_PULLUP, 0); 

	/* PalmOS values
	 * TODO: find out what we can disable.
	 */
	PSSR	= PSSR_OTGPH | PSSR_SSS;
	PSLR	= 0xff000004;
	PSTR	= 0;
	PVCR	= 0x14;
	PUCR	= 0;


	PGSR0	= 0
		| (1 << 26)	/* GPIO26 - not known yet */
		| (1 << 25);	/* GPIO26 - not known yet */
	PGSR1	= 0
		| (1 << 17)	/* GPIO49 - not known yet */
		| (1 << 9)	/* GPIO41 - FFRTS? */
		| (1 << 8)	/* GPIO40 - KP_MKOUT6? */
		| (1 << 7)	/* GPIO39 - FFTXD */
		| (1 << 2);	/* GPIO34 - FFRXD */
	PGSR2	= 0
		| (1 << 31)	/* GPIO95 - AC97_RESET? */
//		| (1 << 23)	/* GPIO87 - not known yet */
		| (1 << 19)	/* GPIO83 - not known yet */
		| (1 << 18)	/* GPIO82 - not known yet */
		| (1 << 16);	/* GPIO80 - not known yet */
	PGSR3	= 0
		| (1 << 19)	/* GPIO115 - not known yet */
		| (1 << 7);	/* GPIO103 - KP_MKOUT0 */
	
	PCFR |= PCFR_OPDE; /* low power: disable oscillator */

	/* ensure oscillator is stable (see 3.6.8.1) */
	while(!(OSCC & OSCC_OOK));

	PCFR |= PCFR_GPROD; /* disable gpio reset */
	return;
}

static void palmt680_pxa_ll_pm_resume(void)
{
	/* re-enable GPIO reset */
	PCFR |= PCFR_GPR_EN;

	resume_vector[0] = resume_vector_save[0];
	resume_vector[1] = resume_vector_save[1];
	resume_vector[2] = resume_vector_save[2];
}

struct pxa_ll_pm_ops palmt680_ll_pm_ops = {
	.suspend = palmt680_pxa_ll_pm_suspend,
	.resume = palmt680_pxa_ll_pm_resume,
};

void palmt680_pxa_ll_pm_init(void)
{
	resume_vector = phys_to_virt(RESUME_VECTOR_PHYS);
	pxa_pm_set_ll_ops(&palmt680_ll_pm_ops);
}
#endif

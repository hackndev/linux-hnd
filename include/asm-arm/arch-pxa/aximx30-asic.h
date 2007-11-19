/*
 * include/asm/arm/arch-pxa/aximx30-asic.h
 *
 * Authors: Giuseppe Zompatori <giuseppe_zompatori@yahoo.it>
 *
 * based on previews work, see below:
 * 
 * include/asm/arm/arch-pxa/hx4700-asic.h
 *      Copyright (c) 2004 SDG Systems, LLC
 *
 * Specifications on http://handhelds.org/moin/moin.cgi/HpIpaqHx4700Hardware
 */

#ifndef _X30_ASIC_H_
#define _X30_ASIC_H_

#include <asm/hardware/ipaq-asic3.h>

/* ASIC3 */

#define X30_ASIC3_PHYS       PXA_CS3_PHYS

// ASIC3 GPIO C bank

#define GPIOC_LED_RED  		   	0	// Output
#define GPIOC_LED_GREEN		   	1	// Output
#define GPIOC_LED_BLUE			2	// Output
#define GPIOC_SD_CS_N  		   	3	// Output

#define GPIOC_CF_CD_N  		   	4	// Input
#define GPIOC_CIOW_N			5	// Output, to CF socket
#define GPIOC_CIOR_N			6	// Output, to CF socket
#define GPIOC_PCE1_N			7	// Input, from CPU

#define GPIOC_PCE2_N			8	// Input, from CPU
#define GPIOC_POE_N			9	// Input, from CPU
#define GPIOC_CF_PWE_N 		   	10	// Input
#define GPIOC_PSKTSEL  		   	11	// Input, from CPU

#define GPIOC_PREG_N			12	// Input, from CPU
#define GPIOC_PWAIT_N  		   	13	// Output, to CPU
#define GPIOC_PIOS16_N 		   	14	// Output, to CPU
#define GPIOC_PIOR_N			15	// Input, from CPU

// ASIC3 GPIO D bank

#define GPIOD_CPU_SS_INT		0	// Input
#define GPIOD_KEY_AP2_N		   	1	// Input, application button
#define GPIOD_BLUETOOTH_HOST_WAKEUP	2	// Input, from Bluetooth module
#define GPIOD_KEY_AP4_N		   	3	// Input, application button

#define GPIOD_CF_CD_N  		   	4	// Input, from CF socket
#define GPIOD_PIO_N			5	// Input
#define GPIOD_AUD_RECORD_N		6	// Input, Record button
#define GPIOD_SDIO_DETECT_N		7	// Input, from SD socket

#define GPIOD_COM_DCD  		   	8	// Input
#define GPIOD_AC_IN_N  		   	9	// Input, detect AC adapter
#define GPIOD_SDIO_INT_N		10	// Input, also connected to CPU
#define GPIOD_CIOIS16_N		   	11	// Input, from CF socket

#define GPIOD_CWAIT_N  		   	12	// Input, from CF socket
#define GPIOD_CF_RNB			13	// Input
#define GPIOD_USBC_DETECT_N		14	// Input
#define GPIOD_ASIC3_PIOW_N		15	// Input, from CPU

#endif /* _X30_ASIC_H_ */

extern struct platform_device x30_asic3;

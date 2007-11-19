/*
 * linux/include/asm-arm/arch-s3c2410/rx3000.h
 *
 * Written by Roman Moravcik <roman.moravcik@gmail.com>
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 */

#ifndef __ASM_ARCH_RX3000_H
#define __ASM_ARCH_RX3000_H

#include <linux/leds.h>
#include <asm/arch/irqs.h>

#define RX3000_PA_ASIC3		(S3C2410_CS1)
#define RX3000_PA_ASIC3_SD	(S3C2410_CS2)
#define RX3000_ASIC3_IRQ_BASE	(IRQ_S3C2440_AC97 + 1)

#define RX3000_PA_WLAN		(S3C2410_CS4)

EXTERN_LED_TRIGGER_SHARED(rx3000_radio_trig);

#endif  // __ASM_ARCH_RX3000_H

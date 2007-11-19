/* bootldr-h1900.S
 *
 * h1900-specific definitions and code.
 */

#include <asm/arch/h1900-init.h>

#define ARCH		H1900
#define INIT_MSC0	0x26e07ff8
#define INIT_MSC1	0x7ff42358
#define INIT_MSC2	0x7ff47ff4
#define INIT_MDCNFG	(MDCNFG_DE0 | MDCNFG_DE1 | MDCNFG_DE2 | MDCNFG_DE3 | \
			 MDCNFG_DCAC0 | (2*MDCNFG_DRAC0) | MDCNFG_DNB0 | \
			 MDCNFG_DTC0 | MDCNFG_DLATCH0 | MDCNFG_DWID0 | \
			 MDCNFG_DCAC2 | (2*MDCNFG_DRAC2) | MDCNFG_DNB2 | \
			 MDCNFG_DTC2 | MDCNFG_DLATCH2 | MDCNFG_DWID2)
/* ipaq/h1910
 * ----Memory Controller----
 * 0x48000000 -> 0x09cd09cd     MDCNFG
 * 000 0 1 0 01 1 10 01 1 0 1
 * (0*DSA1111_0 | 1*DLATCH0 | 0*DADDR0 |
    1*DTC0 | 1*DNB0 | 2*DRAC0 | 1*DCAC0 |
    1*DWID0 | 0*DE1 | 1*DE0)
 */
#include "bootldr-machdefs.S"

Check_H1900:	@ we detect h1900 by asic3 being at 0x0C001000
		mov	r0, #0x0C000000
		add	r0, r0, #0x00001000
		ldrh	r4, [r0, #0]
		ldr	r5, H1900_ASIC3_PART_NUMBER_1
		cmp	r4, r5
		mov	pc, lr
		ldrh	r4, [r0, #4]
		ldr	r5, H1900_ASIC3_PART_NUMBER_2
		cmp	r4, r5
		mov 	pc, lr

Init_H1900:	ldr	r0, GPIO_BASE
		ldr	r1, [r0, #__OFFSET(GPLR0)]
		tst	r1, #1 << 21		@ button was pressed?
		orreq	r12, r12, #BF_ACTION_BUTTON
		biceq	r12, r12, #BF_NORMAL_BOOT
		mov	pc, lr

H1900_ASIC3_PART_NUMBER_1:
		.long	0x4854
H1900_ASIC3_PART_NUMBER_2:
		.long	0x432d

End_H1900:

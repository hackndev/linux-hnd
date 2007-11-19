/* bootldr-lubbock.S
 *
 * Lubbock-specific definitions and code.
 */

#warning Lubbock code won't run on a real Lubbock.
#warning It should be more accurately titled 'skyeye'.
#define MACH_TYPE_Lubbock 89
#include <asm/arch/h1900-init.h>
#define ARCH		Lubbock
#define INIT_MSC0	0x26e07ff8
#define INIT_MSC1	0x7ff42358
#define INIT_MSC2	0x7ff47ff4
#define INIT_MDCNFG	(MDCNFG_DE0 | MDCNFG_DE1 | MDCNFG_DE2 | MDCNFG_DE3 | \
			 MDCNFG_DCAC0 | (2*MDCNFG_DRAC0) | MDCNFG_DNB0 | \
			 MDCNFG_DTC0 | MDCNFG_DLATCH0 | MDCNFG_DWID0 | \
			 MDCNFG_DCAC2 | (2*MDCNFG_DRAC2) | MDCNFG_DNB2 | \
			 MDCNFG_DTC2 | MDCNFG_DLATCH2 | MDCNFG_DWID2)

#include "bootldr-machdefs.S"

Check_Lubbock:	cmp	r0, r0
		mov 	pc, lr

Init_Lubbock:	orr	r12, r12, #BF_ACTION_BUTTON
		bic	r12, r12, #BF_NORMAL_BOOT
		mov	pc, lr

End_Lubbock:

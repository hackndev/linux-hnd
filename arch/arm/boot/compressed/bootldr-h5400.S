/* bootldr-h5400.S
 *
 * h5400-specific definitions and code.
 */

#include <asm/arch/h5400-init.h>

#define ARCH		H5400
#define INIT_MSC0	0x129c24f2
#define INIT_MSC1	0x7ff424fa
#define INIT_MSC2	0x7ff47ff4
#define INIT_MDREFR	(MDREFR_K1RUN | MDREFR_K2RUN | \
			 MDREFR_E1PIN | MDREFR_K0DB2 | MDREFR_K0RUN | \
			 MDREFR_E0PIN | 0x18)
#define INIT_MDCNFG	(MDCNFG_DSA1111_0 | \
			 MDCNFG_DE0 | MDCNFG_DE1 | MDCNFG_DE2 | MDCNFG_DE3 | \
			 MDCNFG_DCAC0 | (2*MDCNFG_DRAC0) | MDCNFG_DNB0 | \
			 (2*MDCNFG_DTC0) | MDCNFG_DLATCH0 | \
			 MDCNFG_DCAC2 | (2*MDCNFG_DRAC2) | MDCNFG_DNB2 | \
			 (2*MDCNFG_DTC2) | MDCNFG_DLATCH2)
/* ipaq3/h5400
 * ----Memory Controller----
 *	0x48000000->0x1ac9	MDCNFG
 *	    000 1 1 0 10 |  1 10 01 0 0 1
 *         (  1*DSA1111 | 1*DLATCH0 | 0*DADDR0 | 2*DTC0 | DNB0 | 2*DRAC0 | 1*DCAC0 | DE0 )
 *	0x48000004->0x020df018	MDREFR
 *          0000 00 1 0 0 0 00 1 1 0 1 | 1 1 1 1 0000 0001 1000
 *          DRI=0x018 E0PIN=1 K0RUN=1 K0DB2=1 E1PIN=1 K1RUN=1 K1DB2=0 K2RUN=1 K2DB2=1 APD=00
 *	0x48000008->0x129c24f2
 *	0x4800000c->0x7ff424fa
 *	0x48000010->0x7ff47ff4
 *	0x48000014->0x3
 *	0x48000018->0x0
 *	0x4800001c->0x40004
 *	0x48000020->0x1fe01fe
 *	0x48000024->0x0
 *	0x48000028->0x10504
 *	0x4800002c->0x10504
 *	0x48000030->0x10504
 *	0x48000034->0x10504
 *	0x48000038->0x4715
 *	0x4800003c->0x4715
 *	0x48000040->0x220032
 *	0x48000044->0x8
 */
#include "bootldr-machdefs.S"

Check_H5400:	@ The h5400 uses MediaQ 1132 at base address 0x04000000
		mov	r0,     #0x04000000
		orr	r0, r0, #0x00040000
		ldr	r0, [r0, #0x400]	@ Get PCI id
		ldr	r1, pci_id_mq1132
		cmp	r0, r1
		mov	pc, lr

Init_H5400:	ldr	r0, GPIO_BASE
		ldr	r1, [r0, #__OFFSET(GPLR0)]
		tst	r1, #(1<<4)		@ button was pressed?
		orreq	r12, r12, #BF_ACTION_BUTTON
		biceq	r12, r12, #BF_NORMAL_BOOT
		mov	pc, lr

End_H5400:

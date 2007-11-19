/* bootldr-aximx5.S
 *
 * Dell Axim X5 specific definitions and code.
 *
 * To enter LAB on Dell Axim hold the JOYPAD CENTER pressed on reset.
 */

#include <asm/arch/aximx5-init.h>
#include <asm/arch/aximx5-gpio.h>

#define ARCH		AXIMX5
#define INIT_MSC0	0x23f223f2
#define INIT_MSC1	0x7ff07ff0
#define INIT_MSC2	0x248c7ff9
#define INIT_MDCNFG	(MDCNFG_DSA1111_0 | MDCNFG_DLATCH0 | (1*MDCNFG_DTC0) | \
			 MDCNFG_DNB0 | (2*MDCNFG_DRAC0) | (1*MDCNFG_DCAC0) | \
			 MDCNFG_DE0)
@@@@@@ todo: KD2DB2 needed???
#define INIT_MDREFR	(MDREFR_K2DB2 | MDREFR_K1RUN | \
			 MDREFR_E1PIN | MDREFR_K0DB2 | (0x24))

@ 0x48000000->0x19c9	MDCNFG
@    000 1 1 0 01 |  1 10 01 0 0 1
@        DSA1111_0 | DLATCH0 | 1*DTC0 | DNB0 | 2*DRAC0 | 1*DCAC0 | DE0
@ 0x48000004->0x0009c024	MDREFR
@    000000 0 0 0 0 00 1 0 0 1 | 1 1 0 0 000000100100
@        K2DB2 | K1RUN | E1PIN | K0DB2 | DRI=0x24 
@ 48000000: 000019c9 0009c024 23f223f2 7ff07ff0
@ 48000010: 248c7ff9 00000002 00000000 00040004
@ 48000020: 01fe01fe 00000000 00010204 00044833
@ 48000030: 00010204 00044833 00010204 00044833
@ 48000040: 00220022 00000008

#define INIT_CMDLINE "console=ttyS2,115200n8"
#include "bootldr-machdefs.S"

Check_AXIMX5:	@ Set up GPIOs as for Axim
		ldr	r0, [r11, #ARCH_GPIO_TABLE]
		add	r0, r0, r10
		mov	r9, lr
		mov	r8, r11
		mov	r11, #0
		bl	init_gpio_msc
		mov	lr, r9
		mov	r11, r8

		@ Small delay: wait until MediaQ power sets up
		mov	r3, #0x1000000
1:		subs	r3, r3, #1
		bne	1b

		@ Axim X5 uses MediaQ 1132 at base address 0x14000000
		mov	r0,     #0x14000000
		orr	r0, r0, #0x00040000

		@ Little endian mode
		mov	r1, #0x0055		@ lower bits = 01
		orr	r1, r1, #0x5500		@ see DC00R notes in specs
		add	r2, r0, #0x380
		strh	r1, [r2]
		@ Enable MediaQ oscillator clock
		mov	r1, #3
		str	r1, [r0, #0x384]

		@ Small delay: wait until oscillator clock runs
		mov	r3, #0x4000000
1:		subs	r3, r3, #1
		bne	1b

		@ Enable CPU Configuration module
		mov	r1, #1
		str	r1, [r0, #0x388]

		@ Yet another small delay
		mov	r3, #0x1000000
1:		subs	r3, r3, #1
		bne	1b

		ldr	r0, [r0, #0x400]	@ Get PCI id
		ldr	r1, pci_id_mq1132
		cmp	r0, r1
		moveq	pc, lr			@ Return with Z flag set

		adrl	r0, GPIO_Init_BOOT
		mov	r9, r11
		mov	r11, #0
		bl	init_gpio_msc
		movs	r11, r9			@ Return with Z flag clear
		mov	pc, lr

Init_AXIMX5:	ldr	r0, GPIO_BASE
		ldr	r1, [r0, #__OFFSET(GPLR0)]
		tst	r1, #(1<<GPIO_NR_AXIMX5_JOY_PRESSED)
		orreq	r12, r12, #BF_ACTION_BUTTON
		biceq	r12, r12, #BF_NORMAL_BOOT
		mov	pc, lr

End_AXIMX5:

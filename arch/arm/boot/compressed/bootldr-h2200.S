/* bootldr-h2200.S
 *
 * iPAQ h2200 specific definitions and code.
 *
 * To enter LAB on h2200 hold the JOYPAD CENTER pressed on reset.
 */

#include <asm/arch/h2200-init.h>
#include <asm/arch/h2200-gpio.h>

#define ARCH		H2200
#define INIT_MSC0	0x246c44cc
#define INIT_MSC1	0x7ff07ff0
#define INIT_MSC2	0x7ff07ff0

@ Since neither DE2 nor DE3 are set, I'm going to assume that I don't have to
@ set the high bits.
#define INIT_MDCNFG	(MDCNFG_DE0 | MDCNFG_DE1 | (0*MDCNFG_DWID0) | \
                         (1*MDCNFG_DCAC0) | (2*MDCNFG_DRAC0) | MDCNFG_DNB0 | \
                         (1*MDCNFG_DTC0))
#define INIT_MDREFR	(MDREFR_K0FREE | MDREFR_APD | MDREFR_K2DB2 | \
			 MDREFR_K1RUN | MDREFR_E1PIN | MDREFR_K0DB2 | \
			 MDREFR_K0RUN | 0x18)

@ 0x48000000->0x01C801CB	MDCNFG
@    000 0 0 0 01 1 10 01 0 0 0
@    000 0 0 0 01 1 10 01 0 1 1
@ 0x48000004->0x0099E018	MDREFR
@    000000 0 0 1 0 0 1 1 0 0 1 1 1 1 0 000000011000
@ 48000000 | 01c801cb 0099e018 246c44cc 7ff07ff0 | .........Dl$....
@ 48000010 | 7ff07ff0 00000002 00000000 00040004 | ................
@ 48000020 | 01fe01fe 00000000 0001c416 0001c416 | ................
@ 48000030 | 0001870a 0001870a 0001870a 0001870a | ................
@ 48000040 | 00220022 00000009                   | ".".....

#define INIT_CMDLINE "console=ttyS0,115200n8"
//#define INIT_CMDLINE "console=tty0"
#include "bootldr-machdefs.S"

Check_H2200:
		@ h2200 uses MediaQ 11x8 at base address 0x04000000
		mov	r0,     #0x04000000
		orr	r0, r0, #0x00040000

		ldr	r0, [r0, #0x400]	@ Get PCI id
		ldr	r1, H2200_MQ_ID
		cmp	r0, r1
		mov	pc, lr			@ Return

Init_H2200:	@ldr	r0, GPIO_BASE
		@ldr	r1, [r0, #__OFFSET(GPLR0)]
		@tst	r1, #(1<<GPIO_NR_AXIMX5_JOY_PRESSED)
		@ Initialize the hamcop GPIOs.
		mov	r0, #0x7000	@ HAMCOP GPIO base
		mov	r1, #0x0000
		strh	r1, [r0, #0x0]	@ GPACON
		strh	r1, [r0, #0x10]	@ GPBCON
		
		@ Now read action button
		ldrh	r1, [r0, #0x14]	@ GPBDAT
		tst	r1, #(1 << 2)	@ action button
		orreq	r12, r12, #BF_ACTION_BUTTON
		biceq	r12, r12, #BF_NORMAL_BOOT

#if 0
#warning OK, I see you have h2210 bringup enabled. I hope you know what you're doing.
		tst	pc, #0xFF000000
		beq	H2200_bringup
#endif
		mov	pc, lr

H2200_MQ_ID:
		.word 0x01884d51

#if 0
#define NAND_CMD_READA  0x00
#define NAND_CMD_READB  0x01
#define NAND_CMD_READC  0x50
#define NAND_CMD_STATUS 0x70
#define NAND_CMD_RESET  0xFF

H2200_bringup:
		dbg("h2200: bringing up the board: initializing memory\r\n")
		bl	setup_memory
		dbg("h2200: bringing up the board: evacuating to memory\r\n")
		mov     r0, #0x0
		mov     r4, #0xA0000000
		orr     r4, r4, #0x00002000
		mov	r1, r4
		mov     r2, #0x1000
	1:     	ldr     r3, [r0], #4
		str     r3, [r1], #4
		subs    r2, r2, #4
		bne     1b
		add	pc, pc, r4
		dbg("h2200: bringing up the board: setting up HAMCOP\r\n")
		mov	r0, #0x48000000
		ldr	r1, H2200_bringup_msc0
		str	r1, [r0, #0x08]
		mov	r0, #0x4000		@ VLIO
		mov	r1, #0x0001
		strh	r1, [r0]
		mov	r0, #0x6000		@ wdt
		mov	r1, #0x0000
		strh	r1, [r0]
		dbg("h2200: bringing up the board: reading NAND flash\r\n")
		mov	r3, #0x5000		@ NAND base
		mov	r0, #0x0003		@ NAND FLASH mode
		strh	r0, [r3, #0x08]		@ NFCONT0
		
		mov	r4,     #0xA0000000	@ bootloader current destination
		orr	r4, r4, #0x00008000
		mov	r5, #((512*1024)/512)	@ size of bootloader in blocks
		mov	r6, #0			@ current block
		
		mov	r0, #NAND_CMD_READA
		strh	r0, [r3, #0x10]		@ NFCMMD
		mov	r0, #0x00
		strh	r0, [r3, #0x14]		@ NFADDR0
		mov	r0, r6
		strh	r0, [r3, #0x14]		@ NFADDR0
		mov	r0, r6, LSL#8
		strh	r0, [r3, #0x14]		@ NFADDR0
		ldrh	r0, [r3, #0x1C]		@ NFDATA
		and	r0, r0, #0xFF
		mov	r7, r0
		ldrh	r0, [r3, #0x1C]		@ NFDATA
		and	r0, r0, #0xFF
		orr	r7, r7, r0,LSL#8
		ldrh	r0, [r3, #0x1C]		@ NFDATA
		and	r0, r0, #0xFF
		orr	r7, r7, r0,LSL#16
		ldrh	r0, [r3, #0x1C]		@ NFDATA
		and	r0, r0, #0xFF
		orr	r7, r7, r0,LSL#24
		
		dbg("First word: ")
		dbghex(r7)
		dbg("\r\n")
		
		
	1:	b	1b
		
		
		
H2200_bringup_msc0:
		.long	0x246c44cc
#endif		

End_H2200:

/* bootldr-xscale.S
 * Copyright (c) 1999-2005 Compaq Computer Corporation
 * Copyright (c) 2003-2005 Joshua Wise
 * Copyright (c) 2004-2005 Andrew Zabolotny
 * All rights reserved
 *
 * LAB bootstrap code for XScale, heavily modified from boot-pxa.s in bootldr.
 */

		.section        ".start", "ax"

		.macro	cpwait, rd
		mrc	p15, 0, \rd, c2, c0, 0	@ arbitrary read of cp15
		mov	\rd, \rd		@ wait for completion
		sub 	pc, pc, #4		@ flush instruction pipeline
		.endm

		.macro	cpwait_ret, lr, rd
		mrc	p15, 0, \rd, c2, c0, 0	@ arbitrary read of cp15
		sub	pc, \lr, \rd, LSR #32	@ wait for completion and
		.endm

#undef ALIGN
#undef set_bit

#define _ASM_ONLY
#include <linux/lab/boot_flags.h>
#include <asm/mach-types.h>
#include <asm/arch/pxa-regs.h>

@ Some preprocessor black magic
#define CONCAT2(x,y)		x##y
#define CONCAT(x,y)		CONCAT2(x,y)

@----------- The machine-dependent method/variable offsets (relative to R11) --
@ FUNC: Architecture check. Returns the Z flag set if architecture matches.
@       Must not modify r11 and r12.
#define	ARCH_CHECK		0
@ FUNC: Machine initialization. Must return boot flags in R12, possibly
@       modified by user actions (pressed buttons, over serial, USB and such).
#define ARCH_INIT		4
@ VAR: The pointer to GPIO initialization table
#define ARCH_GPIO_TABLE		8
@ VAR: Machine identifier (one of MACH_TYPE_xxx values, see include/asm/mach-types.h)
#define ARCH_TYPE		12
@ VAR: The MDREFR register init value for that specific platform
#define ARCH_MDREFR		16
@ VAR: The MDCNFG register init value for that specific platform
#define ARCH_MDCNFG		20
@ VAR: Last comes the specific command-line for this platform
#define ARCH_CMDLINE		24

@ Define this macro to no-op to be able to use pxa.h defines
#undef __REG
#define __REG(x)		x
@ Extract the base address from the address of a XScale register
#define __BASE(x)		(x & 0xffff0000)
@ Extract the offset from the address of a XScale register
#define __OFFSET(x)		(x & 0x0000ffff)

@ Debug output routines
@ NOTE: These macros presumably destroy ALL non-fixed registers because 
@       they cannot use any memory to backup the registers.
#ifdef CONFIG_LAB_DEBUG
@ Usage: dbg("Welcome to LAB")
#define dbg(str) \
		adr	r0, 98f ; \
		adr	lr, 99f ; \
		b	out_str ; \
98:		.asciz	str ; \
		.align	2 ; \
99:
@ Usage: dbghex(r10) or dbghex(#0x12)
#define dbghex(reg) \
.ifnc  reg,r0 ; \
		mov	r0, reg ; \
.endif ; \
		bl      out_hex_word
@ Usage: dbgchr('\n') or dbgchr('A')
#define dbgchr(chr) \
		mov	r0, # ## chr; \
		bl      out_char
#else
#define dbg(str)
#define dbghex(reg)
#define dbgchr(chr)
#endif

@----------------------------------------------------------------------------
@ Boot loader is position-independent. On different platforms it is started
@ from different addresses: 0 if it is the main boot loader, 0x40000 if it 
@ is a secondary bootloader; 0xa8000000 if it is run from RAM (as a regular
@ Linux kernel).
@
@ All static pointers are relative. The base address of the loader can be
@ accessed at any time in R10. To get the real address of anything get the
@ address relative to bootldr_start, and add it to R10.
@
@ Persistent register usage:
@	R9:  current output position on the debug output device
@	     (if applicable). For example, when using display output
@	     this registers contains current address in video RAM.
@	R10: bootloader physical address (0 if running from start of ROM)
@	R11: arch object address
@	R12: boot flags
@----------------------------------------------------------------------------

bootldr_start:
@ 0x00: Reset/wakeup
		b	reset_wakeup
@ 0x04: Undefined instruction
		b	handler_undef
@ 0x08: SWI
		b	handler_SWI
@ 0x0c: Prefetch abort
		b	handler_prefetch_abort
@ 0x10: Data abort
		b	handler_data_abort
@ 0x14: Unused
		nop
@ 0x18: IRQ
		b	handler_irq
@ 0x1c: FIQ
		b	handler_FIQ

abort_storage:
		.word	0
		.word	0
		.word	0
		.word	0
		.word	0
		.word	0
		.word	0
		.word	0
		.word	0
		.word	0
		.word	0
		.word	0
		.word	0

msg_undef:	.asciz	"UND INST"
msg_swi:	.asciz	"SWI"
msg_pref_abort:	.asciz	"PREF ABRT at 0x"
msg_data_abort:	.asciz	"DATA ABRT at 0x"
msg_irq:	.asciz	"IRQ"
msg_fiq:	.asciz	"FIQ"
		.align	2

handler_undef:
		adr	r13, abort_storage
		stmia	r13, {r0-r12}
		adr	r0, msg_undef
		mov	r1, #0
		b	handler_common

handler_SWI:
		adr	r13, abort_storage
		stmia	r13, {r0-r12}
		adr	r0, msg_swi
		mov	r1, #0
		b	handler_common

handler_prefetch_abort:
		adr	r13, abort_storage
		stmia	r13, {r0-r12}
		adr	r0, msg_pref_abort
		mrc	p15, 0, r1, c6, c0, 0	@ fault address
		b	handler_common

handler_data_abort:
		adr     r13, abort_storage
		stmia   r13, {r0-r12}
		adr	r0, msg_data_abort
		mrc	p15, 0, r1, c6, c0, 0	@ fault address
		b	handler_common

handler_irq:
		adr     r13, abort_storage
		stmia   r13, {r0-r12}
		adr	r0, msg_irq
		mov	r1, #0
		b	handler_common

.globl handler_FIQ
handler_FIQ:
		adr     r13, abort_storage
		stmia   r13, {r0-r12}
		adr	r0, msg_fiq
		mov	r1, #0
		b	handler_common

handler_common:
#ifdef CONFIG_LAB_DEBUG
		mov	r8, r1
		bl	out_str
		movs	r0, r8
		bleq	out_hex_word
#endif

		@ restore registers - just in case we're using JTAG 
		@ and want to take a look at the CPU registers
		ldmia   r13, {r0-r12}
		@ fallback to endless loop

@ For now just loop forever
@ I don't know what else we can do without knowing the current architecture
unknown_arch:
#ifdef CONFIG_LAB_DEBUG
		dbg ("LAB pre-boot panic: unknown arch!\n")
#endif
1:
		b	1b

@ Default initial GPIO & MSC table (common for all platforms).
@ We have to be very careful here so that the table can be used on
@ all platforms for platform detection stuff. After the platform is
@ properly detected, the GPIOs and MSCs are re-initialized.
GPIO_Init_BOOT:
		.long	0x00000000	@ GPSRx  was 0x00200000, 22xx 0x02428000 | (1 << 24)
		.long	0x00000000	@ GPSRy  was 0x00000000, 22xx 0x20ff0382
		.long	0x00010000	@ GPSRz  was 0x00000800, 22xx 0x0001431c
		.long	0,0,0		@ GPCRx,y,z
		.long	0x00008000	@ GPDRx, 22xx 0xf7e38c00
		.long	0x0000000A	@ GPDRy, 22xx 0xbcffbf83
		.long	0x00019000	@ GPDRz, 22xx 0x000157ff
		.long	0x80000000	@ GAFR0x
		.long	0x00000010	@ GAFR1x
		.long	0x00008008	@ GAFR0y
		.long	0x00000000	@ GAFR1y
		.long	0xA0000000	@ GAFR0z
		.long	0x00000002	@ GAFR1z
		.long	0x26e026e0	@ MSC0
		.long	0x7ff42358	@ MSC1
		.long	0x74a42494	@ MSC2

@-----------------------------------------------------------------------------
@ Reset entry point
@-----------------------------------------------------------------------------
reset_wakeup:
@ Workaround for PXA250 early termination of SDRAM auto-refresh (Erratum #114)
		@ Skip erratum 114 if running from RAM as this will whack mdrefr and never set it back.
		tst	pc, #0xFF000000
		bne	noerratum114

		ldr	r0, MEM_BASE

		mov	r1, #0x03800000
		orr	r1, r1, #0x000ff000
		str	r1, [r0, #__OFFSET(MDREFR)]
		mov	r0, #0xa0000000
		ldr	r1, [r0]
		ldr	r1, [r0]

		mov	r2, #0x2000
1:		ldr	r1, [r0]
		subs	r2, r2, #1
		bpl	1b
noerratum114:
		@ end of workaround for erratum 114

@ R10 == base physical address of the bootloader
		adr	r10, bootldr_start
		mov	r12, #BF_NORMAL_BOOT
		
		@ must configure memory interface properly before detecting
		@ platform type. Don't do it if running from RAM though:
		@ assume it's already done before starting the kernel.
		adr	r0, GPIO_Init_BOOT
		mov	r11, #0
		tst	r10, #0xff000000
		bleq	init_gpio_msc

@ Bring clocks to life almost immediately, so that we'll have serial.
		bl	setup_clocks

@ Initialize output.
@ This is OK because if the user's platform does not support some
@ particular debug mode, we aren't going to use that mode.
#ifdef CONFIG_LAB_DEBUG
		bl	out_init
#endif

		dbg ("Booting LAB\r\n")
		dbg ("  Kernel base: 0x")
		dbghex (r10)
		dbg ("\r\n")

@ Probe machine type
		dbg ("Probing architectures...\r\n");
		adrl	r11, Arch_Table
1:		ldr	r12, [r11], #4		@ Get the length of this arch struct
		tst	r12, r12
		beq	unknown_arch		@ No more machine data

#ifdef CONFIG_LAB_DEBUG
		dbgchr ('\r');
		dbg ("  Probing: 0x");		@ Tell what we're probing
		ldr	r0, [r11, #ARCH_TYPE]
		dbghex (r0)
#endif

		ldr	r0, [r11, #ARCH_CHECK]
		mov	lr, pc
		add	pc, r10, r0		@ Call architecture check method
		addne	r11, r11, r12
		bne	1b
		
		dbg ("\r  Found:   \r\n")

@ Now initialize machine GPIOs
		dbg ("Initializing system GPIOs.\r\n");
		ldr	r0, [r11, #ARCH_GPIO_TABLE]
		add	r0, r0, r10
		bl	init_gpio_msc

@ Okay, now initialize the machine
		mov	r12, #BF_NORMAL_BOOT
@		mov	r12, #BF_ACTION_BUTTON	@@@@@@@@@ DEBUG @@@@@@@@
		ldr	r0, [r11, #ARCH_INIT]
		mov	lr, pc
		add	pc, r10, r0
@ Fine, now R12 contains the requested bootldr actions

#ifdef CONFIG_LAB_DEBUG
		dbg ("Machine type: 0x")
		ldr	r0, [r11, #ARCH_TYPE]
		dbghex (r0)
		dbg ("\r\n")
#endif

		tst	r12, #BF_ACTION_BUTTON
		bne	force_lab_start

#if 0
@ LAB has no chance yet to fit in 256K
@ So, for now skip the check for WinCE at 256K - in the best case we'll find
@ our own tail there, in the worst case we'll just lock up (on h1910).

		tst	r12, #BF_H1900
		beq	noWince

/*	Check for a wince img and jump to it if we find one.
 *	otherwise we write stuff in ram and bad stuff happens.
 */
CheckForWince:
		mov	r0, #FLASH_BASE
		add	r0, r0,#0x40000
@ look for wince image
		ldr	r1, [r0],#4
		ldr	r2, WINCE_MAGIC_1
		cmp	r1, r2
		bne	noWince
		@ ok, some wince images have 0's where they are erased and
		@ others have F's
		ldr	r3, WINCE_MAGIC_LONG_0
wince_loop:
		ldr	r1, [r0],#4
		cmp	r1, #0
		bne	1f
		b	2f
1:
		cmp	r1,#0xFFFFFFFF
		bne	noWince
2:
		subs	r3,r3,#1
		bne	wince_loop

		ldr	r1,[r0],#4
		ldr	r2,WINCE_MAGIC_2
		cmp	r1,r2
		bne	noWince

WakeUpWince:
		@ now jump to wince
		ldr	r1,WINCE_ADDR
		nop
		nop
		nop
		nop
		mov	pc,r1

WINCE_ADDR:	.long	0x00041000
WINCE_MAGIC_1:	.long	WINCE_PARTITION_MAGIC_1
WINCE_MAGIC_LONG_0:
		.long	WINCE_PARTITION_LONG_0
WINCE_MAGIC_2:	.long	WINCE_PARTITION_MAGIC_2
bootldr_magic:	.long	BOOTLDR_MAGIC

noWince:
#endif

		@ Spew out what is in RCSR first.
		ldr	r1, PMCR_BASE
		ldr	r0, [r1, #__OFFSET(RCSR)]
		mov	r2, #0xF
		str	r2, [r1, #__OFFSET(RCSR)]

		@ Sleep wakeup?
		tst     r0, #4			@ SMR
		bne     wakeup

		@ No? Just boot.

force_lab_start:
		@ We need to clear RCSR here ANYWAY,
		@ we might not've gotten to it before.
		ldr	r1, PMCR_BASE
		ldr	r0, [r1, #__OFFSET(RCSR)]
		str	r0, [r1, #__OFFSET(RCSR)]

		@ check to see if we're operating out of DRAM
		tst	r10, #0xff000000
		bne	run_interactive

		bl	setup_memory

run_interactive:
		dbg ("Running interactively\r\n")

		@ Get ready to call C functions
		ldr	sp, kernel_stack
		dbg ("Kernel stack at: 0x")
		dbghex (sp)
		dbg ("\r\n")

		dbg ("Going to run kernel from 0x");
		ldr	r0, kernel_start
		dbghex (r0)
		dbg ("\r\n")

		@ make sure we don't reloc if we're already in ram
		tst	r10, #0xff000000
		bne	noreloc

#ifdef CONFIG_LAB_DEBUG
		dbg ("Relocating kernel from 0x")
		dbghex (r10)
		dbg (" to 0x")
		ldr	r0, kernel_start
		dbghex (r0)
		dbg (":")
#endif
		@ relocate ourselves from whatever address we are in ROM
		mov	r0, r10			@ Source address
		add	r2, r0, #0x80000	@ End address (+512k)
		ldr	r1, kernel_start	@ Destination address
		mov	r8, r1			@ Hold dest addr for later use

1:		ldmia	r0!, {r3-r6}		@ Copy 16 bytes at once
		stmia	r1!, {r3-r6}
		cmp	r0, r2
		bne	1b

		sub	r0, pc, r10
		add	pc, r0, r8		@ Jump to DRAM

		@ Now we're at 0xa0008xxx
		sub	r0, r11, r10
		add	r11, r0, r10		@ Adjust machine table addr
		mov	r10, r8			@ Modify base address reg

		dbg (" done\r\n")

noreloc:
#ifdef CONFIG_LAB_DEBUG
		@ save r9 in the case C routines will call debug output
		str	r9, saved_r9
		orr	r12, r12, #BF_NEED_RESTORE_R9
#endif
		dbg ("Setting up tagged list...\r\n")
		@ set up the tagged list
		mov	r0, r12			@ arg0: bootflags
		add	r1, r11, #ARCH_CMDLINE	@ arg1: cmdline
		bl	tagize

		dbg ("*** Jumping into kernel\r\n")
		ldr	r7, [r11, #ARCH_TYPE]
		mov	r8, #0x0

		b	continue_kernel		@ Go to main kernel code

wakeup:		ldr	r1, CLOCK_BASE
		mov	r0, #CKEN6_FFUART
		str	r0, [r1,#__OFFSET(CKEN)]@ re-enable UART clock

		@ get SDRAM going again
		bl	setup_memory

		@ retrieve the scratchpad value and jump to that address
		ldr	r7, PMCR_BASE
		ldr	pc, [r7, #__OFFSET(PSPR)]

@-----------------------------------------------------------------------------
@ Load GPIO state and MSCs from a memory array
@ IN:		R0 = GPIO state array address
@		R11 = 0 for initial setup (skip MSC0)
@		   != 0 for final setup (skip MSC0, don't touch PMCR)
@ OUT:		nothing
@ MODIFIES:	R0,R1,R2
@-----------------------------------------------------------------------------
init_gpio_msc:
		ldr	r1, GPIO_BASE
		ldr	r2, [r0], #4
		str	r2, [r1, #__OFFSET(GPSR0)]
		ldr	r2, [r0], #4
		str	r2, [r1, #__OFFSET(GPSR1)]
		ldr	r2, [r0], #4
		str	r2, [r1, #__OFFSET(GPSR2)]

		ldr	r2, [r0], #4
		str	r2, [r1, #__OFFSET(GPCR0)]
		ldr	r2, [r0], #4
		str	r2, [r1, #__OFFSET(GPCR1)]
		ldr	r2, [r0], #4
		str	r2, [r1, #__OFFSET(GPCR2)]

		ldr	r2, [r0], #4
		str	r2, [r1, #__OFFSET(GPDR0)]
		ldr	r2, [r0], #4
		str	r2, [r1, #__OFFSET(GPDR1)]
		ldr	r2, [r0], #4
		str	r2, [r1, #__OFFSET(GPDR2)]

		@ set up GPIOs
		ldr	r2, [r0], #4
		str	r2, [r1, #__OFFSET(GAFR0_L)]
		ldr	r2, [r0], #4
		str	r2, [r1, #__OFFSET(GAFR0_U)]
		ldr	r2, [r0], #4
		str	r2, [r1, #__OFFSET(GAFR1_L)]
		ldr	r2, [r0], #4
		str	r2, [r1, #__OFFSET(GAFR1_U)]
		ldr	r2, [r0], #4
		str	r2, [r1, #__OFFSET(GAFR2_L)]
		ldr	r2, [r0], #4
		str	r2, [r1, #__OFFSET(GAFR2_U)]

		@ let GPIOS be GPIOS
		cmp	r11, #0
		ldreq	r1, PMCR_BASE
		addeq	r1, r1, #__OFFSET(PSSR)
		moveq	r2, #0x30
		streq	r2, [r1]

		@ set up MSC0, MSC1, MSC2
		ldr	r1, MEM_BASE

		ldr	r2, [r0], #4		@ MSC0_INIT
		strne	r2, [r1, #__OFFSET(MSC0)]
		@ spec says to ldr the mscs after a set
		ldrne	r2, [r1, #__OFFSET(MSC0)]

		ldr	r2, [r0], #4		@ MSC1_INIT
		str	r2, [r1, #__OFFSET(MSC1)]
		ldr	r2, [r1, #__OFFSET(MSC1)]

		ldr	r2, [r0], #4		@ MSC2_INIT
		str	r2, [r1, #__OFFSET(MSC2)]
		ldr	r2, [r1, #__OFFSET(MSC2)]

		mov	pc, lr

@-----------------------------------------------------------------------------
@ setup_clocks -- inits the clock registers
@ MODIFIES:	r0,r1
@-----------------------------------------------------------------------------
setup_clocks:
		@ this initializes the clocks to valid values
		ldr	r1,CLOCK_BASE
		ldr	r0,CCCR_INIT
		str	r0,[r1,#__OFFSET(CCCR)]
		ldr	r0,CKEN_INIT
		str	r0,[r1,#__OFFSET(CKEN)]
		ldr	r0,OSCC_INIT
		str	r0,[r1,#__OFFSET(OSCC)]

		mrc	p14, 0, r0, c6, c0, 0
		ldr	r2, [r11, #ARCH_TYPE]
		ldr	r3, mach_h1900
		cmp	r2, r3
		orrne	r0, r0, #3		@ select turbo mode, start FCS
		orreq	r0, r0, #2		@ no turbo mode on h1910
		mcr	p14, 0, r0, c6, c0, 0
		mov	pc, lr

mach_h1900:	.long	MACH_TYPE_H1900

@-----------------------------------------------------------------------------
@ setup_memory -- inits the SDRAM and MSC registers
@ MODIFIES:	r0-r6
@-----------------------------------------------------------------------------
setup_memory:
		@ we are following the sequence in the 
		@ os developers guide pg 13-14

		@ Step 1: wait 200 usec for the internal clocks to stabilize.
		ldr	r0, OSCR_BASE
		mov	r1, #0
		str	r1, [r0, #__OFFSET(OSCR)]
		mov	r3, #0x300 @ 200 usec is x2e1 so this shouldbe fine
1:		ldr	r2, [r0, #__OFFSET(OSCR)]
		cmp	r3, r2
		bgt	1b

		@ Step 2:	Set up the MSC registers
		@ -- The MSC registers have already been set up 
		@    during machine detection and initialization

		@ Step 3: configure the PCMCIA card using mecr,mcmem0,mcatt0,mcatt1,mcio0,mcio133
		@ -- not needed since Linux has its own PCMCIA driver ...

		@ Step 4: configure FLYCNFG
		@ -- a fly by dma register that has apparently disappeared...

		@ Step 5: initial configure of mdrefr k0run,e0pin,k0db2,
		@         kxfree off,apd off, slfrefresh must stay on !
		ldr	r0, MEM_BASE
		ldr	r1, [r11, #ARCH_MDREFR]
		@ If we're running in RAM, don't clear the RUN bits 
		@ and don't set SLFRSH since it will lock up
		tst	r10, #0xff000000
		orreq	r1, r1, #MDREFR_SLFRSH
		biceq	r1, r1, #(MDREFR_K1RUN|MDREFR_K2RUN|MDREFR_E1PIN)
		str	r1, [r0, #__OFFSET(MDREFR)]

		@ Step 6: synchronous static memory -- we dont have any
		mov	r1, #0
		str	r1, [r0, #__OFFSET(SXCNFG)]

		@ Step 7: SDRAM again
		@ MDREFR self refresh turned off
		ldr	r1, [r11, #ARCH_MDREFR]
		str	r1, [r0,#__OFFSET(MDREFR)]
		@ MDCONFIG setup up but not enabled
		ldr	r1, [r11, #ARCH_MDCNFG]
		bic	r1, r1, #(MDCNFG_DE0|MDCNFG_DE1)
		bic	r1, r1, #(MDCNFG_DE2|MDCNFG_DE3)
		str	r1, [r0,#__OFFSET(MDCNFG)]

		@ Step 8: wait for the sdclocks to stabilize (200 usec)
		ldr	r0, OSCR_BASE
		mov	r1, #0
		str	r1, [r0, #__OFFSET(OSCR)]
		mov	r3, #0x300 @ 200 usec is x2e1 so this shouldbe fine
2:		ldr	r2, [r0,#__OFFSET(OSCR)]
		cmp	r3, r2
		bgt	2b

		@ Step 9: trigger refreshes by reading sdram
		mov	r0, #0xa0000000
		mov	r2, #0x800
3:		ldr	r1, [r0]
		subs    r2, r2, #1
		bne	3b

		@ Step 10: enable the sdram in the mdconfig register
		ldr	r0, MEM_BASE
		ldr	r1, [r11, #ARCH_MDCNFG]
		str	r1, [r0,#__OFFSET(MDCNFG)]

		@ Step 11: write out the register mode offset value to the sdram
		mov	r1, #0
		str	r1, [r0,#__OFFSET(MDMRS)]
		mov	pc, lr

@-----------------------------------------------------------------------------
@	Data Area
@-----------------------------------------------------------------------------

@ Machine-specific common data
#if defined CONFIG_ARCH_H5400 || defined CONFIG_ARCH_AXIMX5 || defined CONFIG_ARCH_H2200
pci_id_mq1132:	.long	0x01204D51		@ 'MQ' 1 32
#endif

#if defined CONFIG_ARCH_H1900 || defined CONFIG_MACH_H3900
ASIC3_PART_NUMBER_1:
		.long	0x4854
ASIC3_PART_NUMBER_2:
		.long	0x432d
#endif

kernel_stack:	.word	user_stack + 4096	@ From head.S
kernel_start:	.long	_text

MEM_BASE:	.long	__BASE(MDCNFG)
OSCR_BASE:	.long	__BASE(OSCR)
CLOCK_BASE:	.long	__BASE(CCCR)
GPIO_BASE:	.long	__BASE(GPLR0)
PMCR_BASE:	.long	__BASE(PMCR)
		@ turbo = 2* run = 2* mem = 99.53 Mhz pg 3-33
CCCR_INIT:	.long	((4<<7)|(2<<5)|(1))
CKEN_INIT:	.long	(CKEN16_LCD | CKEN6_FFUART | CKEN5_STUART)
		@ 32 K clock on
OSCC_INIT:	.long	(OSCC_OON)

@-----------------------------------------------------------------------------
@	Architecture-specific routines
@	The machines that are easier detected should be inserted earlier.
@	The detection code should, if possible, touch as little hardware
@	as possible, otherwise on some archs it can lock up before the
@	corresponding detection routine gets a chance to run.
@-----------------------------------------------------------------------------
@ Here's where all arch-specific data begins
Arch_Table:
#ifdef CONFIG_ARCH_LUBBOCK
@ Note: Lubbock just assumes that it is the chosen platform if it is compiled in.
#include "bootldr-lubbock.S"
#endif /* CONFIG_ARCH_LUBBOCK */
#ifdef CONFIG_ARCH_H1900
#include "bootldr-h1900.S"
#endif /* CONFIG_ARCH_H1900 */
#ifdef CONFIG_MACH_H3900 
#include "bootldr-h3900.S"
#endif /* CONFIG_MACH_H3900 */
#ifdef CONFIG_ARCH_H5400
#include "bootldr-h5400.S"
#endif /* CONFIG_ARCH_H5400 */
#ifdef CONFIG_ARCH_H2200
#include "bootldr-h2200.S"
#endif
#ifdef CONFIG_ARCH_AXIMX5
@ NOTE: Axim detection code initializes GPIOs using Axim initialization states,
@ otherwise it cannot detect the MediaQ 1132 chip. Thus, any PDAs that won't
@ like this should be inserted before Axim X5, or should use a LAB that don't
@ support Dell Axim X5.
#include "bootldr-aximx5.S"
#endif /* CONFIG_ARCH_AXIMX5 */
@ And here arch-specific data ends
		.long	0

@ Here are the very-low-level debug output routines
#ifdef CONFIG_LAB_DEBUG
#include "bootldr-debug.S"
#endif

@ Here goes the standard head-xscale.S code
continue_kernel:

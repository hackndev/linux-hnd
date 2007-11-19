/* bootldr-sa1100.S
 * Copyright (c) 1999 Compaq Computer Corporation
 * Copyright (c) 2003 Joshua Wise
 * All rights reserved
 *
 * LAB bootstrap code for SA-1100, adapted from boot-sa1100.s in bootldr.
 */
	
#define ASENTRY(x_) ENTRY(x_)        
#define _C_FUNC(x_) /**/x_/**/
#define _C_LABEL(x_) /**/x_/**/

#include <linux/bootldr/bootconfig.h>
#include <linux/bootldr/boot_flags.h>
#include <linux/bootldr/sa1100.h>
#include <linux/bootldr/arm.h>
#include <asm/arch-sa1100/h3600.h>
#include <asm/arch-sa1100/h3600_gpio.h>
#include <linux/bootldr/h3600_asic.h>

#ifndef _H3800_ASIC1_GPIO_Out
#  define _H3800_ASIC1_GPIO_Out           0x68 // ???
#endif

.globl HandleUndef
HandleUndef:	
        /* save all user registers */
        adr     r13, AbortStorage
        stmia     r13, {r0-r12}
        mov     r12, r14
        bl      EnableUart3Transceiver
	ldr	r0,STR_UNDEF
	ldr     r1,_C_LABEL(SerBase)
	BL	PrintWord
	mov     r0, r12
	ldr     r1,_C_LABEL(SerBase)
	BL	PrintHexWord
	mrc	p15, 0, r0, c6, c0, 0 /* fault address */
	ldr     r1,_C_LABEL(SerBase)
	BL	PrintHexWord
	
	mov     r14, r12
        /* restore registers */
        ldmia     r13, {r0-r12}
        /* now loop */
1:      b 1b
	
.globl HandleSWI
HandleSWI:	
        /* save all user registers */
        adr     r13, AbortStorage
        stmia     r13, {r0-r12}
        mov     r12, r14
        bl      EnableUart3Transceiver
	ldr	r0,STR_SWI
	ldr     r1,_C_LABEL(SerBase)
	BL	PrintWord
	mov     r0, r12
	ldr     r1,_C_LABEL(SerBase)
	BL	PrintHexWord
	mrc	p15, 0, r0, c6, c0, 0 /* fault address */
	ldr     r1,_C_LABEL(SerBase)
	BL	PrintHexWord
	
	mov     r14, r12
        /* restore registers */
        ldmia     r13, {r0-r12}
        /* now loop */
1:      b 1b

.globl HandlePrefetchAbort
HandlePrefetchAbort:	
        /* save all user registers */
        adr     r13, AbortStorage
        stmia     r13, {r0-r12}
        mov     r12, r14
        bl      EnableUart3Transceiver
	ldr	r0,STR_PREFETCH_ABORT
	ldr     r1,_C_LABEL(SerBase)
	BL	PrintWord
	mov     r0, r12
	ldr     r1,_C_LABEL(SerBase)
	BL	PrintHexWord
	mrc	p15, 0, r0, c6, c0, 0 /* fault address */
	ldr     r1,_C_LABEL(SerBase)
	BL	PrintHexWord
	
	mov     r14, r12
        /* restore registers */
        ldmia     r13, {r0-r12}
        /* now loop */
1:      b 1b
	
AbortStorage:
        .word 0
        .word 0
        .word 0
        .word 0
        .word 0
        .word 0
        .word 0
        .word 0
        .word 0
        .word 0
        .word 0
        .word 0
        .word 0
        .word 0
        .word 0
        .word 0

.globl HandleDataAbort
HandleDataAbort:	
        /* save all user registers */
        adr     r13, AbortStorage
        stmia     r13, {r0-r12}
        mov     r12, r14
        bl      EnableUart3Transceiver
	ldr	r0,STR_DATA_ABORT
	ldr     r1,_C_LABEL(SerBase)
	BL	PrintWord
	mov     r0, r12
	ldr     r1,_C_LABEL(SerBase)
	BL	PrintHexWord
	mrc	p15, 0, r0, c6, c0, 0 /* fault address */
	ldr     r1,_C_LABEL(SerBase)
	BL	PrintHexWord
	
	mov     r14, r12
        /* restore registers */
        ldmia     r13, {r0-r12}
        /* now loop */
1:      b 1b

.globl HandleIRQ
HandleIRQ:	
	b       HiReset
                
        /* save all user registers */
        adr     r13, AbortStorage
        stmia     r13, {r0-r12}
        mov     r12, r14
        bl      EnableUart3Transceiver
	ldr	r0,STR_IRQ
	ldr     r1,_C_LABEL(SerBase)
	BL	PrintWord
	mov     r0, r12
	ldr     r1,_C_LABEL(SerBase)
	BL	PrintHexWord
	
	mov     r14, r12
        /* restore registers */
        ldmia     r13, {r0-r12}
        /* now loop */
1:      b 1b

.globl HandleFIQ
HandleFIQ:	
        /* save all user registers */
        adr     r13, AbortStorage
        stmia     r13, {r0-r12}
        mov     r12, r14
        bl      EnableUart3Transceiver
	ldr	r0,STR_FIQ
	ldr     r1,_C_LABEL(SerBase)
	BL	PrintWord
	mov     r0, r12
	ldr     r1,_C_LABEL(SerBase)
	BL	PrintHexWord
	
	mov     r14, r12
        /* restore registers */
        ldmia     r13, {r0-r12}
        /* now loop */
1:      b 1b
	
.globl HandleNotUsed
HandleNotUsed:	
        /* save all user registers */
        adr     r13, AbortStorage
        stmia     r13, {r0-r12}
        mov     r12, r14
        bl      EnableUart3Transceiver
	ldr	r0,STR_NOT_USED
	ldr     r1,_C_LABEL(SerBase)
	BL	PrintWord
	mov     r0, r12
	ldr     r1,_C_LABEL(SerBase)
	BL	PrintHexWord
	
	mov     r14, r12
        /* restore registers */
        ldmia     r13, {r0-r12}
        /* now loop */
1:      b 1b
	
ASENTRY(WaitForInput)
        ldr	r1,SerBase
1:      	
	ldr	r2,[r1,#UTSR1]
	tst	r2,#UTSR1_RNE
        beq     1b     
	mov     pc, lr
	
ConsumeInput:	
	ldr	r1,SerBase
	ldr	r2,[r1,#UTDR]
	ldr	r0,[r1,#UTSR1]
	tst	r0,#UTSR1_RNE
	bne	ConsumeInput
	
HiReset:	
	bl      EnableUart3Transceiver
	ldr	r0,STR_RSET
	ldr     r1,_C_LABEL(SerBase)
	BL	PrintWord

        /* keep track of whether button was pushed in r12 */
	ldr     r12, boot_flags_init_val

#ifdef CONFIG_SA1100_JORNADA56x
	/* Check to see if we are a Jornada 56x (common executable image!) */
	ldr     r1, =JORNADA_GPDPCR_PHYSICAL
	mov     r2, #JORNADA_RS232_ON
	str     r2, [r1, #0]
	ldr     r1, =JORNADA_GPDPDR_PHYSICAL
	ldr     r2, [r1, #0]
	orr     r2, r2, #JORNADA_RS232_ON
	str     r2, [r1, #0]
	ldr     r1, =JORNADA_GPDPLR_PHYSICAL
	ldr     r0, [r1, #0]
        and     r0, r0, #JORNADA_RS232_ON
        teq     r0, #JORNADA_RS232_ON
        beq     1f
	ldr     r1, =JORNADA_GPDPSR_PHYSICAL
	mov     r2, #JORNADA_RS232_ON
	str     r2, [r1, #0]
	ldr     r1, =JORNADA_GPDPLR_PHYSICAL
	ldr     r0, [r1, #0]
        and     r0, r0, #JORNADA_RS232_ON
        teq     r0, #JORNADA_RS232_ON
        bne     1f
	orr	r12,r12,#BF_JORNADA56X
	bic	r12, r12,#BF_SQUELCH_SERIAL
	b	BitsyStart
1:
	/* end of Jornada check */
#endif
	/* btw, bootldr is sp */
		/*	First we need to check for a wince img and jump to it if we find one.
			otherwise we write stuff in ram and bad things happen.
		*/
	/* ok heres the problem. the 38xx ipaqs cant read the action button directly
	   also since we use the power button for suspend it makes a tempermental
	   bootldr forcing switch.
	   so we'll d/dx them early
	   store the orig val of egpio base in r7.
	   first we'll set up our ptr.  its in a sa1100 reg so its safe
	 */
	
	ldr	r4, EGPIOBase
	ldr	r5, [r4]
	mov     r2, #EGPIO_H3600_RS232_ON
        str     r2, [r4, #0]
	/* flush the address bus */
	mov	r0,#FLASH_BASE
	ldr	r1,[r0]
	/* retry the rw reg */
	ldr	r1,[r4]
	cmp	r1,#EGPIO_H3600_RS232_ON
	beq	ima3800
	b	imnota3800
ima3800:
	orr	r12, r12, #BF_H3800
	/* restore the value */
	ldr	r4, EGPIOBase
	str	r5,[r4]	
	add	r4,r4,#_H3800_ASIC2_KPIO_Base
	/* set up the asic2 buttons in alt mode to be read */
	add	r1,r4,#_H3800_ASIC2_KPIO_Alternate
	mov	r0,#0x3e
	str	r0,[r1]
	/* read the action button = kpio5 */
	add	r1,r4,#_H3800_ASIC2_KPIO_Data
	ldr	r0,[r1]
	tst	r0, #(1<<5)	/* action button on 3800, 0 is pressed, 1 is up */	
	beq	BitsyStartForced /* skip if button is down (bit is 0)  */
	b	CheckPartition	
imnota3800:
	bic	r12, r12, #BF_H3800		
	ldr	r1, GPIOBase
	ldr	r0, [r1, #GPIO_GPLR_OFF]
	tst	r0, #(1<<18)	/* check the Action button down bit */
	beq	BitsyStartForced	/* skip if button is down (bit is 0)  */
	b	CheckPartition	

CheckPartition:		
        @ check for the wince cookie, if not present, go to BitsyStart
	ldr     r1, PSPR_ADDRESS
        ldr     r1, [r1]
        ldr     r2, PSPR_WINCE_COOKIE
        cmp     r1, r2
	bne     BitsyStart

	@ boot wince
	mov	r0,#FLASH_BASE
	add	r0,r0,#0x40000
/* look for wince image	 */
	ldr	r1,[r0],#4
	ldr	r2,WINCE_MAGIC_1
	cmp	r1,r2
	bne	BitsyStart
/*	ok, some wince images have 0's where they are erased and
	others have F's */
	ldr	r3,WINCE_MAGIC_LONG_0
wince_loop:	
	ldr	r1,[r0],#4
	cmp	r1,#0
	bne	10f
	b	20f
10:	
	cmp	r1,#0xFFFFFFFF
	bne	BitsyStart
20:	
	subs	r3,r3,#1
	bne	wince_loop	

	ldr	r1,[r0],#4
	ldr	r2,WINCE_MAGIC_2
	cmp	r1,r2
	bne	BitsyStart

	b	WakeUpWince	


BitsyStartForced:
	/* we got here cuz they held the action button dowm,
	   therefore no squelchies */
	bic	r12, r12, #BF_SQUELCH_SERIAL
	bic     r12, r12, #BF_NORMAL_BOOT
BitsyStart:	
	ldr     r0, RTC_BASE_ADDRESS
	ldr     r1, [r0, #RTTR_OFFSET]
        cmp     r1, #0
        bne     1f
        mov     r1, #32768
        str     r1, [r0, #RTTR_OFFSET]
        ldr     r1, [r0, #RCNR_OFFSET]
        ldr     r2, RTC_RUNAHEAD
        sub     r1, r1, r2
        str     r1, [r0, #RCNR_OFFSET]
1:
ENTRY(SleepReset_Resume)
	
	/* disable all interrupts */
	ldr     r1, ICMR_ADDRESS
	mov     r0, #0
        str     r0, [r1, #0]
	
#if defined(CONFIG_SA1100_H3XXX) || defined(CONFIG_SA1100_ASSABET) || defined(CONFIG_ASSABET_NEPONSET) || defined(CONFIG_SA1100_JORNADA720) || defined(CONFIG_SA1100_JORNADA56X)
        /*;initialize the static memory timing */ 
	mov	r1, #DRAM_CONFIGURATION_BASE
#if 1
	ldr     r2, msc2_config
        str     r2, [r1, #MSC2]
	ldr     r2, msc1_config
        str     r2, [r1, #MSC1]
#endif
	/*
	 * We have two kinds of flash on ipaq.  One is 16bit wide, the
	 * other is 32.
	 * Since the ARM must be strapped to the correct boot memory width
	 * to boot correctly, and the state of the strap is reflected in
	 * config register, we'll leave that bit unchanged.
	 */
	ldr     r2, msc0_config
	tst	r12, #BF_JORNADA56X
	beq	92f
	ldr     r2, msc0_config_jornada56x
92:
	ldr	r3, [r1, #MSC0]
	/* save the current bit value */
	and	r3, r3, #MSC_RBW16
	/* make sure the bit is cleared in the new config word */
	bic	r2, r2, #MSC_RBW16
	/* propagate the bit into the new config value */
	orr	r2, r3, r2
        str     r2, [r1, #MSC0]
	/*; disable synchronous rom because we have none */ 
	mov     r2, #0
        str     r2, [r1, #SMCNFG]
        /*; configutre the expansion (PCMCIA) memory */	
	ldr     r2, mecr_config
	tst	r12, #BF_JORNADA56X
	beq	92f
	ldr     r2, mecr_config_jornada56x
92:
        str     r2, [r1, #MECR]
#endif	

#ifdef MODIFY_CPSR	
	# put us in the right frame of mind ...
	msr	cpsr_c, #(F_BIT | I_BIT | SVC_MODE)
#endif	
	/* set clock speed to 191.7MHz */
	ldr	r1, PPCR_ADDRESS	
	mov	r0, #PPCR_206MHZ
	str     r0, [r1, #0]

#if	0
	/* 
	 * look at the power button to see if serial squelch toggling 
	 * is requested 
	 */

	mov	r3, #0x1
button_loop:
	ldr	r1, GPIOBase
	ldr	r0, [r1, #GPIO_GPLR_OFF]
	tst	r0, #(1<<18)	/* check the Action button down bit */
#if 0
	tst	r0, #(1<<0)	/* check the power button down bit */
#endif
	beq	squelch_toggle	/* skip if button is down (bit is 0)  */
	
	subs	r3, r3, #1
	bne	button_loop
	b	squelch_as_is

squelch_toggle:
squelch_spin:		
	/* loop till the button is released. */
	ldr	r0, [r1, #GPIO_GPLR_OFF]
	tst	r0, #(1<<18)	/* check the Action button down bit */
#if 0
	//tst	r0, #(1<<0)	/* check the power button down bit */
#endif
	beq	squelch_spin
	
	bic	r12, r12, #BF_SQUELCH_SERIAL
	bic	r12, r12, #BF_NORMAL_BOOT
	
squelch_as_is:
#endif

        ldr     r1, RCSR_ADDRESS
        ldr     r0, [r1, #0]
        and     r0, r0, #(RCSR_HWR + RCSR_SWR + RCSR_WDR + RCSR_SMR)
        teq     r0, #RCSR_SMR
        beq     WakeupStart

	/*; Initialize the UARTs */
	bl	InitUART1
	bl	InitUART3
	bl      EnableUart3Transceiver
	b	55f
skip_sleep_reset_check:
	/*; Initialize the UARTs */
	bl	InitUART1
	bl	InitUART3
	bl      EnableUart3Transceiver
	ldr	r0,STR_WAKEUP_SKIP
	ldr     r1,_C_LABEL(SerBase)
	BL	PrintWord
55:		
	mov     r0, #'\r'
	ldr     r1, _C_LABEL(SerBase)
	bl      _C_FUNC(PrintChar)
	mov     r0, #'\n'
	ldr     r1, _C_LABEL(SerBase)
	bl      _C_FUNC(PrintChar)
	mov     r0, #'@'
	ldr     r1, _C_LABEL(SerBase)
	bl      _C_FUNC(PrintChar)
	
	mov     r0, pc
	ldr     r1, _C_LABEL(SerBase)
        bl      PrintHexWord

/* check to see if we're operating out of DRAM */
        bic     r4, pc, #0x000000FF
        bic     r4, r4, #0x0000FF00
        bic     r4, r4, #0x00FF0000
	
        cmp     r4, #DRAM_BASE0
	
	bne     RunningInFlash
	
RunningInDRAM:  
	mov     r0, #'D'
	ldr     r1, _C_LABEL(SerBase)
	bl      _C_FUNC(PrintChar)
#if 0
	b       22f
#else
        b       21f	
#endif	
	
RunningInFlash: 	
	mov     r0, #'F'
	ldr     r1, _C_LABEL(SerBase)
	bl      _C_FUNC(PrintChar)
	       
21:     	
/* otherwise, assume we're running in Flash and do a complete reset */

/*; Initialize SDRAM and Flash*/
	mov	r0, #1		/* tell routine to init reggies */
	bl	InitMem

	mov     r0, #'+'
	ldr     r1, _C_LABEL(SerBase)
	bl      _C_FUNC(PrintChar)

	/* r0 contains DRAM size */
	ldr     r1, _C_LABEL(SerBase)
	bl      PrintHexWord

/*; debug print*/
	mov	r0,#'\r'
	ldr     r1, _C_LABEL(SerBase)
	bl      _C_FUNC(PrintChar)
	mov	r0,#'\n'
	ldr     r1, _C_LABEL(SerBase)
	bl      _C_FUNC(PrintChar)
	mov	r0,#'*'
	ldr     r1, _C_LABEL(SerBase)
	bl      _C_FUNC(PrintChar)
	
	ldr     r0, STR_MTST
	ldr     r1, _C_LABEL(SerBase)
	bl      PrintWord
	
TestDramBank0:       	
	/* some test code */
	mov	r5, #DRAM_BASE0
	mov	r6, #0x00000001
TestDramBank0Loop:
	mov     r0, r6
	str	r0, [r5, #48]
	ldr	r0, [r5, #48]
	ldr     r1, _C_LABEL(SerBase)
	bl      PrintHexWord
	tst     r6, #0x80000000
	bne     TestDramBank0Done
	mov     r6, r6, lsl #1
        b       TestDramBank0Loop
TestDramBank0Done:
        
#ifdef CONFIG_DRAM_BANK1_TEST	
	ldr	r0, STR_MB2
	ldr     r1, _C_LABEL(SerBase)
	bl      PrintWord
	
	mov	r5, #DRAM_BASE1
	mov	r6, #0x00000001
TestDramBank1Loop:
#ifndef BOOT_SILENT
	mov     r0, r6
	ldr     r1, _C_LABEL(SerBase)
	bl      PrintHexWord
#endif
	mov     r0, r6
	str	r0, [r5, #48]
	ldr	r0, [r5, #48]
	ldr     r1, _C_LABEL(SerBase)
	bl      PrintHexWord
	tst     r6, #0x80000000
	bne     TestDramBank1Done
	mov     r6, r6, lsl #1
        b       TestDramBank1Loop
TestDramBank1Done:
#endif /* CONFIG_DRAM_BANK1_TEST */

/*; debug print*/
DebugPrintAfterTestDram:        	
	ldr	r0, STR_ENDM
	ldr     r1, _C_LABEL(SerBase)
	bl      PrintWord

#ifdef NOTDEF
memTest:                        

        /* Initial - fill memory */
        mov     r0,#DRAM_BASE0
        add     r1,r0,#DRAM_SIZE0
	ldr     r2,CRC32POLY
	mov     r3,#-1
        mov     r4,#1

10:     orrs    r3,r4,r3,lsl#1
        eorcs   r3,r3,r2
        str     r3,[r0], #4
        mov     r6,r0
	mov     r7,r1
	mov     r8,r2
	mov     r9,r3
	mov     r10,r4
	bl      PrintHexWord
	mov     r0,r6
	mov     r1,r7
	mov     r2,r8
	mov     r3,r9
	mov     r4,r10
        cmp     r0,r1
        bne     10b

        /* Phase two, check */
        mov     r0,#DRAM_BASE0
        mov     r3,#-1

20:     orrs    r3,r4,r3,lsl #1
        eorcs   r3,r3,r2
        ldr     r5,[r0],#4
        cmp     r5,r3
        bne     memError
        mov     r6,r0
	mov     r7,r1
	mov     r8,r2
	mov     r9,r3
	mov     r10,r4
	bl      PrintHexWord
	mov     r0,r6
	mov     r1,r7
	mov     r2,r8
	mov     r3,r9
	mov     r4,r10
        cmp     r0,r1
        bne     20b
	b	memNoError
	
memError:	
	ldr	r0, STR_MEM_ERROR
	ldr     r1, _C_LABEL(SerBase)
	bl      PrintWord
memError1:	
	b	memError1
	
memNoError:     	
#endif	
	
22:             	
EnableCaches:	
/* execution also continues here from the RunningFromDRAM path */


	/*; Get ready to call C functions*/
	ldr	r0, STR_STACK
	bl	PrintWord
	ldr	r0, DW_STACK_START
	bl	PrintHexWord
	
call_main:      

        /* pass boot flags as second arg */
	orr     a2, r12, r12
                        
	ldr	sp,DW_STACK_START
	mov	fp,#0		/* no previous frame, so fp=0*/

	/*
	 * check to see if this is a sleep reset.
	 * if so, then we want to enter the bootmenu directly.
	 * otherwise, we want to proceed as before, allowing 
	 * a key or timeout to control the boot action
	 */	 		
	tst	a2, #BF_SLEEP_RESET
	movne	a1, #1
	moveq	a1, #0

	// Kernel init cruft
	mov	r7, #MACH_TYPE_H3600
	mov	r8, #0
	
	bl	bootldr_main	/* call the main C entry point */

	mov	pc, #FLASH_BASE	/* reboot */

WakeupStart:

	ldr	r0,STR_WAKEUP
	ldr     r1,_C_LABEL(SerBase)
	BL	PrintWord

	
        ldr     r1, PSSR_ADDRESS
        mov     r0, #PSSR_PH
        str     r0, [r1, #0]

        ldr     r1, PSSR_ADDRESS
        mov     r0, #PSSR_DH
        str     r0, [r1, #0]    /* Clearing DRAM hold */

	mov	r1, #DRAM_CONFIGURATION_BASE
	ldr	r0, [r1, #MDREFR]

	/*
	 *mask out all of the bits that are undefined after a reset
	 */
	ldr	r2, mdrefr_valid_bit_mask_after_reset
	and	r0, r0, r2
        
	/* Disable auto power down */
        bic     r0, r0, #MDREFR_EAPD | MDREFR_KAPD
        
	/* Set KnRUN (from Self-refresh and Clock-stop to Self-refresh) */
        orr     r0, r0, #MDREFR_K1RUN
        str     r0, [r1, #MDREFR]

        /* Clear SLFRSH (from Self-refresh to Power-down) */
        bic     r0, r0, #MDREFR_SLFRSH
        str     r0, [r1, #MDREFR]

        /* Set E1PIN (from Power-down to PWRDNX) */
        orr     r0, r0, #MDREFR_E1PIN
        str     r0, [r1, #MDREFR]

#define DEBUG_WAKEUP_SETUP_MDREFR
#ifdef DEBUG_WAKEUP_SETUP_MDREFR
	bl	InitUART3
	mov	r1, #DRAM_CONFIGURATION_BASE
	ldr	r0, [r1, #MDREFR]
        ldr	r1, _C_LABEL(SerBase)
	bl	PrintHexWord
	mov	r1, #DRAM_CONFIGURATION_BASE
#endif /* DEBUG_WAKEUP_SETUP_MDREFR */	
#define CONFIG_WAKEUP_SETUP_MDREFR
#ifdef CONFIG_WAKEUP_SETUP_MDREFR
        ldr     r0, dram_cas0_waveform0
	tst	r12, #BF_JORNADA56X
	beq	92f
        ldr     r0, dram_cas0_waveform0_jornada56x
92:
        str     r0, [r1, #MDCAS00]
        ldr     r0, dram_cas0_waveform1
        str     r0, [r1, #MDCAS01]

        ldr     r0, dram_cas0_waveform2
        str     r0, [r1, #MDCAS02]

        ldr     r0, dram_mdrefr
	tst	r12, #BF_JORNADA56X
	beq	92f
        ldr     r0, dram_mdrefr_jornada56x
92:
        str     r0, [r1, #MDREFR]
#endif /* CONFIG_WAKEUP_SETUP_MDREFR */
	/* 
	 * init the memory controller mode config register
	 */
#ifdef CONFIG_WAKEUP_SETUP_MDREFR
	mov	r0, #0
	bl	InitMem
#endif /* CONFIG_WAKEUP_SETUP_MDREFR */
	

        ldr     r0, [r1, #MDCNFG]
	bic	r0, r0, #MDCNFG_BANK0_ENABLE
	bic	r0, r0, #MDCNFG_BANK1_ENABLE
        str     r0, [r1, #MDCNFG]

        mov     r1, #0xC0000000
        ldr     r0, [r1], #4
        ldr     r0, [r1], #4
        ldr     r0, [r1], #4
        ldr     r0, [r1], #4
        ldr     r0, [r1], #4
        ldr     r0, [r1], #4
        ldr     r0, [r1], #4
        ldr     r0, [r1], #4

#ifdef CONFIG_DRAM_BANK1	
	/* now for bank1 */
        mov     r1, #0xC8000000 
        ldr     r0, [r1], #4
        ldr     r0, [r1], #4
        ldr     r0, [r1], #4
        ldr     r0, [r1], #4
        ldr     r0, [r1], #4
        ldr     r0, [r1], #4
        ldr     r0, [r1], #4
        ldr     r0, [r1], #4
#endif
	
        mov     r1, #DRAM_CONFIGURATION_BASE
        ldr     r0, [r1, #MDCNFG]
	orr	r0, r0, #MDCNFG_BANK0_ENABLE
#ifdef CONFIG_DRAM_BANK1	
	orr	r0, r0, #MDCNFG_BANK1_ENABLE
#endif
        str     r0, [r1, #MDCNFG]

	@ Enable SDRAM auto power down
        ldr     r0, dram_mdrefr
	tst	r12, #BF_JORNADA56X
	beq	92f
        ldr     r0, dram_mdrefr_jornada56x
92:
	orr	r0, r0, #(MDREFR_EAPD + MDREFR_KAPD)
        str     r0, [r1, #MDREFR]
	/* are using the cp15 register one bit 13 as the
	   indication of whether we slept from wince or not.
	   wince sets this bit high (remap interrupt vectors to 0xffff0000)
	   and linux and the bootloader do not.
	  UNFORTUNATELY, wince unsets this bit before sleeping so instead we 
	  use a ram signature to guesstimate the OS origin
	
	*/
	
	mov     r1, #DRAM_BASE0
	add	r1,r1,#SZ_4K
	ldr	r0, [r1],#4
	cmp	r0,#0
	bne	WakeUpLinux


	ldr	r0, [r1],#4
	cmp	r0,#0
	bne	WakeUpLinux
	ldr	r0, [r1],#4
	cmp	r0,#0
	bne	WakeUpLinux


#if defined(CONFIG_SA1100_H3XXX) || defined(CONFIG_SA1100_JORNADA56X)
WakeUpWince:
#if 0
	@ leave a cookie so we know which way to go if we are dual booting and resuming
	ldr     r1, PSPR_ADDRESS
	ldr     r2, PSPR_WINCE_COOKIE
        str     r2, [r1]                
#endif
	@ now jump to wince
	ldr	r1,wince_jp	
	nop
	nop
	nop
	nop
	mov	pc,r1
#endif /* IPAQ */
WakeUpLinux:

#define DEBUGGING_RESUME_HANG	
#ifdef 	DEBUGGING_RESUME_HANG	
#warning DEBUGGING_RESUME_HANG
	bic	r12, r12, #BF_SQUELCH_SERIAL
	bl	InitUART3
	bl      EnableUart3Transceiver
	mov     r0, #'W'
	ldr     r1, _C_LABEL(SerBase)
	bl      _C_FUNC(PrintChar)
	mov     r0, #'A'
	ldr     r1, _C_LABEL(SerBase)
	bl      _C_FUNC(PrintChar)
	mov     r0, #'K'
	ldr     r1, _C_LABEL(SerBase)
	bl      _C_FUNC(PrintChar)
	mov     r0, #'E'
	ldr     r1, _C_LABEL(SerBase)
	bl      _C_FUNC(PrintChar)
#endif	
	
	# clear sleep reset bit
        ldr     r1, RCSR_ADDRESS
        mov     r0, #RCSR_SMR
	str	r0, [r1]
	# save sleep reset flag in our flags reggie
	orr	r12, r12, #BF_SLEEP_RESET

	ldr     r7, PSPR_ADDRESS
        ldr     r6, [r7, #0]
	mov     pc, r6
	
        /**************************************************************/	
        /*       ResetWhileRunningInDRAM                              */
        /*          Bootldr is running from DRAM                      */
	/*          MMU may be enabled                                */
	/*          Disable MMU, then do standard configuration       */
        /**************************************************************/	
ResetWhileRunningInDRAM:
	mov     r11, lr
	mov	r0,#'^'
	ldr     r1, _C_LABEL(SerBase)
	bl      _C_FUNC(PrintChar)
	
        b       EnableCaches

/*      	
 * Enable Ser3 Transceiver
 *   r0: number of times to wiggle the enable bit
 *   modifies r0, r1, r2	
 */
EnableUart3Transceiver: 
#if defined(CONFIG_SA1100_JORNADA56X) || defined(CONFIG_SA1100_H3XXX)
	tst	r12, #BF_JORNADA56X
	beq	2f
	ldr     r1, GPIOBase
	mov     r2, #0x00100000
	str     r2, [r1, #GPIO_GPSR_OFF]
	ldr     r2, =0x0f424000
	str     r2, [r1, #GPIO_GPCR_OFF]
	ldr     r2, =0x080803fc
	str     r2, [r1, #GPIO_GAFR_OFF]
	ldr     r2, =0x0f5243fc
	str     r2, [r1, #GPIO_GPDR_OFF]
	mov     r0, #JORNADA_ASIC_RESET
	str	r0, [r1, #GPIO_GPSR_OFF]
	ldr     r1, =JORNADA_GPDPSR_PHYSICAL
	mov     r2, #JORNADA_RS232_ON
	str     r2, [r1, #0]
	ldr     r1, =JORNADA_GPDPDR_PHYSICAL
	ldr     r2, [r1, #0]
	orr     r2, r2, #JORNADA_RS232_ON
	str     r2, [r1, #0]
        mov     pc, lr	
2:
#endif
#ifdef CONFIG_SA1100_H3XXX
	tst	r12, #BF_H3800
	beq	3f
	ldr	r1, H3800_ASIC1Base
	ldr	r2, [r1, #_H3800_ASIC1_GPIO_Out]
	orr	r2, r2, #GPIO1_RS232_ON
	str	r2, [r1, #_H3800_ASIC1_GPIO_Out]
	mov	pc, lr
3:		
        ldr     r1, EGPIOBase
	mov     r2, #EGPIO_H3600_RS232_ON
        str     r2, [r1, #0]
        ldr     r2, [r1, #0]
#endif
#ifdef CONFIG_SA1100_JORNADA720
#define GPIO_GPIO8	(1<<8)
	ldr     r1, GPIOBase
	mov     r2, #GPIO_GPIO8
	str     r2, [r1, #GPIO_GPSR_OFF]
	ldr     r2, [r1, #GPIO_GPDR_OFF]
	orr     r2, r2, #GPIO_GPIO8
	str     r2, [r1, #GPIO_GPDR_OFF]
#endif
        mov     pc, lr	
	/*
	;; ********************************************************************
	;; InitUART1 - Initialize Serial Communications
	;; ********************************************************************
	;; Following reset, the UART is disabled. So, we do the following:
	*/
InitUART1:
#if defined(CONFIG_SA1100_ASSABET) || defined(CONFIG_ASSABET_NEPONSET)
	ldr	r1,BCRBase
	mov	r2, #BCR_RS232EN
	str	r2, [r1, #0]
#endif
	/* Now clear all 'sticky' bits in serial I registers, cf. [1] 11.11 */
	ldr	r1, SDLCBase
	mov	r2, #0x01
	str	r2, [r1]
        ldr	r1, _C_LABEL(Ser1Base)
	mov	r2, #0xFF
	str	r2, [r1, #UTSR0]         /* UART1 Status Reg. 0        */
	
        /* disable the UART */
	mov	r2, #0x00
	str	r2, [r1, #UTCR3]         /* UART1 Control Reg. 3        */
	/* Set the serial port to sensible defaults: no break, no interrupts, */
	/* no parity, 8 databits, 1 stopbit. */
	mov	r2, #UTCR0_8BIT
	str	r2, [r1, #UTCR0]         /* UART1 Control Reg. 0        */

#define UART_BRD ((3686400 / 16 / UART_BAUD_RATE) - 1)
	mov	r2, #((UART_BRD >> 16) & 0xF)
	str	r2, [r1, #UTCR1]
	mov	r2, #(UART_BRD & 0xFF)
	str	r2, [r1, #UTCR2]
        /* enable the UART TX and RX */
InitUart1Enable:

	mov	r2, #(UTCR3_RXE|UTCR3_TXE)
	str	r2, [r1, #UTCR3]

	tst	r12, #BF_SQUELCH_SERIAL
	bne	10f
	
        /* transmit a character or two */
	mov	r2, #'U'
	str	r2, [r1, #UTDR]
	mov	r2, #'1'
	str	r2, [r1, #UTDR]
10:	
	mov	pc, lr		/* All done, return*/

	/*
	;; ********************************************************************
	;; InitUART3 - Initialize Serial Communications
	;; ********************************************************************
	;; Following reset, the UART is disabled. So, we do the following:
	*/
InitUART3:
        ldr	r1, _C_LABEL(Ser3Base)
        /* disable the UART */
	mov	r2, #0x00
	str	r2, [r1, #UTCR3]         /* UART1 Control Reg. 3        */
	/* Now clear all 'sticky' bits in serial I registers, cf. [1] 11.11 */
	mov	r2, #0xFF
	str	r2, [r1, #UTSR0]         /* UART1 Status Reg. 0        */
	
	/* Set the serial port to sensible defaults: no break, no interrupts, */
	/* no parity, 8 databits, 1 stopbit. */
	mov	r2, #UTCR0_8BIT
	str	r2, [r1, #UTCR0]         /* UART1 Control Reg. 0        */

	/* Set BRD to 1, for a baudrate of 115K2 ([1] 11.11.4.1) */
	/* Set BRD to 3, for a baudrate of 57k6 ([1] 11.11.4.1) */
	/* Set BRD to 5, for a baudrate of 38k4 ([1] 11.11.4.1) */
	/* Set BRD to 23, for a baudrate of 9k6 ([1] 11.11.4.1) */
	mov	r2, #0x00
	str	r2, [r1, #UTCR1]
	mov	r2, #1
	str	r2, [r1, #UTCR2]
        /* enable the UART TX and RX */
InitUart3Enable:
	
	mov	r2, #(UTCR3_RXE|UTCR3_TXE)
	str	r2, [r1, #UTCR3]

	tst	r12, #BF_SQUELCH_SERIAL
	bne	10f

        /* transmit a character or two */
        /* transmit J and uart number for JORNADA56X */
	tst	r12, #BF_JORNADA56X
	beq	2f
	mov	r2, #'J'
	str	r2, [r1, #UTDR]
	b	2f
2:
        /* transmit U and uart number for iPAQs */
	mov	r2, #'U'
	str	r2, [r1, #UTDR]
	mov	r2, #'3'
	str	r2, [r1, #UTDR]
10:		
#ifdef CONFIG_SA1100_H3XXX
        /* set forceon bit, which is in EGPIO U7 */	
	ldr     r1, EGPIOBase
        mov     r2, #EGPIO_H3600_RS232_ON
	str	r2, [r1, #0]
	ldr	r2, [r1, #0]
#endif

	mov	pc, lr		/* All done, return*/

	/*; *****************************************************************/
	/*; InitMem - initialize SDRAM*/
	/*; *****************************************************************/
ASENTRY(InitMem)

	cmp	r0, #0
	beq	20f

	mov 	r11, lr
	
	/* Set up the DRAM in banks 0 and 1 [1] 10.3 */
	mov	r1, #DRAM_CONFIGURATION_BASE
        ldr     r2, dram_cas0_waveform0
	str	r2, [r1, #MDCAS00]

	tst	r12, #BF_JORNADA56X
	beq	92f
        ldr     r2, dram_cas0_waveform0_jornada56x
92:
	str	r2, [r1, #MDCAS00]

        ldr     r2, dram_cas0_waveform1
	str	r2, [r1, #MDCAS01]	

        ldr     r2, dram_cas0_waveform2
	str	r2, [r1, #MDCAS02]	
	
	ldr     r2, dram_mdrefr
	tst	r12, #BF_JORNADA56X
	beq	92f
	ldr     r2, dram_mdrefr_jornada56x
92:
	str	r2, [r1, #MDREFR]


20:
memSizeTry64MB:
	mov     r0, #'6'
	ldr     r1, _C_LABEL(SerBase)
	bl      _C_FUNC(PrintChar)

	tst	r12, #BF_JORNADA56X
	beq	2f
	/* test GPIO for memory size */
	b memSizeTry32MB
2:
	/*
	 * davep: I moved the accesses to before the
	 * enable as per 10.7.1 #6
	 * i.e. access the memory while configured but
	 * disabled
	 */

	/* configure the memory */
	/*
	 * we need to keep DRAM enbabled during RAM testing
	 * preserve current enable bit 
	 */
	mov	r1, #DRAM_CONFIGURATION_BASE
	ldr	r4, [r1, #MDCNFG]
	ldr     r3, dram_mdcnfg_enable
	and	r3, r4, r3
	ldr	r4, dram_mdcnfg_64mb
	tst	r12, #BF_JORNADA56X
	beq	92f
	ldr	r4, dram_mdcnfg_64mb_jornada56x
92:
	orr	r4, r4, r3
	str	r4, [r1, #MDCNFG]

	/* wake up the memory... */
	mov     r2, #DRAM_BASE0
	
	ldr     r3, [r2, #0]	/* read at 0xc0000000 */
	ldr     r3, [r2, #0]	/* read at 0xc0000000 */
	ldr     r3, [r2, #0]	/* read at 0xc0000000 */
	ldr     r3, [r2, #0]	/* read at 0xc0000000 */
	ldr     r3, [r2, #0]	/* read at 0xc0000000 */
	ldr     r3, [r2, #0]	/* read at 0xc0000000 */
	ldr     r3, [r2, #0]	/* read at 0xc0000000 */
	ldr     r3, [r2, #0]	/* read at 0xc0000000 */

	mov     r0, #'W'
	ldr     r1, _C_LABEL(SerBase)
	bl      _C_FUNC(PrintChar)

	/* now, enable it. */
	ldr	r3, dram_mdcnfg_64mb
	tst	r12, #BF_JORNADA56X
	beq	92f
	ldr	r3, dram_mdcnfg_64mb_jornada56x
92:
	ldr     r4, dram_mdcnfg_enable
	orr	r4, r4, r3
	str	r4, [r1, #MDCNFG]
	nop
	nop

	/* set 64/128M dram refresh value. */
	ldr     r3, dram_mdrefr64Mplus
	tst	r12, #BF_JORNADA56X
	beq	92f
	ldr     r3, dram_mdrefr64Mplus_jornada56x
92:
	str	r3, [r1, #MDREFR]
	
	/*
	 * save that which we will overwrite
	 * save value at offset 0
	 * Offset 0 will not move around like other
	 *  addresses like 16M, etc. will when we
	 *  reconfigure the memory size.
	 *  So we save it here, once.
	 */
	mov     r2, #DRAM_BASE0
	mov	r4, r2
	ldr	r5, [r4]

	/*
	 * check for 64M of memory.
	 * Write SZ_64M to offset 0   in DRAM_BANK0.
	 * Write SZ_16M to offset 16M in DRAM_BANK0.
	 * If we have < 64M of memory, then the write
	 *  to 16M will wrap and overwrite address 0.
	 * If we still see SZ_64M at 0, then there
	 *  was no wrap and we have 64M.
	 */

	/* create paddr of 16M into DRAM bank */
	mov     r1, #SZ_16M
	orr     r0, r2, r1

	/* 
	 * save addr and data being overwritten
	 * at offset 16M
	 * NOTE: if this machine is <64M, then
	 *  we are actually saving the value at
	 *  offset 0 due to wrap.
	 *  so we must save this value before we
	 *  write to 0 or we're hosed.
	 * 
	 */
	mov	r6, r0
	ldr	r7, [r6]
	
	mov     r1, #SZ_64M
        str     r1, [r2, #0]	/* 64MB signature at 0xc0000000 */

	mov     r1, #SZ_16M
        str     r1, [r0, #0]	/* 16MB signature at 0xc1000000 */
				/* (or 0xc00000000 if wrapping) */
	
	ldr     r1, [r2, #0]    /* look to see if 0xc0000000 changed... */
	mov     r3, #SZ_64M     /* ...implying 32MB or less */
        cmp     r1, r3

	/*
	 * a match means 64M at least.  We're done and
	 * we can restore values to offset 0 and 16M
	 */
        beq     memSizeDetected64M

	/*
	 * we do not have 64M of DRAM.
	 * We are about to change the DRAM config for
	 * 32M (and 16M).
	 * First, lets restore the values we saved from
	 * before.
	 * This way, we don't have to worry if 16M wraps
	 * to the same place on a 64M machine as it does
	 *  on other configurations.
	 * We also restore the value to offset 0 so
	 *  that if the next case wraps at 16M then
	 *  we'll save the correct value when we
	 *  read offset 16M
	 */
	str	r7, [r6]	/* restore offset 16M */

	/* restore 0 *after* 16M in case of wrap */
	str	r5, [r4]	/* restore offset 0 */

	/*
	 * Now we reconfigure the DRAM to 32M.
	 * The probing process is similar to 64M
	 * We write SZ_32M to offset 0 and
	 *  SZ_16M to offset 16M
	 * We then see if the write to 16M clobbers
	 *  the value at zero.
	 * If not clobbered, we have 32M.
	 * If clobbered, <32M and the only thing we 
	 * sell like that is 16M.
	 * 
	 */
        
memSizeTry32MB: 	
	mov     r0, #'3'
	ldr     r1, _C_LABEL(SerBase)
	bl      _C_FUNC(PrintChar)
	mov	r1, #DRAM_CONFIGURATION_BASE

	/* set non 64/128M dram refresh value. */
	ldr     r3, dram_mdrefr
	tst	r12, #BF_JORNADA56X
	beq	92f
	ldr     r3, dram_mdrefr_jornada56x
92:
	str	r3, [r1, #MDREFR]

	
	ldr     r2, dram_mdcnfg_32mb
	tst	r12, #BF_JORNADA56X
	beq	92f
	ldr     r2, dram_mdcnfg_32mb_jornada56x
92:
        orr     r2, r2, #MDCNFG_BANK0_ENABLE
#ifdef CONFIG_DRAM_BANK1	
        orr     r2, r2, #MDCNFG_BANK1_ENABLE
#endif
	str	r2, [r1, #MDCNFG]
	nop
	nop

	/* paddr of offset 0 */
	mov     r2, #DRAM_BASE0

	/* paddr of offset 16M */
        mov     r1, #SZ_16M
	orr     r0, r2, r1
	
	/* 
	 * save addr and data being overwritten
	 * at offset 16M
	 * as in 64M case, save this value before
	 *  we write to zero in the case of a
	 *  wrap.
	 */
	mov	r6, r0
	ldr	r7, [r6]
	
        ldr	r1, =SZ_32M
        str     r1, [r2, #0]		/* put SZ_32M at 0 */

        ldr	r1, =SZ_16M
        str     r1, [r0, #0]		/* put SZ_16M at 16M (or 0) */
	
        ldr     r1, [r2, #0]		/* see what is at 0 */
	
        mov     r3, #SZ_32M		
        cmp     r1, r3			/* is it 32M ??? */
        beq     memSizeDetected32M

	/*
	 * Q for jamey:	why return beef for 16M machines?
	 * Nothing really uses the return value, but still...
	 */
#if 0
        mov     r1, #SZ_16M
#else	
	orr     r1, r1, #0x000000ef
	orr     r1, r1, #0x0000be00
#endif	

/* 16MB uses same memory controller configuration as 32MB (same number of row bits, fewer column bits */
 
memSizeDetected64M:	
memSizeDetected32M:
	mov     r0, #'D'
	ldr     r1, _C_LABEL(SerBase)
	bl      _C_FUNC(PrintChar)

#if 1
	str	r7, [r6]
#else
	/* test code */
	mov	r7, #0x03
	str	r7, [r6, #4]
	ldr	r7, [r6]
	str	r7, [r6, #8]
#endif	

	
#if 1
	str	r5, [r4]
#else	
	/* test code */
	mov	r5, #0x03
	str	r5, [r4, #4]
	ldr	r5, [r4]
	str	r5, [r4, #8]
#endif
	
	mov     r0, r1			/* move size to r0 */

	mov     r0, #'R'
	ldr     r1, _C_LABEL(SerBase)
	bl      _C_FUNC(PrintChar)

        mov	pc, r11			/* All done, return*/

	/*
	;; ********************************************************************
	;; PrintHexNibble -- prints the least-significant nibble in R0 as a
	;; hex digit
        ;;   r0 contains nibble to write as Hex
        ;;   r1 contains base of serial port
        ;;   writes r0 with RXSTAT, modifies r0,r2 
	;;   Falls through to PrintChar
	;; ********************************************************************
	*/
PrintHexNibble:	
	adr	r2, _C_LABEL(HEX_TO_ASCII_TABLE)
	and	r0, r0, #0xF
	ldr	r0, [r2,r0]	/* convert to ascii */
	b       _C_FUNC(PrintChar)

	/*
	;; ********************************************************************
	;; PrintChar -- prints the character in R0
        ;;   r0 contains the character
        ;;   r1 contains base of serial port
        ;;   writes r0 with UTSR1, modifies r0,r1,r2
	;;********************************************************************
	*/

PrintChar:      
	# see if printing is disabled
	tst	r12, #BF_SQUELCH_SERIAL
	movne	pc, lr		/* return if squelched */

TXFull:	
        ldr     r2,[r1,#UTSR1]/* check status*/
	tst     r2,#UTSR1_TNF /* TX not full */
        beq     TXFull
	str	r0,[r1,#UTDR]
	ldr     r0,[r1,#UTSR1]
TXBusy:     	
        ldr     r2,[r1,#UTSR1]/* check status*/
	tst     r2,#UTSR1_TBY /* TX busy */
        bne     TXBusy
        mov     pc, lr
	
	/*
        ;; ********************************************************************
	;; PrintWord -- prints the 4 characters in R0
        ;;   r0 contains the binary word
        ;;   r1 contains the base of the serial por
        ;;   writes r0 with RXSTAT, modifies r1,r2,r3,r4
	;; ********************************************************************
	*/
PrintWord:	
	mov	r3, r0
	mov     r4, lr
	bl      _C_FUNC(PrintChar)     
	
	mov     r0,r3,LSR #8    /* shift word right 8 bits*/
	bl      _C_FUNC(PrintChar)
	
	mov     r0,r3,LSR #16   /* shift word right 16 bits*/
	bl      _C_FUNC(PrintChar)
	
	mov     r0,r3,LSR #24   /* shift word right 24 bits*/
	bl      _C_FUNC(PrintChar)

	mov	r0, #'\r'
	bl	_C_FUNC(PrintChar)

	mov	r0, #'\n'
	bl	_C_FUNC(PrintChar)
	
        mov     pc, r4

	/*
        ;; ********************************************************************
	;; PrintHexWord -- prints the 4 bytes in R0 as 8 hex ascii characters
	;;   followed by a newline
        ;;   r0 contains the binary word
        ;;   r1 contains the base of the serial port
        ;;   Writes r0 with RXSTAT, modifies r1,r2,r3,r4
ENTRY(PrintHex)
	;; ********************************************************************
	*/
PrintHexWord:	
	mov     r4, lr
	mov	r3, r0
	mov	r0, r3,LSR #28
	bl      PrintHexNibble     
	mov	r0, r3,LSR #24
	bl      PrintHexNibble     
	mov	r0, r3,LSR #20
	bl      PrintHexNibble     
	mov	r0, r3,LSR #16
	bl      PrintHexNibble     
	mov	r0, r3,LSR #12
	bl      PrintHexNibble     
	mov	r0, r3,LSR #8
	bl      PrintHexNibble     
	mov	r0, r3,LSR #4
	bl      PrintHexNibble     
	mov	r0, r3
	bl      PrintHexNibble     

	mov	r0, #'\r'
	bl	_C_FUNC(PrintChar)

	mov	r0, #'\n'
	bl	_C_FUNC(PrintChar)

        mov     pc, r4

	/*
	;; ********************************************************************
	;; copy - copies are region from addr in r0 to r1. r2=stopping point
        ;;   r0 contains target region address
        ;;   r1 contains source region address
        ;;   r2 contains region length
	;; ********************************************************************
	*/
copy:	
	add     r2, r2, r1      /* end of source region in r2 */
1:      	
	cmp	r1,r2
	ldrlt	r3,[r0],#4
	strlt	r3,[r1],#4
	blt	1b
	mov	pc,lr

	/*
	;; ********************************************************************
	;; zi_init - initializes memory region
        ;;  r0 contains target region start address
        ;;  r1 contains target region size
        ;;  r2 contains value to write
	;; ********************************************************************
	*/
zi_init:	
	add     r1, r1, r0      /* put end address (exclusive) into r1 */
1:      
	cmp	r0,r1
	strlt	r2,[r0],#4
	blt	1b
	mov	pc,lr

	/*
	;; ********************************************************************
	;; enableMMU
	;; ********************************************************************
	*/
ENTRY(enableMMU)
	/*; r0 = translation table base*/
	ldr	r0,DW_MMU_TABLE
	mcr	p15, 0, r0, c2, c0, 0

	/*; r0 = domain access control*/
	ldr	r0,MMU_DOMCTRL
	mcr	p15, 0, r0, c3, c0, 0

	/*; enable the MMU*/
	mrc	p15, 0, r0, c1, c0, 0
	orr	r0, r0, #1      /* bit 0 of c1 is MMU enable*/
	
	/* make sure Virtual interrupt vector adjust is OFF */
	bic	r0, r0, #(1<<13)
	
#ifdef CACHE_ENABLED	
	orr	r0, r0, #4      /* bit 2 of c1 is DCACHE enable*/
	orr	r0, r0, #8      /* bit 3 of c1 is WB enable*/
#endif	
	mcr	p15, 0, r0, c1, c0, 0

	/*
	;; flush the I/D caches
        ;;  c7 == cache control operation register
        ;;  crm==c7 opc2==0 indicates Flush I+D Cache
        ;;  r0 is ignored
	*/
	mcr	p15, 0, r0, c7, c7, 0x00
	/*
        ;; flush the I+D TLB
        ;;  c8 is TLB operation register
        ;;  crm=c7 opc2==0 indicates Flush I+D TLB
        ;;  r0 is ignored
	*/
	mcr	p15, 0, r0, c8, c7, 0x00 
	
	/*; return*/
	mov	pc,lr		

	/*
	;; ********************************************************************
	;; flushTLB
	;; ********************************************************************
	*/
ENTRY(flushTLB)
	/*
	;; flush the I/D caches
        ;;  c7 == cache control operation register
        ;;  crm==c7 opc2==0 indicates Flush I+D Cache
        ;;  r0 is ignored
	*/
	mcr	p15, 0, r0, c7, c7, 0x00
	/*
        ;; flush the I+D TLB
        ;;  c8 is TLB operation register
        ;;  crm=c7 opc2==0 indicates Flush I+D TLB
        ;;  r0 is ignored
	*/
	mcr	p15, 0, r0, c8, c7, 0x00 

	mcr	p15, 0, r0, c7, c5, 0
	mov	r0, r0
	mov	r0, r0	
/*;; return*/
	mov	pc,lr		
ENTRY(writeBackDcache)
	/*
	;; r0 points to the start of a 16384 byte region of readable 
	;; data used only for this cache flushingroutine. If this area 
	;; is to be used by other code, then 32K must be loaded and the
	;; flush mcr is not needed.
	/*

	/*; return:	 r0,r1,r2 trashed. data cache is clean*/
	add r1,r0,#32768
l1:	
	ldr r2,[r0],#32
	teq r1, r0
	bne l1
	mcr p15,0,r0,c7,c6,0
	mov pc,lr

ENTRY(asmEnableCaches)
	/*
	;; params:	 r0 = dcache, r1 = icache
	/*

	/*; return:	 r0,r1,r2,r3  trashed. */

	mov	r3,#0
	orr	r3,r0,r3
	orr	r3,r3,r1,lsl #1
#if 0
	mov	r0,r3
	ldr     r1,_C_LABEL(SerBase)
	bl	PrintHexWord
#endif
	/* get a picture of the current cache situation */
	mrc	p15, 0, r2, c1, c0, 0  /* control address */

	/* set dcache, clear it if requested */
	orr	r2,r2,#0x04
	tst	r3,#(1 << 0)
	bne	0f
	bic	r2,r2,#0x04 /* bit 2 is dcache */
/*
	//stmfd	sp!,{r4-r5}
	//mov	r4,r2
	//flush read buffer
	//mcr	p15, 0, r0, c9,c0,0
	// flush the write buffer
	//mcr p15,0,r0,c7,c10,4
	// flush and drain dcaches
	// force the write back
*/
	
	mov	r0,#0xE0000000
	add	r0,r0,#32768
l2:	
	ldr	r1,[r0],#-32	
	cmp	r0,#0
	bgt	l2
	/* now flush the dcache */
	mcr p15,0,r0,c7,c6,0
	/* also the tlb dcache entries */
	mcr p15,0,r0,c8,c6,0
	/* and clear it */
	mcr	p15, 0, r2, c1, c0, 0
	
	nop
	nop
	nop	
0:
	/*mov	r2,r4
	  ldmfd	sp!, {r4-r5}*/
	/* set icache, clear it if requested */
	orr	r2,r2,#0x1000
	tst	r3,#(1<<1)
	bne	1f
	bic	r2,r2,#0x1000  /* bit 12 is icache */
	/* flush icache */
	mcr	p15, 0, r0, c7,c5,0
	// also the tlb icache entries
	mcr p15,0,r0,c8,c5,0
	nop
	nop
	nop
1:	
	/* store the new values */	
	mcr	p15, 0, r2, c1, c0, 0
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	/* check the result after a pause and return it */
	mrc	p15, 0, r0, c1, c0, 0  /* control address */
	mov	pc,lr

ENTRY(readPC)
        mov     r0,pc
        mov     pc,lr
		
ENTRY(readCPR0)
        mrc     p15,0,r0,c0,c0,0
        mov     pc,lr
		
ENTRY(readCPR1)
        mrc     p15,0,r0,c1,c0,0
        mov     pc,lr

ENTRY(readCPR2)
        mrc     p15,0,r0,c2,c0,0
        mov     pc,lr

ENTRY(readCPR3)
        mrc     p15,0,r0,c3,c0,0
        mov     pc,lr

ENTRY(readCPR5)
        mrc     p15,0,r0,c5,c0,0
        mov     pc,lr

ENTRY(readCPR6)
        mrc     p15,0,r0,c6,c0,0
        mov     pc,lr
	
	
ENTRY(readCPR13)
        mrc     p15,0,r0,c13,c0,0
        mov     pc,lr
		
ENTRY(readCPR14)
        mrc     p15,0,r0,c14,c0,0
        mov     pc,lr
		
	/*
	;; ********************************************************************
	;; boot - boots into another image. DOES NOT RETURN!
	;; r0 = pointer to bootinfo
	;; r1 = entry point of kernel
	;; ********************************************************************
	*/
ENTRY(boot)
	/*
        ;; flush the I+D TLB
        ;;  c8 is TLB operation register
        ;;  crm=c7 opc2==0 indicates Flush I+D TLB
        ;;  r0 is ignored
	*/
	mcr	p15, 0, r0, c8, c7, 0x00 /* flush I+D TLB */
        mcr     p15, 0, r0, c7, c10, 4 /* drain the write buffer*/

        /*; make sure the pipeline is emptied*/
        mov     r0,r0
        mov     r0,r0
        mov     r0,r0
        mov     r0,r0

	mov	pc,r1		/* jump to addr. bootloader is done*/

	/*
	;; ********************************************************************
	;; bootLinux - boots into another image. DOES NOT RETURN!
	;; r0 = must contain a zero or else the kernel loops
        ;; r1 = architecture type (6 for old kernels, 17 for new)
	;; r2 = entry point of kernel
	;; ********************************************************************
	*/
ENTRY(bootLinux)
	mov     r10, r1
	mov     r11, r2
	/*
        ;; flush the I+D TLB
        ;;  c8 is TLB operation register
        ;;  crm=c7 opc2==0 indicates Flush I+D TLB
        ;;  r0 is ignored
	*/
	mcr	p15, 0, r0, c8, c7, 0x00 /* flush I+D TLB */
        mcr     p15, 0, r0, c7, c10, 4 /* drain the write buffer*/

	mov     r3, #0x130
	mcr     p15, 0, r3, c1, c0, 0   /* disable the MMU */
        /*; make sure the pipeline is emptied*/
        mov     r0,#0
        mov     r0,r0
        mov     r0,r0
        mov     r0,r0

	/* zero PID in Process ID Virtual Address Map register. */
	mov	r0, #0
	mcr	p15, 0, r0, c13, c0, 0
	
	adr     r2, _C_FUNC(delayedBreakpointPC)
	mov     r0, #0
        mov     r1, r10
	ldr     r2, [r2, #0]
	mov	pc, r11		/* jump to addr. bootloader is done*/

	.align 5
#ifdef HAS_REBOOT_COMMAND
	/* 
	 * remove reboot command until we make it *really* reboot
	 * things.  As it is, doesn't really work  or make much sense.
	 */
	/*
	;; ********************************************************************
	;; reboot - restarts the bootloader. DOES NOT RETURN!
	;; ********************************************************************
	*/
ENTRY(reboot)
       	/*; disable the instruction/data write buffer caches*/
	mrc	p15,0,r2,c1,c0,0
	mov	r3,#~4
	AND	r2,r2,r3
	mov	r3,#~8
	AND	r2,r2,r3
	mov	r3,#~0x1000
	AND	r2,r2,r3
	mcr	p15,0,r2,c1,c0,0

	/*
	;; flush the caches
	;; flush the I/D caches
        ;;  c7 == cache control operation register
        ;;  crm==c7 opc2==0 indicates Flush I+D Cache
        ;;  r0 is ignored
	*/
	mcr	p15, 0, r0, c7, c7, 0x00
	/*
        ;; flush the I+D TLB
        ;;  c8 is TLB operation register
        ;;  crm=c7 opc2==0 indicates Flush I+D TLB
        ;;  r0 is ignored
	*/
	mcr	p15, 0, r0, c8, c7, 0x00
        mcr     p15, 0, r0, c7, c10, 4 /* drain the write buffer*/

        /*; make sure the pipeline is emptied*/
        mov     r0,r0
        mov     r0,r0
        mov     r0,r0

	b	HiReset
#endif /* HAS_REBOOT_COMMAND */

	/*
	;; ********************************************************************
	;; Data Area
	;; ********************************************************************
	*/
.ltorg

	.align 2
CRC32POLY:	
	.word	0x04c11db7
	
#	.globl  start
#	.align	2
#addr_start:	
#	.word	start

	.align	2
/*ENTRY(HEX_TO_ASCII_TABLE)*/
	.globl _C_LABEL(HEX_TO_ASCII_TABLE)
_C_LABEL(HEX_TO_ASCII_TABLE):
	.ascii	"0123456789ABCDEF"

	.align	2
STR_STACK:	
	.ascii	"STKP"

	.align 2
STR_UNDEF:	
        .ascii	"UNDF"

	.align 2
STR_RSET:
	.ascii  "RSET"

	.align	2
STR_SWI:	
        .ascii  "SWI "

	.align	2
STR_PREFETCH_ABORT:	
        .ascii	"PABT"

	.align	2
STR_DATA_ABORT:	
        .ascii	"DABT"
	.word   0

	.align	2
STR_IRQ:	
        .ascii	"IRQ "

	.align	2
STR_FIQ:	
        .ascii	"FIQ"
	
	.align	2
STR_NOT_USED:	
        .ascii	"NUSD"

	.align	2
DW_STACK_START:	
	.word	STACK_BASE+STACK_SIZE-16
	
	.globl _C_LABEL(boot_flags_ptr)
_C_LABEL(boot_flags_ptr):
	.word   STACK_BASE+STACK_SIZE-4
	
	.align	2
DW_MMU_TABLE:	
	.word	MMU_TABLE_START

	.align	2
DW_CACHE_FLUSH_REGION:	
	.word	CACHE_FLUSH_BASE
	
	.align	2
MMU_DOMCTRL:	
	.word	0xFFFFFFFF

	.align	2
STR_MTST:	
        .ascii	"MTST"

	.align	2
STR_MB2:	
        .ascii	"MBK2"

	.align	2
STR_ENDM:	
        .ascii	"ENDM"

	.align	2
STR_PRECACHE:
        .ascii  "PRE$"	

	.align 2
STR_POSTCACHE:
        .ascii  "PST$"	

	.align 2
STR_PREZERO:
        .ascii  "PREZ"	

	.align 2
STR_POSTZERO:
        .ascii  "PSTZ"	

	.align 2
STR_POSTFLUSH:
        .ascii  "FLSH"	


	.align 2
STR_MEM_ERROR:
	.ascii	"DRAM ERROR"

	.align	2
STR_WAKEUP:	
        .ascii	"WKUP"

	.align	2
STR_WAKEUP_SKIP:	
        .ascii	"SKWU"
		
	.globl _C_FUNC(delayedBreakpointPC)
_C_FUNC(delayedBreakpointPC):
        .long   0

	/*; ******************************************************************/
	

.align 4
dram_mdcnfg_enable:
#ifdef CONFIG_DRAM_BANK1	
	.long  MDCNFG_BANK0_ENABLE | MDCNFG_BANK1_ENABLE 
#else
	.long  MDCNFG_BANK0_ENABLE
#endif
.align 4	
dram_mdcnfg_64mb_from_wince:     /* DRAM Configuration [1] 10.2 */
	/* Bitsy development board uses two banks? KM416S4030C, 12 row address bits, 8 col address bits */
	/* Bitsy uses two banks KM416S8030C, 12 row address bits, 9 col address bits */
        /* Have to set DRAC0 to 15 row bits or else you only get 9 col bits */
	/* read from the formfactor unit configuration registers:	 0xF3536257 */
	/* 0xf365 */	
	.long  (MDCNFG_DTIM0_SDRAM | MDCNFG_DWID0_32B \
                | MDCNFG_DRAC0(6) |  MDCNFG_TRP0(3) | MDCNFG_TDL0(3) | MDCNFG_TWR0(3))
dram_mdcnfg_64mb:     /* DRAM Configuration [1] 10.2 */
	/* Bitsy development board uses two banks? KM416S4030C, 12 row address bits, 8 col address bits */
	/* Bitsy uses two banks KM416S8030C, 12 row address bits, 9 col address bits */
        /* Have to set DRAC0 to 15 row bits or else you only get 9 col bits */
	.long  (MDCNFG_DTIM0_SDRAM | MDCNFG_DWID0_32B \
                | MDCNFG_DRAC0(6) |  MDCNFG_TRP0(3) | MDCNFG_TDL0(3) | MDCNFG_TWR0(3))
dram_mdcnfg_64mb_jornada56x:     /* DRAM Configuration [1] 10.2 */
    .long ( (MDCNFG_BANK0_ENABLE | MDCNFG_DTIM0_SDRAM | MDCNFG_DWID0_32B \
           | MDCNFG_DRAC0(6) |  MDCNFG_TRP0(2) | MDCNFG_TDL0(3) | MDCNFG_TWR0(2)) \
           | (MDCNFG_DTIM0_SDRAM | MDCNFG_DWID0_32B \
           | MDCNFG_DRAC0(6) |  MDCNFG_TRP0(2) | MDCNFG_TDL0(3) | MDCNFG_TWR0(2)) << 16) /* 64mb*/

.align 4	
dram_mdcnfg_64mbX:     /* DRAM Configuration [1] 10.2 */
	.long  (MDCNFG_BANK0_ENABLE| MDCNFG_DTIM0_SDRAM | MDCNFG_DWID0_32B \
                | MDCNFG_DRAC0(6) |  MDCNFG_TRP0(3) | MDCNFG_TDL0(3) | MDCNFG_TWR0(3))
		
dram_mdcnfg_32mb_from_wince:     /* DRAM Configuration [1] 10.2 */
	/* 0xf354 */	
	.long  (MDCNFG_DTIM0_SDRAM | MDCNFG_DWID0_32B \
                | MDCNFG_DRAC0(5) |  MDCNFG_TRP0(3) | MDCNFG_TDL0(3) | MDCNFG_TWR0(3))
dram_mdcnfg_32mb:     /* DRAM Configuration [1] 10.2 */
	/* Bitsy development board uses two banks? KM416S4030C, 12 row address bits, 8 col address bits */
	/* Bitsy uses two banks KM416S8030C, 12 row address bits, 9 col address bits */
        /* Have to set DRAC0 to 14 row bits or else you only get 8 col bits */
	/* read from the formfactor unit configuration registers:	 0xF3536257 */
	/* 0x7254, this is value that cpufreq calculates for this sdram part. */
	.long  (MDCNFG_DTIM0_SDRAM | MDCNFG_DWID0_32B \
                | MDCNFG_DRAC0(5) |  MDCNFG_TRP0(2) | MDCNFG_TDL0(3) | MDCNFG_TWR0(1))
dram_mdcnfg_32mb_jornada56x:     /* DRAM Configuration [1] 10.2 */
    .long ( (MDCNFG_BANK0_ENABLE | MDCNFG_DTIM0_SDRAM | MDCNFG_DWID0_32B \
            | MDCNFG_DRAC0(5) |  MDCNFG_TRP0(2) | MDCNFG_TDL0(3) | MDCNFG_TWR0(2)) \
           | (MDCNFG_DTIM0_SDRAM | MDCNFG_DWID0_32B \
            | MDCNFG_DRAC0(5) |  MDCNFG_TRP0(2) | MDCNFG_TDL0(3) | MDCNFG_TWR0(2)) << 16) /* 32mb */
	
dram_mdcnfg_16mb_from_wince:     /* DRAM Configuration [1] 10.2 */
	.long  (MDCNFG_BANK0_ENABLE | MDCNFG_DTIM0_SDRAM | MDCNFG_DWID0_32B \
                | MDCNFG_DRAC0(4) |  MDCNFG_TRP0(3) | MDCNFG_TDL0(3) | MDCNFG_TWR0(3))
dram_mdcnfg_16mb:     /* DRAM Configuration [1] 10.2 */
	.long  (MDCNFG_BANK0_ENABLE | MDCNFG_DTIM0_SDRAM | MDCNFG_DWID0_32B \
                | MDCNFG_DRAC0(4) |  MDCNFG_TRP0(2) | MDCNFG_TDL0(3) | MDCNFG_TWR0(1))
			
	/* MDCAS settings from [1] Table 10-3 (page 10-18) */
dram_cas0_waveform0:
#if defined(CONFIG_SA1100_JORNADA720)
        .long 0x5555557f
#else
        .long 0xAAAAAAA7
#endif /* CONFIG_MACH_JORNADA720) */
dram_cas0_waveform0_jornada56x:
    .long 0xAAAAAA9F
dram_cas0_waveform1:
#if defined(CONFIG_SA1100_JORNADA720)
        .long 0x55555555
#else
	.long 0xAAAAAAAA /* includes Jornada 56x */
#endif /* CONFIG_MACH_JORNADA720) */
dram_cas0_waveform2:
#if defined(CONFIG_SA1100_JORNADA720)
        .long 0x55555555
#else
	.long 0xAAAAAAAA /* includes Jornada 56x */
#endif /* CONFIG_MACH_JORNADA720) */
#if defined(CONFIG_SA1100_JORNADA720)
dram_mdrefr:	
	.long (MDREFR_TRASR(1) | MDREFR_DRI(42) | MDREFR_E1PIN | \
	MDREFR_K1RUN | MDREFR_K1DB2 | MDREFR_K0DB2 | MDREFR_EAPD | MDREFR_KAPD)
dram_mdrefr64Mplus:	
	.long (MDREFR_TRASR(1) | MDREFR_DRI(42) | MDREFR_E1PIN | \
	MDREFR_K1RUN | MDREFR_K1DB2 | MDREFR_K0DB2 | MDREFR_EAPD | MDREFR_KAPD)
#else
dram_mdrefr_from_wince:	
	/* 00302001 */
	.long (MDREFR_TRASR(1) | MDREFR_DRI(512) | MDREFR_E1PIN | MDREFR_K1RUN)
dram_mdrefr: /* cpufreq uses 0x3000C7 */
	.long (MDREFR_TRASR(7) | MDREFR_DRI(0xC) | MDREFR_E1PIN | MDREFR_K1RUN)
dram_mdrefr64Mplus_from_wince:	
	/* wince used 0x00300191 */
	.long (MDREFR_TRASR(1) | MDREFR_DRI(0x19) | MDREFR_E1PIN | \
	MDREFR_K1RUN)
dram_mdrefr64Mplus:	
	/* cpufreq uses 0x300067 */
	.long (MDREFR_TRASR(1) | MDREFR_DRI(0x19) | MDREFR_E1PIN | \
	MDREFR_K1RUN)
#endif /* CONFIG_MACH_JORNADA720) */
dram_mdrefr_jornada56x:
    .long (MDREFR_TRASR(1) | MDREFR_DRI(45) | MDREFR_E1PIN | \
         MDREFR_K1RUN | MDREFR_K0DB2 | MDREFR_EAPD | MDREFR_KAPD) /* 32mb */
dram_mdrefr64Mplus_jornada56x:
    .long (MDREFR_TRASR(1) | MDREFR_DRI(21) | MDREFR_E1PIN | \
         MDREFR_K1RUN | MDREFR_K0DB2 | MDREFR_EAPD | MDREFR_KAPD) /* 64mb */

	.globl msc1_config
	.globl msc2_config
#if defined(CONFIG_SA1100_H3XXX) || defined(CONFIG_SA1100_ASSABET) || defined(CONFIG_ASSABET_NEPONSET) || defined(CONFIG_SA1100_JORNADA56X)
msc0_config:    	
        .long ( ( (MSC_RT_ROMFLASH | MSC_RBW32 | MSC_RDF(14) | MSC_RDN(3) | MSC_RRR(2)) <<  0 )  /* MEMBNK0 150ns flash */ \
        |       ( (MSC_RT_ROMFLASH | MSC_RBW32 | MSC_RDF(31) | MSC_RDN(31) | MSC_RRR(7)) << 16 ) /* MEMBNK1 */ )
msc1_config:    
        .long ( ( (MSC_RT_ROMFLASH | MSC_RBW16 | MSC_RDF(31) | MSC_RDN(31) | MSC_RRR(7)) <<  0 ) /* MEMBNK2 */ \
        |       ( (MSC_RT_ROMFLASH | MSC_RBW16 | MSC_RDF(31) | MSC_RDN(31) | MSC_RRR(7)) << 16 )  /* MEMBNK3 */)
msc2_config:
        .long ( ( (MSC_RT_ROMFLASH | MSC_RBW16 | MSC_RDF(31) | MSC_RDN(31) | MSC_RRR(7)) <<  0 ) /* MEMBNK4 */ \
        |       ( (MSC_RT_BURST4 | MSC_RBW32 | MSC_RDF(3) | MSC_RDN(3) | MSC_RRR(2)) << 16 )  /* MEMBNK5: EGPIO */)
mecr_config:
        .long 0x994A994A /*; read from registers while WinCE was running */
#elif defined(CONFIG_SA1100_JORNADA720)
msc0_config:    	
        .long 0xFFF04F78;
msc1_config:    
        .long ( ( (MSC_RT_ROMFLASH | MSC_RBW32 | MSC_RDF(30) | MSC_RDN(31) | \
               MSC_RRR(7)) <<  0 ) /* MEMBNK2 */ \
        |       ( (MSC_RT_ROMFLASH | MSC_RBW32 | MSC_RDF(31) | MSC_RDN(31) | \
               MSC_RRR(7)) << 16 )  /* MEMBNK3 */)
msc2_config:
        .long 0x201D2959;
mecr_config:
        .long 0x98C698C6;
#endif /* CONFIG_MACH_JORNADA720 */	 
	.globl msc1_config_jornada56x
	.globl msc2_config_jornada56x
msc0_config_jornada56x:
        .long ( ( (MSC_RT_BURST4 | MSC_RBW32 | MSC_RDF(14) | MSC_RDN(3) | \
               MSC_RRR(2)) <<  0 )  /* MEMBNK0 150ns flash */ \
        |       ( (MSC_RT_ROMFLASH | MSC_RBW32 | MSC_RDF(30) | MSC_RDN(31) | \
               MSC_RRR(7)) << 16 ) /* MEMBNK1 */ )
msc1_config_jornada56x:
        .long ( ( (MSC_RT_ROMFLASH | MSC_RBW32 | MSC_RDF(30) | MSC_RDN(31) | \
               MSC_RRR(7)) <<  0 ) /* MEMBNK2 */ \
        |       ( (MSC_RT_ROMFLASH | MSC_RBW32 | MSC_RDF(31) | MSC_RDN(31) | \
               MSC_RRR(7)) << 16 )  /* MEMBNK3 */)
msc2_config_jornada56x:
        /* .long 0x201DEF71; */
        .long ( ( (MSC_RT_SRAM_012 | MSC_RDF(14) | MSC_RDN(15) | \
               MSC_RRR(7)) <<  0 ) \
        |       ( (MSC_RT_VARLAT_345 | MSC_RBW16 | MSC_RDF(3) | MSC_RDN(0) | \
               MSC_RRR(1)) << 16 ) )
mecr_config_jornada56x:
        .long 0x19451945;
.align 4
SDLCBase:	
	.long SDLCBASE
	.globl _C_LABEL(Ser1Base)
_C_LABEL(Ser1Base):
	.long UART2BASE
	.globl _C_LABEL(Ser2Base)
_C_LABEL(Ser2Base):
	.long UART1BASE
	.globl _C_LABEL(Ser3Base)
_C_LABEL(Ser3Base):       	
	.long UART3BASE
	.globl _C_LABEL(SerBase)
SerBase:
#ifdef CONFIG_SA1100_ASSABET
	.long UART1BASE
#else
	.long UART3BASE
#endif	
RTC_BASE_ADDRESS:    
        .long RTC_BASE
RTC_RUNAHEAD:
#if 0
	.long 12153 /* according to sgodsell */
#endif        	
        .long 1297 /* empirically determined.  RTTR seems to be cleared about 40ms before CPU comes out of reset */ 
PPCR_ADDRESS:
	.long PPCR_REG
ICMR_ADDRESS:
	.long ICMR_REG
	
RCSR_ADDRESS:
	.long RCSR_REG
PSSR_ADDRESS:
	.long PSSR_REG
PSPR_ADDRESS:
	.long PSPR_REG
	.globl _C_LABEL(pspr_wince_cookie)
pspr_wince_cookie:      
PSPR_WINCE_COOKIE:
	.long 0xA0F50000 /* maybe this value will work with PocketPC2002 */
	
GPIOBase:
	.long GPIO_BASE

#if defined(CONFIG_SA1100_ASSABET) || defined(CONFIG_ASSABET_NEPONSET)
BCRBase:
	.long ASSABET_BCR
#endif
#if defined(CONFIG_SA1100_H3XXX) || defined(CONFIG_SA1100_JORNADA56X)
EGPIOBase:
        .long 0x49000000
H3800_ASIC1Base:	
	.long _H3800_ASIC2_Base + _H3800_ASIC1_GPIO_Base
wince_jp:
	.long	0x00040000
WINCE_MAGIC_1:	
	.long WINCE_PARTITION_MAGIC_1
WINCE_MAGIC_LONG_0:	
	.long WINCE_PARTITION_LONG_0
WINCE_MAGIC_2:	
	.long WINCE_PARTITION_MAGIC_2
bootldr_magic:  
	.long BOOTLDR_MAGIC
#endif


boot_flags_init_val:
#if defined(CONFIG_SA1100_H3XXX)
        .long	BF_SQUELCH_SERIAL|BF_NORMAL_BOOT
#else
	.long	BF_NORMAL_BOOT
#endif

mdrefr_valid_bit_mask_after_reset:	
	.long MDREFR_SLFRSH|MDREFR_KAPD|MDREFR_EAPD|MDREFR_K2DB2|MDREFR_K1RUN|MDREFR_E1PIN|MDREFR_K0DB2

zzval: .long	0xf8050000 /* or Ser3Base */


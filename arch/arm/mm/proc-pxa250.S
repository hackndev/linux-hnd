/*
 *  linux/arch/arm/mm/proc-pxa250.S
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * MMU functions for the Intel PXA250 XScale CPUs
 *
 */

#include <linux/linkage.h>
#include <linux/init.h>
#include <asm/assembler.h>
#include <asm/procinfo.h>
#include <asm/hardware.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <asm/ptrace.h>
#include "proc-macros.S"

/*
 * This is the maximum size of an area which will be flushed.  If the area
 * is larger than this, then we flush the whole cache
 */
#define MAX_AREA_SIZE	32768

/*
 * the cache line size of the I and D cache
 */
#define CACHELINESIZE	32

/*
 * the size of the data cache
 */
#define CACHESIZE	32768

/*
 * Virtual address used to allocate the cache when flushed
 *
 * This must be an address range which is _never_ used.  It should
 * apparently have a mapping in the corresponding page table for
 * compatibility with future CPUs that _could_ require it.  For instance we
 * don't care.
 *
 * This must be aligned on a 2*CACHESIZE boundary.  The code selects one of
 * the 2 areas in alternance each time the clean_d_cache macro is used.
 * Without this the XScale core exhibits cache eviction problems and no one
 * knows why.
 *
 * Reminder: the vector table is located at 0xffff0000-0xffff0fff.
 */
#define CLEAN_ADDR	0xfffe0000

/*
 * This macro is used to wait for a CP15 write and is needed
 * when we have to ensure that the last operation to the co-pro
 * was completed before continuing with operation.
 */
	.macro	cpwait, rd
	mrc	p15, 0, \rd, c2, c0, 0		@ arbitrary read of cp15
	mov	\rd, \rd			@ wait for completion
	sub 	pc, pc, #4			@ flush instruction pipeline
	.endm

	.macro	cpwait_ret, lr, rd
	mrc	p15, 0, \rd, c2, c0, 0		@ arbitrary read of cp15
	sub	pc, \lr, \rd, LSR #32		@ wait for completion and
						@ flush instruction pipeline
	.endm

	.text

/*
 * We enable the cache here, but we make sure all the status bits for dirty
 * lines are cleared as well (see PXA250 erratum #120).
 */
ENTRY(cpu_pxa250_proc_init)
	@ enable data cache
	ldr	r0, cr_p
	ldmia	r0, {r1, r2}
	orr	r1, r1, #0x4
	orr	r2, r2, #0x4
	stmia	r0, {r1, r2}
	mcr	p15, 0, r1, c1, c0, 0
	cpwait	r0

	@ invalidate data cache
	mcr	p15, 0, r0, c7, c6, 0

	@ fill main cache with write-through lines
	bic	r0, pc, #0x1f
	add	r1, r0, #CACHESIZE
1:	ldr	r2, [r0], #32
	cmp	r0, r1
	bne	1b

	@ enable test feature to force all fills to the mini-cache
	mov	r1, #0x8
	mcr	p15, 0, r1, c15, c15, 3

	@ fill mini-cache with write-through lines (2kbytes, 64 lines)
	add	r1, r0, #2048
2:	ldr	r2, [r0], #32
	cmp	r0, r1
	bne	2b

	@ disable test feature to force all fills to the mini-cache
	mov	r1, #0x0
	mcr	p15, 0, r1, c15, c15, 3

	@ invalidate data cache again
	mcr	p15, 0, r1, c7, c6, 0
	mov	pc, lr

cr_p:	.long	cr_alignment

	.align	5
ENTRY(cpu_pxa250_set_pte)
	str	r1, [r0], #-2048		@ linux version

	bic	r2, r1, #0xff0
	orr	r2, r2, #PTE_TYPE_EXT		@ extended page

	eor	r3, r1, #L_PTE_PRESENT | L_PTE_YOUNG | L_PTE_WRITE | L_PTE_DIRTY

	tst	r3, #L_PTE_USER | L_PTE_EXEC	@ User or Exec?
	orrne	r2, r2, #PTE_EXT_AP_URO_SRW	@ yes -> user r/o, system r/w

	tst	r3, #L_PTE_WRITE | L_PTE_DIRTY	@ Write and Dirty?
	orreq	r2, r2, #PTE_EXT_AP_UNO_SRW	@ yes -> user n/a, system r/w
						@ combined with user -> user r/w

	@
	@ Handle the X bit.  We want to set this bit for the minicache
	@ (U = E = B = W = 0, C = 1) or when write allocate is enabled,
	@ and we have a writeable, cacheable region.  If we ignore the
	@ U and E bits, we can allow user space to use the minicache as
	@ well.
	@
	@  X = C & ~W & ~B
	@      | C & W & B & write_allocate
	@
	eor	ip, r1, #L_PTE_CACHEABLE
	tst	ip, #L_PTE_CACHEABLE | L_PTE_WRITE | L_PTE_BUFFERABLE
	orreq	r2, r2, #PTE_EXT_TEX(1)

	tst	r1, #L_PTE_CACHEABLE
	bicne	r2, r2, #L_PTE_BUFFERABLE	@ clear B only if C is set

	tst	r3, #L_PTE_PRESENT | L_PTE_YOUNG	@ Present and Young?
	movne	r2, #0				@ no -> fault

	str	r2, [r0]			@ hardware version

	@ We try to map 64K page entries when possible.  
	@ We do that for kernel space only since the usage pattern from
	@ the setting of VM area is quite simple.  User space is not worth
	@ the implied complexity because of ever randomly changing PTEs 
	@ (page aging, swapout, etc) requiring constant coherency checks.
	@ Since PTEs are usually set in increasing order, we test the
	@ possibility for a large page only when given the last PTE of a
	@ 64K boundary.
	tsteq	r1, #L_PTE_USER
	andeq	r1, r0, #(15 << 2)
	teqeq	r1, #(15 << 2)
	beq	1f

	mov	ip, #0
	mcr	p15, 0, r0, c7, c10, 1		@ Clean D cache line
	mcr	p15, 0, ip, c7, c10, 4		@ Drain Write (& Fill) Buffer
	mov	pc, lr

	@ See if we have 16 identical PTEs but with consecutive base addresses
1:	bic	r3, r2, #0x0000f000
	mov	r1, #0x0000f000
2:	eor	r2, r2, r3
	teq	r2, r1
	bne	4f
	subs	r1, r1, #0x00001000
	ldr	r2, [r0, #-4]!
	bne	2b
	eors	r2, r2, r3
	bne	4f

	@ Now create our LARGE PTE from the current EXT one.
	bic	r3, r3, #PTE_TYPE_MASK
	orr	r3, r3, #PTE_TYPE_LARGE
	and	r2, r3, #0x30			@ EXT_AP --> LARGE_AP0
	orr	r2, r2, r2, lsl #2		@ add LARGE_AP1
	orr	r2, r2, r2, lsl #4		@ add LARGE_AP3 + LARGE_AP2
	and	r1, r3, #0x3c0			@ EXT_TEX
	bic	r3, r3, #0x3c0
	orr	r2, r2, r1, lsl #(12 - 6)	@ --> LARGE_TEX
	orr	r2, r2, r3			@ add remaining bits

	@ then put it in the pagetable
	mov	r3, r2
3:	strd	r2, [r0], #8
	tst	r0, #(15 << 2)
	bne	3b

	@ Then sync the 2 corresponding cache lines
	sub	r0, r0, #(16 << 2)
	mcr	p15, 0, r0, c7, c10, 1		@ Clean D cache line
4:	orr	r0, r0, #(15 << 2)
	mcr	p15, 0, r0, c7, c10, 1		@ Clean D cache line
	mov	ip, #0
	mcr	p15, 0, ip, c7, c10, 4		@ Drain Write (& Fill) Buffer
	mov	pc, lr

	.align

	__INIT

	.type	__pxa250_setup, #function
__pxa250_setup:
	mov	r0, #PSR_F_BIT|PSR_I_BIT|SVC_MODE
	msr	cpsr_c, r0
	mcr	p15, 0, ip, c7, c7, 0		@ invalidate I, D caches & BTB
	mcr	p15, 0, ip, c7, c10, 4		@ Drain Write (& Fill) Buffer
	mcr	p15, 0, ip, c8, c7, 0		@ invalidate I, D TLBs
	mcr	p15, 0, r4, c2, c0, 0		@ load page table pointer
	mov	r0, #0x1f			@ Domains 0, 1 = client
	mcr	p15, 0, r0, c3, c0, 0		@ load domain access register
	mov	r0, #1				@ Allow access to CP0 and CP13
	orr	r0, r0, #1 << 13		@ Its undefined whether this
	mcr	p15, 0, r0, c15, c1, 0		@ affects USR or SVC modes
	mrc	p15, 0, r0, c1, c0, 0		@ get control register
	bic	r0, r0, #0x0200			@ .... ..R. .... ....
	bic	r0, r0, #0x0002			@ .... .... .... ..A.
	orr	r0, r0, #0x0001			@ .... .... .... ...M  @ see cpu_pxa250_proc_init
	orr	r0, r0, #0x3900			@ ..VI Z..S .... ....
	mov	pc, lr
	.size	__pxa250_setup, . - __pxa250_setup

	__INITDATA

/*
 * Purpose : Function pointers used to access above functions - all calls
 *	     come through these
 */

	.type	pxa250_processor_functions, #object
ENTRY(pxa250_processor_functions)
	.word	v5t_early_abort
	.word	cpu_pxa250_proc_init
	.word	cpu_xscale_proc_fin
	.word	cpu_xscale_reset
	.word	cpu_xscale_do_idle
	.word	cpu_xscale_dcache_clean_area
	.word	cpu_xscale_switch_mm
	.word	cpu_pxa250_set_pte
	.size	pxa250_processor_functions, . - pxa250_processor_functions

	.section ".rodata"

	.type	cpu_arch_name, #object
cpu_arch_name:
	.asciz	"armv5te"
	.size	cpu_arch_name, . - cpu_arch_name

	.type	cpu_elf_name, #object
cpu_elf_name:
	.asciz	"v5"
	.size	cpu_elf_name, . - cpu_elf_name

	.type	cpu_pxa250_name, #object
cpu_pxa250_name:
	.asciz	"XScale-PXA250"
	.size	cpu_pxa250_name, . - cpu_pxa250_name

	.type	cpu_pxa210_name, #object
cpu_pxa210_name:
	.asciz	"XScale-PXA210"
	.size	cpu_pxa210_name, . - cpu_pxa210_name

	.align

	.section ".proc.info", #alloc, #execinstr

	.type	__pxa250_proc_info,#object
__pxa250_proc_info:
	.long	0x69052100
	.long	0xfffff7f0
	.long	0x00000c0a
	b	__pxa250_setup
	.long	cpu_arch_name
	.long	cpu_elf_name
	.long	HWCAP_SWP|HWCAP_HALF|HWCAP_THUMB|HWCAP_FAST_MULT|HWCAP_EDSP
	.long	cpu_pxa250_name
	.long	pxa250_processor_functions
	.long	v4wbi_tlb_fns
	.long	xscale_mc_user_fns
	.long	xscale_cache_fns
	.size	__pxa250_proc_info, . - __pxa250_proc_info

	.type	__pxa210_proc_info,#object
__pxa210_proc_info:
	.long	0x69052120
	.long	0xfffff7f0
	.long	0x00000c0a
	b	__pxa250_setup
	.long	cpu_arch_name
	.long	cpu_elf_name
	.long	HWCAP_SWP|HWCAP_HALF|HWCAP_THUMB|HWCAP_FAST_MULT|HWCAP_EDSP
	.long	cpu_pxa210_name
	.long	pxa250_processor_functions
	.long	v4wbi_tlb_fns
	.long	xscale_mc_user_fns
	.long	xscale_cache_fns
	.size	__pxa210_proc_info, . - __pxa210_proc_info


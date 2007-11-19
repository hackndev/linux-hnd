@
@  lab/modules/armboot-asm.S
@
@  Copyright (C) 2003 Joshua Wise
@  Bootloader port to Linux Kernel, October 07, 2003
@
@  ARMBoot kernel loader
@  assembler routines

	.globl	armboot_asm
	.globl	relocdone

armboot_asm:
	mov	r7,r0		@ Backup registers.
	mov	r8,r1
	mov	r10,r2
	mov	r9,r3
	
	msr 	cpsr, #0xd3	@ Disable IRQs

	mov	r0,#0x4000	@ MMU stuff taken from Demo's code.
	sub	r0,r0,#1
	mcr	p15, 0, r0, c7, c10, 4
	mrc	p15, 0, r0, c2, c0, 0
	mov	r0, r0
	mrc	p15, 0, r3, c1, c0, 0
	mov	r3, #0x120
	mcr	p15, 0, r3, c1, c0, 0
	mrc	p15, 0, r0, c2, c0, 0
	mov	r0,r0		@ MMU is now disabled.
	mov	pc,r8		@ Jump into our relocated self.

relocdone:			@ Welcome to relocatedland - the MMU is off.
	mov	r0,r7		@ Restore zapped register
	
	@ At this point, the MMU is off, and in r0 is the address
	@ of a list of blocks we need to relocate.
	ldr	r1,[r0],#4
	ldr	r2,[r0],#4
	ldr	r3,[r0],#4
	ldr	r4,[r0],#4
	
	@ r1 == old address
	@ r2 == new address
	@ r3 == size
	@ r4 == if this is the last block
	
cploop:	cmp	r3,#0		@ Out of bytes to relocate?
	beq	cpdone		@ All done!
	ldr	r5,[r1],#4	@ Copy a word...
	str	r5,[r2],#4
	sub	r3,r3,#1	@ Subtract from bytes remaining...
	b	cploop		@ And go loop again.
	
cpdone:	cmp	r4,#0		@ We're all done copying words - are we done relocating?
	bne	rldone		@ Yeah? Stop.
	
	ldr	r1,[r0],#4	@ No? Load more values and go again!
	ldr	r2,[r0],#4
	ldr	r3,[r0],#4
	ldr	r4,[r0],#4
	b	cploop
	
rldone:	@ At this point we're done relocating.
	mov	r0, #0
	mcr	p15, 0, r0, c7, c7, 0		@ flush I-cache, D-cache and BTB
	mcr	p15, 0, r0, c7, c10, 4		@ drain write buffer

	mrc	p15, 0, r0, c2, c0, 0
	mov	r0,r0

	mcr	p15, 0, r0, c8, c7, 0		@ flush I-TLB and D-TLB
	mrc	p15, 0, r0, c2, c0, 0
	mov	r0,r0

	@ Print a character
	mov	r2,	#0x40000000
	orr	r2,r2,	#0x00100000
1:
	ldr	r0,[r2,#0x14]
	tst	r0,#(1<<6)
	beq	1b
	mov	r0,#'B'
	str	r0,[r2]
1:
	ldr	r0,[r2,#0x14]
	tst	r0,#(1<<6)
	beq	1b
	mov	r0,#'O'
	str	r0,[r2]
1:
	ldr	r0,[r2,#0x14]
	tst	r0,#(1<<6)
	beq	1b
	mov	r0,#'O'
	str	r0,[r2]
1:
	ldr	r0,[r2,#0x14]
	tst	r0,#(1<<6)
	beq	1b
	mov	r0,#'T'
	str	r0,[r2]
1:
	ldr	r0,[r2,#0x14]
	tst	r0,#(1<<6)
	beq	1b
	mov	r0,#':'
	str	r0,[r2]

	@ Now all that's left to do is to jump into the kernel.

	mov	r0,#0	@ r0 must be 0 for jumping into kernel
	mov	r1,r9	@ r1 must be archtype
	
	mov	pc,r10

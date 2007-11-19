/*
 * linux/arch/arm/boot/compressed/head-h2200.S
 *
 * Copyright (C) 2005 Matt Reimer <mreimer@vpop.net>
 *
 * For HP iPAQ H2200
 *
 * The HTC 1st-stage bootloader loads the first block of the 2nd-stage
 * bootloader (NAND blocks 10-80) and verifies its validity by looking
 * for 0xea0003fe ('b 0x1000') at offset 0x0 and 'ECEC' at offset 0x40;
 * If present, the entire 2nd-stage loader is read and then the 1st-stage
 * jumps into the beginning of the 2nd-stage; otherwise, the 1st-stage
 * loads and jumps into the rescue bootloader (NAND blocks 1-7).
 */

#include <asm/mach-types.h>

		.section	".start", "ax"

		b	0x1000

		.org	0x40
		.ascii	"ECEC"

		.org	0x1000

		mov	r0, #0
		mov	r1, #(MACH_TYPE_H2200 & 0xff)
		orr	r1, r1, #(MACH_TYPE_H2200 & 0xff00)
		mov	r2, #0

#include "head.S"

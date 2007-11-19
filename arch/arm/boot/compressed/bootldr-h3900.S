/* bootldr-h3900.S
 *
 * h3900-specific definitions and code.
 */

#include <asm/arch/h3900.h>
#include <asm/arch/h3900-init.h>
#include <asm/hardware/ipaq-asic2.h>
#include <asm/hardware/ipaq-asic3.h>

#define ARCH		H3900
#define INIT_MSC0	0
#define INIT_MSC1	0
#define INIT_MSC2	0x74a40000
#include "bootldr-machdefs.S"

H3900_ASIC3_BASE:
        	.long	H3900_ASIC3_PHYS
H3900_ASIC2_BASE:
		.long 	H3900_ASIC2_PHYS
_H3900_ASIC3_GPIO_OUT_INIT:
		.long 0x0004
_H3900_ASIC3_GPIO_MASK_INIT:
		.long   0xffff
_H3900_ASIC3_GPIO_DIR_INIT:
		.long   0xffff

Check_H3900:	@ h3900 has asic3 part number at 0x14801000
		@ now that gpios and MSC2 are configured, we can probe asic3
		ldr	r0, H3900_ASIC3_BASE
		add	r0, r0, #0x00001000
		ldrh	r1, [r0, #0]
		ldrh	r2, ASIC3_PART_NUMBER_1
		cmp	r1, r2
		bne	1f
		ldrh	r1, [r0, #4]
		ldrh	r2, ASIC3_PART_NUMBER_2
		cmp	r1, r2
1:		mov	pc, lr

Init_H3900:	@ see if the user is holding down the action button
		ldr	r6, H3900_ASIC2_BASE
		add	r6, r6, #_IPAQ_ASIC2_KPIO_Base
		@ set up the asic2 buttons in alt mode to be read
		add	r1, r6, #_IPAQ_ASIC2_KPIO_Alternate
		mov	r0, #0x3e
		str	r0, [r1]
		@ read the action button = kpio5
		add	r1, r6, #_IPAQ_ASIC2_KPIO_Data
		ldr	r0, [r1]
		tst	r0, #(1<<5)	@ action button on 3800, 0 is pressed, 1 is up
		orreq	r12, r12, #BF_ACTION_BUTTON
		biceq	r12, r12, #BF_NORMAL_BOOT

@ Initialize serial UART
#if defined CONFIG_LAB_DEBUG_FFUART || defined CONFIG_LAB_DEBUG_BTUART \
 || defined CONFIG_LAB_DEBUG_STUART
		ldr	r3, H3900_ASIC3_BASE
		@ set up the mask register
		add	r1,r3,#_IPAQ_ASIC3_GPIO_B_Base
		add	r1,r1,#_IPAQ_ASIC3_GPIO_Mask
		ldr	r0,_H3900_ASIC3_GPIO_MASK_INIT
		str	r0,[r1]
		@ set up the out register
		add	r1,r3,#_IPAQ_ASIC3_GPIO_B_Base
		add	r1,r1,#_IPAQ_ASIC3_GPIO_Out
		ldr	r0,_H3900_ASIC3_GPIO_OUT_INIT
		str	r0,[r1]
		@ set up dir register
		add	r1,r3,#_IPAQ_ASIC3_GPIO_B_Base
		add	r1,r1,#_IPAQ_ASIC3_GPIO_Direction
		ldr	r0,_H3900_ASIC3_GPIO_DIR_INIT
		str	r0,[r1]

		@ set up the out register
		add	r1,r3,#_IPAQ_ASIC3_GPIO_B_Base
		add	r1,r1,#_IPAQ_ASIC3_GPIO_Out
		ldr	r0,_H3900_ASIC3_GPIO_OUT_INIT
		str	r0,[r1]
#endif /* UART */

		mov	pc, lr

End_H3900:

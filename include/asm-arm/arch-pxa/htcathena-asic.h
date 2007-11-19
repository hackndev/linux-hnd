/* 
 * include/asm-arm/arch-pxa/htcathena-asic.h
 * History:
 *
 */

#ifndef _HTCATHENA_ASIC_H_
#define _HTCATHENA_ASIC_H_

#define HTCATHENA_CPLD1_BASE	PXA_CS2_PHYS


#define HTCATHENA_CPLD2_BASE	PXA_CS2_PHYS+0x02000000

#define HTCATHENA_BOARDID0	13
#define HTCATHENA_BOARDID1	14
#define HTCATHENA_BOARDID2	15

#define HTCATHENA_DPRAM_BASE	PXA_CS4_PHYS

extern void      htcathena_cpld2_set( u_int16_t bits );
extern void      htcathena_cpld2_clr( u_int16_t bits );
extern u_int16_t htcathena_cpld2_get(void);

#endif /* _HTCATHENA_GPIO_H */

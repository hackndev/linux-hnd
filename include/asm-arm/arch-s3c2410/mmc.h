/* linux/include/asm-arm/arch-s3c2410/mmc.h
 *
 * (c) 2004-2005 Simtec Electronics
 *	http://www.simtec.co.uk/products/SWLINUX/
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * S3C24XX - MMC/SD platform data
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Changelog:
 *	26-Oct-2005 BJD  Created file
*/

#ifndef __ASM_ARCH_MMC_H
#define __ASM_ARCH_MMC_H __FILE__

struct s3c24xx_mmc_platdata {
	unsigned int	gpio_detect;
	unsigned int	gpio_wprotect;
	unsigned int	detect_polarity;
	unsigned int	wprotect_polarity;

	unsigned long	f_max;
	unsigned long	ocr_avail;

	void		(*set_power)(unsigned int to);
};

#endif /* __ASM_ARCH_MMC_H */

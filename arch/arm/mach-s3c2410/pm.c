/* linux/arch/arm/mach-s3c2410/pm.c
 *
 * Copyright (c) 2006 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * S3C2410 (and compatible) Power Manager (Suspend-To-RAM) support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/sysdev.h>

#include <asm/hardware.h>
#include <asm/io.h>

#include <asm/mach-types.h>

#include <asm/arch/regs-gpio.h>
#include <asm/arch/h1940.h>

#ifdef CONFIG_MACH_G500 
#include <asm/arch/g500.h>
#endif

#include <asm/plat-s3c24xx/cpu.h>
#include <asm/plat-s3c24xx/pm.h>

#ifdef CONFIG_S3C2410_PM_DEBUG
extern void pm_dbg(const char *fmt, ...);
#define DBG(fmt...) pm_dbg(fmt)
#else
#define DBG(fmt...) printk(KERN_DEBUG fmt)
#endif

// space for saving memory location where we copy resume code 
// (a few instructions for now, see sleep.S, should be dynamically
// computed)
static unsigned long s3c24xx_resume_addr_save[4];

static void s3c2410_pm_prepare(void)
{
	/* ensure at least GSTATUS3 has the resume address */

	__raw_writel(virt_to_phys(s3c2410_cpu_resume), S3C2410_GSTATUS3);

	DBG("GSTATUS3 0x%08x\n", __raw_readl(S3C2410_GSTATUS3));
	DBG("GSTATUS4 0x%08x\n", __raw_readl(S3C2410_GSTATUS4));

	if (machine_is_h1940()) {
		void *base = phys_to_virt(H1940_SUSPEND_CHECK);
		unsigned long ptr;
		unsigned long calc = 0;

		/* generate check for the bootloader to check on resume */

		for (ptr = 0; ptr < 0x40000; ptr += 0x400)
			calc += __raw_readl(base+ptr);

		__raw_writel(calc, phys_to_virt(H1940_SUSPEND_CHECKSUM));
	}

	/* the RX3715 uses similar code and the same H1940 and the
	 * same offsets for resume and checksum pointers */

	if (machine_is_rx3715()) {
		void *base = phys_to_virt(H1940_SUSPEND_CHECK);
		unsigned long ptr;
		unsigned long calc = 0;

		/* generate check for the bootloader to check on resume */

		for (ptr = 0; ptr < 0x40000; ptr += 0x4)
			calc += __raw_readl(base+ptr);

		__raw_writel(calc, phys_to_virt(H1940_SUSPEND_CHECKSUM));
	}

	if ( machine_is_aml_m5900() )
		s3c2410_gpio_setpin(S3C2410_GPF2, 1);

#ifdef CONFIG_MACH_G500 
	// save previous memory area before to overwrite it with the resume code
	memcpy(s3c24xx_resume_addr_save, phys_to_virt(G500_SUSPEND_RESUMEAT), 16);

	// now write the resume code
    	memcpy(phys_to_virt(G500_SUSPEND_RESUMEAT), h1940_pm_return, 16);
#endif
}

static int s3c2410_pm_resume(struct sys_device *dev)
{
	unsigned long tmp;

	/* unset the return-from-sleep flag, to ensure reset */

	tmp = __raw_readl(S3C2410_GSTATUS2);
	tmp &= S3C2410_GSTATUS2_OFFRESET;
	__raw_writel(tmp, S3C2410_GSTATUS2);

	if ( machine_is_aml_m5900() )
		s3c2410_gpio_setpin(S3C2410_GPF2, 0);

#ifdef CONFIG_MACH_G500 
	// put back the previous data to the address where we copied the resume code
	memcpy(phys_to_virt(G500_SUSPEND_RESUMEAT), s3c24xx_resume_addr_save, 16);
#endif
	return 0;
}

static int s3c2410_pm_add(struct sys_device *dev)
{
	pm_cpu_prep = s3c2410_pm_prepare;
	pm_cpu_sleep = s3c2410_cpu_suspend;

	return 0;
}

#if defined(CONFIG_CPU_S3C2410)
static struct sysdev_driver s3c2410_pm_driver = {
	.add		= s3c2410_pm_add,
	.resume		= s3c2410_pm_resume,
};

/* register ourselves */

static int __init s3c2410_pm_drvinit(void)
{
	return sysdev_driver_register(&s3c2410_sysclass, &s3c2410_pm_driver);
}

arch_initcall(s3c2410_pm_drvinit);
#endif

#if defined(CONFIG_CPU_S3C2440)
static struct sysdev_driver s3c2440_pm_driver = {
	.add		= s3c2410_pm_add,
	.resume		= s3c2410_pm_resume,
};

static int __init s3c2440_pm_drvinit(void)
{
	return sysdev_driver_register(&s3c2440_sysclass, &s3c2440_pm_driver);
}

arch_initcall(s3c2440_pm_drvinit);
#endif

#if defined(CONFIG_CPU_S3C2442)
static struct sysdev_driver s3c2442_pm_driver = {
	.add		= s3c2410_pm_add,
	.resume		= s3c2410_pm_resume,
};

static int __init s3c2442_pm_drvinit(void)
{
	return sysdev_driver_register(&s3c2442_sysclass, &s3c2442_pm_driver);
}

arch_initcall(s3c2442_pm_drvinit);
#endif

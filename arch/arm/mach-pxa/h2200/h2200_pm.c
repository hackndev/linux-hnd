/*
 * h2200 power management support for the HTC bootloader
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/firmware.h>
#include <linux/workqueue.h>

#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/arch/pxa-regs.h>

#include <asm/arch/h2200.h>
#include <asm/hardware/ipaq-hamcop.h>


/* On resume, the PXA begins executing the code at 0x00000000, which is in
   HAMCOP's SRAM. But there is a conflict: the samcop_sdi driver uses HAMCOP's
   SRAM for DMA, which results in the bootloader being overwritten. So we
   must save the bootloader before it gets overwritten, and restore it at
   suspend.

   The problem with using the HTC bootloader is that it does its
   initialization and then jumps to 0xa0040000, which on Linux
   is somewhere in kernel code. There are a couple of solutions to this
   problem:

   1. Save/restore enough bytes at 0xa0040000 to insert code to jump to the
      resume function. This is still a little risky depending on what's
      at 0xa0040000.

   2. Store a position-independent code fragment into SRAM that jumps to the
      resume function. This is safer since we don't have to worry about
      stomping on the kernel; and we have to copy the bootloader into place
      anyway.

   Both methods have been tested to work. Method #2 seems safer so that is
   what we're using.

   We can jump into the resume function a couple of ways:

      a. load the resume address from PSPR into pc

		ldr r0, [pc, #0]
		ldr pc, [r0]
		.word	 0x40f00008;	// PSPR

      b. load the resume address directly into pc

		ldr pc, [pc, #-4]
		.word resume_addr
*/

static u8  *bootloader;
static int bootloader_valid = 0;

#define BOOTLOADER_LOAD_DELAY (HZ * 30)
#if defined(CONFIG_FW_LOADER_MODULE)
static struct work_struct fw_work;
#else
static struct delayed_work fw_work;
#endif

extern struct pm_ops pxa_pm_ops;

static int (*pxa_pm_enter_orig)(suspend_state_t state);


static int h2200_pxa_pm_enter(suspend_state_t state)
{
	int ret;

	if (!bootloader_valid) {
		printk(KERN_ERR "h2200_pm: no valid bootloader; suspend cancelled\n");
		return 1;
	}

	ret = pxa_pm_enter_orig(state);

	MSC0 = 0x246c7ffc;
	(void)MSC0;
	MSC1 = 0x7ff07ff0;
	(void)MSC1;
	MSC2 = 0x7ff07ff0;
	(void)MSC2;

	return ret;
}

/* Return a pointer to the bootloader. */
u8 *
get_hamcop_bootloader(void)
{
	return bootloader;
}
EXPORT_SYMBOL(get_hamcop_bootloader);

/* Patch the bootloader so it resumes to the address in PSPR
 * instead of 0xa0040000. */
static void h2200_patch_fw(void)
{
	int i;
	u32 *code = (u32 *)bootloader;

	i = 0x75c / sizeof(u32);
	code[i++] = 0xe59f0000; /* ldr r0, [pc, #0] */
	code[i++] = 0xe590f000; /* ldr pc, [r0] */
	code[i++] = 0x40f00008; /* PSPR */
}

#if defined(CONFIG_FW_LOADER) || defined(CONFIG_FW_LOADER_MODULE)
static void h2200_load_bootloader(struct work_struct *notused)
{
	const struct firmware *fw;

	if (request_firmware(&fw, "h2200_bootloader.bin", &h2200_hamcop.dev)) {
		printk(KERN_ERR "h2200_pm: request_firmware failed\n");
#if defined(CONFIG_FW_LOADER_MODULE)
		schedule_work(&fw_work);
#else
		schedule_delayed_work(&fw_work, BOOTLOADER_LOAD_DELAY);
#endif
		return;
	}

	printk("h2200_pm: bootloader loaded\n");
	memcpy(bootloader, fw->data, fw->size < HAMCOP_SRAM_Size ?
				       fw->size : HAMCOP_SRAM_Size);
	release_firmware(fw);
	h2200_patch_fw();
	bootloader_valid = 1;
}
#endif

static int __init h2200_pm_init(void) {

	u8 *sram;

	if (machine_is_h2200()) {

		printk("Initialising wince bootloader suspend workaround\n");

		pxa_pm_enter_orig = pxa_pm_ops.enter;
		pxa_pm_ops.enter = h2200_pxa_pm_enter;

		bootloader_valid = 0;
		bootloader = kmalloc(HAMCOP_SRAM_Size, GFP_KERNEL);
		if (!bootloader) {
		    printk(KERN_ERR "h2200_pm: unable to kmalloc bootloader\n");
		    return -ENOMEM;
		}

		/* Copy what's already in SRAM for now, as it's good in
		 * many cases (e.g. where LAB hasn't already written
		 * over it.) */
		sram = ioremap(H2200_HAMCOP_BASE, HAMCOP_MAP_SIZE);
		memcpy(bootloader, sram, HAMCOP_SRAM_Size);
		iounmap(sram);

		/* If the bootloader is good, there's no need to load
		 * another. Check for the HTC bootloader by looking for
		 * 'b 0x80' at +0x0, and for 'ECEC' at +0x40. */
		if (((u32 *)bootloader)[0]    == 0xea00001e &&
		    ((u32 *)bootloader)[0x10] == 0x43454345) {

		    h2200_patch_fw();
		    bootloader_valid = 1;
		} else
		    printk(KERN_ERR "h2200_pm: bootloader may be invalid; "
			   "resume may not work\n");

#if defined(CONFIG_FW_LOADER) || defined(CONFIG_FW_LOADER_MODULE)
		if (!bootloader_valid) {

		    /* There doesn't seem to be a valid bootloader in
		     * SRAM, so try to load one from userland. Note that
		     * we schedule the work for a little later when compiled
		     * into the kernel (rather than as a module), to give
		     * other modules a chance to initialize; without this
		     * delay, we were getting an error,
		     * "Badness in kref_get". */
		    printk("h2200_pm: requesting bootloader\n");

#if defined(CONFIG_FW_LOADER_MODULE)
		    INIT_WORK(&fw_work, h2200_load_bootloader);
		    schedule_work(&fw_work);
#else
		    INIT_DELAYED_WORK(&fw_work, h2200_load_bootloader);
		    schedule_delayed_work(&fw_work, BOOTLOADER_LOAD_DELAY);
#endif
		}
#else
		bootloader_valid = 1;
#endif
	}
	
	return 0;
}

static void __exit h2200_pm_exit(void)
{
	pxa_pm_ops.enter = pxa_pm_enter_orig;
}

module_init(h2200_pm_init);
module_exit(h2200_pm_exit);

MODULE_AUTHOR("Matt Reimer <mreimer@vpop.net>");
MODULE_DESCRIPTION("HP iPAQ h2200 power management support for HTC bootloader");
MODULE_LICENSE("Dual BSD/GPL");

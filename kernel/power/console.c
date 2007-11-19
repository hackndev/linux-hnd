/*
 * drivers/power/process.c - Functions for saving/restoring console.
 *
 * Originally from swsusp.
 */

#include <linux/vt_kern.h>
#include <linux/kbd_kern.h>
#include <linux/console.h>
#include "power.h"

#if defined(CONFIG_VT) && defined(CONFIG_VT_CONSOLE)
#define SUSPEND_CONSOLE	(MAX_NR_CONSOLES-1)

static int orig_fgconsole, orig_kmsg;

int pm_prepare_console(void)
{
	/* <Alex>
	 * OPIE locks the console so we can't switch vts, this means we hang when 
	 * trying to suspend. I'm not sure how we're supposed to get around it,
	 * other than this. The suspend console is ugly, anyhow. ;-)
	 */
#ifndef CONFIG_MACH_PALMZ72
#ifndef CONFIG_MACH_XSCALE_PALMLD
#ifndef CONFIG_MACH_T3XSCALE

	acquire_console_sem();

	orig_fgconsole = fg_console;

	if (vc_allocate(SUSPEND_CONSOLE)) {
	  /* we can't have a free VC for now. Too bad,
	   * we don't want to mess the screen for now. */
		release_console_sem();
		return 1;
	}

	if (set_console(SUSPEND_CONSOLE)) {
		/*
		 * We're unable to switch to the SUSPEND_CONSOLE.
		 * Let the calling function know so it can decide
		 * what to do.
		 */
		release_console_sem();
		return 1;
	}
	release_console_sem();

	if (vt_waitactive(SUSPEND_CONSOLE)) {
		pr_debug("Suspend: Can't switch VCs.");
		return 1;
	}
	orig_kmsg = kmsg_redirect;
	kmsg_redirect = SUSPEND_CONSOLE;
#endif
#endif
#endif
	return 0;
}

void pm_restore_console(void)
{
#ifndef CONFIG_MACH_PALMZ72
#ifndef CONFIG_MACH_XSCALE_PALMLD
#ifndef CONFIG_MACH_T3XSCALE
	acquire_console_sem();
	set_console(orig_fgconsole);
	release_console_sem();
	kmsg_redirect = orig_kmsg;
#endif
#endif
#endif
	return;
}
#endif

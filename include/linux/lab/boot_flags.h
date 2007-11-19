#ifndef _BOOT_FLAGS_H
#define _BOOT_FLAGS_H

/*
 * these bits control the boot flow.
 * They are stored initially in the PWER register.
 * Then, once RAM is running, they are stored in RAM
 * The current location is pointed to by Boot_flags_ptr.
 */

#define	BF_SQUELCH_SERIAL	(1<<0)
#define	BF_SLEEP_RESET		(1<<1)
#define	BF_NORMAL_BOOT		(1<<2)
#define BF_ACTION_BUTTON        (1<<3)
#define BF_NEED_RESTORE_R9	(1<<4)

/* These defines must die, replaced by MACH_TYPE_xxx */
#define	BF_JORNADA56X		(1<<16) /* for checking in boot-sa1100.s */
#define BF_H3800                (1<<17)

#endif

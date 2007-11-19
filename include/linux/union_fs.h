#ifndef _LINUX_UNION_FS_H
#define _LINUX_UNION_FS_H

#define UNIONFS_VERSION  "2.0"
/*
 * DEFINITIONS FOR USER AND KERNEL CODE:
 */
# define UNIONFS_IOCTL_INCGEN		_IOR(0x15, 11, int)
# define UNIONFS_IOCTL_QUERYFILE	_IOR(0x15, 15, int)

/* We don't support normal remount, but unionctl uses it. */
# define UNIONFS_REMOUNT_MAGIC		0x4a5a4380

/* should be at least LAST_USED_UNIONFS_PERMISSION<<1 */
#define MAY_NFSRO			16

#endif /* _LINUX_UNIONFS_H */


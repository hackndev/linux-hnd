#ifndef LINUX_BUZZER_H
#define LINUX_BUZZER_H

#include <linux/ioctl.h>

#define	BUZZER_IOCTL_BASE	'Z'

struct buzzer_time
{
	u32 on_time;
	u32 off_time;
};

#define	IOC_SETBUZZER	_IOW(BUZZER_IOCTL_BASE, 0, struct buzzer_time)

#endif

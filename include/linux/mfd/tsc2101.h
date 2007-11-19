/*
 * TI TSC2101 Structure Definitions
 *
 * Copyright 2005 Openedhand Ltd.
 *
 * Author: Richard Purdie <richard@o-hand.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

struct tsc2101_ts_event {
	short p;
	short x;
	short y;
};

struct tsc2101_misc_data {
	short bat;
	short aux1;
	short aux2;
	short temp1;
	short temp2;
};

struct tsc2101_data {
	spinlock_t lock;
	int pendown;
	struct tsc2101_platform_info *platform;
	struct input_dev inputdevice;
	struct timer_list ts_timer;
	struct timer_list misc_timer;
	struct tsc2101_misc_data miscdata;
};

struct tsc2101_platform_info {
	void (*send)(int read, int command, int *values, int numval);
	void (*suspend) (void);
	void (*resume) (void);
	int irq;
	int (*pendown) (void);
};





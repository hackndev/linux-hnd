/* 
 * Copyright (c) 2006  Michal Sroczynski <msroczyn@elka.pw.edu.pl> 
 * Copyright (c) 2006  Anton Vorontsov <cbou@mail.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/cpufreq.h>

struct pxa27x_voltage_chip {
	u_int8_t address:7;
	u_int8_t (*mv2cmd)(int mv);

	// for chips without or improper hardware ramp-rate control
	unsigned int step;  // fill voltage ramp by step (usually 50 mV)
	unsigned int delay; // 2^delay 13 MHz cycles delay after each command

	// private
	unsigned int delay_ms;
	struct notifier_block freq_transition;
};

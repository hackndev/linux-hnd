/*
 *  linux/include/asm-arm/arch-pxa/clock.h
 *
 *  Copyright (C) 2006 Erik Hovland
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

struct clk {
	struct list_head	node;
	struct module		*owner;
	struct clk		*parent;
	const char		*name;
	int			id;
	unsigned int		enabled;
	unsigned long		rate;
	unsigned long		ctrlbit;

	void			(*enable)(struct clk *);
	void			(*disable)(struct clk *);
};


extern int clk_register(struct clk *clk);
extern void clk_unregister(struct clk *clk);

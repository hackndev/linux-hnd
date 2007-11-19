/*
 *  linux/drivers/input/serio/sa1100ir.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/serio.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/hardware.h>



struct sa1100_kbd {
	struct serio	io;
	void		*base;
	int		irq;
};

static void sa1100ir_int(int irq, void *dev_id, struct pt_regs *regs)
{
	struct sa1100_kbd *kbd = dev_id;
	unsigned int status;

	do {
		unsigned int flag, data;

		status = readl(kbd->base + UTSR1);
		if (!(status & UTSR1_RNE))
			break;

		flag = (status & UTSR1_FRE ? SERIO_FRAME : 0) |
		       (status & UTSR1_PRE ? SERIO_PARITY : 0);

		data = readl(kbd->base + UTDR);

		serio_interrupt(&kbd->io, data, flag);
	} while (1);

	status = readl(kbd->base + UTSR0) & UTSR0_RID | UTSR0_RBB | UTSR0_REB;
	if (status)
		writel(status, kbd->base + UTSR0);
}

static int sa1100ir_kbd_open(struct serio *io)
{
	struct sa1100_kbd *kbd = io->driver;
	int ret;

	ret = request_irq(kbd->irq, sa1100ir_int, 0, kbd->io.phys, kbd);
	if (ret)
		return ret;

	return 0;
}

static void sa1100ir_kbd_close(struct serio *io)
{
	struct sa1100_kbd *kbd = io->driver;

	free_irq(kbd->irq, kbd);
}

static struct sa1100_kbd sa1100_kbd = {
	.io = {
		.type	= 0,
		.open	= sa1100ir_kbd_open,
		.close	= sa1100ir_kbd_close,
		.name	= "SA11x0 IR port",
		.phys	= "sa11x0/ir",
		.driver	= &sa1100_kbd,
	},
};

static int __init sa1100_kbd_init(void)
{
	serio_register_port(&sa1100_kbd.io);
}

static void __exit sa1100_kbd_exit(void)
{
	serio_unregister_port(&sa1100_kbd.io);
}

module_init(sa1100_kbd_init);
module_exit(sa1100_kbd_exit);

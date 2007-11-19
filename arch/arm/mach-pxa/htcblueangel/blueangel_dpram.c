/*
 *
 *  Based on drivers/serial/8250.c by Russell King.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/serial_reg.h>
#include <linux/circ_buf.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial_core.h>
#include <linux/irq.h>

#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/htcblueangel-gpio.h>

/* HTC DPRAM */
#define PORT_DPRAM      69

struct dpram_data {
	unsigned long data_phys;
	void *data_virt;
	unsigned long ctrl_phys;
	void *ctrl_virt;
} dpram_data;

#define REG0 (*((volatile unsigned short *)(dpram_data.ctrl_virt+0)))
#define REG2 (*((volatile unsigned short *)(dpram_data.ctrl_virt+2)))
#define REG4 (*((volatile unsigned short *)(dpram_data.ctrl_virt+4)))
#define REG6 (*((volatile unsigned short *)(dpram_data.ctrl_virt+6)))
#define REG8 (*((volatile unsigned short *)(dpram_data.ctrl_virt+8)))
#define REG100 (*((volatile unsigned short *)(dpram_data.ctrl_virt+100)))
#define REG214 (*((volatile unsigned short *)(dpram_data.ctrl_virt+214)))

#ifdef DEBUG
#define dprintk(x...) printk(x)
#else
#define dprintk(x...)
#endif

static void
dpram_ack1(void)
{
	REG100 &= 0xfbff;
	REG0 |= 0x200;
	REG0 &= 0xfe00;
}

static void
dpram_ack2(void)
{
	REG100 &= 0xf7ff;
	REG0 |= 0x200;
	REG0 &= 0xfe00;
}

static void
dpram_ack3(void)
{
	REG100 &= 0xefff;
	REG0 |= 0x200;
	REG0 &= 0xfe00;
}

static void
dpram_req1(void)
{
	REG100|=0x400;
	REG0|=0x800;
}

static void
dpram_req2(void)
{
	REG100|=0x800;
	REG0|=0x800;
}

static void
dpram_req3(void)
{
	REG100|=0x1000;
	REG0|=0x800;
}

static void dpram_enable_ms(struct uart_port *port)
{
	dprintk("dpram_enable_ms\n");
}

static void dpram_stop_tx(struct uart_port *port)
{
	dprintk("dpram_stop_tx\n");
}

static void dpram_stop_rx(struct uart_port *port)
{
	dprintk("dpram_stop_rx\n");
}

static void
receive_chars(struct uart_port *up)
{
	struct tty_struct *tty = up->info->tty;
	unsigned int ch, flag;
	int i,count;
	unsigned short s;
	unsigned char *rxptr;

	int max_count = 256;

	rxptr=dpram_data.data_virt+0x200;
	s=REG6;
	dprintk("REG6=0x%x\n", s);
	count=s & 0x3ff;
	if ((s & 0x1000) && count) {
		dprintk("%d bytes data available\n", count);
		for (i = 0 ; i < count ; i++) {
			ch=*rxptr++;
			flag = TTY_NORMAL;
			up->icount.rx++;
			uart_insert_char(up, 0, 0, ch, flag);
		}
		tty_flip_buffer_push(tty);
	}
	REG4|=0x800;
	return;

	do {
		ch = 0; /* serial_in(up, UART_RX); */
		flag = TTY_NORMAL;
		up->icount.rx++;

		uart_insert_char(up, 0, UART_LSR_OE, ch, flag);

	} while ((max_count-- > 0));
	tty_flip_buffer_push(tty);
}

static unsigned char 
transmit_char(struct uart_port *up, struct circ_buf *xmit)
{
	unsigned char ret;

	ret=xmit->buf[xmit->tail];
	xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
	up->icount.tx++;

	return ret;
}
static void transmit_chars(struct uart_port *up)
{
	struct circ_buf *xmit = &up->info->xmit;
	int count,written;
	unsigned short *txptr;
	unsigned short s;

	txptr=dpram_data.data_virt;

	if (up->x_char) {
		dprintk("x_char\n");
		*txptr=up->x_char;
		REG0=(REG0&0xfe00)|1;
		dpram_req1();
		up->icount.tx++;
		up->x_char = 0;
		return;
	}
	if (uart_circ_empty(xmit) || uart_tx_stopped(up)) {
		dpram_stop_tx(up);
		return;
	}

	dprintk("REG0=0x%x\n", REG0);
	count = 255;
	written=0;
	do {
		s=transmit_char(up, xmit);
		written++;
		if (! uart_circ_empty(xmit)) {
			s |= (transmit_char(up, xmit) << 8);
			written++;
		}
		dprintk("transmitting 0x%x\n", s);
		*txptr++=s;
		if (uart_circ_empty(xmit))
			break;
	} while (--count > 0);
	dprintk("written %d\n", written);
	REG0=(REG0&0xfe00)|(written & 0x1ff);
	dpram_req1();
	
	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(up);

	if (uart_circ_empty(xmit))
		dpram_stop_tx(up);
}

static void dpram_start_tx(struct uart_port *port)
{
	dprintk("dpram_start_tx\n");
	transmit_chars(port);
}

static void check_modem_status(struct uart_port *up)
{
	dprintk("check_modem_status\n");
}

/*
 * This handles the interrupt from one port.
 */
static irqreturn_t
dpram_irq_rx(int irq, void *dev_id)
{
	dprintk("dpram_irq_rx\n");
	receive_chars(dev_id);
	return IRQ_HANDLED;
}

static irqreturn_t
dpram_irq_tx(int irq, void *dev_id)
{

	dprintk("dpram_irq_tx\n");
	dpram_ack1();
	transmit_chars(dev_id);
	return IRQ_HANDLED;
}

static unsigned int dpram_tx_empty(struct uart_port *port)
{
	dprintk("dpram_tx_empty\n");
	return TIOCSER_TEMT;
}

static unsigned int dpram_get_mctrl(struct uart_port *port)
{
	dprintk("dpram_get_mctrl\n");
	return TIOCM_CTS | TIOCM_DSR | TIOCM_CAR;
}

static void dpram_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	dprintk("dpram_set_mctrl\n");
}

static void dpram_break_ctl(struct uart_port *port, int break_state)
{
	dprintk("dpram_break_ctl\n");
}

static int dpram_startup(struct uart_port *port)
{
	int retval;
	dprintk("dpram_startup\n");

	/*
	 * Allocate the IRQ
	 */
	retval = request_irq(IRQ_NR_BLUEANGEL_RX_FULL, dpram_irq_rx, 0, "DPRAM_RX", port);
	set_irq_type(IRQ_NR_BLUEANGEL_RX_FULL, IRQ_TYPE_EDGE_FALLING);
	if (! retval) {
		retval = request_irq(IRQ_NR_BLUEANGEL_TX_EMPTY, dpram_irq_tx, 0, "DPRAM_TX", port);
		set_irq_type(IRQ_NR_BLUEANGEL_TX_EMPTY, IRQ_TYPE_EDGE_FALLING);
	}

	REG8=0x1;
	REG214=0x1;
	REG100=0x1;
	REG0=0x0;

	receive_chars(port);
	return retval;
}

static void dpram_shutdown(struct uart_port *port)
{
	dprintk("dpram_shutdown\n");
	free_irq(IRQ_NR_BLUEANGEL_RX_FULL, port);
	free_irq(IRQ_NR_BLUEANGEL_TX_EMPTY, port);
}

static void
dpram_set_termios(struct uart_port *port, struct ktermios *termios,
		       struct ktermios *old)
{
	dprintk("dpram_set_termios\n");
}

static void
dpram_pm(struct uart_port *port, unsigned int state,
	      unsigned int oldstate)
{
	dprintk("dpram_pm %d %d\n", state, oldstate);
}

static void dpram_release_port(struct uart_port *port)
{
	dprintk("dpram_release_port\n");
}

static int dpram_request_port(struct uart_port *port)
{
	dprintk("dpram_request_port\n");
	return 0;
}

static void dpram_config_port(struct uart_port *port, int flags)
{
	dprintk("dpram_config_port\n");
	port->type = PORT_DPRAM;
}

static int
dpram_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	/* we don't want the core code to modify any port params */
	return -EINVAL;
}

static const char *
dpram_type(struct uart_port *port)
{
	return "DPRAM";
}

struct uart_ops dpram_pops = {
	.tx_empty	= dpram_tx_empty,
	.set_mctrl	= dpram_set_mctrl,
	.get_mctrl	= dpram_get_mctrl,
	.stop_tx	= dpram_stop_tx,
	.start_tx	= dpram_start_tx,
	.stop_rx	= dpram_stop_rx,
	.enable_ms	= dpram_enable_ms,
	.break_ctl	= dpram_break_ctl,
	.startup	= dpram_startup,
	.shutdown	= dpram_shutdown,
	.set_termios	= dpram_set_termios,
	.pm		= dpram_pm,
	.type		= dpram_type,
	.release_port	= dpram_release_port,
	.request_port	= dpram_request_port,
	.config_port	= dpram_config_port,
	.verify_port	= dpram_verify_port,
};

static struct uart_port dpram_port = {
	.type		= PORT_DPRAM,
	.iotype		= UPIO_MEM,
	.irq		= IRQ_NR_BLUEANGEL_RX_FULL,
	.uartclk	= 921600 * 16,
	.fifosize	= 64,
	.ops		= &dpram_pops,
	.line		= 0,
};

static struct uart_driver dpram_reg = {
	.owner		= THIS_MODULE,
	.driver_name	= "DPRAM serial",
	.dev_name	= "ttyS",
	.major		= TTY_MAJOR,
	.minor		= 72,
	.nr		= 1,
	.name_base	= 8,
};

int __init dpram_init(void)
{
	int ret,i;
	unsigned short *hwidentp,s;
	char hwident[64];

	dpram_data.data_phys=0x10000000;
	dpram_data.ctrl_phys=0x10800000;
	dpram_data.data_virt=ioremap(dpram_data.data_phys, 4096);	
	if (! dpram_data.data_virt) {
		ret=-ENOMEM;
		goto err1;
	}
	dpram_data.ctrl_virt=ioremap(dpram_data.ctrl_phys, 4096);	
	if (! dpram_data.ctrl_virt) {
		ret=-ENOMEM;
		goto err2;
	}
	dpram_port.membase=dpram_data.ctrl_virt;
	dpram_port.mapbase=dpram_data.ctrl_phys;
	ret = uart_register_driver(&dpram_reg);
	if (ret != 0)
		goto err3;
	ret = uart_add_one_port(&dpram_reg, &dpram_port);
	if (ret != 0)
		goto err4;
	hwidentp=dpram_data.ctrl_virt+0x300;
	for (i=0;i<16;i++) {
		s=*hwidentp++;
		hwident[i*2+1]=s & 0xff;
		hwident[i*2]=s >> 8; 
	}
	hwident[i*2]='\0';
	printk("dpram hwident: '%s'\n",hwident);
	return 0;

err4:
	uart_unregister_driver(&dpram_reg);
err3:
	iounmap(dpram_data.ctrl_virt);
err2:	
	iounmap(dpram_data.data_virt);
err1:
	return ret;
}

void __exit dpram_exit(void)
{
	uart_remove_one_port(&dpram_reg, &dpram_port);
	uart_unregister_driver(&dpram_reg);
	iounmap(dpram_data.ctrl_virt);
	iounmap(dpram_data.data_virt);
}

module_init(dpram_init);
module_exit(dpram_exit);

MODULE_LICENSE("GPL");

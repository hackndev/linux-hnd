/*
 *  linux/include/asm-arm/arch-pxa/serial.h
 *
 * Author:	Nicolas Pitre
 * Copyright:	(C) 2001 MontaVista Software Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <asm/arch/pxa-regs.h>

#define BAUD_BASE	921600

/* Standard COM flags */
#define STD_COM_FLAGS (ASYNC_SKIP_TEST)

#define STD_SERIAL_PORT_DEFNS	\
	{	\
		type:			PORT_PXA,	\
		xmit_fifo_size:		64,		\
		baud_base:		BAUD_BASE,	\
		iomem_base:		&FFUART,	\
		iomem_reg_shift:	2,		\
		io_type:		SERIAL_IO_MEM,	\
		irq:			IRQ_FFUART,	\
		flags:			STD_COM_FLAGS,	\
	}, {	\
		type:			PORT_PXA,	\
		xmit_fifo_size:		64,		\
		baud_base:		BAUD_BASE,	\
		iomem_base:		&STUART,	\
		iomem_reg_shift:	2,		\
		io_type:		SERIAL_IO_MEM,	\
		irq:			IRQ_STUART,	\
		flags:			STD_COM_FLAGS,	\
	}, {	\
		type:			PORT_PXA,	\
		xmit_fifo_size:		64,		\
		baud_base:		BAUD_BASE,	\
		iomem_base:		&BTUART,	\
		iomem_reg_shift:	2,		\
		io_type:		SERIAL_IO_MEM,	\
		irq:			IRQ_BTUART,	\
		flags:			STD_COM_FLAGS,	\
	}

#define EXTRA_SERIAL_PORT_DEFNS

struct platform_pxa_serial_funcs {

	/* Initialize whatever is connected to this serial port. */
	void (*configure)(int state);
#define PXA_UART_CFG_PRE_STARTUP   0
#define PXA_UART_CFG_POST_STARTUP  1
#define PXA_UART_CFG_PRE_SHUTDOWN  2
#define PXA_UART_CFG_POST_SHUTDOWN 3

	/* Enable or disable the individual transmitter/receiver submodules.
	 * On transceivers without echo cancellation (e.g. SIR)
	 * transmitter always has priority; e.g. if both bits are set,
	 * only the transmitter is enabled. */
        void (*set_txrx)(int txrx);
#define PXA_SERIAL_TX 1
#define PXA_SERIAL_RX 2

	/* Get the current state of tx/rx. */
	int (*get_txrx)(void);

	int (*suspend)(struct platform_device *dev, pm_message_t state);
	int (*resume)(struct platform_device *dev);
};

void pxa_set_ffuart_info(struct platform_pxa_serial_funcs *ffuart_funcs);
void pxa_set_btuart_info(struct platform_pxa_serial_funcs *btuart_funcs);
void pxa_set_stuart_info(struct platform_pxa_serial_funcs *stuart_funcs);
void pxa_set_hwuart_info(struct platform_pxa_serial_funcs *hwuart_funcs);

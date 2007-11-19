/*
 * Machine initialization for Dell Axim X5
 *
 * Authors: Martin Demin <demo@twincar.sk>,
 *          Andrew Zabolotny <zap@homelink.ru>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/lcd.h>
#include <linux/backlight.h>
#include <linux/mfd/mq11xx.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/serial.h>
#include <asm/arch/aximx5-init.h>
#include <asm/arch/aximx5-gpio.h>

#include "../generic.h"


/* Uncomment the following line to get serial console via SIR work from
 * the very early booting stage. This is not useful for end-user.
 */
#define EARLY_SIR_CONSOLE

#define IR_TRANSCEIVER_ON \
	mq_regs->SPI.blue_gpio_mode = \
		(mq_regs->SPI.blue_gpio_mode | (1 << 3) | (3 << 14))
#define IR_TRANSCEIVER_OFF \
	mq_regs->SPI.blue_gpio_mode = \
		(mq_regs->SPI.blue_gpio_mode & ~(1 << 3)) | (3 << 14)

static struct mediaq11xx_regs *mq_regs;

static void aximx5_irda_configure (int state)
{
	/* Switch STUART RX/TX pins to SIR */
	pxa_gpio_mode(GPIO46_STRXD_MD);
	pxa_gpio_mode(GPIO47_STTXD_MD);
	/* make sure FIR ICP is off */
	ICCR0 = 0;

	switch (state) {
	case PXA_UART_CFG_POST_STARTUP:
		/* configure STUART to for SIR */
		STISR = STISR_XMODE | STISR_RCVEIR | STISR_RXPL;
		IR_TRANSCEIVER_ON;
		break;
	case PXA_UART_CFG_PRE_SHUTDOWN:
		STISR = 0;
		IR_TRANSCEIVER_OFF;
		break;
	default:
		break;
	}
}

static void aximx5_irda_set_txrx (int txrx)
{
	unsigned old_stisr = STISR;
        unsigned new_stisr = old_stisr;

	if (txrx & PXA_SERIAL_TX) {
		/* Ignore RX if TX is set */
		txrx &= PXA_SERIAL_TX;
		new_stisr |= STISR_XMITIR;
	} else
		new_stisr &= ~STISR_XMITIR;

	if (txrx & PXA_SERIAL_RX)
		new_stisr |= STISR_RCVEIR;
	else
		new_stisr &= ~STISR_RCVEIR;

	if (new_stisr != old_stisr) {
		while (!(STLSR & LSR_TEMT)) ;
		IR_TRANSCEIVER_OFF;
		STISR = new_stisr;
		IR_TRANSCEIVER_ON;
	}
}

static int aximx5_irda_get_txrx (void)
{
	return ((STISR & STISR_XMITIR) ? PXA_SERIAL_TX : 0) |
	       ((STISR & STISR_RCVEIR) ? PXA_SERIAL_RX : 0);
}

static struct platform_pxa_serial_funcs aximx5_serial_funcs = {
	.configure = aximx5_irda_configure,
	.set_txrx = aximx5_irda_set_txrx,
	.get_txrx = aximx5_irda_get_txrx,
};

static struct map_desc aximx5_io_desc[] __initdata = {
	{ 0xf3800000, 0x14040000, 0x00002000, MT_DEVICE },
};

static void __init aximx5_map_io(void)
{
	pxa_map_io();

	/* Configure power management stuff. */
	PGSR0 = GPSRx_SleepValue;
	PGSR1 = GPSRy_SleepValue;
	PGSR2 = GPSRz_SleepValue;

	/* Wake up on CF/SD card insertion, Power and Record buttons,
	   AC adapter plug/unplug */
	PWER = PWER_GPIO0 | PWER_GPIO6 | PWER_GPIO7 | PWER_GPIO10
	     | PWER_RTC | PWER_GPIO4;
	PFER = PWER_GPIO0 | PWER_GPIO4 | PWER_RTC;
	PRER = PWER_GPIO4 | PWER_GPIO10;
	PCFR = PCFR_OPDE;

	/* Set up GPIO direction and alternate function registers */
	GAFR0_L = GAFR0x_InitValue;
	GAFR0_U = GAFR1x_InitValue;
	GAFR1_L = GAFR0y_InitValue;
	GAFR1_U = GAFR1y_InitValue;
	GAFR2_L = GAFR0z_InitValue;
	GAFR2_U = GAFR1z_InitValue;

	GPDR0 = GPDRx_InitValue;
	GPDR1 = GPDRy_InitValue;
	GPDR2 = GPDRz_InitValue;

	GPSR0 = GPSRx_InitValue;
	GPSR1 = GPSRy_InitValue;
	GPSR2 = GPSRz_InitValue;

	GPCR0 = ~GPSRx_InitValue;
	GPCR1 = ~GPSRy_InitValue;
	GPCR2 = ~GPSRz_InitValue;

	/* If serial cable is connected, enable RS-232 transceiver power
	   (transceiver is powered via the DCD line) */
	if (!GET_AXIMX5_GPIO (CRADLE_DETECT_N) &&
	    AXIMX5_CONNECTOR_IS_SERIAL (AXIMX5_CONNECTOR_TYPE))
		SET_AXIMX5_GPIO (RS232_DCD, 1);

	/* Enable SIR on ttyS2 (STUART) */
	iotable_init (aximx5_io_desc, ARRAY_SIZE(aximx5_io_desc));
	mq_regs = (struct mediaq11xx_regs *)0xf3800000;
	pxa_set_stuart_info(&aximx5_serial_funcs);
#ifdef EARLY_SIR_CONSOLE
	aximx5_irda_configure (1);
	aximx5_irda_set_txrx (PXA_SERIAL_TX);
#endif
}

static void __init aximx5_init (void)
{
}

MACHINE_START(AXIMX5, "Dell Axim X5")
	/* Maintainer Martin Demin <demo@twincar.sk> Andrew Zabolotny <zap@homelink.ru> */
	.phys_io	= 0x40000000,
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
	.boot_params	= 0xa0000100,
	.map_io		= aximx5_map_io,
	.init_irq	= pxa_init_irq,
        .timer		= &pxa_timer,
	.init_machine	= aximx5_init,
MACHINE_END

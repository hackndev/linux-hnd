/*
 *  Generic I2S code for pxa25x
 *
 *  Phil Blundell <pb@nexus.co.uk>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  2004-06	modified by Giorgio Padrin
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/sound.h>
#include <linux/soundcard.h>

#include <asm/semaphore.h>
#include <asm/uaccess.h>
#include <asm/hardware.h>
#include <asm/dma.h>
#include <asm/mach-types.h>

#if CONFIG_MACH_H4700
#include <asm/arch/hx4700-gpio.h>
#endif

#include <asm/arch/pxa-regs.h>
#include "pxa-i2s.h"

#define GPIO32_SYSCLK_FREQ	147460000
#define PXA_I2S_48KHZ		48000
#define PXA_I2S_44_1KHZ		44308
#define PXA_I2S_22_05KHZ	22154
#define PXA_I2S_16KHZ		16000
#define PXA_I2S_11_025KHZ	11077
#define PXA_I2S_8KHZ		8000

int
pxa_i2s_sample_rate_divisor(unsigned int sample_rate)
{
	if (sample_rate != PXA_I2S_48KHZ
		&& sample_rate != PXA_I2S_44_1KHZ
		&& sample_rate != PXA_I2S_22_05KHZ
		&& sample_rate != PXA_I2S_16KHZ
		&& sample_rate != PXA_I2S_11_025KHZ
		&& sample_rate != PXA_I2S_8KHZ)
	{
		printk("pxa_i2s_sample_rate_divisor: rate %d invalid\n", sample_rate);
		return -EINVAL;
	}

	return GPIO32_SYSCLK_FREQ/(sample_rate * 256);
}

void
pxa_i2s_init (void)
{
	unsigned long flags;

	/* Setup the uarts */
	local_irq_save(flags);

	pxa_gpio_mode(GPIO28_BITCLK_OUT_I2S_MD);
	pxa_gpio_mode(GPIO29_SDATA_IN_I2S_MD);
	pxa_gpio_mode(GPIO30_SDATA_OUT_I2S_MD);
	pxa_gpio_mode(GPIO31_SYNC_I2S_MD);
#if CONFIG_MACH_H4700
	if (machine_is_h4700())
		pxa_gpio_mode(GPIO_NR_HX4700_I2S_SYSCLK);
	else
#endif
		pxa_gpio_mode(GPIO32_SYSCLK_I2S_MD);

	/* enable the clock to I2S unit */
	CKEN |= CKEN8_I2S;
	SACR0 = SACR0_RST;
	SACR0 = SACR0_ENB | SACR0_BCKD | SACR0_RFTH(8) | SACR0_TFTH(8);
	SACR1 = 0;
	SADIV = pxa_i2s_sample_rate_divisor(PXA_I2S_44_1KHZ);
	SACR0 |= SACR0_ENB;

 	local_irq_restore(flags);
}

void
pxa_i2s_shutdown (void)
{
	SACR0 = SACR0_RST;
	SACR1 = 0;
}

int
pxa_i2s_set_samplerate (long val)
{
	int clk_div = 0, fs = 0;

	/* wait for any frame to complete */
	udelay(125);

	if (val >= 48000)
		val = PXA_I2S_48KHZ;
	else if (val >= 44100)
		val = PXA_I2S_44_1KHZ;
	else if (val >= 22050)
		val = PXA_I2S_22_05KHZ;
	else if (val >= 16000)
		val = PXA_I2S_16KHZ;
	else
		val = PXA_I2S_8KHZ;

	/* Select the clock divisor */
	fs = 256;
	clk_div = pxa_i2s_sample_rate_divisor(val);

	SADIV = clk_div;

	return fs;
}

audio_stream_t pxa_i2s_output_stream = {
	name:			"I2S audio out",
	dcmd:			(DCMD_INCSRCADDR|DCMD_FLOWTRG|DCMD_BURST32|DCMD_WIDTH4),
	drcmr:			&DRCMRTXSADR,
	dev_addr:		__PREG(SADR),
};

audio_stream_t pxa_i2s_input_stream = {
	name:			"I2S audio in",
	dcmd:			(DCMD_INCTRGADDR|DCMD_FLOWSRC|DCMD_BURST32|DCMD_WIDTH4),
	drcmr:			&DRCMRRXSADR,
	dev_addr:		__PREG(SADR),
};

EXPORT_SYMBOL (pxa_i2s_init);
EXPORT_SYMBOL (pxa_i2s_shutdown);
EXPORT_SYMBOL (pxa_i2s_set_samplerate);
EXPORT_SYMBOL (pxa_i2s_output_stream);
EXPORT_SYMBOL (pxa_i2s_input_stream);
MODULE_LICENSE("GPL");

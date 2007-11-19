/* sound/arm/s3c24xx-iis.c
 *
 * Copyright (c) 2004-2005 Simtec Electronics
 *	http://armlinux.simtec.co.uk/
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * S3C24XX ALSA IIS audio driver core
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#define DEBUG

/* Notes:
 *   The I2S pins that are needed (SCLK,LRCLK,SDO,SDI) are always
 *   configured when the I2S system is active. SDO and SDI are
 *   always configured even if the system is not open for both
 *   capture and playback. This ensures that if the DAC is
 *   not muted, then the output line is held in place, reducing
 *   the chance of any random data being picked up. CDCLK is
 *   only selected when the system is in master mode.
 *
 *   When the I2S unit is not being used, all pins are set to
 *   input, so either the internal resistors need to be configured
 *   or the lines need to have external pull-up or pull-downs.
*/

#include <sound/driver.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/pm.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>

#include <asm/hardware.h>
#include <asm/dma.h>
#include <asm/io.h>

#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/pcm_params.h>

#include <asm/arch/regs-iis.h>
#include <asm/arch/regs-gpio.h>
#include <asm/arch/audio.h>

#include "s3c24xx-iis.h"

static struct s3c2410_dma_client s3c24xx_snd_playback_client = {
	.name		= "S3C24XX Audio Playback",
};

static struct s3c2410_dma_client s3c24xx_snd_capture_client = {
	.name		= "S3C24XX Audio Capture",
};


/* debugging macros */

#ifndef CONFIG_SND_S3C2410_DBG
#define DBG(x...) do { } while(0)
#else
#define DBG(x...) do { printk(KERN_DEBUG x); } while(0)
#endif

#ifndef CONFIG_SND_S3C2410_DBG_DMA
#define DBG_DMA(x...) do { } while(0)
#else
#define DBG_DMA(x...) do { printk(KERN_DEBUG x); } while(0)
#endif

static s3c24xx_card_t *our_card;
static struct s3c24xx_iis_drv *reg_drv;

/* s3c24xx_snd_capture_hw
 *
 * the default initialiser for the capture hardware stream,
 * not to be altered
*/

static struct snd_pcm_hardware s3c24xx_snd_hw = {
	.info			= ( SNDRV_PCM_INFO_INTERLEAVED |
				    SNDRV_PCM_INFO_BLOCK_TRANSFER |
				    SNDRV_PCM_INFO_MMAP | 
				    SNDRV_PCM_INFO_MMAP_VALID |
				    0),
	.formats		= ( SNDRV_PCM_FMTBIT_S16_LE |
				    SNDRV_PCM_FMTBIT_U16_LE |
				    SNDRV_PCM_FMTBIT_U8 |
				    SNDRV_PCM_FMTBIT_S8 ),
	.rates			= SNDRV_PCM_RATE_44100,
	.rate_min		= 8000,
	.rate_max		= 48000,
	.channels_min		= 2,
	.channels_max		= 2,
	.buffer_bytes_max	= 128*1024,
	.period_bytes_min	= PAGE_SIZE,
	.period_bytes_max	= PAGE_SIZE*2,
	.periods_min		= 2,
	.periods_max		= 128,
	.fifo_size		= 32,
};

/* debug helpers */

static void s3c24xx_snd_showregs(s3c24xx_card_t *card)
{
	printk(KERN_DEBUG "IIS: CON=%08x, MOD=%08x, PSR=%08x, FCON=%08x\n",
	       readl(card->regs + S3C2410_IISCON),
	       readl(card->regs + S3C2410_IISMOD),
	       readl(card->regs + S3C2410_IISPSR),
	       readl(card->regs + S3C2410_IISFCON));
}

static void s3c24xx_snd_showdma(s3c24xx_runtime_t *or)
{
	dma_addr_t src, dst;

	s3c2410_dma_getposition(or->dma_ch, &src, &dst);
	DBG("dma position: src=0x%lx, dst=0x%lx\n", (long)src, (long)dst);
}

/* conversion */

static struct s3c24xx_platdata_iis *to_platdata(s3c24xx_card_t *card)
{
	return (card->device == NULL) ? NULL : card->device->platform_data;
}

/* helpers */

static inline int s3c24xx_snd_is_clkmaster(s3c24xx_card_t *chip)
{
	return (readl(chip->regs + S3C2410_IISMOD) & S3C2410_IISMOD_SLAVE) ? 0:1;
}

/* audio hardware control */

static void s3c24xx_snd_txctrl(s3c24xx_card_t *card, int on)
{
	unsigned long iisfcon;
	unsigned long iiscon;
	unsigned long iismod;

	iisfcon = readl(card->regs + S3C2410_IISFCON);
	iiscon  = readl(card->regs + S3C2410_IISCON);
	iismod  = readl(card->regs + S3C2410_IISMOD);

	if (on) {
		iisfcon |= S3C2410_IISFCON_TXDMA | S3C2410_IISFCON_TXENABLE;
		iiscon  |= S3C2410_IISCON_TXDMAEN | S3C2410_IISCON_IISEN;
		iiscon  &= ~S3C2410_IISCON_TXIDLE;
		iismod  |= S3C2410_IISMOD_TXMODE;
		iismod  |= S3C2410_IISMOD_RXMODE;

		writel(iismod,  card->regs + S3C2410_IISMOD);
		writel(iisfcon, card->regs + S3C2410_IISFCON);
		writel(iiscon,  card->regs + S3C2410_IISCON);
	} else {
		/* note, we have to disable the FIFOs otherwise bad things
		 * seem to happen when the DMA stops. According to the 
		 * Samsung supplied kernel, this should allow the DMA 
		 * engine and FIFOs to reset. If this isn't allowed, the
		 * DMA engine will simply freeze randomly.
                 */

		iisfcon &= ~S3C2410_IISFCON_TXENABLE;
		iisfcon &= ~S3C2410_IISFCON_TXDMA;
		iiscon  |=  S3C2410_IISCON_TXIDLE;
		iiscon  &= ~S3C2410_IISCON_TXDMAEN;
		//iismod  &= ~S3C2410_IISMOD_TXMODE;

		writel(iiscon,  card->regs + S3C2410_IISCON);
		writel(iisfcon, card->regs + S3C2410_IISFCON);
		//writel(iismod,  card->regs + S3C2410_IISMOD);
	}

	DBG("%s: iismod=0x%08lx, iisfcon=0x%08lx, iiscon=0x%08lx\n",
	    __FUNCTION__, iismod, iisfcon, iiscon);
}

static void s3c24xx_snd_rxctrl(s3c24xx_card_t *card, int on)
{
	unsigned long iisfcon;
	unsigned long iiscon;
	unsigned long iismod;

	iisfcon = readl(card->regs + S3C2410_IISFCON);
	iiscon  = readl(card->regs + S3C2410_IISCON);
	iismod  = readl(card->regs + S3C2410_IISMOD);

	if (on) {
		iisfcon |= S3C2410_IISFCON_RXDMA | S3C2410_IISFCON_RXENABLE;
		iiscon  |= S3C2410_IISCON_RXDMAEN | S3C2410_IISCON_IISEN;
		iiscon  &= ~S3C2410_IISCON_RXIDLE;
		iismod  |= S3C2410_IISMOD_RXMODE;
		iismod  |= S3C2410_IISMOD_TXMODE;

		writel(iismod,  card->regs + S3C2410_IISMOD);
		writel(iisfcon, card->regs + S3C2410_IISFCON);
		writel(iiscon,  card->regs + S3C2410_IISCON);
	} else {
                /* note, we have to disable the FIFOs otherwise bad things
                 * seem to happen when the DMA stops. According to the 
                 * Samsung supplied kernel, this should allow the DMA 
                 * engine and FIFOs to reset. If this isn't allowed, the
                 * DMA engine will simply freeze randomly.
                 */

                iisfcon &= ~S3C2410_IISFCON_RXENABLE;
                iisfcon &= ~S3C2410_IISFCON_RXDMA;
                iiscon  |= S3C2410_IISCON_RXIDLE;
                iiscon  &= ~S3C2410_IISCON_RXDMAEN;
		//iismod  &= ~S3C2410_IISMOD_RXMODE;

		writel(iisfcon, card->regs + S3C2410_IISFCON);
		writel(iiscon,  card->regs + S3C2410_IISCON);
		//writel(iismod,  card->regs + S3C2410_IISMOD);
	}

	DBG("%s: iismod=0x%08lx, iisfcon=0x%08lx, iiscon=0x%08lx\n",
	    __FUNCTION__, iismod, iisfcon, iiscon);
}

int s3c24xx_iis_cfgclksrc(s3c24xx_card_t *card, const char *name)
{
	struct s3c24xx_platdata_iis *pdata = to_platdata(card);
	unsigned long iismod;

	if (name == NULL && pdata != NULL)
		name = pdata->codec_clk;

	if (name == NULL)
		return -EINVAL;

	DBG("%s: clock source %s\n", __FUNCTION__, name);

	/* alter clock source (master/slave) appropriately */

	iismod = readl(card->regs + S3C2410_IISMOD);

	if (strcmp(name, "pclk") == 0 || strcmp(name, "cdclk") == 0) {
		iismod &= ~S3C2410_IISMOD_SLAVE;		
	} else {
		iismod |= S3C2410_IISMOD_SLAVE;
	}

	writel(iismod, card->regs + S3C2410_IISMOD);
	return 0;
}

EXPORT_SYMBOL(s3c24xx_iis_cfgclksrc);

/* s3c24xx_pcm_enqueue
 *
 * place a dma buffer onto the queue for the dma system
 * to handle.
*/

static void s3c24xx_pcm_enqueue(s3c24xx_runtime_t *or)
{
	dma_addr_t pos = or->dma_pos;
	int ret;

	if (0) {
		DBG_DMA("%s: pos=%lx, period=%x, end=%lx, loaded=%d\n", __FUNCTION__,
			(long)or->dma_start, or->dma_period,
			(long)or->dma_end, or->dma_loaded);
	}

	while ( or->dma_loaded < or->dma_limit) {
		unsigned long len = or->dma_period;

		//DBG_DMA("%s: load buffer %lx\n", __FUNCTION__, (long)pos);
		
		// check for the end of the buffer, in case we only need
		// to partially load

		if ((pos + len) > or->dma_end) {
			len  = or->dma_end - pos;
			printk(KERN_DEBUG "%s: corrected dma len %ld\n",
			       __FUNCTION__, len);
		}

		ret = s3c2410_dma_enqueue(or->dma_ch, or, pos, len);

		if (ret == 0) {
			or->dma_loaded++;
			pos += or->dma_period;
			if (pos >= or->dma_end)
				pos = or->dma_start;
		} else
			break;
	}

	or->dma_pos = pos;
}

static int in_buffdone;

static void s3c24xx_snd_playback_buffdone(struct s3c2410_dma_chan *ch,
					  void *bufid, int size,
					  enum s3c2410_dma_buffresult result)
{
	s3c24xx_runtime_t *or = bufid;

	DBG_DMA("%s: %p,%p, or %p, sz %d, res %d\n", 
		__FUNCTION__, ch, bufid, or, size, result);

	if (result == S3C2410_RES_ABORT || result == S3C2410_RES_ERR)
		return;

	in_buffdone = 1;

	if (or->stream)
		snd_pcm_period_elapsed(or->stream);

	spin_lock(&or->lock);
	if (or->state & ST_RUNNING) {
		or->dma_loaded--;
		s3c24xx_pcm_enqueue(or);
	}


	in_buffdone = 0;

	spin_unlock(&or->lock);
}

static void s3c24xx_snd_capture_buffdone(struct s3c2410_dma_chan *ch,
					 void *bufid, int size,
					 enum s3c2410_dma_buffresult result)
{
	s3c24xx_runtime_t *or = bufid;

	DBG_DMA("%s: %p,%p, or %p, sz %d, res %d\n", 
		__FUNCTION__, ch, bufid, or, size, result);

	if (result == S3C2410_RES_ABORT || result == S3C2410_RES_ERR)
		return;

	if (or->stream)
		snd_pcm_period_elapsed(or->stream);

	spin_lock(&or->lock);
	if (or->state & ST_RUNNING) {
		or->dma_loaded--;
		s3c24xx_pcm_enqueue(or);
	}
	spin_unlock(&or->lock);
}



static int call_startup(struct s3c24xx_iis_usr *usr)
{
	struct s3c24xx_iis_ops *ops = usr->ops;

	if (ops && ops->startup)
		return (ops->startup)(usr->pw);

	return 0;
}

static int call_open(struct s3c24xx_iis_usr *usr,
		     struct snd_pcm_substream *substream)
{
	struct s3c24xx_iis_ops *ops = usr->ops;

	if (ops && ops->open)
		return (ops->open)(usr->pw, substream);

	return 0;
}

static int s3c24xx_snd_open(struct snd_pcm_substream *substream)
{
	s3c24xx_card_t *chip = snd_pcm_substream_chip(substream);
	s3c24xx_runtime_t *or;
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret = 0;

	DBG("%s: substream=%p, chip=%p\n", __FUNCTION__, substream, chip);

	runtime->private_data = &chip->playback;
	runtime->hw	      = s3c24xx_snd_hw;

	down(&chip->sem);

	/* todo - request dma channel nicely */

	DBG("%s: state: playback 0x%x, capture 0x%x\n",
	    __FUNCTION__, chip->playback.state, chip->capture.state);

	if (chip->playback.state == 0 && chip->capture.state == 0) {

		/* ensure we hold all the modules we need */

		if (chip->base.ops) {
			if (try_module_get(chip->base.ops->owner))
				chip->base.ops_claimed = 1;
			else {
				dev_err(chip->device, "cannot claim module\n");
				ret = -EINVAL;
				goto exit_err;
			}
		}

		if (chip->chip.ops) {
			if (try_module_get(chip->chip.ops->owner))
				chip->chip.ops_claimed = 1;
			else {
				dev_err(chip->device, "cannot claim module\n");
				ret = -EINVAL;
				goto exit_err;
			}
		}

		/* ensure the chip is started */

		ret = call_startup(&chip->base);
		if (ret)
			goto exit_err;

		ret = call_startup(&chip->chip);
		if (ret)
			goto exit_err;

		/* configure the pins for audio (iis) control */

		s3c2410_gpio_cfgpin(S3C2410_GPE0, S3C2410_GPE0_I2SLRCK);
		s3c2410_gpio_cfgpin(S3C2410_GPE1, S3C2410_GPE1_I2SSCLK);
		s3c2410_gpio_cfgpin(S3C2410_GPE3, S3C2410_GPE3_I2SSDI);
		s3c2410_gpio_cfgpin(S3C2410_GPE4, S3C2410_GPE4_I2SSDO);
		
		if (s3c24xx_snd_is_clkmaster(chip) || chip->output_cdclk) {
			s3c2410_gpio_cfgpin(S3C2410_GPE2, S3C2410_GPE2_CDCLK);
			
			/* todo - configure our clock rates */
		} else {
			/* leave GPE2 alone, probably being used elsewhere */
		}

		if (chip->output_cdclk) {
			writel((0 << S3C2410_IISPSR_EXTSHFIT) |
			       (0 << S3C2410_IISPSR_INTSHIFT),
			       chip->regs + S3C2410_IISPSR);
		}
	}

	/* initialise the stream */

	runtime->hw = s3c24xx_snd_hw;

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		or = &chip->capture;
	else
		or = &chip->playback;

	/* call all the registered helpers */

	ret = call_open(&chip->base, substream);
	if (ret < 0)
		goto exit_err;

	ret = call_open(&chip->chip, substream);
	if (ret < 0)
		goto exit_err;

	or->state |= ST_OPENED;

	runtime->private_data = or;
	or->stream = substream;

	up(&chip->sem);
	return ret;

 exit_err:
	up(&chip->sem);
	return ret;
}


static void call_shutdown(struct s3c24xx_iis_usr *usr)
{
	struct s3c24xx_iis_ops *ops = usr->ops;

	if (ops && ops->shutdown)
		(ops->shutdown)(usr->pw);
}

static int call_close(struct s3c24xx_iis_usr *usr, 
		      struct snd_pcm_substream *substream)
{
	struct s3c24xx_iis_ops *ops = usr->ops;

	if (ops && ops->close)
		return (ops->close)(usr->pw, substream);
	
	return 0;
}

/* close callback */
static int s3c24xx_snd_close(struct snd_pcm_substream *substream)
{
	s3c24xx_card_t *chip = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	s3c24xx_runtime_t *or = runtime->private_data;

	DBG("%s: substream=%p, chip=%p\n", __FUNCTION__, substream, chip);
		
	down(&chip->sem); 

	snd_assert(or->state != 0, chip = chip);

	/* mark stream as closed */
	or->state &= ~ST_OPENED;

	/* close all the devices associated with this */
	call_close(&chip->chip, substream);
	call_close(&chip->base, substream);

	/* are both shut-down? */
	if (chip->playback.state == 0 && chip->capture.state == 0) {
		/* call shut-down methods for all */
				
		call_shutdown(&chip->chip);
		call_shutdown(&chip->base);

		/* de-configure the IIS pins, go to input */

		s3c2410_gpio_cfgpin(S3C2410_GPE0, S3C2410_GPE0_INP);
		s3c2410_gpio_cfgpin(S3C2410_GPE1, S3C2410_GPE1_INP);
		
		if (s3c24xx_snd_is_clkmaster(chip) && !chip->output_cdclk)
			s3c2410_gpio_cfgpin(S3C2410_GPE2, S3C2410_GPE2_INP);
		
		s3c2410_gpio_cfgpin(S3C2410_GPE3, S3C2410_GPE3_INP);
		s3c2410_gpio_cfgpin(S3C2410_GPE4, S3C2410_GPE4_INP);

		/* free any hold we had on the modules */

		if (chip->base.ops_claimed) {
			chip->base.ops_claimed = 0;
			module_put(chip->base.ops->owner);
		}

		if (chip->chip.ops_claimed) {
			chip->chip.ops_claimed = 0;
			module_put(chip->chip.ops->owner);
		}
	}
	
	up(&chip->sem);
	return 0;
}


/* hw_params callback */
static int s3c24xx_snd_pcm_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *hw_params)

{
	s3c24xx_runtime_t *or = substream->runtime->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned long totbytes = params_buffer_bytes(hw_params); 
	unsigned long flags;

	DBG("%s: sub=%p, hwp=%p\n", __FUNCTION__, substream, hw_params);

	/* prepare DMA */

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	runtime->dma_bytes = totbytes;

	spin_lock_irqsave(&or->lock, flags);
	or->dma_loaded	= 0;
	or->dma_limit	= runtime->hw.periods_min;
	or->dma_period	= params_period_bytes(hw_params);
	or->dma_start	= runtime->dma_addr;
	or->dma_pos	= or->dma_start;
	or->dma_end	= or->dma_start + totbytes;
	spin_unlock_irqrestore(&or->lock, flags);

	DBG("%s: or->dma_period = %ld\n", __FUNCTION__, or->dma_period);
	
	DBG("%s: runtime->dma_bytes=%d\n", __FUNCTION__, runtime->dma_bytes);
	DBG("%s: runtime->dma_addr=%lx\n", __FUNCTION__, runtime->dma_addr);

	return 0;
}

/* hw_free callback */
static int s3c24xx_snd_pcm_hw_free(struct snd_pcm_substream *substream)
{
	DBG("%s: substream %p\n", __FUNCTION__, substream);

	// TODO - do we need to ensure DMA flushed
	snd_pcm_set_runtime_buffer(substream, NULL);
	return 0;
}

/* frequency control */

static long s3c24xx_snd_getdiv(s3c24xx_card_t *card, unsigned int rate,
			       unsigned long *iismod, unsigned long iisdiv)
{
	unsigned long clkrate = clk_get_rate(card->clock);
	long div;
	long diff;

	if (iisdiv & S3C2410_IISMOD_384FS)
		clkrate /= 384;
	else
		clkrate /= 256;

	div = clkrate / rate;

	/* is divisor in range */

	if (div > 32 || div < 1)
		return -1L;

	/* is the rate generated near enough our clock rate? */

	diff = (clkrate / div) - rate;

	if (diff > 100 || diff < 100)
		return -1L;
	       
	/* update info and return */

	*iismod &= ~S3C2410_IISMOD_384FS;
	*iismod |= iisdiv;

	return (div-1) << S3C2410_IISPSR_INTSHIFT;
}

/* s3c24xx_snd_setrate
 *
 * configure the clock rate for when the s3c24xx is in clock master
 * mode.
*/

static int s3c24xx_snd_setrate(s3c24xx_card_t *chip, struct snd_pcm_runtime *run)
{
	unsigned long iismod = readl(chip->regs + S3C2410_IISMOD);
	unsigned long rate;
	unsigned long tmp;
	long prescaler;

	/* if this is set to slave mode, then our clocks are all
	 * inputs anyway, so we cannot alter the frequency here */

	if (iismod & S3C2410_IISMOD_SLAVE) {
		printk(KERN_ERR "%s: called with IISMOD as slave\n",
		       __FUNCTION__);
		return -EINVAL;
	}

	if (iismod & S3C2410_IISMOD_16BIT)
		rate = run->rate * 32;
	else
		rate = run->rate * 16;


	DBG("%s: clock rate %ld\n", __FUNCTION__, rate);

	prescaler = s3c24xx_snd_getdiv(chip, rate, &iismod,
				       S3C2410_IISMOD_256FS);
	if (prescaler < 0)
		prescaler = s3c24xx_snd_getdiv(chip, rate, &iismod,
					       S3C2410_IISMOD_384FS);

	if (prescaler < 0) {
		printk(KERN_ERR "cannot get rate %d Hz\n", run->rate);
		return -ERANGE;
	}

	/* update timing registers */

	writel(iismod, chip->regs + S3C2410_IISMOD);

	tmp = readl(chip->regs + S3C2410_IISPSR) & ~S3C2410_IISPSR_INTMASK;
	tmp |= prescaler;
	writel(tmp, chip->regs + S3C2410_IISPSR);

	return 0;
}

EXPORT_SYMBOL(s3c24xx_snd_setrate);

/* s3c24xx_iismod_cfg
 *
 * Update the s3c24xx IISMOD register
*/

void s3c24xx_iismod_cfg(s3c24xx_card_t *chip,
			unsigned long set,
			unsigned long mask)
{
	unsigned long flags;
	unsigned long tmp;

	local_irq_save(flags);

	tmp = readl(chip->regs + S3C2410_IISMOD);
	tmp &= ~mask;
	tmp |= set;

	DBG("%s: new iismod 0x%08lx\n", __FUNCTION__, tmp);

	writel(tmp, chip->regs + S3C2410_IISMOD);
	local_irq_restore(flags);
}

EXPORT_SYMBOL(s3c24xx_iismod_cfg);

static int call_prepare(struct s3c24xx_iis_usr *usr,
			struct snd_pcm_substream *substream,
			struct snd_pcm_runtime *rt)
{
	struct s3c24xx_iis_ops *ops = usr->ops;

	if (ops && ops->prepare)
		return (ops->prepare)(usr->pw, substream, rt);

	return 0;
}

/* prepare callback */

static int s3c24xx_snd_pcm_prepare(struct snd_pcm_substream *substream)
{
	s3c24xx_card_t *chip = snd_pcm_substream_chip(substream);
	s3c24xx_runtime_t *or = substream->runtime->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret = 0;

	DBG("%s: substream=%p, chip=%p\n", __FUNCTION__, substream, chip);
	DBG("%s: r->dma_addr=%lx, r->dma_area=%p\n", __FUNCTION__,
	    (long)runtime->dma_addr, runtime->dma_area);

	DBG("%s: dma: period=%d, start=%lx, pos=%lx, end=%lx\n", __FUNCTION__,
	    or->dma_period, (long)or->dma_start,
	    (long)or->dma_pos, (long)or->dma_end);
	
	/* flush the DMA channel */

	s3c2410_dma_ctrl(or->dma_ch, S3C2410_DMAOP_FLUSH);
	or->dma_loaded = 0;
	or->dma_pos = or->dma_start;

	/* call the register ops for prepare, to allow things like
	 * sample rate and format to be set
	 */

	ret = call_prepare(&chip->base, substream, runtime);
	if (ret < 0) {
		dev_err(chip->device, "prepare base failed %d\n", ret);
		goto exit_err;
	}

	ret = call_prepare(&chip->chip, substream, runtime);
	if (ret < 0) {
		dev_err(chip->device, "prepare chip failed %d\n", ret);
		goto exit_err;
	}

	/* enqueue dma buffers */
	s3c24xx_pcm_enqueue(or);

 exit_err:
	DBG("%s: returning %d\n", __FUNCTION__, ret);
	return ret;
}

/* s3c24xx_snd_lrsync
 *
 * Wait for the LR signal to allow synchronisation to the L/R clock
 * from the DAC. May only be needed for slave mode.
*/

static int s3c24xx_snd_lrsync(s3c24xx_card_t *chip)
{
	unsigned long iiscon;
	int timeout = 10000;

	while (1) {
		iiscon = readl(chip->regs + S3C2410_IISCON);
		if (iiscon & S3C2410_IISCON_LRINDEX)
			break;

		if (--timeout < 0)
			return -ETIMEDOUT;
	}

	DBG("%s: timeout at end %d\n", __FUNCTION__, timeout);

	return 0;
}

/* trigger callback */
static int s3c24xx_snd_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	s3c24xx_runtime_t *or = substream->runtime->private_data;
	s3c24xx_card_t *chip = snd_pcm_substream_chip(substream);
	unsigned long flags;
	int ret = -EINVAL;

	DBG("************************************************************\n");
	DBG("%s: substream=%p, cmd=%d\n", __FUNCTION__, substream, cmd);

	spin_lock_irqsave(&or->lock, flags);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		DBG("%s: trigger START\n", __FUNCTION__);
		
		or->state |= ST_RUNNING;

		/* TODO - check we have some enqueued buffers */

		/* start the IIS engine */

		s3c24xx_snd_showregs(chip);
		s3c2410_dma_ctrl(or->dma_ch, S3C2410_DMAOP_START);

		if (!s3c24xx_snd_is_clkmaster(chip)) {
			ret = s3c24xx_snd_lrsync(chip);
			if (ret) {
				dev_err(chip->device,
					"failed to sync to lrclk\n");
				goto exit_err;
			}
		}

		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			s3c24xx_snd_rxctrl(chip, 1);
		else
			s3c24xx_snd_txctrl(chip, 1);

		/* at this point the dma will start running, but
		 * we have a small problem that although buffers have
		 * been loaded, it is possible that only one buffer
		 * has been, which causes problems if the next dma
		 * request comes in before the irq has loaded a new
		 * buffer
		 */
		
		s3c2410_dma_ctrl(or->dma_ch, S3C2410_DMAOP_STARTED);

#if 0
		s3c24xx_snd_showregs(chip);
		s3c24xx_snd_showdma(or);	
		//msleep(1);
		udelay(100);
		s3c24xx_snd_showregs(chip);
		s3c24xx_snd_showdma(or);	
#endif

		ret = 0;
		break;

	case SNDRV_PCM_TRIGGER_STOP:
		or->state &= ~ST_RUNNING;

		DBG("%s: trigger STOP\n", __FUNCTION__);

		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			s3c24xx_snd_rxctrl(chip, 0);
		else
			s3c24xx_snd_txctrl(chip, 0);
		
		s3c2410_dma_ctrl(or->dma_ch, S3C2410_DMAOP_STOP);
		s3c24xx_snd_showdma(or);	
		s3c24xx_snd_showregs(chip);

		ret = 0;
		break;

#if 0
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		DBG("%s: trigger PAUSE_PUSH\n", __FUNCTION__);
		s3c2410_dma_ctrl(or->dma_ch, S3C2410_DMAOP_PAUSE);
		break;

	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		DBG("%s: trigger RESUME\n", __FUNCTION__);
		s3c2410_dma_ctrl(or->dma_ch, S3C2410_DMAOP_RESUME);
		break;
#endif

	default:
		ret = -EINVAL;
	}

 exit_err:
	DBG("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n");

	spin_unlock_irqrestore(&or->lock, flags);
	return ret;
}


/* pointer callback */
static snd_pcm_uframes_t s3c24xx_snd_pcm_pointer(struct snd_pcm_substream *substream)
{
	s3c24xx_runtime_t *or = substream->runtime->private_data;
	unsigned long flags;
	unsigned long res;
	dma_addr_t src, dst;
	snd_pcm_uframes_t ret;

	spin_lock_irqsave(&or->lock, flags);
	s3c2410_dma_getposition(or->dma_ch, &src, &dst);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		res = dst - or->dma_start;
	else
		res = src - or->dma_start;

	spin_unlock_irqrestore(&or->lock, flags);

	/* we seem to be getting the odd error from the pcm library due
	 * to out-of-bounds pointers. this is maybe due to the dma engine
	 * not having loaded the new values for the channel before being
	 * callled... (todo - fix )
	 */

	if (res > (or->dma_end - or->dma_start))
		DBG("%s: %lx,%lx (%lx) from %lx,%lx\n", __FUNCTION__,
		    (long)src, (long)dst, res,
		    (long)or->dma_start, (long)or->dma_end);

	if (res >= snd_pcm_lib_buffer_bytes(substream)) {
		DBG("%s: %lx,%lx (%lx) from %lx,%lx\n", __FUNCTION__,
		    (long)src, (long)dst, res,
		    (long)or->dma_start, (long)or->dma_end);
		
		if (res > snd_pcm_lib_buffer_bytes(substream))
			DBG("%s: res %lx >= %lx\n", __FUNCTION__,
			    res, (long)snd_pcm_lib_buffer_bytes(substream));
		
		if (res == snd_pcm_lib_buffer_bytes(substream))
			res = 0;
	}

	ret = bytes_to_frames(substream->runtime, res);

	if (in_buffdone) {
		//ret &= ~((PAGE_SIZE/4)-1);
		//ret -= 4;
	}

	//ret -= 1;

#if 0
	DBG("%s: %lx,%lx: result %ld, returning %d\n",
	    __FUNCTION__, (long)src, (long)dst, res, ret);
#endif

	return ret;
}

static int s3c24xx_snd_pcm_mmap(struct snd_pcm_substream *substream,
				struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret;

	ret = dma_mmap_writecombine(substream->pcm->card->dev, vma,
				     runtime->dma_area,
				     runtime->dma_addr,
				     runtime->dma_bytes);

	printk("%s: dev=%p, vma=%p, rtime: %p, %08x, %d: %d\n",
	       __FUNCTION__, substream->pcm->card->dev, vma,
	       runtime->dma_area,
	       runtime->dma_addr,
	       runtime->dma_bytes, ret);
	       
	return ret;
}

static int sc24xx_snd_dma_prealloc(snd_pcm_t *pcm, int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size = s3c24xx_snd_hw.buffer_bytes_max;

	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;

	buf->area = dma_alloc_writecombine(pcm->card->dev, size,
					   &buf->addr, GFP_KERNEL);

	if (buf->area == NULL) {
		dev_err(pcm->card->dev, "failed to allocate dma buff\n");
		return -ENOMEM;
	}

	DBG("%s: stream %d: dma area %p, 0x%lx\n", __FUNCTION__,
	    stream, buf->area, (long)buf->addr);

	return 0;
}

static void sc24xx_snd_dma_free(snd_pcm_t *pcm, int stream)
{
	struct snd_pcm_substream*substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size = s3c24xx_snd_hw.buffer_bytes_max;

	dma_free_writecombine(pcm->card->dev, size, buf->area, buf->addr);

	buf->area = NULL;
	buf->addr = (dma_addr_t)NULL;
}

/* operators */
static snd_pcm_ops_t s3c24xx_snd_pcm_ops = {
	.open		= s3c24xx_snd_open,
	.close		= s3c24xx_snd_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= s3c24xx_snd_pcm_hw_params,
	.hw_free	= s3c24xx_snd_pcm_hw_free,
	.prepare	= s3c24xx_snd_pcm_prepare,
	.trigger	= s3c24xx_snd_pcm_trigger,
	.pointer	= s3c24xx_snd_pcm_pointer,
	.mmap		= s3c24xx_snd_pcm_mmap,
};

static int snd_card_s3c24xx_pcm(s3c24xx_card_t *card)
{
	snd_pcm_t *pcm;
	int err;

	DBG("snd_card_s3c24xx_pcm: card=%p\n", card);

	err = snd_pcm_new(card->card, "S3C24XX-IIS", 0, 1, 1, &pcm);
	if (err < 0) {
		printk(KERN_ERR "cannot register new pcm");
		return err;
	}

	DBG("snd_card_s3c24xx_pcm: new pcm %p\n", pcm);

	card->pcm	  = pcm;
	pcm->private_data = card;

	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &s3c24xx_snd_pcm_ops);
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &s3c24xx_snd_pcm_ops);

	/* allocate dma resources */

	err = sc24xx_snd_dma_prealloc(pcm, SNDRV_PCM_STREAM_PLAYBACK);
	if (err)
		goto exit_err;

	err = sc24xx_snd_dma_prealloc(pcm, SNDRV_PCM_STREAM_CAPTURE);
	if (err)
		goto exit_err;

	strcpy(pcm->name, "s3c24xx");

	DBG("snd_card_s3c24xx_pcm: ok\n");

	return 0;

 exit_err:
	return err;
}

static int s3c24xx_iis_free(s3c24xx_card_t *card)
{
	sc24xx_snd_dma_free(card->pcm, SNDRV_PCM_STREAM_PLAYBACK);
	sc24xx_snd_dma_free(card->pcm, SNDRV_PCM_STREAM_CAPTURE);

	return 0;
}

static int s3c24xx_iis_dma_init(s3c24xx_card_t *card)
{
	int err = 0;

	our_card->playback.dma_ch = DMACH_I2S_OUT;
	our_card->capture.dma_ch = DMACH_I2S_IN;

	err = s3c2410_dma_request(our_card->playback.dma_ch,
				  &s3c24xx_snd_playback_client, NULL);

	//err = request_dma(our_card->playback.dma_ch, "audio playback");
	if (err) {
		printk(KERN_ERR "failed to get dma channel\n");
		return err;
	}
	/* channel needs configuring for mem=>device, increment memory addr,
	 * sync to pclk, half-word transfers to the IIS-FIFO. */

#if 1
	s3c2410_dma_devconfig(our_card->playback.dma_ch,
			      S3C2410_DMASRC_MEM, 0x03,
			      S3C2410_PA_IIS + S3C2410_IISFIFO);

	s3c2410_dma_config(our_card->playback.dma_ch,
			   2, S3C2410_DCON_SYNC_PCLK|S3C2410_DCON_HANDSHAKE);
#endif

	s3c2410_dma_set_buffdone_fn(our_card->playback.dma_ch,
				    s3c24xx_snd_playback_buffdone);

	/* attach capture channel */
	err = s3c2410_dma_request(our_card->capture.dma_ch,
				  &s3c24xx_snd_capture_client, NULL);

	//err = request_dma(our_card->capture.dma_ch, "audio capture");
	if (err) {
		printk(KERN_ERR "failed to get dma channel\n");
		return err;
	}

#if 1
	s3c2410_dma_config(our_card->capture.dma_ch,
			   2, S3C2410_DCON_HANDSHAKE |S3C2410_DCON_SYNC_PCLK);

	s3c2410_dma_devconfig(our_card->capture.dma_ch,
			      S3C2410_DMASRC_HW, 0x3,
			      S3C2410_PA_IIS + S3C2410_IISFIFO);
#endif

	s3c2410_dma_set_buffdone_fn(our_card->capture.dma_ch,
				    s3c24xx_snd_capture_buffdone);

	return 0;
}

static int s3c24xx_iis_dma_free(s3c24xx_card_t *card)
{
	s3c2410_dma_free(card->capture.dma_ch, &s3c24xx_snd_capture_client);
	s3c2410_dma_free(card->playback.dma_ch, &s3c24xx_snd_playback_client);

	return 0;
}

int s3c24xx_iis_remove(struct device *dev)
{
	s3c24xx_card_t *card = dev_get_drvdata(dev);

	card = dev_get_drvdata(dev);
	if (card == NULL)
		return 0;

	if (reg_drv)
		(reg_drv->remove)(card->card);

	s3c24xx_iis_dma_free(card);
	s3c24xx_iis_free(card);

	kfree(card);
	dev_set_drvdata(dev, NULL);

	our_card = NULL;

	return 0;
}

EXPORT_SYMBOL(s3c24xx_iis_remove);

#if 1

static ssize_t s3c24xx_iis_show_regs(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	s3c24xx_card_t *our_card = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE,
			"IISMOD = %08x\n"
			"IISCON = %08x\n"
			"IISPSR = %08x\n"
			"IISFCON= %08x\n",
			readl(our_card->regs + S3C2410_IISCON),
			readl(our_card->regs + S3C2410_IISMOD),
			readl(our_card->regs + S3C2410_IISPSR),
			readl(our_card->regs + S3C2410_IISFCON));
}

static DEVICE_ATTR(regs, S_IRUGO, s3c24xx_iis_show_regs, NULL);
#endif

s3c24xx_card_t *s3c24xx_iis_probe(struct device *dev)
{
	struct s3c24xx_platdata_iis *pdata = dev->platform_data;
	snd_card_t *card;
	int err;

	dev_info(dev, "probe called\n");

	card = snd_card_new(-1, 0, THIS_MODULE, sizeof(s3c24xx_card_t));

	our_card = (s3c24xx_card_t *)card->private_data;

	if (card == NULL) {
		printk(KERN_ERR "snd_card_new() failed\n");
		err = -EINVAL;
		goto exit_err;
	}
	
	our_card->card     = card;
	our_card->device   = dev;
	our_card->base.ops = (pdata) ? pdata->ops : NULL;
	our_card->base.pw  = (pdata) ? pdata->ops_pw : NULL;

	/* get resources such as clocks and IO */

	our_card->clock = clk_get(dev, "iis");
	DBG("our_card.clock = %p\n", our_card->clock);

	if (IS_ERR(our_card->clock)) {
		err = PTR_ERR(our_card->clock);
		goto exit_err;
	}

	our_card->regs = ioremap(S3C2410_PA_IIS, 0x100);
	if (our_card->regs == NULL) {
		err = -ENXIO;
		goto exit_err;
	}

	DBG("our_card.regs at %p\n", our_card->regs);
	
	clk_enable(our_card->clock);

	init_MUTEX(&our_card->sem);
	spin_lock_init(&our_card->capture.lock);
	spin_lock_init(&our_card->playback.lock);

	/* ensure the channels are both shutdown */

	s3c24xx_snd_txctrl(our_card, 0);
	s3c24xx_snd_rxctrl(our_card, 0);

	/* static dma channel setup for the moment */

	err = s3c24xx_iis_dma_init(our_card);
	if (err)
		goto exit_err;

	/* default names for s3c2410 audio driver */

	strcpy(card->driver,    "S3C24XX");
	strcpy(card->shortname, "S3C24XX");
	strcpy(card->longname,  "Samsung S3C24XX");

	err = snd_card_s3c24xx_pcm(our_card);
	if (err) {
		printk(KERN_ERR "failed to init pcm devices\n");
		goto exit_err;
	}

	dev_info(dev, "soundcard attached ok (%p)\n", our_card);

	dev_set_drvdata(dev, our_card);
	device_create_file(dev, &dev_attr_regs);

	if (reg_drv)
		(reg_drv->probe)(our_card->card);

	return our_card;

 exit_err:
	dev_err(dev, "error initialised card (%d)\n", err);
	snd_card_free(our_card->card);
	return ERR_PTR(err);
}

EXPORT_SYMBOL(s3c24xx_iis_probe);

snd_card_t *s3c24xx_iis_getcard(struct platform_device *dev)
{
	s3c24xx_card_t *card = platform_get_drvdata(dev);

	return card ? card->card : NULL;
}

int s3c24xx_iis_drv_add(struct s3c24xx_iis_drv *drv)
{
	reg_drv = drv;

	if (our_card)
		(reg_drv->probe)(our_card->card);

	return 0;
}

EXPORT_SYMBOL(s3c24xx_iis_drv_add);

int s3c24xx_iis_drv_remove(struct s3c24xx_iis_drv *drv)
{
	if (our_card)
		(drv->remove)(our_card->card);

	reg_drv = NULL;
	return 0;
}

EXPORT_SYMBOL(s3c24xx_iis_drv_remove);

EXPORT_SYMBOL(s3c24xx_iis_getcard);

#ifdef CONFIG_PM

static int call_suspend(struct s3c24xx_iis_usr *usr)
{
	struct s3c24xx_iis_ops *ops = usr->ops;

	if (ops && ops->suspend)
		return (ops->suspend)(usr->pw);

	return 0;
}

int s3c24xx_iis_suspend(struct device *dev, pm_message_t state)
{
	s3c24xx_card_t *card = dev_get_drvdata(dev);

	if (card) {
		if (card->card->power_state == SNDRV_CTL_POWER_D3cold)
			goto exit;

		/* inform sound core that we wish to suspend */

		snd_pcm_suspend_all(card->pcm);
		snd_power_change_state(card->card, SNDRV_CTL_POWER_D3cold);

		/* inform any of our clients */

		call_suspend(&card->chip);
		call_suspend(&card->base);

		clk_disable(card->clock);
	}

 exit:
	return 0;
}

static int call_resume(struct s3c24xx_iis_usr *usr)
{
	struct s3c24xx_iis_ops *ops = usr->ops;

	if (ops && ops->resume)
		return (ops->resume)(usr->pw);

	return 0;
}

int s3c24xx_iis_resume(struct device *dev)
{
	s3c24xx_card_t *card = dev_get_drvdata(dev);

	if (card) {
		if (card->card->power_state == SNDRV_CTL_POWER_D0)
			goto exit;

		clk_enable(card->clock);

		/* inform any of our clients */

		call_resume(&card->base);
		call_resume(&card->chip);

		/* inform sound core that we wish to resume */

		snd_power_change_state(card->card, SNDRV_CTL_POWER_D0);
	}

 exit:
	return 0;
}

EXPORT_SYMBOL(s3c24xx_iis_suspend);
EXPORT_SYMBOL(s3c24xx_iis_resume);

#endif /* CONFIG_PM */

MODULE_DESCRIPTION("S3C24XX IIS Core");
MODULE_AUTHOR("Ben Dooks, <ben@simtec.co.uk>");
MODULE_LICENSE("GPL");

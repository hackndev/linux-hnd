/*
 * TI TSC2102 Common Code
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

#define DEBUG

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mfd/tsc2101.h> 
#include <linux/platform_device.h>
#include <sound/control.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/irqs.h>
#include "tsc2101.h"
#include <asm/arch/tps65010.h>

// VF
#include <linux/platform_device.h>

#include "../../sound/arm/pxa2xx-i2sound.h"

static u32 tsc2101_regread(struct tsc2101_data *devdata, int regnum)
{
	u32 reg;
	devdata->platform->send(TSC2101_READ, regnum, &reg, 1);
	return reg;
}

static void tsc2101_regwrite(struct tsc2101_data *devdata, int regnum, u32 value)
{
	u32 reg;
	devdata->platform->send(TSC2101_READ, regnum, &reg, 1);
	/*if(((regnum >> 11) & 0x0f) == 2)
	{
		printk("Changing %X: %04X to %04X\n", regnum, reg, value);
	}*/
	reg = value;
	devdata->platform->send(TSC2101_WRITE, regnum, &reg, 1);
	return;
}
/*
static int tsc2101_ts_penup(struct tsc2101_data *devdata)
{
	return !( tsc2101_regread(devdata, TSC2101_REG_STATUS) & TSC2101_STATUS_DAVAIL);
}
*/
#define TSC2101_ADC_DEFAULT (TSC2101_ADC_RES(TSC2101_ADC_RES_12BITP) | TSC2101_ADC_AVG(TSC2101_ADC_4AVG) | TSC2101_ADC_CL(TSC2101_ADC_CL_1MHZ_12BIT) | TSC2101_ADC_PV(TSC2101_ADC_PV_500us) | TSC2101_ADC_AVGFILT_MEAN) 

/*
 * 	Sound part
 */

#define LIV_MAX		0x7F
#define VOLtoTSC(x)	((x * LIV_MAX) / 100)
#define TSCtoVOL(x)	((x * 100) / LIV_MAX)

static struct tsc2101_data *snd_data;
static u8 flags;

static void tsc2101_set_route(void)
{
	u32 reg;

	reg = tsc2101_regread(snd_data, TSC2101_REG_AUDIOCON7);

	if(reg & TSC2101_HDDETFL) //Headphones
	{
		tsc2101_regwrite(snd_data, TSC2101_REG_AUDIOCON5, TSC2101_DAC2SPK1(1) | TSC2101_DAC2SPK2(2) | TSC2101_MUTSPK1 | TSC2101_MUTSPK2);

		tsc2101_regwrite(snd_data, TSC2101_REG_AUDIOCON6, TSC2101_MUTCELL | TSC2101_MUTLSPK | TSC2101_LDSCPTC | TSC2101_CAPINTF);

		flags |= FLAG_HEADPHONES;
	}
	else //Loudspeaker
	{
		tsc2101_regwrite(snd_data, TSC2101_REG_AUDIOCON5, TSC2101_DAC2SPK1(3) | TSC2101_MUTSPK1 | TSC2101_MUTSPK2);

		tsc2101_regwrite(snd_data, TSC2101_REG_AUDIOCON6, TSC2101_MUTCELL | TSC2101_SPL2LSK | TSC2101_LDSCPTC | TSC2101_CAPINTF);

		flags &= ~FLAG_HEADPHONES;
	}
}

static irqreturn_t tsc2101_headset_int(int irq, void *dev_id)
{
	tsc2101_set_route();
	return IRQ_HANDLED;
}

static int tsc2101_snd_activate(void) {
	u32 data;

	if(snd_data == NULL)
		return 1;
	
	snd_pxa2xx_i2sound_i2slink_get();

	tsc2101_regwrite(snd_data, TSC2101_REG_PWRDOWN, 0xFFFC); //power down whole codec
			
	/*Mute Analog Sidetone */
	/*Select MIC_INHED input for headset */
	/*Cell Phone In not connected */
	tsc2101_regwrite(snd_data, TSC2101_REG_MIXERPGA,
			TSC2101_ASTMU | TSC2101_ASTG(0x45) | TSC2101_MICADC | TSC2101_MICSEL(1) );

	/* ADC, DAC, Analog Sidetone, cellphone, buzzer softstepping enabled */
	/* 1dB AGC hysteresis */
	/* MICes bias 2V */
	tsc2101_regwrite(snd_data, TSC2101_REG_AUDIOCON4, TSC2101_MB_HED(0) | TSC2101_MB_HND | TSC2101_ADSTPD |
			TSC2101_DASTPD | TSC2101_ASSTPD | TSC2101_CISTPD | TSC2101_BISTPD );

	/* Set codec output volume */
	tsc2101_regwrite(snd_data, TSC2101_REG_DACPGA, ~(TSC2101_DALMU | TSC2101_DARMU) );

	/* OUT8P/N muted, CPOUT muted */

	/* Headset/Hook switch detect disabled */
	tsc2101_regwrite(snd_data, TSC2101_REG_AUDIOCON7, TSC2101_DETECT | TSC2101_HDDEBNPG(2) | TSC2101_DGPIO2);

	/*Mic support*/
	tsc2101_regwrite(snd_data, TSC2101_REG_MICAGC, TSC2101_MMPGA(0) | TSC2101_MDEBNS(0x03) | TSC2101_MDEBSN(0x03) );


	//Hack - PalmOS sets that, so cleaning it
	tsc2101_regwrite(snd_data, TSC2101_REG_HEADSETPGA, 0x0000);

	data = request_irq (IRQ_GPIO(55), tsc2101_headset_int,  SA_SAMPLE_RANDOM, "TSC2101 Headphones", (void*)55);
	set_irq_type (IRQ_GPIO(55), IRQT_RISING);

	if(data)
		return data;

	tsc2101_set_route();
	return 0;
}

static void tsc2101_snd_deactivate(void) {
	snd_pxa2xx_i2sound_i2slink_free();
	free_irq(IRQ_GPIO(55), (void*) 55);
}
// FIXME - is it really suspend == deactivate?
static int tsc2101_snd_suspend(pm_message_t state) {
	tsc2101_snd_deactivate();
	return 0;
}

static int tsc2101_snd_resume(void) {
	tsc2101_snd_activate();
	return 0;
}


static int tsc2101_snd_set_rate(unsigned int rate) {
	uint div, fs_44;
	u32 data;

	printk("Rate: %d ", rate);

	if (rate >= 48000)
		{ div = 0; fs_44 = 0; }
	else if (rate >= 44100)
		{ div = 0; fs_44 = 1; }
	else if (rate >= 32000)
		{ div = 1; fs_44 = 0; }
	else if (rate >= 29400)
		{ div = 1; fs_44 = 1; }
	else if (rate >= 24000)
		{ div = 2; fs_44 = 0; }
	else if (rate >= 22050)
		{ div = 2; fs_44 = 1; }
	else if (rate >= 16000)
		{ div = 3; fs_44 = 0; }
	else if (rate >= 14700)
		{ div = 3; fs_44 = 1; }
	else if (rate >= 12000)
		{ div = 4; fs_44 = 0; }
	else if (rate >= 11025)
		{ div = 4; fs_44 = 1; }
	else if (rate >= 9600)
		{ div = 5; fs_44 = 0; }
	else if (rate >= 8820)
		{ div = 5; fs_44 = 1; }
	else if (rate >= 8727)
		{ div = 6; fs_44 = 0; }
	else if (rate >= 8018)
		{ div = 6; fs_44 = 1; }
	else if (rate >= 8000)
		{ div = 7; fs_44 = 0; }
	else
		{ div = 7; fs_44 = 1; }

	/* wait for any frame to complete */
	udelay(125);

	/* Set AC1 */
	data = tsc2101_regread(snd_data, TSC2101_REG_AUDIOCON1);
	/* Clear prev settings */
	data &= ~(TSC2101_DACFS(0xF) | TSC2101_ADCFS(0xF));

	data |= TSC2101_DACFS(div) | TSC2101_ADCFS(div);
	tsc2101_regwrite(snd_data, TSC2101_REG_AUDIOCON1, data);

	/* Set the AC3 */
	data = tsc2101_regread(snd_data, TSC2101_REG_AUDIOCON3);
	/*Clear prev settings */
	data &= ~(TSC2101_REFFS | TSC2101_SLVMS);

	data |= fs_44 ? TSC2101_REFFS : 0;
	tsc2101_regwrite(snd_data, TSC2101_REG_AUDIOCON3, data);


	/* program the PLLs */
	if (fs_44) {
		/* 44.1 khz - 12 MHz Mclk */
		printk("selected 44.1khz PLL\n");
		tsc2101_regwrite(snd_data, TSC2101_REG_PLL1, TSC2101_PLL_ENABLE | TSC2101_PLL_PVAL(1) | TSC2101_PLL_JVAL(7));
		tsc2101_regwrite(snd_data, TSC2101_REG_PLL2, TSC2101_PLL2_DVAL(5264));
	} else {
		/* 48 khz - 12 Mhz Mclk */
		printk("selected 48khz PLL\n");
		tsc2101_regwrite(snd_data, TSC2101_REG_PLL1, TSC2101_PLL_ENABLE | TSC2101_PLL_PVAL(1) | TSC2101_PLL_JVAL(8));
		tsc2101_regwrite(snd_data, TSC2101_REG_PLL2, TSC2101_PLL2_DVAL(1920));
	}

	return 0;
}

static int tsc2101_snd_open_stream(int stream) {
	u32 reg, pwdn;

	reg = tsc2101_regread(snd_data, TSC2101_REG_AUDIOCON5);

	if(stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if(flags & FLAG_HEADPHONES) {
			reg &= ~(TSC2101_MUTSPK1 | TSC2101_MUTSPK2);
			pwdn = TSC2101_MBIAS_HND |
				TSC2101_MBIAS_HED |
				TSC2101_ASTPWD |
				TSC2101_ADPWDN |
				TSC2101_VGPWDN |
				TSC2101_COPWDN |
				TSC2101_LSPWDN;
		} else {
			reg |= (TSC2101_MUTSPK1 | TSC2101_MUTSPK2);
			pwdn = TSC2101_MBIAS_HND |
				TSC2101_MBIAS_HED |
				TSC2101_ASTPWD |
				TSC2101_SPI1PWDN |
				TSC2101_SPI2PWDN |
				TSC2101_VGPWDN |
				TSC2101_COPWDN;
		}

	} else {
		pwdn = TSC2101_ASTPWD |
			TSC2101_MBIAS_HED |
			TSC2101_DAPWDN |
			TSC2101_SPI1PWDN |
			TSC2101_SPI2PWDN |
			TSC2101_VGPWDN |
			TSC2101_COPWDN |
			TSC2101_LSPWDN;
	}

	tsc2101_regwrite(snd_data, TSC2101_REG_AUDIOCON5, reg);
	tsc2101_regwrite(snd_data, TSC2101_REG_PWRDOWN, pwdn);
	return 0;
}
		        
static void tsc2101_snd_close_stream(int stream) {
	u32 reg;

	if(flags & FLAG_HEADPHONES) {
		reg = tsc2101_regread(snd_data, TSC2101_REG_AUDIOCON5);
		reg |= TSC2101_MUTSPK1 | TSC2101_MUTSPK2;
		tsc2101_regwrite(snd_data, TSC2101_REG_AUDIOCON5, reg);
	}

	tsc2101_regwrite(snd_data, TSC2101_REG_PWRDOWN,
		~(TSC2101_EFFCTL) );
}

static int __pcm_playback_volume_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo) {
	uinfo->type			= SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count			= 2;
	uinfo->value.integer.min	= 0;
	uinfo->value.integer.max	= 100;
	return 0;
}

/*
 * Alsa mixer interface function for getting the volume read from the DGC in a 
 * 0 -100 alsa mixer format.
 */
static int __pcm_playback_volume_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol) {
	u32 val	= tsc2101_regread(snd_data, TSC2101_REG_DACPGA);

	ucontrol->value.integer.value[0] = 100 - TSCtoVOL(TSC2101_DALVLI(val) );
	ucontrol->value.integer.value[1] = 100 - TSCtoVOL(TSC2101_DARVLI(val) );

	return 0;
}

static int __pcm_playback_volume_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol) {
	u32 val = tsc2101_regread (snd_data, TSC2101_REG_DACPGA);

	val &= ~(TSC2101_DALVL(0xFF) | TSC2101_DARVL(0xFF) );
	val |= TSC2101_DALVL(VOLtoTSC(100 - ucontrol->value.integer.value[0] ) - 2 );
	val |= TSC2101_DARVL(VOLtoTSC(100 - ucontrol->value.integer.value[1] ) - 2 );

	tsc2101_regwrite(snd_data, TSC2101_REG_DACPGA, val);
	return 2;
}

static int __pcm_playback_switch_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo) {
	uinfo->type			= SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count			= 2;
	uinfo->value.integer.min	= 0;
	uinfo->value.integer.max	= 1;
	return 0;
}

/* 
 * When DGC_DALMU (bit 15) is 1, the left channel is muted.
 * When DGC_DALMU is 0, left channel is not muted.
 * Same logic apply also for the right channel.
 */
static int __pcm_playback_switch_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol) {
	u32 val = tsc2101_regread (snd_data, TSC2101_REG_DACPGA);
	
	ucontrol->value.integer.value[0] = !(val & TSC2101_DALMU);
	ucontrol->value.integer.value[1] = !(val & TSC2101_DARMU);
	return 0;
}

static int __pcm_playback_switch_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol) {
	u32 val = tsc2101_regread(snd_data, TSC2101_REG_DACPGA);

	// in alsa mixer 1 --> on, 0 == off. In tsc2101 registry 1 --> off, 0 --> on
	// so if values are same, it's time to change the registry value.
	if (!ucontrol->value.integer.value[0])
		val |= TSC2101_DALMU;	// mute --> turn bit on
	else
		val &= ~TSC2101_DALMU;	// unmute --> turn bit off
	/* L */

	if (!ucontrol->value.integer.value[1])
		val |= TSC2101_DARMU;	// mute --> turn bit on
	else
		val &= ~TSC2101_DARMU;	// unmute --> turn bit off
	/* R */

	tsc2101_regwrite(snd_data, TSC2101_REG_DACPGA, val);
	return 2;
}

static int __headset_playback_volume_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo) {
	uinfo->type			= SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count			= 1;
	uinfo->value.integer.min	= 0;
	uinfo->value.integer.max	= 100;
	return 0;
}

static int __headset_playback_volume_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol) {
	u32 val	= tsc2101_regread(snd_data, TSC2101_REG_HEADSETPGA);

	ucontrol->value.integer.value[0] = TSCtoVOL(TSC2101_ADPGA_HEDI(val) );
	return 0;
}

static int __headset_playback_volume_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol) {	
	u32 val = tsc2101_regread(snd_data, TSC2101_REG_HEADSETPGA);

	val &= ~TSC2101_ADPGA_HED(0xFF);
	val |= TSC2101_ADPGA_HED(VOLtoTSC(ucontrol->value.integer.value[0] ) );

	tsc2101_regwrite(snd_data, TSC2101_REG_HEADSETPGA, val);
	return 1;
}

static int __headset_playback_switch_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo) {
	uinfo->type 			= SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count			= 1;
	uinfo->value.integer.min	= 0;
	uinfo->value.integer.max	= 1;
	return 0;
}

static int __headset_playback_switch_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol) {
	u32 val = tsc2101_regread(snd_data, TSC2101_REG_HEADSETPGA);

	ucontrol->value.integer.value[0] = !(val & TSC2101_ADMUT_HED);
	return 0;
}

	// in alsa mixer 1 --> on, 0 == off. In tsc2101 registry 1 --> off, 0 --> on
	// so if values are same, it's time to change the registry value...	
static int __headset_playback_switch_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol) {
	u32 val = tsc2101_regread(snd_data, TSC2101_REG_HEADSETPGA);
	
	if (!ucontrol->value.integer.value[0])
		val	= val | TSC2101_ADMUT_HED;		// mute --> turn bit on
	else
		val	= val & ~TSC2101_ADMUT_HED;	// unmute --> turn bit off			

	tsc2101_regwrite(snd_data, TSC2101_REG_HEADSETPGA, val);
	return 1;
}

static int __handset_playback_volume_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo) {
	uinfo->type			= SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count			= 1;
	uinfo->value.integer.min	= 0;
	uinfo->value.integer.max	= 100;
	return 0;
}

static int __handset_playback_volume_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol) {
	u32 val	= tsc2101_regread(snd_data, TSC2101_REG_HANDSETPGA);

	ucontrol->value.integer.value[0] = TSCtoVOL(TSC2101_ADPGA_HNDI(val) );
	return 0;
}

static int __handset_playback_volume_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol) {	
	u32 val = tsc2101_regread(snd_data, TSC2101_REG_HANDSETPGA);

	val &= ~TSC2101_ADPGA_HND(0xFF);
	val |= TSC2101_ADPGA_HND(VOLtoTSC(ucontrol->value.integer.value[0] ) );

	tsc2101_regwrite(snd_data, TSC2101_REG_HANDSETPGA, val);
	return 1;
}

static int __handset_playback_switch_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo) {
	uinfo->type 			= SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count			= 1;
	uinfo->value.integer.min	= 0;
	uinfo->value.integer.max	= 1;
	return 0;
}

static int __handset_playback_switch_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol) {
	u32 val = tsc2101_regread(snd_data, TSC2101_REG_HANDSETPGA);

	ucontrol->value.integer.value[0] = !(val & TSC2101_ADMUT_HND);
	return 0;
}

	// in alsa mixer 1 --> on, 0 == off. In tsc2101 registry 1 --> off, 0 --> on
	// so if values are same, it's time to change the registry value...	
static int __handset_playback_switch_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol) {
	u32 val = tsc2101_regread(snd_data, TSC2101_REG_HANDSETPGA);
	
	if (!ucontrol->value.integer.value[0])
		val	= val | TSC2101_ADMUT_HND;		// mute --> turn bit on
	else
		val	= val & ~TSC2101_ADMUT_HND;	// unmute --> turn bit off			

	tsc2101_regwrite(snd_data, TSC2101_REG_HANDSETPGA, val);
	return 1;
}

static struct snd_kcontrol_new tsc2101_control[] __devinitdata={
	{
		.name  = "PCM Playback Volume",
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.index = 0,
		.access= SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info  = __pcm_playback_volume_info,
		.get   = __pcm_playback_volume_get,
		.put   = __pcm_playback_volume_put,
	}, {
		.name  = "PCM Playback Switch",
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.index = 0,
		.access= SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info  = __pcm_playback_switch_info,
		.get   = __pcm_playback_switch_get,
		.put   = __pcm_playback_switch_put,
	}, {
		.name  = "Not Used Headset Input Volume",
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.index = 1,
		.access= SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info  = __headset_playback_volume_info,
		.get   = __headset_playback_volume_get,
		.put   = __headset_playback_volume_put,
	}, {
		.name  = "Not Used Headset Input Switch",
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.index = 1,
		.access= SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info  = __headset_playback_switch_info,
		.get   = __headset_playback_switch_get,
		.put   = __headset_playback_switch_put,
	}, {
		.name  = "Handset Input Volume",
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.index = 2,
		.access= SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info  = __handset_playback_volume_info,
		.get   = __handset_playback_volume_get,
		.put   = __handset_playback_volume_put,
	}, {
		.name  = "Handset Input Switch",
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.index = 2,
		.access= SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info  = __handset_playback_switch_info,
		.get   = __handset_playback_switch_get,
		.put   = __handset_playback_switch_put,
	}	
};

static int tsc2101_snd_add_mixer_controls(struct snd_card *acard) {
	int i;
	int err = 0;

	for (i = 0; i < ARRAY_SIZE(tsc2101_control); i++) {
		if ((err = snd_ctl_add(acard, snd_ctl_new1(&tsc2101_control[i], acard ) ) ) < 0) {
			return err;
		}
	}

	return 0;
}

static struct snd_pxa2xx_i2sound_board tsc2101_audio = {
	        .name                   = "tsc2101 Audio",
	        .desc                   = "TI tsc2101 Audio driver",
	        .acard_id               = "tsc2101",
	        .info                   = SND_PXA2xx_I2SOUND_INFO_CAN_CAPTURE | SND_PXA2xx_I2SOUND_INFO_CLOCK_FROM_PXA |
					  SND_PXA2xx_I2SOUND_INFO_SYSCLOCK_DISABLE,
	        .activate               = tsc2101_snd_activate,
	        .deactivate             = tsc2101_snd_deactivate,
		.set_rate		= tsc2101_snd_set_rate,
	        .open_stream            = tsc2101_snd_open_stream,
	        .close_stream           = tsc2101_snd_close_stream,
	        .add_mixer_controls     = tsc2101_snd_add_mixer_controls,

		.streams_hw		= {
			{
			.rates		= SNDRV_PCM_RATE_8000  | SNDRV_PCM_RATE_16000 |
				       SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_44100 |
				       SNDRV_PCM_RATE_48000,
			.rate_min	=  8000,
			.rate_max	= 48000,
			},
			{
			.rates		= SNDRV_PCM_RATE_8000  | SNDRV_PCM_RATE_16000 |
				       SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_44100 |
				       SNDRV_PCM_RATE_48000,
			.rate_min	=  8000,
			.rate_max	= 48000,
			}
		},
#ifdef CONFIG_PM
	        .suspend                = tsc2101_snd_suspend,
	        .resume                 = tsc2101_snd_resume,
#endif  
};


/*
 * 	Touchscreen part
 */

extern void tsc2101_ts_setup(struct platform_device *dev);
extern void tsc2101_ts_report(struct tsc2101_data *devdata, int x, int y, int p, int pendown);

static void tsc2101_readdata(struct tsc2101_data *devdata, struct tsc2101_ts_event *ts_data)
{
	int z1,z2,fixadc=0;
	u32 values[4],status;

	status=tsc2101_regread(devdata, TSC2101_REG_STATUS);
    
	if (status & (TSC2101_STATUS_XSTAT | TSC2101_STATUS_YSTAT | TSC2101_STATUS_Z1STAT 
		| TSC2101_STATUS_Z2STAT)) {

		/* Read X, Y, Z1 and Z2 */
		devdata->platform->send(TSC2101_READ, TSC2101_REG_X, &values[0], 4);

		ts_data->x=values[0];
		ts_data->y=values[1];
		z1=values[2];
		z2=values[3];

		/* Calculate Pressure */
		if ((z1 != 0) && (ts_data->x!=0) && (ts_data->y!=0)) 
			ts_data->p = ((ts_data->x * (z2 -z1) / z1));
		else
			ts_data->p=0;
	}
	
    if (status & TSC2101_STATUS_BSTAT) {
		devdata->platform->send(TSC2101_READ, TSC2101_REG_BAT, &values[0], 1);
	   	devdata->miscdata.bat=values[0];
	   	fixadc=1;
	}	
	if (status & TSC2101_STATUS_AX1STAT) {
		devdata->platform->send(TSC2101_READ, TSC2101_REG_AUX1, &values[0], 1);
		devdata->miscdata.aux1=values[0];
		fixadc=1;
	}
    if (status & TSC2101_STATUS_AX2STAT) {
    	devdata->platform->send(TSC2101_READ, TSC2101_REG_AUX2, &values[0], 1);
    	devdata->miscdata.aux2=values[0];
    	fixadc=1;
	}
    if (status & TSC2101_STATUS_T1STAT) {
    	devdata->platform->send(TSC2101_READ, TSC2101_REG_TEMP1, &values[0], 1);
    	devdata->miscdata.temp1=values[0];
    	fixadc=1;
	}
    if (status & TSC2101_STATUS_T2STAT) {
    	devdata->platform->send(TSC2101_READ, TSC2101_REG_TEMP2, &values[0], 1);
    	devdata->miscdata.temp2=values[0];
    	fixadc=1;
    }
    if (fixadc) {
		/* Switch back to touchscreen autoscan */
		tsc2101_regwrite(devdata, TSC2101_REG_ADC, TSC2101_ADC_DEFAULT | TSC2101_ADC_PSM | TSC2101_ADC_ADMODE(0x2));
	}    	
}

static void tsc2101_ts_enable(struct tsc2101_data *devdata)
{
	//tsc2101_regwrite(devdata, TSC2101_REG_RESETCTL, 0xbb00);
	
	/* PINTDAV is data available only */
	tsc2101_regwrite(devdata, TSC2101_REG_STATUS, 0x2000);

	/* disable buffer mode */
	tsc2101_regwrite(devdata, TSC2101_REG_BUFMODE, 0x0);

	/* use internal reference, 100 usec power-up delay,
	 * power down between conversions, 1.25V internal reference */
	tsc2101_regwrite(devdata, TSC2101_REG_REF, 0x16);
			   
	/* enable touch detection, 84usec precharge time, 32 usec sense time */
	tsc2101_regwrite(devdata, TSC2101_REG_CONFIG, 0x08);
			   
	/* 3 msec conversion delays, 12 MHz MClk  */
	tsc2101_regwrite(devdata, TSC2101_REG_DELAY, 0x0900 | 0xc);
	
	/*
	 * TSC2101-controlled conversions
	 * 12-bit samples
	 * continuous X,Y,Z1,Z2 scan mode
	 * average (mean) 4 samples per coordinate
	 * 1 MHz internal conversion clock
	 * 500 usec panel voltage stabilization delay
	 */
	tsc2101_regwrite(devdata, TSC2101_REG_ADC, TSC2101_ADC_DEFAULT | TSC2101_ADC_PSM | TSC2101_ADC_ADMODE(0x2));

	return;
}

static void tsc2101_ts_disable(struct tsc2101_data *devdata)
{
	/* stop conversions and power down */
	tsc2101_regwrite(devdata, TSC2101_REG_ADC, 0x4000);
}

static void ts_interrupt_main(struct tsc2101_data *devdata, int isTimer, struct pt_regs *regs)
{
	unsigned long flags;
	struct tsc2101_ts_event ts_data;

	spin_lock_irqsave(&devdata->lock, flags);

	if (devdata->platform->pendown()) {
		/* if touchscreen is just touched */
		devdata->pendown = 1;
		tsc2101_readdata(devdata, &ts_data);
		tsc2101_ts_report(devdata, ts_data.x, ts_data.y, ts_data.p, /*1*/ !!ts_data.p);
		mod_timer(&(devdata->ts_timer), jiffies + HZ / 100);
	} else if (devdata->pendown > 0 && devdata->pendown < 3) {
		/* if touchscreen was touched short time ago and it's possible that it's only surface indirectness */
		mod_timer(&(devdata->ts_timer), jiffies + HZ / 100);
		devdata->pendown++;
	} else {
		/* if touchscreen was touched long time ago - it was probably released */
		if (devdata->pendown) {
			tsc2101_readdata(devdata, &ts_data);
			tsc2101_ts_report(devdata, 0, 0, 0, 0);
			}
		devdata->pendown = 0;

		set_irq_type(devdata->platform->irq,IRQT_FALLING);

		/* This must be checked after set_irq_type() to make sure no data was missed */
		if (devdata->platform->pendown()) {
			tsc2101_readdata(devdata, &ts_data);
			mod_timer(&(devdata->ts_timer), jiffies + HZ / 100);
		} 
	}
	
	spin_unlock_irqrestore(&devdata->lock, flags);
}


static void tsc2101_timer(unsigned long data)
{
	struct tsc2101_data *devdata = (struct tsc2101_data *) data;

	ts_interrupt_main(devdata, 1, NULL);
}

static irqreturn_t tsc2101_handler(int irq, void *dev_id)
{
	struct tsc2101_data *devdata = dev_id;
	
	set_irq_type(devdata->platform->irq,IRQT_NOEDGE);
	ts_interrupt_main(devdata, 0, NULL);
	return IRQ_HANDLED;
}


static void tsc2101_get_miscdata(struct tsc2101_data *devdata)
{
	static int i=0;
	unsigned long flags;


	if (devdata->pendown == 0) {
		i++;
		spin_lock_irqsave(&devdata->lock, flags);
		if (i==1) 
			tsc2101_regwrite(devdata, TSC2101_REG_ADC, TSC2101_ADC_DEFAULT | TSC2101_ADC_ADMODE(0x6));
		else if (i==2) 
			tsc2101_regwrite(devdata, TSC2101_REG_ADC, TSC2101_ADC_DEFAULT | TSC2101_ADC_ADMODE(0x7));
		else if (i==3)
			tsc2101_regwrite(devdata, TSC2101_REG_ADC, TSC2101_ADC_DEFAULT | TSC2101_ADC_ADMODE(0x8));
		else if (i==4)
			tsc2101_regwrite(devdata, TSC2101_REG_ADC, TSC2101_ADC_DEFAULT | TSC2101_ADC_ADMODE(0xa));
		else if (i>=5) {
			tsc2101_regwrite(devdata, TSC2101_REG_ADC, TSC2101_ADC_DEFAULT | TSC2101_ADC_ADMODE(0xc));
			i=0;
		}
		spin_unlock_irqrestore(&devdata->lock, flags);
	}
}


static void tsc2101_misc_timer(unsigned long data)
{
	
	struct tsc2101_data *devdata = (struct tsc2101_data *) data;
	tsc2101_get_miscdata(devdata);
	mod_timer(&(devdata->misc_timer), jiffies + HZ);	
}

void tsc2101_print_miscdata(struct platform_device *dev)
{
	struct tsc2101_data *devdata = platform_get_drvdata(dev);

	printk(KERN_ERR "TSC2101 Bat:   %04x\n",devdata->miscdata.bat);
	printk(KERN_ERR "TSC2101 Aux1:  %04x\n",devdata->miscdata.aux1);
	printk(KERN_ERR "TSC2101 Aux2:  %04x\n",devdata->miscdata.aux2);
	printk(KERN_ERR "TSC2101 Temp1: %04x\n",devdata->miscdata.temp1);
	printk(KERN_ERR "TSC2101 Temp2: %04x\n",devdata->miscdata.temp2);
}

#ifdef CONFIG_PM

static int tsc2101_suspend(struct platform_device *dev, pm_message_t state)
{
	struct tsc2101_data *devdata = platform_get_drvdata(dev);
	
//	if (level == SUSPEND_POWER_DOWN) {
		tsc2101_ts_disable(devdata);
		devdata->platform->suspend();
//	}
	
	return 0;
}

static int tsc2101_resume(struct platform_device *dev)
{
	struct tsc2101_data *devdata = platform_get_drvdata(dev);

//	if (level == RESUME_POWER_ON) {
		devdata->platform->resume();
		tsc2101_ts_enable(devdata);
//	}

	return 0;
}

#else
#define tsc2101_suspend NULL
#define tsc2101_resume NULL
#endif

static int __init tsc2101_probe(struct platform_device *dev)
{
	struct tsc2101_data *devdata;
	struct tsc2101_ts_event ts_data;

	if (!(devdata = kcalloc(1, sizeof(struct tsc2101_data), GFP_KERNEL)))
		return -ENOMEM;
	
	platform_set_drvdata(dev,devdata);
	spin_lock_init(&devdata->lock);
	devdata->platform = dev->dev.platform_data;

	init_timer(&devdata->ts_timer);
	devdata->ts_timer.data = (unsigned long) devdata;
	devdata->ts_timer.function = tsc2101_timer;

	init_timer(&devdata->misc_timer);
	devdata->misc_timer.data = (unsigned long) devdata;
	devdata->misc_timer.function = tsc2101_misc_timer;

	/* request irq */
	if (request_irq(devdata->platform->irq, tsc2101_handler, 0, "tsc2101", devdata)) {
		printk(KERN_ERR "tsc2101: Could not allocate touchscreen IRQ!\n");
		kfree(devdata);
		return -EINVAL;
	}
	
	tsc2101_ts_setup(dev);
	tsc2101_ts_enable(devdata);

	set_irq_type(devdata->platform->irq,IRQT_FALLING);
	/* Check there is no pending data */
	tsc2101_readdata(devdata, &ts_data);

	mod_timer(&(devdata->misc_timer), jiffies + HZ);	
	
	/* Sound driver */
	snd_data = devdata;
	return 0;
}


static  int __exit tsc2101_remove(struct platform_device *dev)
{
	struct tsc2101_data *devdata = platform_get_drvdata(dev);

	free_irq(devdata->platform->irq, devdata);
	del_timer_sync(&devdata->ts_timer);
	del_timer_sync(&devdata->misc_timer);
	input_unregister_device(devdata->inputdevice);
	tsc2101_ts_disable(devdata);
	kfree(devdata);

	return 0;
}

static struct platform_driver tsc2101_driver = {
	.driver		= {
	.name 		= "tsc2101",
	.owner		= THIS_MODULE,
	.bus 		= &platform_bus_type,
	},
	.probe 		= tsc2101_probe,
/*	.remove 	= __exit_p(tsc2101_remove),*/
	.remove 	= tsc2101_remove,
	.suspend 	= tsc2101_suspend,
	.resume 	= tsc2101_resume,
};

static int __init tsc2101_init(void)
{
	int ret;
	ret = platform_driver_register(&tsc2101_driver);
	/* Sound driver */
	snd_pxa2xx_i2sound_card_activate(&tsc2101_audio);
	return ret;
}

static void __exit tsc2101_exit(void)
{
	platform_driver_unregister(&tsc2101_driver);

	/* Sound driver */
	snd_pxa2xx_i2sound_card_deactivate();
}

module_init(tsc2101_init);
module_exit(tsc2101_exit);

MODULE_LICENSE("GPL");

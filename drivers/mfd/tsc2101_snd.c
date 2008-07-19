#include <linux/mfd/tsc2101.h>
#include <linux/module.h>
#include <sound/control.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>

#include "../../sound/arm/pxa2xx-i2sound.h"

static struct tsc2101_driver tsc2101_snd_driver;
static struct tsc2101_snd tsc2101_snd;


static int tsc2101_snd_suspend(pm_message_t state)
{
	return 0;
}

static int tsc2101_snd_resume(void)
{
	return 0;
}

static void tsc2101_set_route(void)
{
	u32 reg;
	unsigned long flags;

	/* FIXME: is spinlock USEFUL here? */
	spin_lock_irqsave(&tsc2101_snd_driver.tsc->lock, flags);

	reg = tsc2101_regread(tsc2101_snd_driver.tsc, TSC2101_REG_AUDIOCON7);

	if(reg & TSC2101_HDDETFL) /* Headphones */
	{
		tsc2101_regwrite(tsc2101_snd_driver.tsc, TSC2101_REG_AUDIOCON5, TSC2101_DAC2SPK1(1) | TSC2101_DAC2SPK2(2) | TSC2101_MUTSPK1 | TSC2101_MUTSPK2);

		tsc2101_regwrite(tsc2101_snd_driver.tsc, TSC2101_REG_AUDIOCON6, TSC2101_MUTCELL | TSC2101_MUTLSPK | TSC2101_LDSCPTC | TSC2101_CAPINTF);

		tsc2101_snd.flags |= FLAG_HEADPHONES;
	}
	else /* Loudspeaker */
	{
		tsc2101_regwrite(tsc2101_snd_driver.tsc, TSC2101_REG_AUDIOCON5, TSC2101_DAC2SPK1(3) | TSC2101_MUTSPK1 | TSC2101_MUTSPK2);

		tsc2101_regwrite(tsc2101_snd_driver.tsc, TSC2101_REG_AUDIOCON6, TSC2101_MUTCELL | TSC2101_SPL2LSK | TSC2101_LDSCPTC | TSC2101_CAPINTF);

		tsc2101_snd.flags &= ~FLAG_HEADPHONES;
	}

	spin_unlock_irqrestore(&tsc2101_snd_driver.tsc->lock, flags);
}

static irqreturn_t tsc2101_headset_int(int irq, void *dev_id)
{
	tsc2101_set_route();
	return IRQ_HANDLED;
}


static int tsc2101_snd_activate(void) {
	u32 data;
	unsigned long flags;

	if(tsc2101_snd_driver.tsc == NULL)
		return 1;

	
	snd_pxa2xx_i2sound_i2slink_get();

	/* FIXME: is spinlock USEFUL here? */
	spin_lock_irqsave(&tsc2101_snd_driver.tsc->lock, flags);

	tsc2101_regwrite(tsc2101_snd_driver.tsc, TSC2101_REG_PWRDOWN, 0xFFFC); //power down whole codec
			
	/*Mute Analog Sidetone */
	/*Select MIC_INHED input for headset */
	/*Cell Phone In not connected */
	tsc2101_regwrite(tsc2101_snd_driver.tsc, TSC2101_REG_MIXERPGA,
			TSC2101_ASTMU | TSC2101_ASTG(0x45) | TSC2101_MICADC | TSC2101_MICSEL(1) );

	/* ADC, DAC, Analog Sidetone, cellphone, buzzer softstepping enabled */
	/* 1dB AGC hysteresis */
	/* MICes bias 2V */
	tsc2101_regwrite(tsc2101_snd_driver.tsc, TSC2101_REG_AUDIOCON4, TSC2101_MB_HED(0) | TSC2101_MB_HND | TSC2101_ADSTPD |
			TSC2101_DASTPD | TSC2101_ASSTPD | TSC2101_CISTPD | TSC2101_BISTPD );

	/* Set codec output volume */
	tsc2101_regwrite(tsc2101_snd_driver.tsc, TSC2101_REG_DACPGA, ~(TSC2101_DALMU | TSC2101_DARMU) );

	/* OUT8P/N muted, CPOUT muted */

	/* Headset/Hook switch detect disabled */
	tsc2101_regwrite(tsc2101_snd_driver.tsc, TSC2101_REG_AUDIOCON7, TSC2101_DETECT | TSC2101_HDDEBNPG(2) | TSC2101_DGPIO2);

	/*Mic support*/
	tsc2101_regwrite(tsc2101_snd_driver.tsc, TSC2101_REG_MICAGC, TSC2101_MMPGA(0) | TSC2101_MDEBNS(0x03) | TSC2101_MDEBSN(0x03) );


	/* Hack - PalmOS sets that, so cleaning it */
	tsc2101_regwrite(tsc2101_snd_driver.tsc, TSC2101_REG_HEADSETPGA, 0x0000);

	data = request_irq (IRQ_GPIO(55), tsc2101_headset_int,  SA_SAMPLE_RANDOM, "TSC2101 Headphones", (void*)55);
	set_irq_type (IRQ_GPIO(55), IRQT_RISING);

	spin_unlock_irqrestore(&tsc2101_snd_driver.tsc->lock, flags);

	if(data)
		return data;

	tsc2101_set_route();
	return 0;
}

static void tsc2101_snd_deactivate(void) {
	snd_pxa2xx_i2sound_i2slink_free();
	free_irq(IRQ_GPIO(55), (void*) 55);
}

static int tsc2101_snd_set_rate(unsigned int rate) {
	uint div, fs_44;
	u32 data;
	unsigned long flags;

	printk(KERN_DEBUG "tsc2101_ts: Rate: %d ", rate);

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

	/* FIXME: is this spin_lock USEFUL? */
	spin_lock_irqsave(&tsc2101_snd_driver.tsc->lock, flags);
	/* wait for any frame to complete */
	udelay(125);

	/* Set AC1 */
	data = tsc2101_regread(tsc2101_snd_driver.tsc, TSC2101_REG_AUDIOCON1);

	/* Clear prev settings */
	data &= ~(TSC2101_DACFS(0xF) | TSC2101_ADCFS(0xF));

	/* set divisor */
	data |= TSC2101_DACFS(div) | TSC2101_ADCFS(div);
	tsc2101_regwrite(tsc2101_snd_driver.tsc, TSC2101_REG_AUDIOCON1, data);

	/* Set the AC3 */
	data = tsc2101_regread(tsc2101_snd_driver.tsc, TSC2101_REG_AUDIOCON3);

	/*Clear prev settings */
	data &= ~(TSC2101_REFFS | TSC2101_SLVMS);

	/* setting Fsref to 44.1 kHz or 48 kHz */
	data |= fs_44 ? TSC2101_REFFS : 0;
	tsc2101_regwrite(tsc2101_snd_driver.tsc, TSC2101_REG_AUDIOCON3, data);


	/* program the PLLs */
	if (fs_44) {
		/* 44.1 khz - 12 MHz Mclk */
		printk(KERN_DEBUG "tsc2101_ts: selected 44.1khz PLL\n");
		tsc2101_regwrite(tsc2101_snd_driver.tsc, TSC2101_REG_PLL1, TSC2101_PLL_ENABLE | TSC2101_PLL_PVAL(1) | TSC2101_PLL_JVAL(7));
		tsc2101_regwrite(tsc2101_snd_driver.tsc, TSC2101_REG_PLL2, TSC2101_PLL2_DVAL(5264));
	} else {
		/* 48 khz - 12 Mhz Mclk */
		printk(KERN_DEBUG "tsc2101_ts: selected 48khz PLL\n");
		tsc2101_regwrite(tsc2101_snd_driver.tsc, TSC2101_REG_PLL1, TSC2101_PLL_ENABLE | TSC2101_PLL_PVAL(1) | TSC2101_PLL_JVAL(8));
		tsc2101_regwrite(tsc2101_snd_driver.tsc, TSC2101_REG_PLL2, TSC2101_PLL2_DVAL(1920));
	}

	spin_unlock_irqrestore(&tsc2101_snd_driver.tsc->lock, flags);
	return 0;
}

static int tsc2101_snd_open_stream(int stream) {
	u32 reg, pwdn;
	unsigned long flags;

	/* FIXME: is this spin_lock USEFUL? */
	spin_lock_irqsave(&tsc2101_snd_driver.tsc->lock, flags);

	reg = tsc2101_regread(tsc2101_snd_driver.tsc, TSC2101_REG_AUDIOCON5);

	if(stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if(tsc2101_snd.flags & FLAG_HEADPHONES) {
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

	tsc2101_regwrite(tsc2101_snd_driver.tsc, TSC2101_REG_AUDIOCON5, reg);
	tsc2101_regwrite(tsc2101_snd_driver.tsc, TSC2101_REG_PWRDOWN, pwdn);

	spin_unlock_irqrestore(&tsc2101_snd_driver.tsc->lock, flags);
	return 0;
}


static void tsc2101_snd_close_stream(int stream) {
	u32 reg;
	unsigned long flags;

	/* FIXME: is this spin_lock USEFUL? */
	spin_lock_irqsave(&tsc2101_snd_driver.tsc->lock, flags);

	if(tsc2101_snd.flags & FLAG_HEADPHONES) {
		reg = tsc2101_regread(tsc2101_snd_driver.tsc, TSC2101_REG_AUDIOCON5);
		reg |= TSC2101_MUTSPK1 | TSC2101_MUTSPK2;
		tsc2101_regwrite(tsc2101_snd_driver.tsc, TSC2101_REG_AUDIOCON5, reg);
	}

	tsc2101_regwrite(tsc2101_snd_driver.tsc, TSC2101_REG_PWRDOWN,
		~(TSC2101_EFFCTL) );

	spin_unlock_irqrestore(&tsc2101_snd_driver.tsc->lock, flags);
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
	u32 val	= tsc2101_regread(tsc2101_snd_driver.tsc, TSC2101_REG_DACPGA);

	ucontrol->value.integer.value[0] = 100 - TSCtoVOL(TSC2101_DALVLI(val) );
	ucontrol->value.integer.value[1] = 100 - TSCtoVOL(TSC2101_DARVLI(val) );

	return 0;
}

static int __pcm_playback_volume_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol) {
	u32 val = tsc2101_regread (tsc2101_snd_driver.tsc, TSC2101_REG_DACPGA);

	val &= ~(TSC2101_DALVL(0xFF) | TSC2101_DARVL(0xFF) );
	val |= TSC2101_DALVL(VOLtoTSC(100 - ucontrol->value.integer.value[0] ) - 2 );
	val |= TSC2101_DARVL(VOLtoTSC(100 - ucontrol->value.integer.value[1] ) - 2 );

	tsc2101_regwrite(tsc2101_snd_driver.tsc, TSC2101_REG_DACPGA, val);
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
	u32 val = tsc2101_regread (tsc2101_snd_driver.tsc, TSC2101_REG_DACPGA);
	
	ucontrol->value.integer.value[0] = !(val & TSC2101_DALMU);
	ucontrol->value.integer.value[1] = !(val & TSC2101_DARMU);
	return 0;
}

static int __pcm_playback_switch_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol) {
	u32 val = tsc2101_regread(tsc2101_snd_driver.tsc, TSC2101_REG_DACPGA);

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

	tsc2101_regwrite(tsc2101_snd_driver.tsc, TSC2101_REG_DACPGA, val);
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
	u32 val	= tsc2101_regread(tsc2101_snd_driver.tsc, TSC2101_REG_HEADSETPGA);

	ucontrol->value.integer.value[0] = TSCtoVOL(TSC2101_ADPGA_HEDI(val) );
	return 0;
}

static int __headset_playback_volume_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol) {	
	u32 val;
	unsigned long flags;

	/* FIXME: is this spin_lock USEFUL? */
	spin_lock_irqsave(&tsc2101_snd_driver.tsc->lock, flags);

	val = tsc2101_regread(tsc2101_snd_driver.tsc, TSC2101_REG_HEADSETPGA);

	val &= ~TSC2101_ADPGA_HED(0xFF);
	val |= TSC2101_ADPGA_HED(VOLtoTSC(ucontrol->value.integer.value[0] ) );

	tsc2101_regwrite(tsc2101_snd_driver.tsc, TSC2101_REG_HEADSETPGA, val);

	spin_unlock_irqrestore(&tsc2101_snd_driver.tsc->lock, flags);

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
	u32 val = tsc2101_regread(tsc2101_snd_driver.tsc, TSC2101_REG_HEADSETPGA);

	ucontrol->value.integer.value[0] = !(val & TSC2101_ADMUT_HED);
	return 0;
}

	// in alsa mixer 1 --> on, 0 == off. In tsc2101 registry 1 --> off, 0 --> on
	// so if values are same, it's time to change the registry value...	
static int __headset_playback_switch_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol) {
	u32 val;
	unsigned long flags;

	/* FIXME: is this spin_lock USEFUL? */
	spin_lock_irqsave(&tsc2101_snd_driver.tsc->lock, flags);

	val = tsc2101_regread(tsc2101_snd_driver.tsc, TSC2101_REG_HEADSETPGA);
	
	if (!ucontrol->value.integer.value[0])
		val	= val | TSC2101_ADMUT_HED;		// mute --> turn bit on
	else
		val	= val & ~TSC2101_ADMUT_HED;	// unmute --> turn bit off			

	tsc2101_regwrite(tsc2101_snd_driver.tsc, TSC2101_REG_HEADSETPGA, val);

	spin_unlock_irqrestore(&tsc2101_snd_driver.tsc->lock, flags);

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
	u32 val;
	unsigned long flags;

	/* FIXME: is this spin_lock USEFUL? */
	spin_lock_irqsave(&tsc2101_snd_driver.tsc->lock, flags);

	val = tsc2101_regread(tsc2101_snd_driver.tsc, TSC2101_REG_HANDSETPGA);
	spin_unlock_irqrestore(&tsc2101_snd_driver.tsc->lock, flags);

	ucontrol->value.integer.value[0] = TSCtoVOL(TSC2101_ADPGA_HNDI(val) );
	return 0;
}

static int __handset_playback_volume_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol) {	
	u32 val;
	unsigned long flags;

	/* FIXME: is this spin_lock USEFUL? */
	spin_lock_irqsave(&tsc2101_snd_driver.tsc->lock, flags);
	
	val = tsc2101_regread(tsc2101_snd_driver.tsc, TSC2101_REG_HANDSETPGA);

	val &= ~TSC2101_ADPGA_HND(0xFF);
	val |= TSC2101_ADPGA_HND(VOLtoTSC(ucontrol->value.integer.value[0] ) );

	tsc2101_regwrite(tsc2101_snd_driver.tsc, TSC2101_REG_HANDSETPGA, val);
	spin_unlock_irqrestore(&tsc2101_snd_driver.tsc->lock, flags);
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
	u32 val;
	unsigned long flags;

	/* FIXME: is this spin_lock USEFUL? */
	spin_lock_irqsave(&tsc2101_snd_driver.tsc->lock, flags);

	val = tsc2101_regread(tsc2101_snd_driver.tsc, TSC2101_REG_HANDSETPGA);

	spin_unlock_irqrestore(&tsc2101_snd_driver.tsc->lock, flags);
	ucontrol->value.integer.value[0] = !(val & TSC2101_ADMUT_HND);
	return 0;
}

	// in alsa mixer 1 --> on, 0 == off. In tsc2101 registry 1 --> off, 0 --> on
	// so if values are same, it's time to change the registry value...	
static int __handset_playback_switch_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol) {
	u32 val;
	unsigned long flags;

	/* FIXME: is this spin_lock USEFUL? */
	spin_lock_irqsave(&tsc2101_snd_driver.tsc->lock, flags);

	val = tsc2101_regread(tsc2101_snd_driver.tsc, TSC2101_REG_HANDSETPGA);
	
	if (!ucontrol->value.integer.value[0])
		val	= val | TSC2101_ADMUT_HND;		// mute --> turn bit on
	else
		val	= val & ~TSC2101_ADMUT_HND;	// unmute --> turn bit off			

	tsc2101_regwrite(tsc2101_snd_driver.tsc, TSC2101_REG_HANDSETPGA, val);
	spin_unlock_irqrestore(&tsc2101_snd_driver.tsc->lock, flags);
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

static int tsc2101_audio_suspend(pm_message_t state) {
	tsc2101_snd_deactivate();
	return 0;
}

static int tsc2101_audio_resume(void) {
	tsc2101_snd_activate();
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
	        .suspend                = tsc2101_audio_suspend,
	        .resume                 = tsc2101_audio_resume,
#endif  
};

static int tsc2101_snd_probe(struct tsc2101 *tsc)
{

	tsc2101_snd_driver.tsc = tsc;
	tsc2101_snd_driver.priv = (void *) &tsc2101_snd;

	snd_pxa2xx_i2sound_card_activate(&tsc2101_audio);
	printk(KERN_NOTICE "tsc2101_snd: sound driver initialized\n");

	return 0;
}

static int tsc2101_snd_remove(void)
{
	snd_pxa2xx_i2sound_card_deactivate();
	printk(KERN_NOTICE "tsc2101_snd: sound driver removed\n");
	return 0;
}


static int __init tsc2101_snd_init(void)
{
	return tsc2101_register(&tsc2101_snd_driver,TSC2101_DEV_SND);
}

static void __exit tsc2101_snd_exit(void)
{
	tsc2101_unregister(&tsc2101_snd_driver,TSC2101_DEV_SND);
}

static struct tsc2101_driver tsc2101_snd_driver = {
	.suspend	= tsc2101_snd_suspend,
	.resume		= tsc2101_snd_resume,
	.probe		= tsc2101_snd_probe,
	.remove		= tsc2101_snd_remove,
};

module_init(tsc2101_snd_init);
module_exit(tsc2101_snd_exit);

MODULE_LICENSE("GPL");

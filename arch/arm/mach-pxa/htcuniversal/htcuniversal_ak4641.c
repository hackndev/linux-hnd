/*
 * Audio support for codec Asahi Kasei AK4641
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2006 Giorgio Padrin <giorgio@mandarinlogiq.org>
 *
 * History:
 *
 * 2006-03	Written -- Giorgio Padrin
 * 2006-09	Test and debug on machine (HP hx4700) -- Elshin Roman <roxmail@list.ru>
 *
 * AK4641 codec device driver
 *
 * Copyright (c) 2005 SDG Systems, LLC
 *
 * Based on code:
 *   Copyright (c) 2002 Hewlett-Packard Company
 *   Copyright (c) 2000 Nicolas Pitre <nico@cam.org>
 *   Copyright (c) 2000 Lernout & Hauspie Speech Products, N.V.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 */

#include <sound/driver.h>
 
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/delay.h>
#include <linux/i2c.h>

#include <sound/core.h>
#include <sound/control.h>
#include <sound/initval.h>
#include <sound/info.h>

#include "htcuniversal_ak4641.h"

/* Registers */
#define R_PM1		0x00
#define R_PM2		0x01
#define R_SEL1		0x02
#define R_SEL2		0x03
#define	R_MODE1		0x04
#define R_MODE2		0x05
#define R_DAC		0x06
#define R_MIC		0x07
#define REG_TIMER	0x08
#define REG_ALC1	0x09
#define REG_ALC2	0x0a
#define R_PGA		0x0b
#define R_ATTL		0x0c
#define R_ATTR		0x0d
#define REG_VOL		0x0e
#define R_STATUS	0x0f
#define REG_EQLO	0x10
#define REG_EQMID	0x11
#define REG_EQHI	0x12
#define REG_BTIF	0x13

/* Register flags */
/* REG_PWR1 */
#define R_PM1_PMADC	0x01
#define R_PM1_PMMIC	0x02
#define REG_PWR1_PMAUX  0x04
#define REG_PWR1_PMMO   0x08
#define R_PM1_PMLO	0x10
/* unused               0x20 */
/* unused               0x40 */
#define R_PM1_PMVCM	0x80

/* REG_PWR2 */
#define R_PM2_PMDAC	0x01
/* unused               0x02 */
/* unused               0x04 */
#define R_PM2_PMMO2	0x08
#define REG_PWR2_MCKAC  0x10
/* unused               0x20 */
/* unused               0x40 */
#define R_PM2_MCKPD	0x80

/* REG_SEL1 */
#define R_SEL1_PSMO2	0x01
/* unused               0x02 */
/* unused               0x04 */
/* unused               0x08 */
#define REG_SEL1_MICM   0x10
#define REG_SEL1_DACM   0x20
#define REG_SEL1_PSMO   0x40
#define REG_SEL1_MOGN   0x80

/* REG_SEL2 */
#define R_SEL2_PSLOR	0x01
#define R_SEL2_PSLOL	0x02
#define REG_SEL2_AUXSI  0x04
/* unused               0x08 */
#define REG_SEL2_MICL   0x10
#define REG_SEL2_AUXL   0x20
/* unused               0x40 */
#define R_SEL2_DACL	0x80

/* REG_MODE1 */
#define REG_MODE1_DIF0  0x01
#define REG_MODE1_DIF1  0x02
/* unused               0x04 */
/* unused               0x08 */
/* unused               0x10 */
/* unused               0x20 */
/* unused               0x40 */
/* unused               0x80 */

/* REG_MODE2 */
/* unused               0x01 */
#define REG_MODE2_LOOP  0x02
#define REG_MODE2_HPM   0x04
/* unused               0x08 */
/* unused               0x10 */
#define REG_MODE2_MCK0  0x20
#define REG_MODE2_MCK1  0x40
/* unused               0x80 */

/* REG_DAC */
#define REG_DAC_DEM0    0x01
#define REG_DAC_DEM1    0x02
#define REG_DAC_EQ      0x04
/* unused               0x08 */
#define R_DAC_DATTC	0x10
#define R_DAC_SMUTE	0x20
#define REG_DAC_TM      0x40
/* unused               0x80 */

/* REG_MIC */
#define R_MIC_MGAIN	0x01
#define R_MIC_MSEL	0x02
#define R_MIC_MICAD	0x04
#define R_MIC_MPWRI	0x08
#define R_MIC_MPWRE	0x10
#define REG_MIC_AUXAD   0x20
/* unused               0x40 */
/* unused               0x80 */

/* REG_TIMER */

#define REG_TIMER_LTM0  0x01
#define REG_TIMER_LTM1  0x02
#define REG_TIMER_WTM0  0x04
#define REG_TIMER_WTM1  0x08
#define REG_TIMER_ZTM0  0x10
#define REG_TIMER_ZTM1  0x20
/* unused               0x40 */
/* unused               0x80 */

#define REG_ALC1_LMTH   0x01
#define REG_ALC1_RATT   0x02
#define REG_ALC1_LMAT0  0x04
#define REG_ALC1_LMAT1  0x08
#define REG_ALC1_ZELM   0x10
#define REG_ALC1_ALC1   0x20
/* unused               0x40 */
/* unused               0x80 */

/* REG_ALC2 */

/* REG_PGA */

/* REG_ATTL */

/* REG_ATTR */

/* REG_VOL */
#define REG_VOL_ATTM	0x80

/* REG_STATUS */
#define R_STATUS_DTMIC	0x01

/* REG_EQ controls use 4 bits for each of 5 EQ levels */

/* Bluetooth not yet implemented */
#define REG_BTIF_PMAD2  0x01
#define REG_BTIF_PMDA2  0x02
#define REG_BTIF_PMBIF  0x04
#define REG_BTIF_ADC2   0x08
#define REG_BTIF_DAC2   0x10
#define REG_BTIF_BTFMT0 0x20
#define REG_BTIF_BTFMT1 0x40
/* unused               0x80 */

/* begin {{ I2C }} */

static struct i2c_driver snd_ak4641_i2c_driver = {
	.driver = {
		.name = "ak4641-i2c"
	},
};

static int snd_ak4641_i2c_init(void)
{
	return i2c_add_driver(&snd_ak4641_i2c_driver);
}

static void snd_ak4641_i2c_free(void)
{
	i2c_del_driver(&snd_ak4641_i2c_driver);
}

static inline int snd_ak4641_i2c_probe(struct snd_ak4641 *ak)
{
	if (ak->i2c_client.adapter == NULL) return -EINVAL;
	ak->i2c_client.addr = 0x12;
	if (i2c_smbus_xfer(ak->i2c_client.adapter, ak->i2c_client.addr,
			   0, 0, 0, I2C_SMBUS_QUICK, NULL) < 0)
		return -ENODEV;
	else return 0;
}

static int snd_ak4641_i2c_attach(struct snd_ak4641 *ak)
{
	int ret = 0;
	if ((ret = snd_ak4641_i2c_probe(ak)) < 0) return ret;
	snprintf(ak->i2c_client.name, sizeof(ak->i2c_client.name),
		 "ak4641-i2c at %d-%04x",
		 i2c_adapter_id(ak->i2c_client.adapter), ak->i2c_client.addr);
	return i2c_attach_client(&ak->i2c_client);
}

static void snd_ak4641_i2c_detach(struct snd_ak4641 *ak)
{
	i2c_detach_client(&ak->i2c_client);
}

/* end {{ I2C }} */


/* begin {{ Registers & Cache Ops }} */

static int snd_ak4641_hwsync(struct snd_ak4641 *ak, int read, u8 reg)
{
	struct i2c_msg msgs[2];
	u8 buf[2];
	int ret;

	snd_assert(reg < ARRAY_SIZE(ak->regs), return -EINVAL);

	/* setup i2c msgs */
	msgs[0].addr = ak->i2c_client.addr;
	msgs[0].flags = 0;
	msgs[0].buf = buf;
	if (!read)
		msgs[0].len = 2;
	else {
		msgs[1].flags = I2C_M_RD;
		msgs[1].addr = msgs[0].addr;
		msgs[1].buf = msgs[0].buf + 1;
		msgs[0].len = 1;
		msgs[1].len = 1;
	}

	buf[0] = reg;
	
	/* regs[reg] -> buffer, on write */
	if (!read) buf[1] = ak->regs[reg];

	/* i2c transfer */
	ret = i2c_transfer(ak->i2c_client.adapter, msgs, read ? 2 : 1);
	if (ret != (read ? 2 : 1)) return ret; /* transfer error */ //@@ error ret < 0, or not ?

	/* regs[reg] <- buffer, on read */
	if (read) ak->regs[reg] = buf[1];

	return 0;
}

static inline int snd_ak4641_hwsync_read(struct snd_ak4641 *ak, u8 reg)
{
	return snd_ak4641_hwsync(ak, 1, reg);
}

static inline int snd_ak4641_hwsync_write(struct snd_ak4641 *ak, u8 reg)
{
	return snd_ak4641_hwsync(ak, 0, reg);
}

static int snd_ak4641_hwsync_read_all(struct snd_ak4641 *ak)
{
	u8 reg;
	for (reg = 0; reg < ARRAY_SIZE(ak->regs); reg++)
		if (snd_ak4641_hwsync_read(ak, reg) < 0) return -1;
	return 0;	
}

static int snd_ak4641_hwsync_write_all(struct snd_ak4641 *ak)
{
	u8 reg;
	for (reg = 0; reg < ARRAY_SIZE(ak->regs); reg++)
		if (snd_ak4641_hwsync_write(ak, reg) < 0) return -1;
	return 0;	
}

static int snd_ak4641_reg_changed(struct snd_ak4641 *ak, u8 reg)
{
	if ((reg != R_PGA && ak->powered_on) ||
	    (reg == R_PGA && (ak->regs[R_PM1] & R_PM1_PMMIC)))
 		return snd_ak4641_hwsync_write(ak, reg);
	return 0;
}

/* end {{ Registers & Cache Ops }}*/


static inline void snd_ak4641_lock(struct snd_ak4641 *ak)
{
	down(&ak->sem);
}

static inline void snd_ak4641_unlock(struct snd_ak4641 *ak)
{
	up(&ak->sem);
}

#define WRITE_MASK(i, val, mask)	(((i) & ~(mask)) | ((val) & (mask)))


/* begin {{ Controls }} */

#define INV_RANGE(val, mask) \
	(~(val) & (mask))

/*-begin----------------------------------------------------------*/
static int snd_ak4641_actl_playback_volume_info(struct snd_kcontrol *kcontrol,
						struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 2;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 0xff;
	return 0;
}

static int snd_ak4641_actl_playback_volume_get(struct snd_kcontrol *kcontrol,
					       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ak4641 *ak = (struct snd_ak4641 *) kcontrol->private_data;

	snd_ak4641_lock(ak);
	ucontrol->value.integer.value[0] = INV_RANGE(ak->regs[R_ATTL], 0xff);
	ucontrol->value.integer.value[1] = INV_RANGE(ak->regs[R_ATTR], 0xff);
	snd_ak4641_unlock(ak);
	return 0;
}

static int snd_ak4641_actl_playback_volume_put(struct snd_kcontrol *kcontrol,
					       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ak4641 *ak = (struct snd_ak4641 *) kcontrol->private_data;
	
	snd_ak4641_lock(ak);
	ak->regs[R_ATTL] = INV_RANGE(ucontrol->value.integer.value[0], 0xff);
	ak->regs[R_ATTR] = INV_RANGE(ucontrol->value.integer.value[1], 0xff);
	snd_ak4641_reg_changed(ak, R_ATTL);
	snd_ak4641_reg_changed(ak, R_ATTR);
	snd_ak4641_unlock(ak);
	return 0;
}
/*-end------------------------------------------------------------*/

/*-begin----------------------------------------------------------*/
static int snd_ak4641_actl_mic_gain_info(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 0x7f;
	return 0;
}

static int snd_ak4641_actl_mic_gain_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ak4641 *ak = (struct snd_ak4641 *) kcontrol->private_data;

	ucontrol->value.integer.value[0] = ak->regs[R_PGA];
	return 0;
}

static int snd_ak4641_actl_mic_gain_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ak4641 *ak = (struct snd_ak4641 *) kcontrol->private_data;
	
	snd_ak4641_lock(ak);
	ak->regs[R_PGA] = ucontrol->value.integer.value[0];
	snd_ak4641_reg_changed(ak, R_PGA);
	snd_ak4641_unlock(ak);
	return 0;
}
/*-end------------------------------------------------------------*/

#define ACTL(ctl_name, _name) \
static struct snd_kcontrol_new snd_ak4641_actl_ ## ctl_name = \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = _name, \
  .info = snd_ak4641_actl_ ## ctl_name ## _info, \
  .get = snd_ak4641_actl_ ## ctl_name ## _get, .put = snd_ak4641_actl_ ## ctl_name ## _put };

ACTL(playback_volume, "Master Playback Volume")
ACTL(mic_gain, "Mic Capture Gain")

struct snd_ak4641_uctl_bool {
	int (*get) (struct snd_ak4641 *uda);
	int (*set) (struct snd_ak4641 *uda, int on);
};

static int snd_ak4641_actl_bool_info(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = 1;
	return 0;
}

static int snd_ak4641_actl_bool_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ak4641 *ak = (struct snd_ak4641 *) kcontrol->private_data;
	struct snd_ak4641_uctl_bool *uctl =
		(struct snd_ak4641_uctl_bool *) kcontrol->private_value;

	ucontrol->value.integer.value[0] = uctl->get(ak);
	return 0;
}

static int snd_ak4641_actl_bool_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ak4641 *ak = (struct snd_ak4641 *) kcontrol->private_data;
	struct snd_ak4641_uctl_bool *uctl =
		(struct snd_ak4641_uctl_bool *) kcontrol->private_value;

	return uctl->set(ak, ucontrol->value.integer.value[0]);
}

/*-begin----------------------------------------------------------*/
static int snd_ak4641_uctl_playback_switch_get(struct snd_ak4641 *ak)
{
	return (ak->regs[R_DAC] & R_DAC_SMUTE) == 0x00;
}

static int snd_ak4641_uctl_playback_switch_set(struct snd_ak4641 *ak, int on)
{
	snd_ak4641_lock(ak);
	ak->regs[R_DAC] = WRITE_MASK(ak->regs[R_DAC],
				     on ? 0x00 : R_DAC_SMUTE, R_DAC_SMUTE);
	snd_ak4641_reg_changed(ak, R_DAC);
	snd_ak4641_unlock(ak);
	return 0;
}
/*-end------------------------------------------------------------*/

/*-begin----------------------------------------------------------*/
static int snd_ak4641_uctl_mic_boost_get(struct snd_ak4641 *ak)
{
	return (ak->regs[R_MIC] & R_MIC_MGAIN) == R_MIC_MGAIN;
}

static int snd_ak4641_uctl_mic_boost_set(struct snd_ak4641 *ak, int on)
{
	snd_ak4641_lock(ak);
	ak->regs[R_MIC] = WRITE_MASK(ak->regs[R_MIC],
				     on ? R_MIC_MGAIN : 0x00, R_MIC_MGAIN);
	snd_ak4641_reg_changed(ak, R_MIC);
	snd_ak4641_unlock(ak);
	return 0;
}
/*-end------------------------------------------------------------*/

/*-begin----------------------------------------------------------*/
static int snd_ak4641_uctl_mono_out_get(struct snd_ak4641 *ak)
{
        printk("mono_out status 0x%8.8x -> 0x%8.8x\n",ak->regs[R_SEL1], ak->regs[R_SEL1] & REG_SEL1_PSMO);
	return (ak->regs[R_SEL1] & REG_SEL1_PSMO) == REG_SEL1_PSMO;
}

static int snd_ak4641_uctl_mono_out_set(struct snd_ak4641 *ak, int on)
{
        printk("phone mic enable called. on=%d\n",on);
	snd_ak4641_lock(ak);
	ak->regs[R_PM1] = WRITE_MASK(ak->regs[R_PM1], on ? R_PM1_PMMIC : 0x00, R_PM1_PMMIC);
	ak->regs[R_PM1] = WRITE_MASK(ak->regs[R_PM1], on ? REG_PWR1_PMMO : 0x00, REG_PWR1_PMMO);
	snd_ak4641_reg_changed(ak, R_PM1);

	snd_ak4641_hwsync_write(ak, R_PGA);	/* mic PGA gain is reset when PMMIC = 0 */

	/* internal mic */
        ak->regs[R_MIC]	= WRITE_MASK(ak->regs[R_MIC], on ? R_MIC_MPWRI : 0x0, R_MIC_MPWRI);
        ak->regs[R_MIC]	= WRITE_MASK(ak->regs[R_MIC],                    0x0, R_MIC_MSEL);
        snd_ak4641_hwsync_write(ak, R_MIC);

//        ak->regs[REG_BTIF]	= WRITE_MASK(ak->regs[REG_BTIF], 0x0, REG_BTIF_DAC2);
//        snd_ak4641_hwsync_write(ak, REG_BTIF);
	/* */
//	ak->regs[REG_VOL] = WRITE_MASK(ak->regs[REG_VOL], on ? REG_VOL_ATTM : 0x00, REG_VOL_ATTM);
//	ak->regs[R_SEL1] = WRITE_MASK(ak->regs[R_SEL1], on ? REG_SEL1_MOGN : 0x00, REG_SEL1_MOGN);
	ak->regs[R_SEL1] = WRITE_MASK(ak->regs[R_SEL1], on ? REG_SEL1_MICM : 0x00, REG_SEL1_MICM);
	ak->regs[R_SEL1] = WRITE_MASK(ak->regs[R_SEL1], on ? REG_SEL1_PSMO : 0x00, REG_SEL1_PSMO);
	snd_ak4641_reg_changed(ak, R_SEL1);
	snd_ak4641_unlock(ak);
	return 0;
}
/*-end------------------------------------------------------------*/

#define ACTL_BOOL(ctl_name, _name) \
static struct snd_ak4641_uctl_bool snd_ak4641_actl_ ## ctl_name ## _pvalue = \
{ .get = snd_ak4641_uctl_ ## ctl_name ## _get, \
  .set = snd_ak4641_uctl_ ## ctl_name ## _set }; \
static struct snd_kcontrol_new snd_ak4641_actl_ ## ctl_name = \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = _name, .info = snd_ak4641_actl_bool_info, \
  .get = snd_ak4641_actl_bool_get, .put = snd_ak4641_actl_bool_put, \
  .private_value = (unsigned long) &snd_ak4641_actl_ ## ctl_name ## _pvalue };

ACTL_BOOL(playback_switch, "Master Playback Switch")
ACTL_BOOL(mic_boost, "Mic Boost (+20dB)")
ACTL_BOOL(mono_out, "Phone mic enable")

static void snd_ak4641_headphone_on(struct snd_ak4641 *ak, int on);
static void snd_ak4641_speaker_on(struct snd_ak4641 *ak, int on);
static void snd_ak4641_select_mic(struct snd_ak4641 *ak);

void snd_ak4641_hp_connected(struct snd_ak4641 *ak, int connected)
{
	snd_ak4641_lock(ak);
	if (connected != ak->hp_connected) {
		ak->hp_connected = connected;

		/* headphone or speaker, on playback */
		if (ak->playback_on) {
			if (connected) {
				snd_ak4641_headphone_on(ak, 1);
				snd_ak4641_speaker_on(ak, 0);
			} else {
				snd_ak4641_speaker_on(ak, 1);
				snd_ak4641_headphone_on(ak, 0);
			}
		}
		
		/* headset or internal mic, on capture */
		if (ak->capture_on) 
			snd_ak4641_select_mic(ak);
	}
	snd_ak4641_unlock(ak);
}

/* end {{ Controls }} */


/* begin {{ Headphone Detected Notification }} */

static void snd_ak4641_hp_detected_w_fn(void *p)
{
	struct snd_ak4641 *ak = (struct snd_ak4641 *)p;

	snd_ak4641_hp_connected(ak, ak->hp_detected.detected);
}

void snd_ak4641_hp_detected(struct snd_ak4641 *ak, int detected)
{
	if (detected != ak->hp_detected.detected) {
		ak->hp_detected.detected = detected;
		queue_work(ak->hp_detected.wq, &ak->hp_detected.w);
	}
}

static int snd_ak4641_hp_detected_init(struct snd_ak4641 *ak)
{
	INIT_WORK(&ak->hp_detected.w, snd_ak4641_hp_detected_w_fn);
	ak->hp_detected.detected = ak->hp_connected;
	ak->hp_detected.wq = create_singlethread_workqueue("ak4641");
	if (ak->hp_detected.wq) return 0;
	else return -1;
}

static void snd_ak4641_hp_detected_free(struct snd_ak4641 *ak)
{
	destroy_workqueue(ak->hp_detected.wq);
}

/* end {{ Headphone Detected Notification }} */


/* begin {{ Codec Control }} */

static void snd_ak4641_headphone_on(struct snd_ak4641 *ak, int on)
{
	if (on) {
		ak->regs[R_PM1] = WRITE_MASK(ak->regs[R_PM1], R_PM1_PMLO, R_PM1_PMLO);
		snd_ak4641_hwsync_write(ak, R_PM1);
		ak->headphone_out_on(1);
		ak->regs[R_SEL2] = WRITE_MASK(ak->regs[R_SEL2],
					      R_SEL2_PSLOL | R_SEL2_PSLOR,
					      R_SEL2_PSLOL | R_SEL2_PSLOR);
		snd_ak4641_hwsync_write(ak, R_SEL2);
	} else {
		ak->regs[R_SEL2] = WRITE_MASK(ak->regs[R_SEL2],
					      0x00, R_SEL2_PSLOL | R_SEL2_PSLOR);
		snd_ak4641_hwsync_write(ak, R_SEL2);
		ak->headphone_out_on(0);
		ak->regs[R_PM1] = WRITE_MASK(ak->regs[R_PM1], 0x00, R_PM1_PMLO);
		snd_ak4641_hwsync_write(ak, R_PM1);
	}
}

static void snd_ak4641_speaker_on(struct snd_ak4641 *ak, int on)
{
	if (on) {
		ak->regs[R_PM1] = WRITE_MASK(ak->regs[R_PM1], R_PM1_PMLO, R_PM1_PMLO);
		snd_ak4641_hwsync_write(ak, R_PM1);
		ak->speaker_out_on(1);
		ak->regs[R_SEL2] = WRITE_MASK(ak->regs[R_SEL2],
					      R_SEL2_PSLOL | R_SEL2_PSLOR,
					      R_SEL2_PSLOL | R_SEL2_PSLOR);
		snd_ak4641_hwsync_write(ak, R_SEL2);
	} else {
		ak->regs[R_SEL2] = WRITE_MASK(ak->regs[R_SEL2],
					      0x00, R_SEL2_PSLOL | R_SEL2_PSLOR);
		snd_ak4641_hwsync_write(ak, R_SEL2);
		ak->speaker_out_on(0);
		ak->regs[R_PM1] = WRITE_MASK(ak->regs[R_PM1], 0x00, R_PM1_PMLO);
		snd_ak4641_hwsync_write(ak, R_PM1);
	}
}

static inline int snd_ak4641_power_on(struct snd_ak4641 *ak)
{
	ak->reset_pin(1);
	ak->power_on_chip(1);
	msleep(1);
	ak->reset_pin(0);
	ak->powered_on = 1;
	return 0;
}

static inline int snd_ak4641_power_off(struct snd_ak4641 *ak)
{
	ak->powered_on = 0;
	ak->power_on_chip(0);
	return 0;
}

static inline void snd_ak4641_headphone_out_on(struct snd_ak4641 *ak, int on)
{
	if (ak->headphone_out_on) ak->headphone_out_on(on);
}

static inline void snd_ak4641_speaker_out_on(struct snd_ak4641 *ak, int on)
{
	if (ak->speaker_out_on) ak->speaker_out_on(on);
}

static int snd_ak4641_playback_on(struct snd_ak4641 *ak)
{
	if (ak->playback_on) return 0;

	ak->regs[R_PM2] = WRITE_MASK(ak->regs[R_PM2],
				     R_PM2_PMDAC, R_PM2_MCKPD | R_PM2_PMDAC);
	ak->regs[R_PM1] = WRITE_MASK(ak->regs[R_PM1], R_PM1_PMLO, R_PM1_PMLO);
	snd_ak4641_hwsync_write(ak, R_PM2);
	snd_ak4641_hwsync_write(ak, R_PM1);
	if (ak->hp_connected) snd_ak4641_headphone_on(ak, 1);
	else snd_ak4641_speaker_on(ak, 1);	
	
	ak->playback_on = 1;

	return 0;
}

static int snd_ak4641_playback_off(struct snd_ak4641 *ak)
{
	if (!ak->playback_on) return 0;

	ak->playback_on = 0;

	if (ak->hp_connected) snd_ak4641_headphone_on(ak, 0);
	else snd_ak4641_speaker_on(ak, 0);
	ak->regs[R_PM1] = WRITE_MASK(ak->regs[R_PM1], 0x00, R_PM1_PMLO);
	ak->regs[R_PM2] = WRITE_MASK(ak->regs[R_PM2],
				    (!ak->capture_on ? R_PM2_MCKPD : 0x00) | R_PM2_PMDAC,
				    R_PM2_MCKPD | R_PM2_PMDAC);
	snd_ak4641_hwsync_write(ak, R_PM1);
	snd_ak4641_hwsync_write(ak, R_PM2);	

	return 0;
}

static void snd_ak4641_select_mic(struct snd_ak4641 *ak)
{
	int mic = 0;
	u8 r_mic;

	if (ak->hp_connected) {
		/* check headset mic */
		ak->regs[R_MIC]	= WRITE_MASK(ak->regs[R_MIC], R_MIC_MPWRE, R_MIC_MPWRE);
		snd_ak4641_hwsync_write(ak, R_MIC);
		snd_ak4641_hwsync_read(ak, R_STATUS);
		mic = (ak->regs[R_STATUS] & R_STATUS_DTMIC) == R_STATUS_DTMIC;
		
		printk("htcuniversal_ak4641_select_mic: mic=%d\n",mic);

	 r_mic = WRITE_MASK(ak->regs[R_MIC],
			   R_MIC_MSEL | (ak->capture_on ? R_MIC_MPWRE : 0x00),
			   R_MIC_MSEL | R_MIC_MPWRI | R_MIC_MPWRE);
	}
	else	
	 r_mic = WRITE_MASK(ak->regs[R_MIC],
			         0x00 | (ak->capture_on ? R_MIC_MPWRI : 0x00),
			   R_MIC_MSEL | R_MIC_MPWRI | R_MIC_MPWRE);

	if (r_mic != ak->regs[R_MIC]) {
		ak->regs[R_MIC] = r_mic;
		snd_ak4641_hwsync_write(ak, R_MIC);
	}
} 

static int snd_ak4641_capture_on(struct snd_ak4641 *ak)
{
	if (ak->capture_on) return 0;

	if (!ak->playback_on) {
		ak->regs[R_PM2] = WRITE_MASK(ak->regs[R_PM2], 0x00, R_PM2_MCKPD);
		snd_ak4641_hwsync_write(ak, R_PM2);
	}
	ak->regs[R_PM1] = WRITE_MASK(ak->regs[R_PM1], R_PM1_PMMIC | R_PM1_PMADC,
						      R_PM1_PMMIC | R_PM1_PMADC);
	snd_ak4641_hwsync_write(ak, R_PM1);
	snd_ak4641_hwsync_write(ak, R_PGA);	/* mic PGA gain is reset when PMMIC = 0 */

	ak->capture_on = 1;

	snd_ak4641_select_mic(ak);
	
	msleep(47);	/* accounts for ADC init cycle, time enough for fs >= 44.1 kHz */

	return 0;
}

static int snd_ak4641_capture_off(struct snd_ak4641 *ak)
{
	if (!ak->capture_on) return 0;

	ak->regs[R_MIC] = WRITE_MASK(ak->regs[R_MIC],
				     0x00, R_MIC_MPWRI | R_MIC_MPWRE | R_MIC_MSEL);
	ak->regs[R_PM1] = WRITE_MASK(ak->regs[R_PM1], 0x00, R_PM1_PMMIC | R_PM1_PMADC);
	snd_ak4641_hwsync_write(ak, R_MIC);
	snd_ak4641_hwsync_write(ak, R_PM1);
	if (!ak->playback_on) {
		ak->regs[R_PM2] = WRITE_MASK(ak->regs[R_PM2], R_PM2_MCKPD, R_PM2_MCKPD);
		snd_ak4641_hwsync_write(ak, R_PM2);
	}

	ak->capture_on = 0;

	return 0;
}

int snd_ak4641_open_stream(struct snd_ak4641 *ak, int stream)
{
	snd_ak4641_lock(ak);
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		ak->playback_stream_opened = 1;
		snd_ak4641_playback_on(ak);
	} else {
		ak->capture_stream_opened = 1;
		snd_ak4641_capture_on(ak);
	}
	snd_ak4641_unlock(ak);
	return 0;
}

int snd_ak4641_close_stream(struct snd_ak4641 *ak, int stream)
{
	snd_ak4641_lock(ak);
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		ak->playback_stream_opened = 0;
		snd_ak4641_playback_off(ak);
	} else {
		ak->capture_stream_opened = 0;
		snd_ak4641_capture_off(ak);
	}
	snd_ak4641_unlock(ak);
	return 0;
}

static int snd_ak4641_init_regs(struct snd_ak4641 *ak)
{
	snd_ak4641_hwsync_read_all(ak);

	//@@ MEMO: add some configs

	ak->regs[R_PM1] = WRITE_MASK(ak->regs[R_PM1], R_PM1_PMVCM, R_PM1_PMVCM);
	ak->regs[R_DAC] = WRITE_MASK(ak->regs[R_DAC], 0x00, R_DAC_DATTC);
	snd_ak4641_hwsync_write(ak, R_PM1);
	snd_ak4641_hwsync_write(ak, R_DAC);
	
	return 0;
}

int snd_ak4641_suspend(struct snd_ak4641 *ak, pm_message_t state)
{
	snd_ak4641_lock(ak);
	if (ak->playback_on) snd_ak4641_playback_off(ak);
	if (ak->capture_on) snd_ak4641_capture_off(ak);
	snd_ak4641_power_off(ak);
	snd_ak4641_unlock(ak);
	return 0;
}

int snd_ak4641_resume(struct snd_ak4641 *ak)
{
	snd_ak4641_lock(ak);
	snd_ak4641_power_on(ak);
	snd_ak4641_hwsync_write_all(ak);
	if (ak->playback_stream_opened) snd_ak4641_playback_on(ak);
	if (ak->capture_stream_opened) snd_ak4641_capture_on(ak);
	snd_ak4641_unlock(ak);
	return 0;
}

static void snd_ak4641_init_ak(struct snd_ak4641 *ak)
{
	init_MUTEX(&ak->sem);
	ak->i2c_client.driver = &snd_ak4641_i2c_driver;
}

int snd_ak4641_activate(struct snd_ak4641 *ak)
{
	int ret = 0;

	snd_ak4641_init_ak(ak);
	snd_ak4641_lock(ak);
	snd_ak4641_power_on(ak);
	if ((ret = snd_ak4641_i2c_attach(ak)) < 0)
		goto failed_i2c_attach;
	snd_ak4641_init_regs(ak);
	if ((ret = snd_ak4641_hp_detected_init(ak)) < 0)
		goto failed_hp_detected_init;
	snd_ak4641_unlock(ak);
	return 0;

 failed_hp_detected_init:
	snd_ak4641_i2c_detach(ak);
 failed_i2c_attach:
	snd_ak4641_power_off(ak);
	snd_ak4641_unlock(ak);
	return ret;
}

void snd_ak4641_deactivate(struct snd_ak4641 *ak)
{
	snd_ak4641_lock(ak);
	snd_ak4641_hp_detected_free(ak);
	snd_ak4641_i2c_detach(ak);
	snd_ak4641_power_off(ak);
	snd_ak4641_unlock(ak);
}

int snd_ak4641_add_mixer_controls(struct snd_ak4641 *ak, struct snd_card *card)
{
	snd_ak4641_lock(ak);
	snd_ctl_add(card, snd_ctl_new1(&snd_ak4641_actl_playback_volume, ak));
	snd_ctl_add(card, snd_ctl_new1(&snd_ak4641_actl_playback_switch, ak));
	snd_ctl_add(card, snd_ctl_new1(&snd_ak4641_actl_mic_gain, ak));
	snd_ctl_add(card, snd_ctl_new1(&snd_ak4641_actl_mic_boost, ak));
	snd_ctl_add(card, snd_ctl_new1(&snd_ak4641_actl_mono_out, ak));
	snd_ak4641_unlock(ak);
	return 0;
}

/* end {{ Codec Control }} */


/* begin {{ Module }} */

static int __init snd_ak4641_module_on_load(void)
{
	snd_ak4641_i2c_init();
	return 0;
}

static void __exit snd_ak4641_module_on_unload(void)
{
	snd_ak4641_i2c_free();
}

module_init(snd_ak4641_module_on_load);
module_exit(snd_ak4641_module_on_unload);

EXPORT_SYMBOL(snd_ak4641_activate);
EXPORT_SYMBOL(snd_ak4641_deactivate);
EXPORT_SYMBOL(snd_ak4641_add_mixer_controls);
EXPORT_SYMBOL(snd_ak4641_open_stream);
EXPORT_SYMBOL(snd_ak4641_close_stream);
EXPORT_SYMBOL(snd_ak4641_suspend);
EXPORT_SYMBOL(snd_ak4641_resume);
EXPORT_SYMBOL(snd_ak4641_hp_connected);
EXPORT_SYMBOL(snd_ak4641_hp_detected);

MODULE_AUTHOR("Giorgio Padrin");
MODULE_DESCRIPTION("Audio support for codec Asahi Kasei AK4641");
MODULE_LICENSE("GPL");

/* end {{ Module }} */

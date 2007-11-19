/*
 * AK4535 mixer device driver
 *
 * Copyright (c) 2002 Hewlett-Packard Company
 *
 * Copyright (c) 2000 Nicolas Pitre <nico@cam.org>
 *
 * Portions are Copyright (C) 2000 Lernout & Hauspie Speech Products, N.V.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/soundcard.h>
#include <linux/i2c.h>

#include <asm/uaccess.h>
#include <asm/mach-types.h>

#include <sound/driver.h>
#include <sound/core.h>
#include <sound/control.h>
#include <sound/initval.h>
#include <sound/info.h>

#include "ak4535.h"

#define REC_MASK	(SOUND_MASK_LINE | SOUND_MASK_MIC)
#define DEV_MASK (REC_MASK | SOUND_MASK_PCM | SOUND_MASK_BASS)

#define REG_PWR1_PMADC		0x01
#define REG_PWR1_PMMIC		0x02
#define REG_PWR1_PMAUX		0x04
#define REG_PWR1_PMMO		0x08
#define REG_PWR1_PMLO		0x10
/* unused			0x20 */
/* unused			0x40 */
#define REG_PWR1_PMVCM		0x80

#define REG_PWR2_PMDAC		0x01
#define REG_PWR2_PMHPR		0x02
#define REG_PWR2_PMHPL		0x04
#define REG_PWR2_PMSPK		0x08
/* unused			0x10 */
/* unused			0x20 */
/* unused			0x40 */
/* unused			0x80 */

#define REG_SEL1_MOUT2		0x01
#define REG_SEL1_ALCS		0x02
#define REG_SEL1_SPPS		0x04
/* unused			0x08 */
#define REG_SEL1_MICM		0x10
#define REG_SEL1_DACM		0x20
#define REG_SEL1_PSMO		0x40
#define REG_SEL1_MATT		0x80

#define REG_SEL2_HPR		0x01
#define REG_SEL2_HPL		0x02
/* unused			0x04 */
#define REG_SEL2_HPM		0x08
#define REG_SEL1_MICL		0x10
#define REG_SEL1_AUXL		0x20
#define REG_SEL1_PSLO		0x40
/* unused			0x80 */

#define REG_MODE1_DIF0		0x01
#define REG_MODE1_DIF1		0x02
#define REG_MODE1_BF		0x04
/* unused			0x08 */
/* unused			0x10 */
/* unused			0x20 */
/* unused			0x40 */
/* unused			0x80 */

#define REG_MODE2_ALC1		0x01
#define REG_MODE2_ALC2		0x02
#define REG_MODE2_LOOP		0x04
#define REG_MODE2_MCK0		0x08
#define REG_MODE2_MCK1		0x10
/* unused			0x20 */
/* unused			0x40 */
/* unused			0x80 */

#define REG_DAC_DEM0		0x01
#define REG_DAC_DEM1		0x02
#define REG_DAC_BST0		0x04
#define REG_DAC_BST1		0x08
#define REG_DAC_DATTC		0x10
#define REG_DAC_SMUTE		0x20
#define REG_DAC_TM0		0x40
#define REG_DAC_TM1		0x80

//@@@ - i2c_get_client is defined but not declared in i2c.h
static struct i2c_client *ak4535_i2c_client=NULL; //@@@
struct i2c_client *ak4535_get_i2c_client(void) { return ak4535_i2c_client; }

static struct ak4535_reg_info {
	unsigned char num;
	unsigned char default_value;
} ak4535_reg_info[] = {
	{ REG_PWR1, REG_PWR1_PMVCM | REG_PWR1_PMMO | REG_PWR1_PMADC | REG_PWR1_PMMIC },
	{ REG_PWR2, REG_PWR2_PMDAC | REG_PWR2_PMSPK },
	{ REG_SEL1, REG_SEL1_MOUT2 | REG_SEL1_ALCS | REG_SEL1_SPPS | REG_SEL1_DACM | REG_SEL1_PSMO },
	{ REG_SEL2, 0x83 },
	{ REG_MODE1, REG_MODE1_DIF1 },
	{ REG_MODE2, 0x1 },
	{ REG_DAC, 0x1d },
	{ REG_MIC, 0x0d },
	{ REG_TIMER, 0x40 },
	{ REG_ALC1, 0x50 },
	{ REG_ALC2, 0x2e },
	{ REG_PGA, 0x10 },
	{ REG_RATT, 0 },
	{ REG_LATT, 0 },
	{ REG_VOL, 0x87 }
}; 

u8 ak4535_read_reg (struct i2c_client *clnt, int regaddr)
{
	char buffer[2];
	int r;
	buffer[0] = regaddr;

	r = i2c_master_send(clnt, buffer, 1);
	if (r != 1) {
		printk(KERN_ERR "ak4535: write failed, status %d\n", r);
		return -1;
       }

	r = i2c_master_recv(clnt, buffer, 1);
	if (r != 1) {
		printk(KERN_ERR "ak4535: read failed, status %d\n", r);
		return -1;
	}

	return buffer[0];
}

void ak4535_write_reg(struct i2c_client *clnt, int regaddr, int data)
{
	char buffer[2];
	int r;

	buffer[0] = regaddr;
	buffer[1] = data;

#ifdef DEBUG_VERBOSE
	printk("ak4535: write %x %x\n", regaddr, data);
#endif
	
	r = i2c_master_send(clnt, buffer, 2);
	if (r != 2) {
		printk(KERN_ERR "ak4535: write failed, status %d\n", r);
	}
}

static void ak4535_sync(struct i2c_client *clnt)
{
	struct ak4535 *akm = container_of(clnt, struct ak4535, client);
	int i;

#ifdef DEBUG_VERBOSE
	for (i = 0; i < REG_MAX; i++) {
		int v = ak4535_read_reg(clnt, i);
		printk("register %x: old value %x\n", i, v);
	}
#endif

	for (i = 0; i < ARRAY_SIZE(ak4535_reg_info); i++)
		ak4535_write_reg(clnt, ak4535_reg_info[i].num, akm->regs[i]);

#ifdef DEBUG_VERBOSE
	for (i = 0; i < REG_MAX; i++) {
		int v = ak4535_read_reg(clnt, i);
		printk("register %x: new value %x\n", i, v);
	}
#endif
}

static void ak4535_power_down (struct i2c_client *clnt)
{
	ak4535_write_reg(clnt, REG_PWR1, 0);
	ak4535_write_reg(clnt, REG_PWR2, 0);
}

static void ak4535_cmd_init(struct i2c_client *clnt)
{
	struct ak4535 *akm = container_of(clnt, struct ak4535, client);

	akm->active = 1;

	/* resend all parameters */
	ak4535_sync(clnt);
}

static int ak4535_configure(struct i2c_client *clnt, struct ak4535_cfg *conf)
{
	struct ak4535 *akm = container_of(clnt, struct ak4535, client);
	int ret = 0;

	printk("ak4535_configure: mic_connected=%d\n", conf->mic_connected);

	akm->mic_connected = 1;
	akm->line_connected = 0;

	return ret;
}

int ak4535_update(struct i2c_client *clnt, int cmd, void *arg)
{
	struct ak4535 *akm = container_of(clnt, struct ak4535, client);
	unsigned char newregnum = 0;
	unsigned char altregnum = 0;
	unsigned char tmp;
	unsigned char val = *(int *)arg;

	switch (cmd) {
	case I2C_SET_PCM: /* set volume.  val =  0 to 100 => 0 to 7 */
		newregnum = REG_VOL;
		if (val > 7) {
			printk("pcmvol=%d must be in range [0,7]", val);
			val = 7;
		}
		tmp = akm->regs[REG_VOL] & ~(7 << 4);
		tmp |= (val << 4);
		akm->regs[REG_VOL] = tmp;
		break;

	case I2C_SET_BASS:   /* set bass.    val = 50 to 100 => 0 to 3 */
		newregnum = REG_DAC;
		if (val > 3) {
			printk("bass=%d must be in range [0,3]", val);
			val = 3;
		}
		tmp = akm->regs[REG_DAC] & ~(3 << 2);
		tmp |= (val << 2);
		akm->regs[REG_DAC] = tmp;
		break;

	case I2C_SET_TREBLE: /* set treble.  val = 50 to 100 => 0 to 3 */
		/* not supported */
		return -EINVAL;

	case I2C_SET_LINE_GAIN:
		newregnum = REG_LATT;
		akm->regs[REG_LATT] = (unsigned char)val;
		altregnum = REG_RATT;
		akm->regs[REG_RATT] = (unsigned char)val;
		break;

	case I2C_SET_MIC_GAIN:
		newregnum = REG_PGA;
		if (val > 71) {
			printk("mic_gain=%d must be in range [0,71]", val);
			val = 71;
		}
		akm->regs[REG_PGA] = (unsigned char)val;
		break;

	case I2C_SET_HEADPHONE_SWITCH:
		if (val) {
			newregnum = REG_PWR2;
			akm->regs[REG_PWR2] |= (REG_PWR2_PMHPR|REG_PWR2_PMHPL);
			akm->regs[REG_PWR2] &= ~REG_PWR2_PMSPK;
		} else {
			newregnum = REG_PWR2;
			akm->regs[REG_PWR2] &= ~(REG_PWR2_PMHPR|REG_PWR2_PMHPL);
			akm->regs[REG_PWR2] |= REG_PWR2_PMSPK;
		}
		break;
	case I2C_SET_HEADPHONE_OUTPUT:
		if (val) {
			newregnum = REG_SEL2;
			akm->regs[REG_SEL2] &= ~(REG_SEL2_HPL | REG_SEL2_HPR);
		} else {
			newregnum = REG_SEL2;
			akm->regs[REG_SEL2] |= REG_SEL2_HPL | REG_SEL2_HPR;
		}
		break;

	case I2C_SET_AGC:
		break;

 	case I2C_SET_RECSRC_LINE:
		newregnum = REG_MIC;
		akm->regs[REG_MIC] = 0x17;
	 	break;

 	case I2C_SET_RECSRC_MIC:
		newregnum = REG_MIC;
		akm->regs[REG_MIC] = 0x0d;
	 	break;

	default:
		return -EINVAL;
	}		

	if (akm->active) {
		ak4535_write_reg(clnt, newregnum, akm->regs[newregnum]);
		if (altregnum)
			ak4535_write_reg(clnt, altregnum, akm->regs[altregnum]);
	}
	return 0;
}

static int ak4535_mixer_ioctl(struct i2c_client *clnt, int cmd, void *arg)
{
	struct ak4535 *akm = container_of(clnt, struct ak4535, client);
	struct i2c_gain gain;
	int val, nr = _IOC_NR(cmd), ret = 0;


	if (cmd == SOUND_MIXER_INFO) {
		struct mixer_info mi;

		strncpy(mi.id, "AK4535", sizeof(mi.id));
		strncpy(mi.name, "Philips AK4535", sizeof(mi.name));
		mi.modify_counter = akm->mod_cnt;
		return copy_to_user(arg, &mi, sizeof(mi));
	}

	else if (cmd == SOUND_MIXER_WRITE_RECSRC) {
		ret = get_user(val, (int *)arg);

		if (ret)
		 	goto out;

		if (val & SOUND_MASK_LINE)
		  {
		    akm->line_connected = 1;
		    akm->mic_connected = 0;
		    ak4535_update(clnt, I2C_SET_RECSRC_LINE, arg);
		    ak4535_update(clnt, I2C_SET_HEADPHONE_SWITCH, &akm->line_connected);
		    ak4535_update(clnt, I2C_SET_HEADPHONE_OUTPUT, &akm->line_connected);
		  }
		if (val & SOUND_MASK_MIC)
		  {
		    akm->mic_connected = 1;
		    akm->line_connected = 0;
		    ak4535_update(clnt, I2C_SET_RECSRC_MIC, arg);
		    ak4535_update(clnt, I2C_SET_HEADPHONE_SWITCH, &akm->line_connected);
		    ak4535_update(clnt, I2C_SET_HEADPHONE_OUTPUT, &akm->line_connected);
		  }
		return put_user(val, (int *)arg);
	}

	else if (cmd == SOUND_MIXER_READ_RECSRC) {
		val = 0;
		if (akm->line_connected) val |= SOUND_MASK_LINE;
		if (akm->mic_connected) val |= SOUND_MASK_MIC;
		return put_user(val, (int *) arg);
	}

	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		ret = get_user(val, (int *)arg);
		if (ret)
			goto out;

		gain.left    = val & 255;
		gain.right   = val >> 8;

		switch (nr) {
		case SOUND_MIXER_PCM:
		        akm->volume = val;
			akm->pcm = val;
			akm->mod_cnt++;
			/* input value [0,100], ak4535 uses [0,7] */
			val = gain.left;
			val = 7 * val / 100;
			ak4535_update(clnt, I2C_SET_PCM, &val);
			break;

		case SOUND_MIXER_BASS:
			akm->bass = val;
			akm->mod_cnt++;
			/* input value [0,100], ak4535 uses [0,3] */
			val = gain.left;
			val = 3 * val / 100;
			ak4535_update(clnt, I2C_SET_BASS, &val);
			break;

		case SOUND_MIXER_TREBLE:
			return -EINVAL;

		case SOUND_MIXER_LINE:
			if (!akm->line_connected)
				return -EINVAL;
			akm->line = val;
			akm->mod_cnt++;
			/* input value [0,100], ak4535 uses [255,0] */
			val = gain.left;
			val = 0xff & ~(255 * val / 100);
			ak4535_update(clnt, I2C_SET_LINE_GAIN, &val);
			break;

		case SOUND_MIXER_MIC:
			if (!akm->mic_connected)
				return -EINVAL;
			akm->mic = val;
			akm->mod_cnt++;
			/* input value [0,100], ak4535 uses [0,71] */
			val = gain.left;
			val = 71 * val / 100;
			ak4535_update(clnt, I2C_SET_MIC_GAIN, &val);
			break;

		case SOUND_MIXER_RECSRC:
			break;

		case SOUND_MIXER_AGC:
			ak4535_update(clnt, I2C_SET_AGC, &val);
			break;

		default:
			ret = -EINVAL;
		}
	}

	if (ret == 0 && _IOC_DIR(cmd) & _IOC_READ) {
		int nr = _IOC_NR(cmd);
		ret = 0;

		switch (nr) {
		case SOUND_MIXER_VOLUME:     val = akm->volume;	break;
		case SOUND_MIXER_PCM:        val = akm->pcm;	break;
		case SOUND_MIXER_BASS:       val = akm->bass;	break;
		case SOUND_MIXER_TREBLE:     val = akm->treble;	break;
		case SOUND_MIXER_LINE:       val = akm->line;	break;
		case SOUND_MIXER_MIC:        val = akm->mic;	break;
		case SOUND_MIXER_RECSRC:     val = REC_MASK;	goto devmask;
		case SOUND_MIXER_RECMASK:    val = REC_MASK;	goto devmask;
		case SOUND_MIXER_DEVMASK:    val = DEV_MASK;	goto devmask;
		devmask:
			if (!akm->mic_connected) val &= ~SOUND_MASK_MIC;
			if (!akm->line_connected) val &= ~SOUND_MASK_LINE;
			break;

		case SOUND_MIXER_CAPS:       val = 0;		break;
		case SOUND_MIXER_STEREODEVS: val = 0;		break;
		default:	val = 0;     ret = -EINVAL;	break;
		}

		if (ret == 0)
			ret = put_user(val, (int *)arg);
	}
out:
	return ret;
}

static struct i2c_driver ak4535;

static struct i2c_client client_template = {
	name: "(unset)",
	flags:  I2C_CLIENT_ALLOW_USE,
	driver: &ak4535
};

static int ak4535_attach(struct i2c_adapter *adap, int addr, int zero_or_minus_one)		
{
	struct ak4535 *akm;
	struct i2c_client *clnt;
	int i;

	akm = kmalloc(sizeof(*akm), GFP_KERNEL);
	if (!akm)
		return -ENOMEM;

	memset(akm, 0, sizeof(*akm));

	/* set default values of registers */
	for (i = 0; i < ARRAY_SIZE(ak4535_reg_info); i++) {
		int addr = ak4535_reg_info[i].num;
		akm->regs[addr] = ak4535_reg_info[i].default_value;
	}
	akm->mic_connected = 1;


	clnt = &akm->client;
	memcpy(clnt, &client_template, sizeof(*clnt));
	clnt->adapter = adap;
	clnt->addr = addr;
	strcpy(clnt->name, "ak4535");

	ak4535_i2c_client = clnt;	/* no i2c_get_client, so export */

	i2c_attach_client(clnt);

	return 0;
}

static int ak4535_detach_client(struct i2c_client *clnt)
{
	i2c_detach_client(clnt);

	kfree(clnt);

	return 0;
}

/* Addresses to scan */
static unsigned short normal_i2c[] = {0x10,I2C_CLIENT_END};
static unsigned short probe[]        = { I2C_CLIENT_END, I2C_CLIENT_END };
static unsigned short ignore[]       = { I2C_CLIENT_END, I2C_CLIENT_END };
static unsigned short force[]        = { ANY_I2C_BUS, 0x10, I2C_CLIENT_END, I2C_CLIENT_END };

static struct i2c_client_address_data addr_data = {
	normal_i2c,
	probe,
	ignore,
	force
};

static int ak4535_attach_adapter(struct i2c_adapter *adap)
{
	return i2c_probe(adap, &addr_data, ak4535_attach);
}

static int ak4535_open(struct i2c_client *clnt)
{
	ak4535_cmd_init(clnt);
	return 0;
}

static void ak4535_close(struct i2c_client *clnt)
{
	struct ak4535 *akm = container_of(clnt, struct ak4535, client);

	ak4535_power_down (clnt);

	akm->active = 0;
}

int ak4535_command(struct i2c_client *clnt, unsigned int cmd, void *arg)
{
	int ret = -EINVAL;

	if (_IOC_TYPE(cmd) == 'M')
		ret = ak4535_mixer_ioctl(clnt, cmd, arg);
	else if (cmd == I2C_AK4535_CONFIGURE)
		ret = ak4535_configure(clnt, arg);
	else if (cmd == I2C_AK4535_OPEN)
		ret = ak4535_open(clnt);
	else if (cmd == I2C_AK4535_CLOSE)
		(ret = 0), ak4535_close(clnt);

	return ret;
}

static struct i2c_driver ak4535 = {
	.owner          = THIS_MODULE,
	.name		= AK4535_NAME,
	.id		= I2C_DRIVERID_AK4535,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= ak4535_attach_adapter,
	.detach_client	= ak4535_detach_client,
	.command        = ak4535_command
};

static int __init ak4535_init(void)
{
	if (machine_is_h5400())
		return i2c_add_driver(&ak4535);
	else
		return -ENODEV;
}

static void __exit ak4535_exit(void)
{
	if (machine_is_h5400())
		i2c_del_driver(&ak4535);
}

module_init(ak4535_init);
module_exit(ak4535_exit);

EXPORT_SYMBOL(ak4535_get_i2c_client);

MODULE_AUTHOR("Jamey Hicks, Phil Blundell");
MODULE_DESCRIPTION("AK4535 CODEC driver");

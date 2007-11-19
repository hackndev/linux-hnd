/*
 *
 * AK4535 codec driver
 *
 * Copyright (c) 2002 Hewlett-Packard Company
 *
 * Copyright (c) 2000 Nicolas Pitre <nico@cam.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 */

#define AK4535_NAME "ak4535"

struct ak4535_cfg {
	unsigned int fs:16;
	unsigned int format:3;
	unsigned int mic_connected:1;
	unsigned int line_connected:1;
};

#define FMT_I2S		0
#define FMT_LSB16	1
#define FMT_LSB18	2
#define FMT_LSB20	3
#define FMT_MSB		5

#define I2C_AK4535_CONFIGURE	0x13800001
#define I2C_AK4535_OPEN		0x13800002
#define I2C_AK4535_CLOSE	0x13800003

struct i2c_gain {
	unsigned int	left:8;
	unsigned int	right:8;
};

#define I2C_SET_PCM		0x13800010
#define I2C_SET_TREBLE		0x13800011
#define I2C_SET_BASS		0x13800012
#define I2C_SET_LINE_GAIN	0x13800013
#define I2C_SET_MIC_GAIN	0x13800014

struct i2c_agc {
	unsigned int	level:8;
	unsigned int	enable:1;
	unsigned int	attack:7;
	unsigned int	decay:8;
	unsigned int	channel:8;
};

#define I2C_SET_AGC		0x13800015

/* Used internally to select recording source internal/external microphone */
#define I2C_SET_RECSRC_MIC		0x13800016
#define I2C_SET_RECSRC_LINE		0x13800017
#define I2C_SET_HEADPHONE_SWITCH	0x13800018
#define I2C_SET_HEADPHONE_OUTPUT	0x13800019

#define REG_MAX 0x10

#define REG_PWR1	0x00
#define REG_PWR2	0x01
#define REG_SEL1	0x02
#define REG_SEL2	0x03
#define REG_MODE1	0x04
#define REG_MODE2	0x05
#define REG_DAC		0x06
#define REG_MIC		0x07
#define REG_TIMER	0x08
#define REG_ALC1	0x09
#define REG_ALC2	0x0a
#define REG_PGA		0x0b
#define REG_LATT	0x0c
#define REG_RATT	0x0d
#define REG_VOL		0x0e
#define REG_STATUS	0x0f

struct ak4535 {
	unsigned char   regs[REG_MAX];
	int		active;
	unsigned short  volume;
	unsigned short	pcm;
	unsigned short	bass;
	unsigned short	treble;
	unsigned short	line;
	unsigned short	mic;
	int             mic_connected;
	int             line_connected;
	int		mod_cnt;
	struct i2c_client client;
};

int ak4535_update(struct i2c_client *clnt, int cmd, void *arg);

int ak4535_command(struct i2c_client *clnt, unsigned int cmd, void *arg);
int snd_card_ak4535_mixer_new(snd_card_t *card);
void snd_card_ak4535_mixer_del(snd_card_t *card);
int ak4535_mute(struct i2c_client *clnt, int mute);
u8 ak4535_read_reg (struct i2c_client *clnt, int regaddr);
void ak4535_write_reg(struct i2c_client *clnt, int regaddr, int data);

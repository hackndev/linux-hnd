/*
 * Audio support for codec Asahi Kasei AK4641
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2006 Giorgio Padrin <giorgio@mandarinlogiq.org>
 */

#ifndef __SOUND_AK4641_H
#define __SOUND_AK4641_H

#include <linux/i2c.h>

struct snd_ak4641 {
	struct semaphore sem;

	u8 regs[0x14];	/* registers cache */

	unsigned int
		powered_on:1,
		playback_on:1,
		playback_stream_opened:1,
		capture_on:1,
		capture_stream_opened:1;

	unsigned int
		hp_connected:1;

	/* -- configuration (to fill before activation) -- */
	void (*power_on_chip)(int on);
	void (*reset_pin)(int on);
	void (*headphone_out_on)(int on);
	void (*speaker_out_on)(int on);

	struct i2c_client i2c_client; /* to fill .adapter */
	/* ----------------------------------------------- */

	struct snd_ak4641_hp_detected {
		int detected;
		struct workqueue_struct *wq;
		struct work_struct w;
	} hp_detected;
};


/* Note: opening, closing, suspending and resuming a stream
 * require the clocks (MCLK and I2S ones) running
 */

/* don't forget to specify I2C adapter in i2c_client field */
int snd_ak4641_activate(struct snd_ak4641 *ak);

void snd_ak4641_deactivate(struct snd_ak4641 *ak);
int snd_ak4641_add_mixer_controls(struct snd_ak4641 *ak, struct snd_card *card);
int snd_ak4641_open_stream(struct snd_ak4641 *ak, int stream);
int snd_ak4641_close_stream(struct snd_ak4641 *ak, int stream);
int snd_ak4641_suspend(struct snd_ak4641 *ak, pm_message_t state);
int snd_ak4641_resume(struct snd_ak4641 *ak);

void snd_ak4641_hp_connected(struct snd_ak4641 *ak, int connected); /* non atomic context */
void snd_ak4641_hp_detected(struct snd_ak4641 *ak, int detected); /* atomic context */

#endif /* __SOUND_AK4641_H */

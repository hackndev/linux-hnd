/*
 * AC97 controller handler for Dell Axim X5.
 * Handles touchscreen, battery and sound.
 *
 * Copyright © 2004 Andrew Zabolotny <zap@homelink.ru>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/soc-old.h>
#include <linux/delay.h>
#include <linux/battery.h>
#include <linux/mfd/mq11xx.h>
#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/aximx5-gpio.h>

#include "../sound/oss/wm97xx.h"

static wm97xx_public *tspub;
extern int driver_pxa_ac97_wm97xx;
static struct mediaq11xx_base *mq_base;

/* Time interval in jiffies at which BMON and AUX inputs are sampled.
 */
#define BMON_AUX_SAMPLE_INTERVAL	(10*HZ)

/*----------------------------------------------* Battery monitor *----------*/

/* Discharge curve for main battery (11 points) */
static u16 main_charge [] =
{ 1432, 1448, 1463, 1470, 1479, 1490, 1516, 1537, 1558, 1589, 1619, 1684, 4096 };

/* Discharge curve for backup battery (11 points) */
static u16 backup_charge [] =
{ 3252, 3277, 3287, 3298, 3308, 3337, 3360, 3382, 3402, 3428, 3455, 3486, 4096 };

/* Compute the percentage of remaining charge given current
 * voltage reading and the discharge curve.
 * Returns a value in the range 0-65536 (100%) or -1 on error.
 */
static int compute_charge (int val, u16 *dc)
{
	int i = 0;

	if (val < dc [0] / 2)
		return -1;
	if (val < dc [0])
		return 0;

	while (i < 11 && val >= dc [i + 1])
		i++;

	if (i >= 11)
		return 0x10000;

	return (i * 5958) + (5958 * (val - dc [i])) / (dc [i + 1] - dc [i]);
}

static int aximx5_m_get_max_charge (struct battery *bat)
{
	return mq_base->get_GPIO (mq_base, 66) ? 1400 : 3400;
}

static int aximx5_m_get_min_charge (struct battery *bat)
{
	return 0;
}

static int aximx5_m_get_charge (struct battery *bat)
{
	int bmon = tspub->get_auxconv (tspub, WM97XX_AC_BMON);
	return (compute_charge (bmon, main_charge) *
		aximx5_m_get_max_charge (bat)) >> 16;
}

static int aximx5_m_get_max_voltage (struct battery *bat)
{
	return 3700;
}

static int aximx5_m_get_min_voltage (struct battery *bat)
{
	return 3100;
}

static int aximx5_m_get_voltage (struct battery *bat)
{
	int bmon = tspub->get_auxconv (tspub, WM97XX_AC_BMON);
	if (bmon < 0)
		return bmon;

	/* My guess is that a 3V reference voltage is connected to the WM9705
	 * AVDD input (see WM9705 datasheet) (perhaps the backup battery).
	 * Compute the real battery voltage given the fact that it is divided
	 * by three by the internal 10k-20k divider, and the fact that
	 * 3000mV == 4096 (in ADC coordinate system).
	 */

	return (3 * 3000 * bmon) >> 12;
}

static int aximx5_m_get_status (struct battery *bat)
{
	return GET_AXIMX5_GPIO (CHARGING) ? BATTERY_STATUS_CHARGING :
		GET_AXIMX5_GPIO (AC_POWER_N) ? BATTERY_STATUS_DISCHARGING :
		BATTERY_STATUS_NOT_CHARGING;
}

static struct battery aximx5_main_battery = {
	.name			= "main",
	//.id
	//.type
	.get_max_voltage	= &aximx5_m_get_max_voltage,
	.get_min_voltage	= &aximx5_m_get_min_voltage,
	.get_voltage		= &aximx5_m_get_voltage,
	.get_max_charge		= &aximx5_m_get_max_charge,
	.get_min_charge		= &aximx5_m_get_min_charge,
	.get_charge		= &aximx5_m_get_charge,
	.get_status		= &aximx5_m_get_status
};

static int aximx5_b_get_max_charge (struct battery *bat)
{
	/* Most CR3032 batteries have approximatively 200mAh capacity.
	 */
	return 200;
}

static int aximx5_b_get_min_charge (struct battery *bat)
{
	return 0;
}

static int aximx5_b_get_charge (struct battery *bat)
{
	int aux = tspub->get_auxconv (tspub, WM97XX_AC_AUX1);
	return (compute_charge (aux, backup_charge) *
		aximx5_b_get_max_charge (bat)) >> 16;
}

static int aximx5_b_get_max_voltage (struct battery *bat)
{
	return 3000;
}

static int aximx5_b_get_min_voltage (struct battery *bat)
{
	return 2700;
}

static int aximx5_b_get_voltage (struct battery *bat)
{
	int aux = tspub->get_auxconv (tspub, WM97XX_AC_AUX1);
	if (aux < 0)
		return aux;

	/* When the battery voltage is 3.00V, the AUX ADC shows
	 * a value of 3518. So I found the coefficient for the
	 * reverse translation: (3000 * 4096) / 3518 = 3493
	 */

	return (aux * 3493) >> 12;
}

static int aximx5_b_get_status (struct battery *bat)
{
	return BATTERY_STATUS_NOT_CHARGING;
}

static struct battery aximx5_backup_battery = {
	.name			= "backup",
	//.id
	//.type
	.get_max_voltage	= &aximx5_b_get_max_voltage,
	.get_min_voltage	= &aximx5_b_get_min_voltage,
	.get_voltage		= &aximx5_b_get_voltage,
	.get_max_charge		= &aximx5_b_get_max_charge,
	.get_min_charge		= &aximx5_b_get_min_charge,
	.get_charge		= &aximx5_b_get_charge,
	.get_status		= &aximx5_b_get_status
};

void wm97xx_prepare_aux (wm97xx_public *wmpub, wm97xx_aux_conv ac, int stage)
{
	/* Enable a 3.3k load resistor on the backup battery,
	 * otherwise the voltage measurements won't be precise.
	 */
	mq_base->set_GPIO (mq_base, 61, stage ?
			   (MQ_GPIO_IN | MQ_GPIO_OUT0) :
			   (MQ_GPIO_IN | MQ_GPIO_OUT1));
	/* Wait 1ms for battery voltage to stabilize */
	if (stage == 0)
		udelay (1000);
}

/*----------------------------------------------* Amplifier power *----------*/

/* Update the Axim-specific GPIOs so that power amplifier is enabled/disabled
 * as appropiate.
 */
static int aximx5_amplifier (struct ac97_codec *codec, int on)
{
	static int power_on = 0;

	if (on)
		on = 1;
	if (power_on != on) {
		mq_base->set_power (mq_base, MEDIAQ_11XX_SPI_DEVICE_ID, on);
		power_on = on;
	}

	/* GPIO24 seems to be amplifier power, GPIO23 amplifier mute.
	 * When the sound is disabled, we disable GPIO23 first; when
	 * the sound is enabled, we do it vice versa. This avoids
	 * loud clicks in the speaker.
	 */
	set_task_state (current, TASK_INTERRUPTIBLE);
	if (on) {
		mq_base->set_GPIO (mq_base, 24, MQ_GPIO_OUT1);
		schedule_timeout(HZ/4);
		mq_base->set_GPIO (mq_base, 23, MQ_GPIO_OUT1);
	} else {
		mq_base->set_GPIO (mq_base, 23, MQ_GPIO_OUT0);
		schedule_timeout(HZ/4);
		mq_base->set_GPIO (mq_base, 24, MQ_GPIO_OUT0);
	}

	return 0;
}

static int __init
aximx5_ac97_init(void)
{
	wm97xx_params params;
	int mixer;

	/* Pull in the DMA touchscreen driver */
	//driver_pxa_ac97_wm97xx = 1;

	tspub = wm97xx_get_device (0);
	if (unlikely (!tspub)) {
		printk (KERN_INFO "The WM9705 chip has not been initialized\n");
		return -ENODEV;
	}

	/* Retrieve current driver settings */
	tspub->get_params (tspub, &params);
	/* On Axim a delay of 3 AC97 frame works ok */
	params.delay = 3;
	/* IRQ55 is touchscreen */
	params.pen_irq = AXIMX5_IRQ (TOUCHSCREEN);
	/* On Axim X5 the bottom-left is touchscreen origin, while the
	 * graphics origin is in top-left.
	 */
	params.invert = 2;
	/* Use a low pen detect threshold, otherwise on low pressure
	 * we often get very biased ADC readings.
	 */
	if (!params.pdd)
		params.pdd = 1;
	if (!params.pil)
		params.pil = 1;

	tspub->set_params (tspub, &params);
	tspub->apply_params (tspub);

	set_irq_type(AXIMX5_IRQ (TOUCHSCREEN), IRQT_BOTHEDGE);

	mq_driver_get ();
	mq_device_enum (&mq_base, 1);
        if (mq_base) {
		tspub->codec->codec_ops->amplifier = aximx5_amplifier;
		/* Initialize master volume */
		mixer = tspub->codec->read_mixer (tspub->codec, SOUND_MIXER_VOLUME);
		tspub->codec->write_mixer (tspub->codec, SOUND_MIXER_VOLUME, 0, 0);
		tspub->codec->write_mixer (tspub->codec, SOUND_MIXER_VOLUME,
					   mixer & 0xff, mixer >> 8);
        }

	/* Start BMON sampling ... */
	tspub->setup_auxconv (tspub, WM97XX_AC_BMON, BMON_AUX_SAMPLE_INTERVAL,
			      NULL);
	/* Start AUX sampling ... */
	tspub->setup_auxconv (tspub, WM97XX_AC_AUX1, BMON_AUX_SAMPLE_INTERVAL,
			      wm97xx_prepare_aux);
	/* Register main Axim battery */
	battery_class_register (&aximx5_main_battery);
	/* ... and the backup battery */
	battery_class_register (&aximx5_backup_battery);

	return 0;
}

static void __exit 
aximx5_ac97_exit(void)
{
	tspub->setup_auxconv (tspub, WM97XX_AC_BMON, 0, NULL);
	tspub->setup_auxconv (tspub, WM97XX_AC_AUX1, 0, NULL);

	tspub->codec->codec_ops->amplifier = NULL;
	mq_driver_put ();
}

module_init(aximx5_ac97_init);
module_exit(aximx5_ac97_exit);

MODULE_AUTHOR("Andrew Zabolotny <zap@homelink.ru>");
MODULE_DESCRIPTION("AC97 controller driver for Dell Axim X5");
MODULE_LICENSE("GPL");

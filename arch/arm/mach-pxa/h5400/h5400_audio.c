/*
 * Hardware audio controls for HP iPAQ h5400
 *
 * Copyright © 2005 Erik Hovland <erik@hovland.org>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/pm.h>
#include <sound/ak4535.h>
#include <linux/device.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/arch/h5400-asic.h>
#include <linux/platform_device.h>

#include <linux/mfd/samcop_base.h>

/* It's up to the <machine>_audio drive to initialize 
 * the ak4535_device_controls variable, as it's needed
 * to initialize the ak4535 module.
 */
 
ak4535_device_controls_t ak4535_device_controls;

static void h5400_codec_set_power (int mode)
{
	samcop_set_gpio_b (&h5400_samcop.dev,
			   SAMCOP_GPIO_GPB_CODEC_POWER_ON,
			   mode);
}

static void h5400_audio_set_power (int mode)
{
	samcop_set_gpio_b (&h5400_samcop.dev,
			   SAMCOP_GPIO_GPB_AUDIO_POWER_ON,
			   mode);
}

static void h5400_audio_set_mute (int mode)
{
}

static int __init h5400_audio_init(void)
{
	ak4535_device_controls.set_codec_power = h5400_codec_set_power;
	ak4535_device_controls.set_audio_power = h5400_audio_set_power;
	ak4535_device_controls.set_audio_mute = h5400_audio_set_mute;
	return (0);
}

static void __exit h5400_audio_exit(void)
{
	ak4535_device_controls.set_codec_power = NULL;
	ak4535_device_controls.set_audio_power = NULL;
	ak4535_device_controls.set_audio_mute = NULL;
}


module_init(h5400_audio_init);
module_exit(h5400_audio_exit);

EXPORT_SYMBOL(ak4535_device_controls);
MODULE_AUTHOR("Erik Hovland <erik@hovland.org>");
MODULE_DESCRIPTION("Hardware audio controls for HP iPAQ h5xxx");
MODULE_LICENSE("GPL");

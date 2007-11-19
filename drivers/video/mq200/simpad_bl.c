/*
 * GPLv2 <zecke@handhelds.org
 *
 * Implementation of the backlight_driver for
 * the mq200 framebuffer
 *
 */
#include <asm/types.h>
#include <asm/hardware.h>
#include <asm/io.h>

#include <linux/device.h>
#include <linux/backlight.h>
#include <linux/fb.h>

#include "mq200_data.h"


#define SIMPAD_BACKLIGHT_MASK	0x00a10044
#define MAX_BRIGHT 254

#define mq200_p2v( x )		\
   (((x) - 0x4b800000) + 0xf2800000)
#define READ32(x)   readl (x)
#define REG32(x,y)  writel(y,x)
#define PWM_CONTROL 0xF2E0E03C


static int simpad_bl_get_brightness(struct backlight_device* dev)
{
    u32 pwmctl;

    pwmctl = *((volatile unsigned long*) mq200_p2v(PWM_CONTROL));
    pwmctl &= ~SIMPAD_BACKLIGHT_MASK;
    pwmctl = pwmctl >> 8;
    pwmctl = MAX_BRIGHT - pwmctl;

    return pwmctl;
}

static int simpad_bl_set_brightness(struct backlight_device* dev, int bright)
{
    unsigned long dutyCycle, pwmcontrol;

    if(bright > MAX_BRIGHT)
	bright = MAX_BRIGHT;


    /*
     * Determine dutyCycle.
     * Note: the lower the value, the brighter the display!
     */

    dutyCycle = MAX_BRIGHT - bright;

    /*
     *Configure PWM0 (source clock = oscillator clock, pwm always enabled,
     *zero, clock pre-divider = 4) pwm frequency = 12.0kHz
     */
    pwmcontrol = READ32(PWM_CONTROL);
    REG32(PWM_CONTROL, 0x00000044 | (pwmcontrol & 0xffffff00));

    /* Write to pwm duty cycle register.  */
    REG32(PWM_CONTROL, ((dutyCycle << 8) & 0x0000ff00) |
	  (pwmcontrol & 0xffff00ff));

    /* see if we need to enable/disable the backlight */

    return 0;
}

static int simpad_bl_set_power(struct backlight_device *bd, int state)
{
    return 0;
}

static int simpad_bl_get_power(struct backlight_device *bd)
{
    return FB_BLANK_UNBLANK;
}

static struct backlight_properties simpad_bl_props = {
    .owner = THIS_MODULE,
    .max_brightness = MAX_BRIGHT,
    .get_brightness = simpad_bl_get_brightness,
    .set_brightness = simpad_bl_set_brightness,
    .get_power      = simpad_bl_get_power,
    .set_power      = simpad_bl_set_power,
};

static struct backlight_device *simpad_bl_device = NULL;
static int __init simpad_bl_init(void) {
    simpad_bl_device = backlight_device_register("mq200_fb0", NULL,
						 &simpad_bl_props);
    return simpad_bl_device != NULL;
}

static void __exit simpad_bl_exit(void) {
    backlight_device_unregister(simpad_bl_device);
}


module_init(simpad_bl_init);
module_exit(simpad_bl_exit);
MODULE_AUTHOR("Holger Hans Peter Freyther");
MODULE_LICENSE("GPL");


/*
 * GPLv2 <zecke@handhelds.org
 *
 * Implementation of the lcd_driver for the mq200 framebuffer
 *
 */
#include <asm/arch/simpad.h>
#include <asm/hardware.h>

#include <linux/device.h>
#include <linux/lcd.h>

extern long get_cs3_shadow(void);
extern void set_cs3_bit(int);
extern void clear_cs3_bit(int);

#define UNUSED(x) x=x

static int simpad_lcd_get_power(struct lcd_device* dev)
{
    UNUSED(dev);

    return (get_cs3_shadow() & DISPLAY_ON) ? 0 : 4;
}

static int simpad_lcd_set_power(struct lcd_device* dev, int power)
{
    UNUSED(dev);

    if( power == 4 )
	clear_cs3_bit(DISPLAY_ON);
    else
	set_cs3_bit(DISPLAY_ON);

    return 0;
}

static struct lcd_properties simpad_lcd_props = {
    .owner      = THIS_MODULE,
    .get_power    = simpad_lcd_get_power,
    .set_power    = simpad_lcd_set_power,
    .max_contrast = 0
};

static struct lcd_device* simpad_lcd_device = NULL;

static int __init simpad_lcd_init(void) {
    simpad_lcd_device = lcd_device_register("mq200_fb0", NULL,
					    &simpad_lcd_props);
    return simpad_lcd_device != NULL;
}

static void __exit simpad_lcd_exit(void) {
    lcd_device_unregister(simpad_lcd_device);
}

module_init(simpad_lcd_init);
module_exit(simpad_lcd_exit);

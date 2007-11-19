/*
 * Support for XScale PWM driven backlights.
 *
 * Authors: Vladimir "Farcaller" Pouzanov <farcaller@gmail.com>
 *          Alex Osborne <bobofdoom@gmail.com>
 *
 * Based on Sharp Corgi driver
 *
 */

#warning ---------------------------------------------------------------------
#warning PXA PWM backlight driver is deperecated, please use corgi_bl instead.
#warning ---------------------------------------------------------------------

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/pxapwm-bl.h>

static struct backlight_ops pxapwmbl_ops;
static void (*pxapwmbl_mach_turn_bl_on)(void);
static void (*pxapwmbl_mach_turn_bl_off)(void);

static void pxapwmbl_send_intensity(struct pxapwmbl_platform_data *bl, int intensity)
{
	unsigned long flags;
	unsigned int period, prescaler;
	
	if (bl->limit)
		intensity &= bl->limit_mask;

	// set intensity to 0 if in power saving or fb is blanked
	if (bl->dev->props.power != FB_BLANK_UNBLANK)
		intensity = 0;
	if (bl->dev->props.fb_blank != FB_BLANK_UNBLANK)
		intensity = 0;

	/* Does the backlight frequency need to be configured on a per device basis?
	 * What are sensible defaults? Farcaller had prescaler 2, period 0x12bi (~6.1khz).
	 * LD is configured for 1.2khz.
	 *
	 * h2200_lcd says "450 Hz which is quite enough."
	 */
	if (intensity) {
		period = bl->period;
		prescaler = bl->prescaler; 
	} else {
		period = 0;
		prescaler=0;
	}
	
	//printk("pxapwmbl: setting intensity to %d [bl->intensity=%d]\n", intensity, bl->intensity);
	spin_lock_irqsave(&bl->lock, flags);

	// poweron backlight if possible and needed
	if (pxapwmbl_mach_turn_bl_on){
		if ((bl->intensity <= bl->off_threshold) && (intensity > bl->off_threshold)){
	    		pxapwmbl_mach_turn_bl_on();
		}
	}

	//printk("pxapwmbl: setting intensity to %d [bl->intensity = %d]\n", intensity, bl->intensity);
	switch (bl->pwm) {
		case 0:
			PWM_CTRL0   = prescaler;
			PWM_PWDUTY0 = intensity;
			PWM_PERVAL0 = period;
			break;
		case 1:
			PWM_CTRL1   = prescaler;
			PWM_PWDUTY1 = intensity;
			PWM_PERVAL1 = period;
			break;
			
		/* Note: PXA270 has 2 more PWMs, however the registers don't seem to be
		 * defined in pxa-regs.h. If you have device which uses PWM 2 or 3, 
		 * you'll need to add them here.
		 */
			
		default:
			printk(KERN_ERR "pxapwmbl_send_intensity: invalid PWM selected in"
					" platform data.\n");
	}

	// poweroff backlight if possible and needed
	if (pxapwmbl_mach_turn_bl_off){
		if ((bl->intensity > bl->off_threshold) && (intensity <= bl->off_threshold)){
			pxapwmbl_mach_turn_bl_off();
		}
	}

	bl->dev->props.brightness = intensity;
	bl->intensity=intensity;
	
	spin_unlock_irqrestore(&bl->lock, flags);
}

#ifdef CONFIG_PM
static int stored_intensity = 0;
static int pxapwmbl_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct pxapwmbl_platform_data *bl = pdev->dev.platform_data;
	stored_intensity = bl->intensity;
	pxapwmbl_send_intensity(bl, 0);
	return 0;
}

static int pxapwmbl_resume(struct platform_device *pdev)
{
	struct pxapwmbl_platform_data *bl = pdev->dev.platform_data;
	pxapwmbl_send_intensity(bl,stored_intensity);
	return 0;
}
#else
#define pxapwmbl_suspend NULL
#define pxapwmbl_resume	NULL
#endif

static int pxapwmbl_update_status(struct backlight_device *bd)
{
	int intensity;
	struct pxapwmbl_platform_data *bl = class_get_devdata(&bd->class_dev);
	
	intensity = bd->props.brightness;

	if (intensity > bd->props.max_brightness)
		intensity = bd->props.max_brightness;

	if (intensity < 0)
		intensity = 0;

	pxapwmbl_send_intensity(bl, intensity);

	return 0;
}

static int pxapwmbl_get_intensity(struct backlight_device *bd)
{
	struct pxapwmbl_platform_data *bl = class_get_devdata(&bd->class_dev);
	return bl->intensity;
}

/*
 * Called when the battery is low to limit the backlight intensity.
 * If limit==0 clear any limit, otherwise limit the intensity
 */
void pxapwmbl_limit_intensity(struct platform_device *pdev, int limit)
{
	struct pxapwmbl_platform_data *bl = pdev->dev.platform_data;

	bl->limit = (limit ? 1 : 0);
	pxapwmbl_send_intensity(bl, bl->intensity);
}
EXPORT_SYMBOL(pxapwmbl_limit_intensity);

static struct backlight_ops pxapwmbl_ops = {
	.get_brightness	= pxapwmbl_get_intensity,
	.update_status	= pxapwmbl_update_status,
};

static int pxapwmbl_probe(struct platform_device *pdev)
{
	struct pxapwmbl_platform_data *bl = pdev->dev.platform_data;

	bl->dev = backlight_device_register ("pxapwm-bl", &pdev->dev,
					     bl, &pxapwmbl_ops);

	bl->dev->props.max_brightness = bl->max_intensity;

	pxapwmbl_mach_turn_bl_on  = bl->turn_bl_on;
	pxapwmbl_mach_turn_bl_off = bl->turn_bl_off;

	bl->limit = 0;
	bl->lock = SPIN_LOCK_UNLOCKED;

	bl->dev->props.brightness = bl->default_intensity;
	bl->dev->props.power = FB_BLANK_UNBLANK;
	bl->dev->props.fb_blank = FB_BLANK_UNBLANK;

	bl->off_threshold = 5;
		
	if (IS_ERR (bl->dev))
		return PTR_ERR (bl->dev);

	platform_set_drvdata(pdev, bl->dev);

	pxapwmbl_send_intensity(bl, bl->default_intensity);
	CKEN = CKEN | (bl->pwm?CKEN1_PWM1:CKEN0_PWM0);
	bl->intensity = bl->default_intensity;
	pxapwmbl_limit_intensity(pdev, 0);

	printk("PXA PWM backlight driver initialized.\n");
	return 0;
}

static int pxapwmbl_remove(struct platform_device *pdev)
{
	struct pxapwmbl_platform_data *bl = pdev->dev.platform_data;

	pxapwmbl_send_intensity(bl, bl->default_intensity);

	backlight_device_unregister(bl->dev);

	printk("PXA PWM backlight driver unloaded\n");
	return 0;
}

static struct platform_driver pxapwmbl_driver = {
	.probe		= pxapwmbl_probe,
	.remove		= pxapwmbl_remove,
	.suspend	= pxapwmbl_suspend,
	.resume		= pxapwmbl_resume,
	.driver		= {
		.name	= "pxapwm-bl",
	},
};

static int __init pxapwmbl_init(void)
{
	int pdreg = platform_driver_register(&pxapwmbl_driver);

	return pdreg;
}

static void __exit pxapwmbl_exit(void)
{
	platform_driver_unregister(&pxapwmbl_driver);
}

module_init(pxapwmbl_init);
module_exit(pxapwmbl_exit);

MODULE_AUTHOR("Vladimir Pouzanov <farcaller@gmail.com>");
MODULE_DESCRIPTION("XScale PWM Backlight Driver");
MODULE_LICENSE("GPL");

#ifndef _HTCUNIVERSAL_LEDS_H
#define _HTCUNIVERSAL_LEDS_H

#define HTC_PHONE_BL_LED	3
#define HTC_KEYBD_BL_LED	4
#define	HTC_VIBRA		5
#define HTC_CAM_FLASH_LED	8

void htcuniversal_led_brightness_set(struct led_classdev *led_cdev, enum led_brightness value);

#endif

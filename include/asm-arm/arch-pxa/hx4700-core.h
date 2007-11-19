/* Core Hardware driver for Hx4700 (ASIC3, EGPIOs)
 *
 * Copyright (c) 2005-2006 SDG Systems, LLC
 *
 * 2005-03-29   Todd Blumer       Converted basic structure to support hx4700
 */

#ifndef _HX4700_CORE_H
#define _HX4700_CORE_H
#include <linux/leds.h>

#define EGPIO0_VCC_3V3_EN		(1<<0)	/* WLAN support chip power */
#define EGPIO1_WL_VREG_EN		(1<<1)	/* WLAN Power */
#define EGPIO2_VCC_2V1_WL_EN		(1<<2)	/* unused */
#define EGPIO3_SS_PWR_ON		(1<<3)	/* smart slot power on */
#define EGPIO4_CF_3V3_ON		(1<<4)	/* CF 3.3V enable */
#define EGPIO5_BT_3V3_ON		(1<<5)	/* Bluetooth 3.3V enable */
#define EGPIO6_WL1V8_EN			(1<<6)	/* WLAN 1.8V enable */
#define EGPIO7_VCC_3V3_WL_EN		(1<<7)	/* WLAN 3.3V enable */
#define EGPIO8_USB_3V3_ON		(1<<8)	/* unused */

void hx4700_egpio_enable( u_int16_t bits );
void hx4700_egpio_disable( u_int16_t bits );

struct hx4700_core_funcs {
    int (*udc_detect)( void );
};

EXTERN_LED_TRIGGER_SHARED(hx4700_radio_trig);

#endif /* _HX4700_CORE_H */

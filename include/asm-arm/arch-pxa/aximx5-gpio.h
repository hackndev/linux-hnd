/*
 * Dell Axim X5 GPIO definitions.
 *
 * Authors: Martin Demin <demo@twincar.sk>,
 *          Andrew Zabolotny <anpaza@mail.ru>
 *
 * Here are the definition of most of the Dell Axim GPIOs.
 * Keep in mind that this file is a result of a great reverse-engineering
 * work, and not from a Dell source, thus some definitions may be wrong
 * (especially those that aren't used yet in drivers). These are usually
 * marked either by "Unconfirmed guess" comments, or a ??? in comment.
 *
 * Only GPIOs with alternate function=0 are defined here (even if unknown
 * there is a comment saying it is unknown, and this means they use AF=0).
 */

#ifndef _AXIMX5_GPIO_H_
#define _AXIMX5_GPIO_H_

#define GET_AXIMX5_GPIO(gpio) \
	(GPLR(GPIO_NR_AXIMX5_ ## gpio) & GPIO_bit(GPIO_NR_AXIMX5_ ## gpio))

#define SET_AXIMX5_GPIO(gpio, setp) \
do { \
if (setp) \
	GPSR(GPIO_NR_AXIMX5_ ## gpio) = GPIO_bit(GPIO_NR_AXIMX5_ ## gpio); \
else \
	GPCR(GPIO_NR_AXIMX5_ ## gpio) = GPIO_bit(GPIO_NR_AXIMX5_ ## gpio); \
} while (0)

#define SET_AXIMX5_GPIO_N(gpio, setp) \
do { \
if (setp) \
	GPCR(GPIO_NR_AXIMX5_ ## gpio ## _N) = GPIO_bit(GPIO_NR_AXIMX5_ ## gpio ## _N); \
else \
	GPSR(GPIO_NR_AXIMX5_ ## gpio ## _N) = GPIO_bit(GPIO_NR_AXIMX5_ ## gpio ## _N); \
} while (0)

#define AXIMX5_IRQ(gpio) \
	IRQ_GPIO(GPIO_NR_AXIMX5_ ## gpio)

#define GPIO_NR_AXIMX5_POWER_BUTTON_N		0
#define GPIO_NR_AXIMX5_RESET_BUTTON_N		1
#define GPIO_NR_AXIMX5_MQ1132_INT		2 /* Interrupt from MediaQ 1132 */
#define GPIO_NR_AXIMX5_BATTERY_LATCH		3 /* Battery latch closed (1) or open (0) */
#define GPIO_NR_AXIMX5_CRADLE_DETECT_N		4 /* PDA is in cradle (0) or out of cradle (1) */
/* GPIO5 is unknown */
#define GPIO_NR_AXIMX5_SD_DETECT_N		6 /* SD/MMC card inserted (1) */
#define GPIO_NR_AXIMX5_PCMCIA_DETECT_N		7 /* CF card inserted (1) */
#define GPIO_NR_AXIMX5_PCMCIA_BVD1		8 /* Unconfirmed guess */
#define GPIO_NR_AXIMX5_MAINBAT_RDY		9 /* Main battery inserted (huh?) */
#define GPIO_NR_AXIMX5_BUTTON_RECORD		10
#define GPIO_NR_AXIMX5_BUTTON_CALENDAR		11
#define GPIO_NR_AXIMX5_BUTTON_CONTACTS		12
#define GPIO_NR_AXIMX5_BUTTON_MAILBOX		13
#define GPIO_NR_AXIMX5_BUTTON_HOME		14
#define GPIO_NR_AXIMX5_AC_POWER_N		17 /* AC power (0) or battery power (1) */
#define GPIO_NR_AXIMX5_RS232_DCD		19 /* Power to serial level converter */
/* GPIO20 is unknown (output) */
/* GPIO21 is unknown (input) */
#define GPIO_NR_AXIMX5_USB_PULL_UP_N		22
/* GPIO23 is unknown (output) */
/* GPIO24 is unknown (output) */
#define GPIO_NR_AXIMX5_POWER_OK			25 /* Power source is enough ??? */
#define GPIO_NR_AXIMX5_PCMCIA_ADD_EN_N		26 /* !enable address to be driven to PCMCIA bus */
#define GPIO_NR_AXIMX5_JOY_PRESSED		27 /* Joystick pressed (any of JOY_? bits); 0 - down  */
#define GPIO_NR_AXIMX5_TOUCHSCREEN		32 /* Touchscreen comparator */
/* GPIO58 is unknown (output) */
#define GPIO_NR_AXIMX5_CHARGING			59
#define GPIO_NR_AXIMX5_FLASH_WE			60 /* Flash write enable */
#define GPIO_NR_AXIMX5_PCMCIA_BVD2		61 /* Unconfirmed guess */
#define GPIO_NR_AXIMX5_PCMCIA_RESET		62
#define GPIO_NR_AXIMX5_PCMCIA_BUFF_EN_N		63 /* enable PCMCIA buffers ??? */
#define GPIO_NR_AXIMX5_JOY_NW			64 /* 0 - down, 1 - up */
#define GPIO_NR_AXIMX5_JOY_NE			65 /* 0 - down, 1 - up */
#define GPIO_NR_AXIMX5_JOY_SE			66 /* 0 - down, 1 - up */
#define GPIO_NR_AXIMX5_JOY_SW			67 /* 0 - down, 1 - up */
#define GPIO_JOY_DIR				(GPLR2 & 15)
/* The up, down, left and right joystick directions are given by combination
 * of two respective buttons. Most of the time the joystick "center" button
 * (GPIO27) is also pressed together with above buttons, although when
 * pressing it carefully you can trigger just the above GPIOs, without 27.
 */
#define GPIO_NR_AXIMX5_SCROLL_DOWN		68
#define GPIO_NR_AXIMX5_SCROLL_PUSH		69
#define GPIO_NR_AXIMX5_SCROLL_UP		70
#define GPIO_NR_AXIMX5_MQ1132_POWER_N		71 /* MediaQ power */
#define GPIO_NR_AXIMX5_MQ1132_ADD_EN		72 /* MediaQ address bus enable */
#define GPIO_NR_AXIMX5_LED_MANUAL_SELECT_N	73 /* Enable LED select via MediaQ GPIO54 */
#define GPIO_NR_AXIMX5_LED_CHARGER_SELECT	74 /* Enable LED select by charger */
#define GPIO_NR_AXIMX5_LED_GREEN_ENABLE_N	80 /* Enable Green LED (0) */

/* GPIOs 75-77 determine the type of cradle that has been plugged in */
#define AXIMX5_CONNECTOR_TYPE \
	((GPLR2 >> (75 - 64)) & 7)
#define AXIMX5_CONNECTOR_IS_SERIAL(x) \
	((x) == 2 || (x) == 4)
#define AXIMX5_CONNECTOR_IS_USB(x) \
        ((x) == 3 || (x) == 6)
#define AXIMX5_CONNECTOR_IS_CRADLE(x) \
        ((x) < 4)

/*------------------------------- MediaQ 1132 GPIOs --------------------------*/

/* GPIO0 -- seems unused */
#define GPIO_NR_AXIMX5_MQ1132_FP_ENABLE		1 /* enable the flat panel */
#define GPIO_NR_AXIMX5_MQ1132_CF_IREQ		2 /* the CompactFlash interrupt request */
/* GPIO20 -- vertical scan direction ??? */
/* GPIO21 -- unknown ??? default output 0 */
/* GPIO22 -- also somehow related to flat panel; 0 - image is disabled */
#define GPIO_NR_AXIMX5_MQ1132_AMP_MUTE		23 /* Mute the amplifier */
#define GPIO_NR_AXIMX5_MQ1132_AMP_SHUTDOWN	24 /* Shutdown the amplifier */
/* GPIO25 -- unknown ??? default output 1 */
#define GPIO_NR_AXIMX5_MQ1132_LED_SELECT	54 /* 0 - green; 1 - yellow */
/* GPIO55 -- unused ??? maybe somehow SD/MMC related? */
/* GPIO60 -- seems unused */
#define GPIO_NR_AXIMX5_MQ1132_BACKUP_LOAD	61 /* Connect a 3.3k resistor to backup battery */
/* GPIO62 -- unknown ??? default input/output 1 */
#define GPIO_NR_AXIMX5_MQ1132_IR_POWER		63 /* Infrared transceiver power */
#define GPIO_NR_AXIMX5_MQ1132_BACKLIGHT		64 /* Enable backlight */
#define GPIO_NR_AXIMX5_MQ1132_PCMCIA_POWER	65 /* Enable power to PCMCIA slot */
#define GPIO_NR_AXIMX5_MQ1132_BATTERY_TYPE	66 /* Detect battery: (0)3400mAh, (1)1400mAh */

#endif /* _AXIMX5_GPIO_H_ */

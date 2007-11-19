/*
 * Asus Mypal 600/620 GPIO definitions.
 *
 * Author: Nicolas Pouillon <nipo@ssji.net>
 *
 * 2006-02-08 by Vincent Benony : adding GPO bit definitions, and small updates
 *
 */

#ifndef _ASUS_620_GPIO_
#define _ASUS_620_GPIO_

#define GET_A620_GPIO(gpio) \
	(GPLR(GPIO_NR_A620_ ## gpio) & GPIO_bit(GPIO_NR_A620_ ## gpio))

#define SET_A620_GPIO(gpio, setp) \
do { \
if (setp) \
	GPSR(GPIO_NR_A620_ ## gpio) = GPIO_bit(GPIO_NR_A620_ ## gpio); \
else \
	GPCR(GPIO_NR_A620_ ## gpio) = GPIO_bit(GPIO_NR_A620_ ## gpio); \
} while (0)

#define SET_A620_GPIO_N(gpio, setp) \
do { \
if (setp) \
	GPCR(GPIO_NR_A620_ ## gpio ## _N) = GPIO_bit(GPIO_NR_A620_ ## gpio ## _N); \
else \
	GPSR(GPIO_NR_A620_ ## gpio ## _N) = GPIO_bit(GPIO_NR_A620_ ## gpio ## _N); \
} while (0)

#define A620_IRQ(gpio) \
	IRQ_GPIO(GPIO_NR_A620_ ## gpio)

#define GPIO_NR_A620_POWER_BUTTON_N		(0)
/* 1 is reboot non maskable */
#define GPIO_NR_A620_HOME_BUTTON_N		(2)
#define GPIO_NR_A620_CALENDAR_BUTTON_N	(3)
#define GPIO_NR_A620_CONTACTS_BUTTON_N	(4)
#define GPIO_NR_A620_TASKS_BUTTON_N		(5)
/* 6 missing */

/* hand probed */
#define GPIO_NR_A620_CF_IRQ			(7)
/* hand probed */
#define GPIO_NR_A620_CF_BVD1			(8)

/* Not sure, just a guess */
#define GPIO_NR_A620_CF_READY_N			(9)

#define GPIO_NR_A620_USB_DETECT			(10)
#define GPIO_NR_A620_RECORD_BUTTON_N		(11)
#define GPIO_NR_A620_HEARPHONE_N		(12)
/* 13 missing (output) */
#define GPIO_NR_A620_CF_RESET			(13)
/* hand probed */
#define GPIO_NR_A620_CF_POWER			(14)
#define GPIO_NR_A620_COM_DETECT_N		(15)
#define GPIO_NR_A620_BACKLIGHT			(16)
/* Not sure, guessed */
#define GPIO_NR_A620_CF_ENABLE			(17)


/*
 * 18 Power detect, see below
 * 19 missing (output)
 *
 * 18,20
 * going 0/1 with Power
 * input
 * 18: Interrupt on none
 * 20: Interrupt on RE/FE
 *
 * Putting GPIO_NR_A620_AC_IN on 20 is arbitrary
 */
#define GPIO_NR_A620_AC_IN				(20)

/* hand probed */
#define GPIO_NR_A620_CF_VS1_N			(21)
#define GPIO_NR_A620_CF_DETECT_N		(21)
#define GPIO_NR_A620_CF_VS2_N			(22)

/* 23-26 is SSP to AD7873 */
#define GPIO_NR_A620_STYLUS_IRQ			(27)

/*
 * 28-32 IIS to UDA1380
 *
 * 33 USB pull-up
 * Must be input when disconnected (floating)
 * Must be output set to 1 when connected
 */
#define GPIO_NR_A620_USBP_PULLUP		(33)

/*
 * Serial connector has no handshaking
 * 34,39 Serial port
 *
 * 38 missing
 *
 * Joystick directions and button
 * Center is 35
 * Directions triggers one or two of 36,67,40,41
 *
 *   36   36,37     37
 *
 *  36,41   35    37,40
 *
 *   41   40,41     40
 */

#define GPIO_NR_A620_JOY_PUSH_N			(35)
#define GPIO_NR_A620_JOY_NW_N			(36)
#define GPIO_NR_A620_JOY_NE_N			(37)
#define GPIO_NR_A620_JOY_SE_N			(40)
#define GPIO_NR_A620_JOY_SW_N			(41)

#define GPIO_A620_JOY_DIR	(((GPLR1&0x300)>>6)|((GPLR1&0x30)>>4))

/*
 * 42-45 Bluetooth
 * 46-47 IrDA
 *
 * 48-57 Card ?
 * 54 is led control
 */
#define GPIO_NR_A620_LED_ENABLE			(54)

/*
 *
 * 58 through 77 is LCD signals
 * 74,75 seems related to TCON chip
 *  -74 is TCON presence probing
 *  -75 is set to GAFR2 when TCON is here
 */

#define GPIO_NR_A620_TCON_HERE_N		(74)
#define GPIO_NR_A620_TCON_EN			(75)

/*
 * Power management (scaling to 200, 300, 400MHz)
 * Chart is:
 *     78 79
 * 200  1  0
 * 300  0  1
 * 400  1  1
 */
#define GPIO_NR_A620_PWRMGMT0			(78)
#define GPIO_NR_A620_PWRMGMT1			(79)

/*
 * 81-84 missing
 */



/*
 * Other infos
 */

#define ASUS_IDE_BASE 			0x00000000
#define ASUS_IDE_SIZE  			0x04000000
#define ASUS_IDE_VIRTUAL		0xF8000000

#define GPO_A620_USB			(1 <<  0) // 0x00000001
#define GPO_A620_LCD_POWER1		(1 <<  1) // 0x00000002
#define GPO_A620_LCD_POWER2		(1 <<  2) // 0x00000004
#define GPO_A620_LCD_POWER3		(1 <<  3) // 0x00000008
#define GPO_A620_BACKLIGHT		(1 <<  4) // 0x00000010
#define GPO_A620_BLUETOOTH		(1 <<  5) // 0x00000020
#define GPO_A620_MICROPHONE		(1 <<  6) // 0x00000040
#define GPO_A620_SOUND			(1 <<  7) // 0x00000080
#define GPO_A620_UNK_1			(1 <<  8) // 0x00000100
#define GPO_A620_BLUE_LED		(1 <<  9) // 0x00000200
#define GPO_A620_UNK_2			(1 << 10) // 0x00000400
#define GPO_A620_RED_LED		(1 << 11) // 0x00000800
#define GPO_A620_UNK_3			(1 << 12) // 0x00001000
#define GPO_A620_CF_RESET		(1 << 13) // 0x00002000
#define GPO_A620_IRDA_FIR_MODE		(1 << 14) // 0x00004000
#define GPO_A620_IRDA_POWER_N		(1 << 15) // 0x00008000

#define GPIO_I2C_EARPHONES_DETECTED       (1 << 2) // 0x00000004

void asus620_gpo_set(unsigned long bits);
void asus620_gpo_clear(unsigned long bits);


#endif /* _ASUS_620_GPIO_ */

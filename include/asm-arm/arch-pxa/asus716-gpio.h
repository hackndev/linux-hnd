/*
 * Asus Mypal 716 GPIO definitions.
 */

#ifndef _ASUS_716_GPIO_
#define _ASUS_716_GPIO_

#define A716_GPIO(gpio) (GPIO_NR_A716_ ## gpio)

#define GET_A716_GPIO(gpio) \
	(GPLR(GPIO_NR_A716_ ## gpio) & GPIO_bit(GPIO_NR_A716_ ## gpio))

#define SET_A716_GPIO(gpio, setp) \
do { \
if (setp) \
	GPSR(GPIO_NR_A716_ ## gpio) = GPIO_bit(GPIO_NR_A716_ ## gpio); \
else \
	GPCR(GPIO_NR_A716_ ## gpio) = GPIO_bit(GPIO_NR_A716_ ## gpio); \
} while (0)

#define SET_A716_GPIO_N(gpio, setp) \
do { \
if (setp) \
	GPCR(GPIO_NR_A716_ ## gpio ## _N) = GPIO_bit(GPIO_NR_A716_ ## gpio ## _N); \
else \
	GPSR(GPIO_NR_A716_ ## gpio ## _N) = GPIO_bit(GPIO_NR_A716_ ## gpio ## _N); \
} while (0)

#define A716_IRQ(gpio) \
	IRQ_GPIO(GPIO_NR_A716_ ## gpio)

#define GPIO_NR_A716_POWER_BUTTON_N		(0)
/* 1 is reboot non maskable */		//	(1)
#define GPIO_NR_A716_HOME_BUTTON_N		(2)
#define GPIO_NR_A716_TASKS_BUTTON_N		(3)
#define GPIO_NR_A716_CALENDAR_BUTTON_N		(4)
#define GPIO_NR_A716_CONTACTS_BUTTON_N		(5)
/* 6 MMC Clock */
#define GPIO_NR_A716_CF_READY			(7)
#define GPIO_NR_A716_CF_IRQ			(8)
#define GPIO_NR_A716_CF_CARD_DETECT_N		(9)
#define GPIO_NR_A716_SD_CARD_DETECT_N		(10)
#define GPIO_NR_A716_BLUETOOTH_READY		(11)
#define GPIO_NR_A716_USB_CABLE_DETECT_N		(12)
/* 13-14 unknown */
#define GPIO_NR_A716_AC_DETECT			(15)
#define GPIO_NR_A716_BACKLIGHT			(16)
#define GPIO_NR_A716_BLUE_LED_ENABLE		(17)
/* 18 unknown */
#define GPIO_NR_A716_PCMCIA_SOME		(19)
/* 20-22 unknown */
/* 23-26 is SSP to AD7873 */
#define GPIO_NR_A716_STYLUS_IRQ_N		(27)
/* 28-32 I2S to UDA1380 */
#define GPIO_NR_A716_PCA9535_IRQ		(33)
/* 34-37 Serial Port */
#define GPIO_NR_A716_WIFI_READY			(38)
/* 39-41 Serial Cable Port */
/* 42-45 BlueTooth Port */
/* 46-47 IrDA */
/* 48-57 Card Space (Pcmcia) */
/* 58-73 LCD signals */
#define GPIO_NR_A716_LCD_ENABLE			(74)
/* 75-77 LCD signals */
/* 78 unknown */
#define GPIO_NR_A716_SD_SLOT_EXIST		(79)
/* 80 nCS4 */


/*
 * Asus Mypal 716 GPO definitions.
 */

#define GPO_A716_USB			(1 <<  0) // 0x00000001
#define GPO_A716_LCD_POWER1		(1 <<  1) // 0x00000002
#define GPO_A716_LCD_POWER2		(1 <<  2) // 0x00000004
#define GPO_A716_LCD_POWER3		(1 <<  3) // 0x00000008
#define GPO_A716_BACKLIGHT		(1 <<  4) // 0x00000010
#define GPO_A716_WIFI_1p8V		(1 <<  5) // 0x00000020
#define GPO_A716_MICROPHONE_N		(1 <<  6) // 0x00000040
#define GPO_A716_UNK1			(1 <<  7) // 0x00000080
#define GPO_A716_FIR_MODE		(1 <<  8) // 0x00000100
#define GPO_A716_IRDA_POWER_N		(1 <<  9) // 0x00000200
#define GPO_A716_LCD_ENABLE		(1 << 10) // 0x00000400
#define GPO_A716_CF_SLOTS_ENABLE	(1 << 11) // 0x00000800
#define GPO_A716_CF_SLOT1_POWER3_N	(1 << 12) // 0x00001000
#define GPO_A716_CF_SLOT2_POWER3_N	(1 << 13) // 0x00002000
#define GPO_A716_ADS_SELECT		(1 << 14) // 0x00004000
#define GPO_A716_UNK2			(1 << 15) // 0x00008000
#define GPO_A716_CPU_MODE_SEL0		(1 << 16) // 0x00010000
#define GPO_A716_CPU_MODE_SEL1		(1 << 17) // 0x00020000
#define GPO_A716_SD_POWER_N		(1 << 18) // 0x00040000
#define GPO_A716_BLUETOOTH_POWER  	(1 << 19) // 0x00080000
#define GPO_A716_CHARGING_N		(1 << 20) // 0x00100000
#define GPO_A716_CF_SLOT2_RESET		(1 << 21) // 0x00200000
#define GPO_A716_BLUETOOTH_RESET	(1 << 22) // 0x00400000
#define GPO_A716_SERIAL_SOME1		(1 << 23) // 0x00800000
#define GPO_A716_CF_SLOT1_RESET		(1 << 24) // 0x01000000
#define GPO_A716_CF_SLOT1_POWER1_N	(1 << 25) // 0x02000000
#define GPO_A716_POWER_LED_RED		(1 << 26) // 0x04000000
#define GPO_A716_CF_SLOT2_POWER2	(1 << 27) // 0x08000000
#define GPO_A716_UNK5			(1 << 28) // 0x10000000
#define GPO_A716_CF_SLOT1_POWER2_N	(1 << 29) // 0x20000000
#define GPO_A716_CF_SLOT2_POWER1_N	(1 << 30) // 0x40000000
#define GPO_A716_WIFI_3p3V		(1 << 31) // 0x80000000

void a716_gpo_set(unsigned long bits);
unsigned long a716_gpo_get(void);
void a716_gpo_clear(unsigned long bits);

#define GPIO_I2C_BLUETOOTH_EXIST	(1 << 0) // 0x00000001
#define GPIO_I2C_WIFI_DETECTED		(1 << 1) // 0x00000002
#define GPIO_I2C_EARPHONES_DETECTED	(1 << 2) // 0x00000004
#define GPIO_I2C_POWER_AC_DETECTED	(1 << 3) // 0x00000008
#define GPIO_I2C_SPEAKER_DISABLE	(1 << 4) // 0x00000010
#define GPIO_I2C_EXPANDER_CONFIGURED_N	(1 << 5) // 0x00000020
#define GPIO_I2C_JOYPAD0_N		(1 << 8) // 0x00000100
#define GPIO_I2C_JOYPAD1_N		(1 << 9) // 0x00000200
#define GPIO_I2C_JOYPAD2_N		(1 << 10) // 0x00000400
#define GPIO_I2C_JOYPAD3_N		(1 << 11) // 0x00000800
#define GPIO_I2C_BUTTON_UP_N		(1 << 12) // 0x00001000
#define GPIO_I2C_JOYPAD4_N		(1 << 13) // 0x00002000
#define GPIO_I2C_BUTTON_RECORD_N	(1 << 14) // 0x00004000
#define GPIO_I2C_BUTTON_DOWN_N		(1 << 15) // 0x00008000


#endif /* _ASUS_716_GPIO_ */

/* HTC Apache GPIOs */
#ifndef _HTCAPACHE_GPIO_H_
#define _HTCAPACHE_GPIO_H_


/****************************************************************
 * Micro-controller interface
 ****************************************************************/

u32 htcapache_read_mc_reg(u32 reg);
void htcapache_write_mc_reg(u32 reg, u32 data);
#define GPIO_NR_HTCAPACHE_MC_SDA	56
#define GPIO_NR_HTCAPACHE_MC_SCL	57


/****************************************************************
 * EGPIO
 ****************************************************************/

int htcapache_egpio_get(int bit);
void htcapache_egpio_set(int bit, int value);
int htcapache_egpio_to_irq(int bit);
#define GPIO_NR_HTCAPACHE_EGPIO_IRQ	15


/****************************************************************
 * Power
 ****************************************************************/

#define EGPIO_NR_HTCAPACHE_PWR_IN_PWR		0
#define EGPIO_NR_HTCAPACHE_PWR_IN_HIGHPWR	7
#define EGPIO_NR_HTCAPACHE_PWR_CHARGE		40
#define EGPIO_NR_HTCAPACHE_PWR_HIGHCHARGE	39


/****************************************************************
 * Sound
 ****************************************************************/

#define EGPIO_NR_HTCAPACHE_SND_POWER	21  // XXX - not sure
#define EGPIO_NR_HTCAPACHE_SND_RESET	24  // XXX - not sure
#define EGPIO_NR_HTCAPACHE_SND_PWRJACK	23
#define EGPIO_NR_HTCAPACHE_SND_PWRSPKR	22
#define EGPIO_NR_HTCAPACHE_SND_IN_JACK	2


/****************************************************************
 * Touchscreen
 ****************************************************************/

#define GPIO_NR_HTCAPACHE_TS_PENDOWN	36
#define GPIO_NR_HTCAPACHE_TS_DAV	114
#define GPIO_NR_HTCAPACHE_TS_ALERT	20  // XXX - not sure


/****************************************************************
 * LEDS
 ****************************************************************/

#define GPIO_NR_HTCAPACHE_LED_FLASHLIGHT	87
#define EGPIO_NR_HTCAPACHE_LED_VIBRA	 	35
#define EGPIO_NR_HTCAPACHE_LED_KBD_BACKLIGHT	34


/****************************************************************
 * LEDS
 ****************************************************************/

#define GPIO_NR_HTCAPACHE_LED_FLASHLIGHT	87
#define EGPIO_NR_HTCAPACHE_LED_VIBRA	 	35
#define EGPIO_NR_HTCAPACHE_LED_KBD_BACKLIGHT	34


/****************************************************************
 * BlueTooth
 ****************************************************************/

#define EGPIO_NR_HTCAPACHE_BT_POWER	27
#define EGPIO_NR_HTCAPACHE_BT_RESET	26


/****************************************************************
 * Wifi
 ****************************************************************/

#define EGPIO_NR_HTCAPACHE_WIFI_POWER1	20
#define EGPIO_NR_HTCAPACHE_WIFI_POWER2	17
#define EGPIO_NR_HTCAPACHE_WIFI_POWER3	16
#define EGPIO_NR_HTCAPACHE_WIFI_RESET	19
#define EGPIO_NR_HTCAPACHE_WIFI_IN_IRQ	5


/****************************************************************
 * Mini-SD card
 ****************************************************************/

#define GPIO_NR_HTCAPACHE_SD_CARD_DETECT_N	13
#define GPIO_NR_HTCAPACHE_SD_POWER_N		89


/****************************************************************
 * USB
 ****************************************************************/

#define GPIO_NR_HTCAPACHE_USB_PUEN	99
#define EGPIO_NR_HTCAPACHE_USB_PWR	37


/****************************************************************
 * Front buttons
 ****************************************************************/

#define GPIO_NR_HTCAPACHE_FKEY_IRQ	14


/****************************************************************
 * Front buttons
 ****************************************************************/

#define GPIO_NR_HTCAPACHE_FKEY_IRQ	14


/****************************************************************
 * Buttons on side
 ****************************************************************/

#define GPIO_NR_HTCAPACHE_BUTTON_POWER		0
#define GPIO_NR_HTCAPACHE_BUTTON_RECORD		95
#define GPIO_NR_HTCAPACHE_BUTTON_VOLUP		9
#define GPIO_NR_HTCAPACHE_BUTTON_VOLDOWN	10
#define GPIO_NR_HTCAPACHE_BUTTON_BROWSER	94
#define GPIO_NR_HTCAPACHE_BUTTON_CAMERA		93


/****************************************************************
 * Pull out keyboard
 ****************************************************************/

#define GPIO_NR_HTCAPACHE_KP_PULLOUT	116

#define GPIO_NR_HTCAPACHE_KP_PULLOUT	116

#define GPIO_NR_HTCAPACHE_KP_MKIN0	100
#define GPIO_NR_HTCAPACHE_KP_MKIN1	101
#define GPIO_NR_HTCAPACHE_KP_MKIN2	102
#define GPIO_NR_HTCAPACHE_KP_MKIN3	37
#define GPIO_NR_HTCAPACHE_KP_MKIN4	38
#define GPIO_NR_HTCAPACHE_KP_MKIN5	90
#define GPIO_NR_HTCAPACHE_KP_MKIN6	91

#define GPIO_NR_HTCAPACHE_KP_MKOUT0	103
#define GPIO_NR_HTCAPACHE_KP_MKOUT1	104
#define GPIO_NR_HTCAPACHE_KP_MKOUT2	105
#define GPIO_NR_HTCAPACHE_KP_MKOUT3	106
#define GPIO_NR_HTCAPACHE_KP_MKOUT4	107
#define GPIO_NR_HTCAPACHE_KP_MKOUT5	108
#define GPIO_NR_HTCAPACHE_KP_MKOUT6	40

#define GPIO_NR_HTCAPACHE_KP_MKIN0_MD 	(GPIO_NR_HTCAPACHE_KP_MKIN0 | GPIO_ALT_FN_1_IN)
#define GPIO_NR_HTCAPACHE_KP_MKIN1_MD 	(GPIO_NR_HTCAPACHE_KP_MKIN1 | GPIO_ALT_FN_1_IN)
#define GPIO_NR_HTCAPACHE_KP_MKIN2_MD 	(GPIO_NR_HTCAPACHE_KP_MKIN2 | GPIO_ALT_FN_1_IN)
#define GPIO_NR_HTCAPACHE_KP_MKIN3_MD 	(GPIO_NR_HTCAPACHE_KP_MKIN3 | GPIO_ALT_FN_3_IN)
#define GPIO_NR_HTCAPACHE_KP_MKIN4_MD 	(GPIO_NR_HTCAPACHE_KP_MKIN4 | GPIO_ALT_FN_2_IN)
#define GPIO_NR_HTCAPACHE_KP_MKIN5_MD 	(GPIO_NR_HTCAPACHE_KP_MKIN5 | GPIO_ALT_FN_1_IN)
#define GPIO_NR_HTCAPACHE_KP_MKIN6_MD 	(GPIO_NR_HTCAPACHE_KP_MKIN6 | GPIO_ALT_FN_1_IN)

#define GPIO_NR_HTCAPACHE_KP_MKOUT0_MD (GPIO_NR_HTCAPACHE_KP_MKOUT0 | GPIO_ALT_FN_2_OUT)
#define GPIO_NR_HTCAPACHE_KP_MKOUT1_MD (GPIO_NR_HTCAPACHE_KP_MKOUT1 | GPIO_ALT_FN_2_OUT)
#define GPIO_NR_HTCAPACHE_KP_MKOUT2_MD (GPIO_NR_HTCAPACHE_KP_MKOUT2 | GPIO_ALT_FN_2_OUT)
#define GPIO_NR_HTCAPACHE_KP_MKOUT3_MD (GPIO_NR_HTCAPACHE_KP_MKOUT3 | GPIO_ALT_FN_2_OUT)
#define GPIO_NR_HTCAPACHE_KP_MKOUT4_MD (GPIO_NR_HTCAPACHE_KP_MKOUT4 | GPIO_ALT_FN_2_OUT)
#define GPIO_NR_HTCAPACHE_KP_MKOUT5_MD (GPIO_NR_HTCAPACHE_KP_MKOUT5 | GPIO_ALT_FN_2_OUT)
#define GPIO_NR_HTCAPACHE_KP_MKOUT6_MD (GPIO_NR_HTCAPACHE_KP_MKOUT6 | GPIO_ALT_FN_1_OUT)

#endif

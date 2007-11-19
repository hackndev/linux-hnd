/* 
 * include/asm-arm/arch-pxa/hx4700-gpio.h
 * History:
 *
 * 2004-12-10 Michael Opdenacker. Wrote down GPIO settings as identified by Jamey Hicks.
 *            Reused the h2200-gpio.h file as a template.
 */

#ifndef _X30_GPIO_H_
#define _X30_GPIO_H_

#include <asm/arch/pxa-regs.h>

#define GET_X30_GPIO(gpio) \
	(GPLR(GPIO_NR_X30_ ## gpio) & GPIO_bit(GPIO_NR_X30_ ## gpio))

#define SET_X30_GPIO(gpio, setp) \
do { \
if (setp) \
	GPSR(GPIO_NR_X30_ ## gpio) = GPIO_bit(GPIO_NR_X30_ ## gpio); \
else \
	GPCR(GPIO_NR_X30_ ## gpio) = GPIO_bit(GPIO_NR_X30_ ## gpio); \
} while (0)

#define SET_X30_GPIO_N(gpio, setp) \
do { \
if (setp) \
	GPCR(GPIO_NR_X30_ ## gpio ## _N) = GPIO_bit(GPIO_NR_X30_ ## gpio ## _N); \
else \
	GPSR(GPIO_NR_X30_ ## gpio ## _N) = GPIO_bit(GPIO_NR_X30_ ## gpio ## _N); \
} while (0)

#define X30_IRQ(gpio) \
	IRQ_GPIO(GPIO_NR_X30_ ## gpio)
	
#define GPIO_NR_X30_KEY_ON			0
#define GPIO_NR_X30_GP_RST_N			1

#define GPIO_NR_X30_PWR_SCL			3
#define GPIO_NR_X30_PWR_SDA			4
#define GPIO_NR_X30_PWR_CAP0			5
#define GPIO_NR_X30_PWR_CAP1			6
#define GPIO_NR_X30_PWR_CAP2			7
#define GPIO_NR_X30_PWR_CAP3			8
#define GPIO_NR_X30_CLK_PIO_CPU_13MHz 	9
#define GPIO_NR_X30_CLK_TOUT_32KHz		10
#define GPIO_NR_X30_CPU_BF_DOOR_N		11
#define GPIO_NR_X30_ASIC3_EXT_INT		12
#define GPIO_NR_X30_W3220_INT		13
#define GPIO_NR_X30_WLAN_IRQ_N		14
#define GPIO_NR_X30_CS1_N			15

#define GPIO_NR_X30_RDY			18
#define GPIO_NR_X30_TOUCHPANEL_SPI_CLK 	19
#define GPIO_NR_X30_SDCS2_N			20
#define GPIO_NR_X30_SDCS3_N			21
#define GPIO_NR_X30_LCD_RL			22
#define GPIO_NR_X30_SYNAPTICS_SPI_CLK	23
#define GPIO_NR_X30_SYNAPTICS_SPI_CS_N	24
#define GPIO_NR_X30_SYNAPTICS_SPI_DO		25
#define GPIO_NR_X30_SYNAPTICS_SPI_DI		26
#define GPIO_NR_X30_CODEC_ON			27
#define GPIO_NR_X30_I2S_BCK			28
#define GPIO_NR_X30_I2S_DIN			29
#define GPIO_NR_X30_I2S_DOUT			30
#define GPIO_NR_X30_I2S_SYNC			31
#define GPIO_NR_X30_RS232_ON			32
#define GPIO_NR_X30_CS5_N			33
#define GPIO_NR_X30_COM_RXD			34
#define GPIO_NR_X30_COM_CTS			35
#define GPIO_NR_X30_COM_DCD			36
#define GPIO_NR_X30_COM_DSR			37
#define GPIO_NR_X30_COM_RING			38
#define GPIO_NR_X30_COM_TXD			39
#define GPIO_NR_X30_COM_DTR			40
#define GPIO_NR_X30_COM_RTS			41
#define GPIO_NR_X30_BT_RXD			42
#define GPIO_NR_X30_BT_TXD			43
#define GPIO_NR_X30_BT_UART_CTS		44
#define GPIO_NR_X30_BT_UART_RTS		45

#define GPIO_NR_X30_POE_N			48
#define GPIO_NR_X30_PWE_N			49
#define GPIO_NR_X30_PIOR_N			50
#define GPIO_NR_X30_PIOW_N			51
#define GPIO_NR_X30_CPU_BATT_FAULT_N		52

#define GPIO_NR_X30_PCE2_N			54
#define GPIO_NR_X30_PREG_N			55
#define GPIO_NR_X30_PWAIT_N			56
#define GPIO_NR_X30_PIOIS16_N		57
#define GPIO_NR_X30_TOUCHPANEL_IRQ_N		58
#define GPIO_NR_X30_LCD_PC1			59
#define GPIO_NR_X30_CF_RNB			60	/* HaRET: I 1 0 FE */
#define GPIO_NR_X30_W3220_RESET_N		61
#define GPIO_NR_X30_LCD_RESET_N		62
#define GPIO_NR_X30_CPU_SS_RESET_N		63

#define GPIO_NR_X30_TOUCHPANEL_PEN_PU	65
#define GPIO_NR_X30_ASIC3_SDIO_INT_N		66
#define GPIO_NR_X30_EUART_PS			67

#define GPIO_NR_X30_LCD_SLIN1		70
#define GPIO_NR_X30_ASIC3_RESET_N		71
#define GPIO_NR_X30_CHARGE_EN_N		72
#define GPIO_NR_X30_LCD_UD_1			73

#define GPIO_NR_X30_EARPHONE_DET_N		75
#define GPIO_NR_X30_USB_PUEN			76

#define GPIO_NR_X30_CS2_N			78
#define GPIO_NR_X30_CS3_N			79
#define GPIO_NR_X30_CS4_N			80
#define GPIO_NR_X30_CPU_GP_RESET_N		81
#define GPIO_NR_X30_EUART_RESET		82
#define GPIO_NR_X30_WLAN_RESET_N		83
#define GPIO_NR_X30_LCD_SQN			84
#define GPIO_NR_X30_PCE1_N			85
#define GPIO_NR_X30_TOUCHPANEL_SPI_DI	86
#define GPIO_NR_X30_TOUCHPANEL_SPI_DO	87
#define GPIO_NR_X30_TOUCHPANEL_SPI_CS_N	88

#define GPIO_NR_X30_FLASH_VPEN		91
#define GPIO_NR_X30_HP_DRIVER		92
#define GPIO_NR_X30_EUART_INT		93
#define GPIO_NR_X30_KEY_AP3			94
#define GPIO_NR_X30_BATT_OFF			95
#define GPIO_NR_X30_USB_CHARGE_RATE		96
#define GPIO_NR_X30_BL_DETECT_N		97

#define GPIO_NR_X30_KEY_AP1			99
#define GPIO_NR_X30_AUTO_SENSE		100	/* to backlight circuit */

#define GPIO_NR_X30_SYNAPTICS_POWER_ON	102
#define GPIO_NR_X30_SYNAPTICS_INT		103
#define GPIO_NR_X30_PSKTSEL			104
#define GPIO_NR_X30_IR_ON_N			105
#define GPIO_NR_X30_CPU_BT_RESET_N		106
#define GPIO_NR_X30_SPK_SD_N			107

#define GPIO_NR_X30_CODEC_ON_N		109
#define GPIO_NR_X30_LCD_LVDD_3V3_ON		110
#define GPIO_NR_X30_LCD_AVDD_3V3_ON		111
#define GPIO_NR_X30_LCD_N2V7_7V3_ON		112
#define GPIO_NR_X30_I2S_SYSCLK		113
#define GPIO_NR_X30_CF_RESET			114	/* HaRET: O 0 0 */
#define GPIO_NR_X30_USB2_DREQ		115
#define GPIO_NR_X30_CPU_HW_RESET_N		116
#define GPIO_NR_X30_I2C_SCL			117
#define GPIO_NR_X30_I2C_SDA			118

#define EGPIO0_VCC_3V3_EN		(1<<0)	/* what use? */
#define EGPIO1_WL_VREG_EN		(1<<1)	/* WLAN Power */
#define EGPIO2_VCC_2V1_WL_EN		(1<<2)	/* unused */
#define EGPIO3_SS_PWR_ON		(1<<3)	/* smart slot power on */
#define EGPIO4_CF_3V3_ON		(1<<4)	/* CF 3.3V enable */
#define EGPIO5_BT_3V3_ON		(1<<5)	/* Bluetooth 3.3V enable */
#define EGPIO6_WL1V8_EN			(1<<6)	/* WLAN 1.8V enable */
#define EGPIO7_VCC_3V3_WL_EN		(1<<7)	/* WLAN 3.3V enable */
#define EGPIO8_USB_3V3_ON		(1<<8)	/* unused */

#define GPIO_NR_X30_SDCS2_N_MD		(20 | GPIO_ALT_FN_1_OUT | GPIO_DFLT_HIGH)
#define GPIO_NR_X30_SDCS3_N_MD		(21 | GPIO_ALT_FN_1_OUT | GPIO_DFLT_HIGH)
#define GPIO_NR_X30_SYNAPTICS_SPI_CLK_MD	(23 | GPIO_ALT_FN_2_IN)
#define GPIO_NR_X30_SYNAPTICS_SPI_CS_N_MD	(24 | GPIO_ALT_FN_2_IN)
#define GPIO_NR_X30_SYNAPTICS_SPI_DO_MD	(25 | GPIO_ALT_FN_2_OUT)
#define GPIO_NR_X30_SYNAPTICS_SPI_DI_MD	(26 | GPIO_ALT_FN_1_IN)
#define GPIO_NR_X30_I2S_BCK_MD		(28 | GPIO_ALT_FN_1_OUT)
#define GPIO_NR_X30_I2S_DIN_MD		(29 | GPIO_ALT_FN_2_IN)
#define GPIO_NR_X30_I2S_DOUT_MD		(30 | GPIO_ALT_FN_1_OUT)
#define GPIO_NR_X30_I2S_SYNC_MD		(31 | GPIO_ALT_FN_1_OUT)
#define GPIO_NR_X30_COM_RXD_MD		(34 | GPIO_ALT_FN_1_IN)
#define GPIO_NR_X30_COM_CTS_MD		(35 | GPIO_ALT_FN_1_IN)
#define GPIO_NR_X30_COM_DCD_MD		(36 | GPIO_ALT_FN_1_IN)
#define GPIO_NR_X30_COM_DSR_MD		(37 | GPIO_ALT_FN_1_IN)
#define GPIO_NR_X30_COM_RING_MD		(38 | GPIO_ALT_FN_1_IN)
#define GPIO_NR_X30_COM_TXD_MD		(39 | GPIO_ALT_FN_2_OUT)
#define GPIO_NR_X30_COM_DTR_MD		(40 | GPIO_ALT_FN_2_OUT)
#define GPIO_NR_X30_COM_RTS_MD		(41 | GPIO_ALT_FN_2_OUT)
#define GPIO_NR_X30_POE_N_MD			(48 | GPIO_ALT_FN_2_OUT | GPIO_DFLT_HIGH)
#define GPIO_NR_X30_PWE_N_MD			(49 | GPIO_ALT_FN_2_OUT | GPIO_DFLT_HIGH)
#define GPIO_NR_X30_PIOR_N_MD		(50 | GPIO_ALT_FN_2_OUT | GPIO_DFLT_HIGH)
#define GPIO_NR_X30_PIOW_N_MD		(51 | GPIO_ALT_FN_2_OUT | GPIO_DFLT_HIGH)
#define GPIO_NR_X30_PCE2_N_MD		(54 | GPIO_ALT_FN_2_OUT | GPIO_DFLT_HIGH)
#define GPIO_NR_X30_PREG_N_MD		(55 | GPIO_ALT_FN_2_OUT | GPIO_DFLT_HIGH)
#define GPIO_NR_X30_PWAIT_N_MD		(56 | GPIO_ALT_FN_1_IN | GPIO_DFLT_HIGH)
#define GPIO_NR_X30_PIOIS16_N_MD		(57 | GPIO_ALT_FN_1_IN | GPIO_DFLT_HIGH)
#define GPIO_NR_X30_PCE1_N_MD		(85 | GPIO_ALT_FN_1_OUT | GPIO_DFLT_HIGH)
#define GPIO_NR_X30_PSKTSEL_MD		(104 | GPIO_ALT_FN_1_OUT)
#define GPIO_NR_X30_I2S_SYSCLK_MD		(113 | GPIO_ALT_FN_1_OUT)

void x30_egpio_enable( unsigned long bits );
void x30_egpio_disable( unsigned long bits );

#endif /* _X30_GPIO_H */

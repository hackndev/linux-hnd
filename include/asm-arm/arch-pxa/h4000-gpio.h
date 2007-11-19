
#ifndef _H4000_GPIO_H_
#define _H4000_GPIO_H_

#define GET_H4000_GPIO(gpio) \
        (GPLR(GPIO_NR_H4000_ ## gpio) & GPIO_bit(GPIO_NR_H4000_ ## gpio))

#define SET_H4000_GPIO(gpio, setp) \
do { \
if (setp) \
        GPSR(GPIO_NR_H4000_ ## gpio) = GPIO_bit(GPIO_NR_H4000_ ## gpio); \
else \
        GPCR(GPIO_NR_H4000_ ## gpio) = GPIO_bit(GPIO_NR_H4000_ ## gpio); \
} while (0)

#define SET_H4000_GPIO_N(gpio, setp) \
do { \
if (setp) \
        GPCR(GPIO_NR_H4000_ ## gpio) = GPIO_bit(GPIO_NR_H4000_ ## gpio); \
else \
        GPSR(GPIO_NR_H4000_ ## gpio) = GPIO_bit(GPIO_NR_H4000_ ## gpio); \
} while (0)

#define H4000_IRQ(gpio) \
        IRQ_GPIO(GPIO_NR_H4000_ ## gpio)
	
/* Active-low signals are denoted by suffix _N  */
/* FE == falling edge, RE == Rising edge */

#define GPIO_NR_H4000_POWER_BUTTON_N  0                  /* ; RE FE ; Input */
#define GPIO_NR_H4000_WARM_RESET_N    1                            /* Input */
#define GPIO_NR_H4000_SD_DETECT_N     2  /* SD Card insert  ; RE FE ; Input */
#define GPIO_NR_H4000_CHARGING        3  /* is it charging     ; FE ; Input */
#define GPIO_NR_H4000_AC_IN_N         4  /* Power plugged in; RE FE ; Input */
#define GPIO_NR_H4000_BATTERY_DOOR    5                            /* Input */
#define GPIO_NR_H4000_CRITICAL_LOW_N  6         /* Battery very low ; Input */
#define GPIO_NR_H4000_WARM_RESET_0_N  7                            /* Input */
#define GPIO_NR_H4000_SD_IRQ_N        8              /* SD Card IRQ ; Input */
#define GPIO_NR_H4000_ASIC3_IRQ       9  /* ASIC3 IRQ               ; Input */
#define GPIO_NR_H4000_USB_DETECT_N    10 /* USB is connected; RE FE ; Input */
#define GPIO_NR_H4000_WLAN_MAC_IRQ_N  11 /* Wireless 802.11b   ; FE ; Input */
// GPIO12_32KHz_MD - Blutooth (& WLAN?) MAC Clock /* Output */
#define GPIO_NR_H4000_CRADLE_DCD      13 /* serial connected; RE FE ; Input */
#define GPIO_NR_H4000_CODEC_RST       14                         /* ; Output*/
#define GPIO_NR_H4000_WARM_RST_EN     15                         /* ; Output*/
#define GPIO_NR_H4000_LCD_PWM         16                 /* LCD_PWM ; Output*/
#define GPIO_NR_H4000_SOFT_RST_N      17                         /* ; Output*/
// 18
#define GPIO_NR_H4000_UP_BUTTON_N     19 /* h4150 Only              ; Input */
#define GPIO_NR_H4000_LEFT_BUTTON_N   20 /* h4150 Only              ; Input */
#define GPIO_NR_H4000_ACTION_BUTTON_N 21 /* h4150 Only              ; Input */
#define GPIO_NR_H4000_DOWN_BUTTON_N   22 /* h4150 Only              ; Input */
#define GPIO_NR_H4000_TP_SPI_CLK      23       /* To touchpanel ADC ; Output */
#define GPIO_NR_H4000_TP_SPI_CS_N     24                         /* ; Output */
#define GPIO_NR_H4000_TP_SPI_TXD      25                         /* ; Output */
#define GPIO_NR_H4000_TP_SPI_RXD      26                         /* ; Input */
#define GPIO_NR_H4000_PEN_IRQ_N       27                      /* FE ; Input */
#define GPIO_NR_H4000_AC_BITCLK       28                         /* ; Output */
#define GPIO_NR_H4000_AC_SDATA_IN     29                         /* ; Input */
#define GPIO_NR_H4000_AC_SDATA_OUT    30                         /* ; Output */
#define GPIO_NR_H4000_AC_WS           31                         /* ; Output */
#define GPIO_NR_H4000_AC_AUDIO_CLK    32                         /* ; Output */
// GPIO33 is CS5# not connected       33
#define GPIO_NR_H4000_CRADLE_RXD      34                         /* ; Input */
#define GPIO_NR_H4000_CRADLE_CTS_N    35                         /* ; Input */
#define GPIO_NR_H4000_FW_LOW_N        36                         /* ; Output */
#define GPIO_NR_H4000_CRADLE_DSR      37                         /* ; Input */
#define GPIO_NR_H4000_CRADLE_RING     38                         /* ; Input */
#define GPIO_NR_H4000_CRADLE_TXD      39                         /* ; Output */
#define GPIO_NR_H4000_CRADLE_DTR      40                         /* ; Output */
#define GPIO_NR_H4000_CRADLE_RTS_N    41                         /* ; Output */
// GPIO42 BTUART/HWUART                                          /* ; Input */
// GPIO43 -"-                                                    /* ; Output */
// GPIO44 -"-                                                    /* ; Input */
// GPIO45 -"-                                                    /* ; Output */
#define GPIO_NR_H4000_IR_RXD          46                         /* ; Input */
#define GPIO_NR_H4000_IR_TXD          47                         /* ; Input */
// 48 is POE#
// 49 is PWE#
// 50 is not connected
// 51 is not connected
// 52 is PCE1#
// 53 is PCE2#
#define GPIO_NR_H4000_ASIC3_RESET_N   54                         /* ; Output */
// 55 is PREG#
// 56 is PWAIT#
// 57 is pulled to ground
// 58 is LCD B0  ; Output
// 59 is LCD B1  ; Output
// 60 is LCD B2  ; Output
// 61 is LCD B3  ; Output
// 62 is LCD B4  ; Output
// 63 is LCD G0  ; Output
// 64 is LCD G1  ; Output
// 65 is LCD G2  ; Output
// 66 is LCD G3  ; Output
// 67 is LCD G4  ; Output
// 68 is LCD G5  ; Output
// 69 is LCD R0  ; Output
// 70 is LCD R1  ; Output
// 71 is LCD R2  ; Output
// 72 is LCD R3  ; Output
// 73 is LCD R4  ; Output
//#define GPIO_NR_H4000_              74                         /* ; not connected */
//#define GPIO_NR_H4000_              75                         /* ; not connected */
#define GPIO_NR_H4000_LCD_PCLK        76                         /* ; Output*/
#define GPIO_NR_H4000_LCD_BIAS        77                         /* ; Output*/
#define GPIO_NR_H4000_RIGHT_BUTTON_N  78 /* h4150 Only              ; Input*/
// 79 is CS3# connected to ASIC3_CS#
// 80 is CS4# connected to SDIO_CS#
// 81 not connected?
// 82 not connected?
// 83 not connected?
// 84 not connected?

/* GPIO aliases */
#define GPIO_NR_H4000_SERIAL_DETECT   GPIO_NR_H4000_CRADLE_DCD


#endif /* _H4000_GPIO_H_ */

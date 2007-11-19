#ifndef _H5400_GPIO_H_
#define _H5400_GPIO_H_

#define GET_H5400_GPIO(gpio) \
	(GPLR(GPIO_NR_H5400_ ## gpio) & GPIO_bit(GPIO_NR_H5400_ ## gpio))

#define SET_H5400_GPIO(gpio, setp) \
do { \
if (setp) \
	GPSR(GPIO_NR_H5400_ ## gpio) = GPIO_bit(GPIO_NR_H5400_ ## gpio); \
else \
	GPCR(GPIO_NR_H5400_ ## gpio) = GPIO_bit(GPIO_NR_H5400_ ## gpio); \
} while (0)

#define SET_H5400_GPIO_N(gpio, setp) \
do { \
if (setp) \
	GPCR(GPIO_NR_H5400_ ## gpio) = GPIO_bit(GPIO_NR_H5400_ ## gpio); \
else \
	GPSR(GPIO_NR_H5400_ ## gpio) = GPIO_bit(GPIO_NR_H5400_ ## gpio); \
} while (0)

#define H5400_IRQ(gpio) \
	IRQ_GPIO(GPIO_NR_H5400_ ## gpio)

#define GPIO_NR_H5400_POWER_BUTTON   0
#define GPIO_NR_H5400_RESET_BUTTON_N 1
#define GPIO_NR_H5400_OPT_INT        2
#define GPIO_NR_H5400_BACKUP_POWER   3
#define GPIO_NR_H5400_ACTION_BUTTON  4
#define GPIO_NR_H5400_COM_DCD_SOMETHING  5 /* what is this really ? */
/* 6 not connected */
#define GPIO_NR_H5400_RESET_BUTTON_AGAIN_N 7 /* connected to gpio 1 as well */
/* 8 not connected */
#define GPIO_NR_H5400_RSO_N          9       /* reset output from max1702 which regulates 3.3 and 2.5 */ 
#define GPIO_NR_H5400_ASIC_INT_N    10       /* from companion asic */
#define GPIO_NR_H5400_BT_ENV_0      11       /* to LMX9814, set to 1 according to regdump */
/* 12 not connected */
#define GPIO_NR_H5400_BT_ENV_1      13       /* to LMX9814, set to 1 according to regdump */
#define GPIO_NR_H5400_BT_WU         14       /* from LMX9814, Defined as HOST_WAKEUP in the LMX9820 data sheet */
/* 15 is CS1# */
/* 16 not connected */
/* 17 not connected */
/* 18 is pcmcia ready */
/* 19 is dreq1 */
/* 20 is dreq0 */
#define GPIO_NR_H5400_OE_RD_NWR	    21       /* output enable on rd/nwr signal to companion asic */
/* 22 is not connected */
#define GPIO_NR_H5400_OPT_SPI_CLK   23       /* to extension pack */
#define GPIO_NR_H5400_OPT_SPI_CS_N  24       /* to extension pack */
#define GPIO_NR_H5400_OPT_SPI_DOUT  25       /* to extension pack */
#define GPIO_NR_H5400_OPT_SPI_DIN   26       /* to extension pack */
/* 27 not connected */
#define GPIO_NR_H5400_I2S_BITCLK    28       /* connected to AC97 codec */
#define GPIO_NR_H5400_I2S_DATAOUT   29       /* connected to AC97 codec */
#define GPIO_NR_H5400_I2S_DATAIN    30       /* connected to AC97 codec */
#define GPIO_NR_H5400_I2S_LRCLK     31       /* connected to AC97 codec */
#define GPIO_NR_H5400_I2S_SYSCLK    32       /* connected to AC97 codec */
/* 33 is CS5# */
#define GPIO_NR_H5400_COM_RXD       34       /* connected to cradle/cable connector */ 
#define GPIO_NR_H5400_COM_CTS       35       /* connected to cradle/cable connector */ 
#define GPIO_NR_H5400_COM_DCD       36       /* connected to cradle/cable connector */ 
#define GPIO_NR_H5400_COM_DSR       37       /* connected to cradle/cable connector */ 
#define GPIO_NR_H5400_COM_RI        38       /* connected to cradle/cable connector */ 
#define GPIO_NR_H5400_COM_TXD       39       /* connected to cradle/cable connector */ 
#define GPIO_NR_H5400_COM_DTR       40       /* connected to cradle/cable connector */ 
#define GPIO_NR_H5400_COM_RTS       41       /* connected to cradle/cable connector */ 

#define GPIO_NR_H5400_BT_RXD        42       /* connected to BT (LMX9814) */ 
#define GPIO_NR_H5400_BT_TXD        43       /* connected to BT (LMX9814) */ 
#define GPIO_NR_H5400_BT_CTS        44       /* connected to BT (LMX9814) */ 
#define GPIO_NR_H5400_BT_RTS        45       /* connected to BT (LMX9814) */ 

#define GPIO_NR_H5400_IRDA_RXD      46
#define GPIO_NR_H5400_IRDA_TXD      47

#define GPIO_NR_H5400_POE_N         48       /* used for pcmcia */ 
#define GPIO_NR_H5400_PWE_N         49       /* used for pcmcia */ 
#define GPIO_NR_H5400_PIOR_N        50       /* used for pcmcia */ 
#define GPIO_NR_H5400_PIOW_N        51       /* used for pcmcia */   
#define GPIO_NR_H5400_PCE1_N        52       /* used for pcmcia */ 
#define GPIO_NR_H5400_PCE2_N        53       /* used for pcmcia */ 
#define GPIO_NR_H5400_PSKTSEL       54       /* used for pcmcia */ 
#define GPIO_NR_H5400_PREG_N        55       /* used for pcmcia */ 
#define GPIO_NR_H5400_PWAIT_N       56       /* used for pcmcia */ 
#define GPIO_NR_H5400_IOIS16_N      57       /* used for pcmcia */ 

#define GPIO_NR_H5400_IRDA_SD       58       /* to hsdl3002 sd */ 
/* 59 not connected */
#define GPIO_NR_H5400_POWER_SD_N    60       /* controls power to SD */
#define GPIO_NR_H5400_POWER_RS232_N 61       /* inverted FORCEON to rs232 transceiver */
#define GPIO_NR_H5400_POWER_ACCEL_N 62       /* controls power to accel */
/* 63 is not connected */
#define GPIO_NR_H5400_OPT_NVRAM     64       /* controls power to expansion pack */  
#define GPIO_NR_H5400_CHG_EN        65       /* to sc801 en */
#define GPIO_NR_H5400_USB_PULLUP    66       /* USB d+ pullup via 1.5K resistor */
#define GPIO_NR_H5400_BT_2V8_N      67       /* 2.8V used by bluetooth */
#define GPIO_NR_H5400_EXT_CHG_RATE  68       /* enables external charging rate */
/* 69 is not connected */
#define GPIO_NR_H5400_CIR_RESET     70       /* consumer IR reset */
#define GPIO_NR_H5400_POWER_LIGHT_SENSOR_N 71
#define GPIO_NR_H5400_BT_M_RESET    72
#define GPIO_NR_H5400_STD_CHG_RATE  73
#define GPIO_NR_H5400_SD_WP_N       74
#define GPIO_NR_H5400_MOTOR_ON_N    75       /* external pullup on this */  
#define GPIO_NR_H5400_HEADPHONE_DETECT 76
#define GPIO_NR_H5400_USB_CHG_RATE  77       /* select rate for charging via usb */  
/* 78 is CS2# */
/* 79 is CS3# */
/* 80 is CS4# */

#define IRQ_GPIO_H5400_ASIC_INT                IRQ_GPIO(GPIO_NR_H5400_ASIC_INT_N)

#endif /* _H5400_GPIO_H */

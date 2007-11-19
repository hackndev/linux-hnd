/*
 * RoverP1 GPIO/eGPIO definitions.
 *
 * Authors: Konstantine A. Beklemishev (konstantine@r66.ru)
 */

#ifndef _ROVERP1_GPIO_H_
#define _ROVERP1_GPIO_H_

#define GET_ROVERP1_GPIO(gpio) \
	(GPLR(GPIO_NR_ROVERP1_ ## gpio) & GPIO_bit(GPIO_NR_ROVERP1_ ## gpio))

#define SET_ROVERP1_GPIO(gpio, setp) \
do { \
if (setp) \
	GPSR(GPIO_NR_ROVERP1_ ## gpio) = GPIO_bit(GPIO_NR_ROVERP1_ ## gpio); \
else \
	GPCR(GPIO_NR_ROVERP1_ ## gpio) = GPIO_bit(GPIO_NR_ROVERP1_ ## gpio); \
} while (0)

#define SET_ROVERP1_GPIO_N(gpio, setp) \
do { \
if (setp) \
	GPCR(GPIO_NR_ROVERP1_ ## gpio ## _N) = GPIO_bit(GPIO_NR_ROVERP1_ ## gpio ## _N); \
else \
	GPSR(GPIO_NR_ROVERP1_ ## gpio ## _N) = GPIO_bit(GPIO_NR_ROVERP1_ ## gpio ## _N); \
} while (0)

#define ROVERP1_IRQ(gpio) \
	IRQ_GPIO(GPIO_NR_ROVERP1_ ## gpio)

#define ROVERP1_eGPIO_PHYS 0x0a000000
#define ROVERP1_eGPIO_VIRT 0xf0000000

u32 eGPIO_initValue=0x8221;
u32 *eGPIO_base;

#define REINIT_ROVERP1_eGPIO\
	writel(eGPIO_initValue, ROVERP1_eGPIO_VIRT);

#define SET_ROVERP1_eGPIO(egpio, setp)\
if (setp) \
	{\
	eGPIO_initValue = eGPIO_initValue|(1<<egpio);\
	REINIT_ROVERP1_eGPIO;\
	}\
else\
	{\
	eGPIO_initValue = eGPIO_initValue&(~(1<<egpio));\
	REINIT_ROVERP1_eGPIO;\
	}

#define GET_ROVERP1_eGPIO(egpio) ((eGPIO_initValue&(1<<egpio))>>egpio)


	
/* GPIO definitions */
#define GPIO_NR_ROVERP1_POWER_BTN	(0)
#define GPIO_NR_ROVERP1_RESET_BTN	(1)
#define GPIO_NR_ROVERP1_USB_DETECT	(2)
/* #define GPIO_NR_ROVERP1_	(3) */
#define GPIO_NR_ROVERP1_MEMO		(4)
#define GPIO_NR_ROVERP1_REC_BTN		(5)
/* #define GPIO_NR_ROVERP1_	(6) */
#define GPIO_NR_ROVERP1_TASK		(7)
/*#define GPIO_NR_ROVERP1_	(8)
 * #define GPIO_NR_ROVERP1_	(9)
 * #define GPIO_NR_ROVERP1_	(10)
 * #define GPIO_NR_ROVERP1_	(11) */
#define GPIO_NR_ROVERP1_CONTACT		(12)
#define GPIO_NR_ROVERP1_JOG_UP		(13)
/* #define GPIO_NR_ROVERP1_	(14) */
#define GPIO_NR_ROVERP1_CALENDAR	(15)
/* #define GPIO_NR_ROVERP1_	(16)
 * #define GPIO_NR_ROVERP1_	(17)
 * #define GPIO_NR_ROVERP1_	(18)
 * #define GPIO_NR_ROVERP1_	(19) */
#define GPIO_NR_ROVERP1_JOG_ENTER	(20)
/* #define GPIO_NR_ROVERP1_	(21)
 * #define GPIO_NR_ROVERP1_	(22)
 * #define GPIO_NR_ROVERP1_	(23)
 * #define GPIO_NR_ROVERP1_	(24)
 * #define GPIO_NR_ROVERP1_	(25)
 * #define GPIO_NR_ROVERP1_	(26)
 * #define GPIO_NR_ROVERP1_	(27)
 * #define GPIO_NR_ROVERP1_	(28)
 * #define GPIO_NR_ROVERP1_	(29)
 * #define GPIO_NR_ROVERP1_	(30)
 * #define GPIO_NR_ROVERP1_	(31)
 * #define GPIO_NR_ROVERP1_	(32) */
#define GPIO_NR_ROVERP1_JOG_DOWN	(33)
/* #define GPIO_NR_ROVERP1_	(34)
 * #define GPIO_NR_ROVERP1_	(35)
 * #define GPIO_NR_ROVERP1_	(36)
 * #define GPIO_NR_ROVERP1_	(37)
 * #define GPIO_NR_ROVERP1_	(38)
 * #define GPIO_NR_ROVERP1_	(39)
 * #define GPIO_NR_ROVERP1_	(40)
 * #define GPIO_NR_ROVERP1_	(41)
 * #define GPIO_NR_ROVERP1_	(42)
 * #define GPIO_NR_ROVERP1_	(43)
 * #define GPIO_NR_ROVERP1_	(44)
 * #define GPIO_NR_ROVERP1_	(45)
 * #define GPIO_NR_ROVERP1_	(46)
 * #define GPIO_NR_ROVERP1_	(47)
 * #define GPIO_NR_ROVERP1_	(48)
 * #define GPIO_NR_ROVERP1_	(49)
 * #define GPIO_NR_ROVERP1_	(50)
 * #define GPIO_NR_ROVERP1_	(51)
 * #define GPIO_NR_ROVERP1_	(52)
 * #define GPIO_NR_ROVERP1_	(53)
 * #define GPIO_NR_ROVERP1_	(54)
 * #define GPIO_NR_ROVERP1_	(55)
 * #define GPIO_NR_ROVERP1_	(56)
 * #define GPIO_NR_ROVERP1_	(57)
 * #define GPIO_NR_ROVERP1_	(58)
 * #define GPIO_NR_ROVERP1_	(59)
 * #define GPIO_NR_ROVERP1_	(60)
 * #define GPIO_NR_ROVERP1_	(61)
 * #define GPIO_NR_ROVERP1_	(62)
 * #define GPIO_NR_ROVERP1_	(63)
 * #define GPIO_NR_ROVERP1_	(64)
 * #define GPIO_NR_ROVERP1_	(65)
 * #define GPIO_NR_ROVERP1_	(66)
 * #define GPIO_NR_ROVERP1_	(67)
 * #define GPIO_NR_ROVERP1_	(68)
 * #define GPIO_NR_ROVERP1_	(69)
 * #define GPIO_NR_ROVERP1_	(70)
 * #define GPIO_NR_ROVERP1_	(71)
 * #define GPIO_NR_ROVERP1_	(72)
 * #define GPIO_NR_ROVERP1_	(73)
 * #define GPIO_NR_ROVERP1_	(74)
 * #define GPIO_NR_ROVERP1_	(75)
 * #define GPIO_NR_ROVERP1_	(76)
 * #define GPIO_NR_ROVERP1_	(77)
 * #define GPIO_NR_ROVERP1_	(78) */
#define GPIO_NR_ROVERP1_JOG_LEFT	(79)
#define GPIO_NR_ROVERP1_JOG_RIGHT	(80)

/* eGPIO definitions */
#define eGPIO_NR_ROVERP1_BL		(0)  /* backLight on/of     */
#define eGPIO_NR_ROVERP1_FLASH_PROTECT	(4)  /* flashProtect output */
#define eGPIO_NR_ROVERP1_LCD_ON		(5)  /* lcd data output     */
#define eGPIO_NR_ROVERP1_LCD_PWR_ON	(6)  /* lcd power           */
#define eGPIO_NR_ROVERP1_USB_EN		(9)  /* usb power           */
#define eGPIO_NR_ROVERP1_AUD_PWR_ON	(13) /* codec power         */
#define eGPIO_NR_ROVERP1_AMP_SHDW	(14) /* apm shutdown        */
#define eGPIO_NR_ROVERP1_AMP_PWR_ON	(15) /* amp power           */


#endif /* _ROVERP1_GPIO_H_ */

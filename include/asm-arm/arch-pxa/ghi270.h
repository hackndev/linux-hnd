/* 
 * include/asm-arm/arch-pxa/ghi270.h
 *
 * History:
 *
 * 2007-02-26 SDG Systems	Created.
 */

#ifndef _GHI270_GPIO_H_
#define _GHI270_GPIO_H_

#include <asm/arch/pxa-regs.h>

#define GHI270_GPIO0_KEY_nON			0
#define GHI270_GPIO1_GP_nRST			1   /* generic */
/* GPIO2_SYS_EN */
#define GHI270_GPIO3_PWR_SCL			3   /* generic */
#define GHI270_GPIO4_PWR_SDA			4   /* generic */
#define GHI270_GPIO5_PWR_CAP0			5   /* ? */
#define GHI270_GPIO6_PWR_CAP1			6   /* ? */
#define GHI270_GPIO7_PWR_CAP2			7   /* ? */
#define GHI270_GPIO8_PWR_CAP3			8   /* ? */
#define GHI270_GPIO9_PRF_EN			9   /* 3.3V power to periphs */
#define GHI270_GPIO10_PWR_FAULT			10
#define GHI270_GPIO11_KEYPAD_LED		11  /* generic PWM2 */
#define GHI270_GPIO12_CIF_DD_7			12  /* generic */
#define GHI270_GPIO13_PWR_INT			13  /* generic */
#define GHI270_GPIO14_PCMCIA_CD1		14  /* PCMCIA slot 0 CD */
#define GHI270_GPIO15_nCS1			15  /* generic */
/* GPIO16_PWM0 */				    /* backlight brightness */
#define GPIO17_CIF_DD_6				17  /* generic */
/* GPIO18_RDY */				    /* mem controller */

#define GHI270_GPIO19_PCMCIA_PRESET1		19
#define GHI270_GPIO20_PCMCIA_PRESET2		20
#define GHI270_GPIO21_PCMCIA_RDY_IRQ1		21
#define GHI270_GPIO22_PCMCIA_RDY_IRQ2		22

/* GPIO23_SCLK */				    /* SSP */
#define GHI270_HG_GPIO23_BARCLK			23
/* GPIO24_SFRM */				    /* SSP */
/* GPIO25_STXD */				    /* SSP */
#define GHI270_HG_GPIO25_BARTXD			25
/* GPIO26_SRXD */				    /* SSP */
#define GHI270_HG_GPIO26_BARRXD			26
#define GHI270_H_GPIO26_CHGEN			26  /* Charge Enable activehi */

/* GPIO27_SEXTCLK */				    /* LCD_XRES */
/* GPIO28_BITCLK */				    /* AC97 */
/* GPIO29_SDATA_IN */				    /* AC97 */
/* GPIO30_SDATA_OUT */				    /* AC97 */
/* GPIO31_SYNC */				    /* AC97 */

/* GPIO32_MMCCLK */				    /* MMC */
/* GPIO33_nCS_5 */
/* GPIO34_FFRXD */				    /* FFUART */
/* GPIO35_FFCTS */				    /* FFUART */
#define GPIO35_KP_MKOUT6			35
#define GHI270_GPIO36_MMC_WP			36  /* MMC WP sense */
#define GHI270_GPIO37_PCMCIA_nPWR1		37  /* PCMCIA slot 0 power */
#define GHI270_GPIO38_MMC_CD			38  /* MMC card detect */
/* GPIO39_FFTXD */				    /* FFUART */
/* GPIO40_FFDTR */				    /* FFUART */
#define GHI270_GPIO41_DEBUG_LED			41  /* 0 = on, 1 = off */
#define GPIO41_KP_MKOUT7			41  /* 0 = on, 1 = off */

/* GPIO42_BTRXD */				    /* BTUART */
/* GPIO43_BTTXD */				    /* BTUART */
/* GPIO44_BTCTS */				    /* BTUART */
/* GPIO45_BTRTS */				    /* BTUART */

/* GPIO46_ICPRXD */				    /* FIR */
/* GPIO47_ICPTXD */				    /* FIR */

/* GPIO48_nPOE */				    /* PCMCIA */
/* GPIO49_nPWE */				    /* PCMCIA */
/* GPIO50_nPIOR */				    /* PCMCIA */
/* GPIO51_nPIOW */				    /* PCMCIA */

#define GHI270_GPIO52_EXT_GPIO0			52  /* IRQOUT from UCB1400 */

#define GPIO53_CIF_MCLK				53  /* generic */
#define GHI270_HG_GPIO53_CHGEN			53  /* Charge Enable activehi */
#define GPIO54_CIF_PCLK				54  /* generic */

/* GPIO55_nPREG */				    /* PCMCIA */
/* GPIO56_nPWAIT */				    /* PCMCIA */
/* GPIO57_nIOIS16 */				    /* PCMCIA */

/* GPIO58_LDD_0 */				    /* LCD */
/* GPIO59_LDD_1 */				    /* LCD */
/* GPIO60_LDD_2 */				    /* LCD */
/* GPIO61_LDD_3 */				    /* LCD */
/* GPIO62_LDD_4 */				    /* LCD */
/* GPIO63_LDD_5 */				    /* LCD */
/* GPIO64_LDD_6 */				    /* LCD */
/* GPIO65_LDD_7 */				    /* LCD */
/* GPIO66_LDD_8 */				    /* LCD */
/* GPIO67_LDD_9 */				    /* LCD */
/* GPIO68_LDD_10 */				    /* LCD */
/* GPIO69_LDD_11 */				    /* LCD */
/* GPIO70_LDD_12 */				    /* LCD */
/* GPIO71_LDD_13 */				    /* LCD */
/* GPIO72_LDD_14 */				    /* LCD */
/* GPIO73_LDD_15 */				    /* LCD */
/* GPIO74_LCD_FCLK */				    /* LCD */
/* GPIO75_LCD_LCLK */				    /* LCD */
/* GPIO76_LCD_PCLK */				    /* LCD */
/* GPIO77_LCD_ACBIAS */				    /* LCD */

#define GPIO78_nPCE_2				78  /* generic */
#define GPIO79_pSKTSEL				79  /* generic */
/* GPIO80_nCS_4 */
#define GPIO81_CIF_DD_0				81  /* generic */
#define GPIO82_CIF_DD_5				82  /* generic */
#define GPIO83_CIF_DD_4				83  /* generic */
#define GPIO84_CIF_FV				84  /* generic */
#define GPIO85_CIF_LV				85  /* generic */
#define GPIO86_nPCE_1				86  /* generic */

#define GHI270_H_GPIO87_VIBRATOR		87  /* ext gpio 1; 1 = on */
#define GHI270_HG_GPIO87_NAND_RNB		87  /* ext gpio 1; 1 = on */

#define GPIO88_USBHPWR1				88  /* generic */
#define GPIO89_USBHPEN1				89  /* generic */

#define GHI270_GPIO90_USBC_DET			90  /* 0 = present */
#define GHI270_GPIO91_USBC_ACT			91  /* 1 = active */

/* GPIO92_MMCDAT0 */				    /* MMC */

#define GHI270_GPIO93_GREEN_LED			93  /* green LED; 1 = on */
#define GHI270_GPIO94_RED_LED			94  /* red LED; 1 = on */
#define GPIO95_KP_MKIN6				95  /* generic */
#define GPIO96_KP_DKIN3				96  /* generic */
#define	GPIO97_KP_MKIN3				97  /* generic */

#define GHI270_GPIO98_VS0_1			98  /* PCMCIA */
#define GHI270_GPIO99_VS1_1			98  /* PCMCIA */

#define	GPIO100_KP_MKIN0			100 /* generic */
#define	GPIO101_KP_MKIN1			101 /* generic */
#define	GPIO102_KP_MKIN2			102 /* generic */
#define	GPIO103_KP_MKOUT0			103 /* generic */
#define	GPIO104_KP_MKOUT1			104 /* generic */
#define	GPIO105_KP_MKOUT2			105 /* generic */
#define GHI270_H_GPIO106_NAND_RNB		106
#define GPIO106_KP_MKOUT3			106
#define GPIO107_KP_MKOUT4			107 /* generic */
#define	GPIO108_KP_MKOUT5			108 /* generic */

/* GPIO109_MMCDAT1 */				/* MMC */
/* GPIO110_MMCDAT2 */				/* MMC */
/* GPIO111_MMCDAT3 */				/* MMC */
/* GPIO112_MMCCMD */				/* MMC */

/* GPIO113_AC97_RESET_N */			/* AC97 */

#define GPIO114_CIF_DD_1			114 /* generic */
#define GPIO115_CIF_DD_3			115 /* generic */
#define GHI270_HG_GPIO115_GPS_nSLEEP		115
#define GPIO116_CIF_DD_2			116 /* generic */
#define GHI270_HG_GPIO116_GPS_nRESET		116

#define	GPIO117_I2CSCL				117 /* generic */
#define GPIO118_I2CSDA				118 /* generic */

/* GPIOs 119 and 120 seem not to be connected */

#endif /* _GHI270_GPIO_H */
/* vim600: set noexpandtab sw=8: */

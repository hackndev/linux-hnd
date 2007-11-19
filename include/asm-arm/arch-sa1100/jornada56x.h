/*
 * linux/include/asm-arm/arch-sa1100/jornada56x.h
 *
 * Created 2002/3/6 by John Ankcorn <jca@alum.mit.edu>
 *
 * This file contains the hardware specific definitions for HP Jornada 56x
 *
 */

#ifndef __ASM_ARCH_JORNADA56X_H
#define __ASM_ARCH_JORNADA56X_H


#ifndef __ASSEMBLY__

/* SA-1110 GPIO lines */
#define GPIO_JORNADA56X_ASIC		GPIO_GPIO(10)
#define GPIO_JORNADA56X_ASIC_IRQ	IRQ_GPIO10
#define GPIO_JORNADA56X_POWER_SWITCH	GPIO_GPIO0
#define GPIO_JORNADA56X_POWER_SWITCH_IRQ	IRQ_GPIO0
#define GPIO_JORNADA56X_TOUCH		GPIO_GPIO13
#define GPIO_JORNADA56X_TOUCH_IRQ	IRQ_GPIO13

#define JORNADA56X_ASIC_BASE		0xf0000000 /* physical 0x40000000 */

#define PMU_BASE		((volatile long *)JORNADA56X_ASIC_BASE)
#define JORNADA_SCR	PMU_BASE[0]	/* System Control Register - scr */
#define JORNADA_ASIC_SLEEP_EN		0x0001
#define JORNADA_RCLK_EN			0x0002 /* 1=Enable internal system reference clock 3.6864MHz */
#define JORNADA_INT_TEST_BIT		0x0004
#define JORNADA_PLL_EN			0x0008	/*1=Enable PLL, 0=Bypass PLL */
#define JORNADA_PLL_LOCK		0x0010	/*1=Locked, 0=Not locked */
#define JORNADA_SPI_FCLK_EN		0x0020
#define JORNADA_GPIOAFR	PMU_BASE[1]	/*GPIO Alternative Function Register - gpioafr */
#define JORNADA_GP_PWM1			0x02
#define JORNADA_GP_PWM2			0x04
#define JORNADA_GP_MW			0x08
#define JORNADA_GP_L3			0x10
#define JORNADA_GP_SPI			0x20
#define JORNADA_PWM1_CKDR	PMU_BASE[2]
#define JORNADA_PWM2_CKDR	PMU_BASE[3]
#define JORNADA_PCR	PMU_BASE[4]	/* Power Control register - pcr */
#define JORNADA_L3CLK_EN		0x0001
#define JORNADA_PWM1CLK_EN		0x0002
#define JORNADA_PWM2CLK_EN		0x0004
#define JORNADA_MW_CLK_EN		0x0010
#define JORNADA_I2S_CLK_EN		0x0020
#define JORNADA_GPIO_INT_CLK_EN		0x0040
#define JORNADA_UART_CLK_EN		0x0080
#define JORNADA_SM_CLK_EN		0x0100
#define JORNADA_MMC_CLK_EN		0x0200
#define JORNADA_SPI_CLK_EN		0x0400	
#define JORNADA_CF_CLK_EN		0x0800	
#define JORNADA_MUX_CLK0		0x1000	
#define JORNADA_MUX_CLK1		0x2000	
#if 0 /* don't use this register */
#define JORNADA_SRR	PMU_BASE[5]
#define JORNADA_L3_RST			0x0001
#define JORNADA_PWM_RST			0x0002
#define JORNADA_MW_RST			0x0008
#define JORNADA_I2S_RST			0x0010
#define JORNADA_AL_RST			0x0020
#define JORNADA_GPIO_RST		0x0040
#define JORNADA_INT_RST			0x0080
#define JORNADA_UART_RST		0x0100
#define JORNADA_SM_RST			0x0200
#define JORNADA_MMC_RST			0x0400
#define JORNADA_SPI_RST			0x0800
#define JORNADA_CF_RST			0x1000	
#endif

/* Interrupt Control Register Locations */
#define INTR_BASE		((volatile long *)(JORNADA56X_ASIC_BASE+0x100))
#define JORNADA_INT_EN	INTR_BASE[0]
/* or to set, and to clear */
#define JORNADA_INT_STAT	INTR_BASE[1] /* write 1 to clear */
/* Interrupt Source for Group 1 - int_en / int_stat */
#define JORNADA_MMC_INT			0x0001
#define JORNADA_MMC_DETECT_INT		0x0002
#define JORNADA_MMC_REMOVE_INT		0x0004
#define JORNADA_L3_DONE_INT		0x0008
#define JORNADA_MW_TX_INT		0x0020
#define JORNADA_MW_RX_INT		0x0040
#define JORNADA_MW_TUR_INT		0x0080
#define JORNADA_MW_TOR_INT		0x0100
#define JORNADA_MW_ROR_INT		0x0200
#define JORNADA_ALARM_ACK_INT		0x0400
#define JORNADA_CF_REMOVE		0x0800
#define JORNADA_GPIO_B_INT		0x1000
#define JORNADA_GPIO_C_INT		0x2000
#define JORNADA_INT_WK_EN	INTR_BASE[2]
#define JORNADA_INT_WK_POL	INTR_BASE[3]
#define JORNADA_INT_TEST	INTR_BASE[4]
#define JORNADA_GPIOB_RE_EN	INTR_BASE[5] /* rising edge enable */ 
#define JORNADA_GPIOB_FE_EN	INTR_BASE[6] /* falling edge */
#define JORNADA_GPIOB_STAT	INTR_BASE[7]
#define JORNADA_GPIOB_WK_EN	INTR_BASE[8]
#define JORNADA_GPIOB_WK_POL	INTR_BASE[9]
#define JORNADA_GPIOC_RE_EN	INTR_BASE[10]
#define JORNADA_GPIOC_FE_EN	INTR_BASE[11]
#define JORNADA_GPIOC_STAT	INTR_BASE[12]
#define JORNADA_GPIOC_WK_EN	INTR_BASE[13]
#define JORNADA_GPIOC_WK_POL	INTR_BASE[14]
/* no inputs on gpio d */
#define JORNADA_INT_EN2	INTR_BASE[15]
#define JORNADA_INT_STAT2	INTR_BASE[16]
/* Interrupt Source for Group 2 - int_en2 / int_stat2 */
#define JORNADA_UART_INT		0x0001
#define JORNADA_UART_RID_INT		0x0002
#define JORNADA_SM_DETECT_INT		0x0004
#define JORNADA_SM_REMOVE_INT		0x0008
#define JORNADA_SM_RFS_INT		0x0010
#define JORNADA_SM_TFS_INT		0x0020
#define JORNADA_SM_RID_INT		0x0040
#define JORNADA_SPI_TX_INT		0x0080	/*SPI_TX FIFO below threshold */
#define JORNADA_SPI_RX_INT		0x0100	/*SPI_RX FIFO above threshold */
#define JORNADA_SPI_TUR_INT		0x0200	/*SPI_TX FIFO under-run */
#define JORNADA_SPI_TOR_INT		0x0400	/*SPI_TX FIFO overrun */
#define JORNADA_SPI_ROR_INT		0x0800	/*SPI_RX FIFO overrun */
#define JORNADA_CF_INT			0x1000	/*CF IREQ Interrupt */
#define JORNADA_CF_INSERT		0x2000	/*CF Card Detect (insertion) */
#define JORNADA_CF_STSCHG		0x4000	/*CF Status change */
#define JORNADA_CF_WAIT_ERR		0x8000	/*CF wait time-out */
#define JORNADA_INT_WK_EN2	INTR_BASE[17]	/* wake up enable */
#define JORNADA_INT_WK_POL2	INTR_BASE[18]	/* wake up polarity rising/falling edge -not used */
#define JORNADA_INT_TEST2	INTR_BASE[19]

/* CF Control Register Locations */
#define CF_BASE		((volatile long *)(JORNADA56X_ASIC_BASE+0x280))
/* look at SA-1111 */
#define JORNADA_CFSR	CF_BASE[0]	/* CF Status Register Bits */
#define JORNADA_CF_READY 		0x0001
#define JORNADA_CF_VALID  		0x0002
#define JORNADA_CF_VS1  		0x0004
#define JORNADA_CF_VS2  		0x0008
#define JORNADA_CF_WP  			0x0010
#define JORNADA_CF_BVD1  		0x0020
#define JORNADA_CF_BVD2  		0x0040
#define JORNADA_CF_WAIT_TOUT  		0x0080
#define JORNADA_CF_INSERT_LATCH		0x0100
#define JORNADA_CF_REMOVE_LATCH		0x0200
#define JORNADA_CFCR	CF_BASE[1]	/* CF Status Register Bits */
#define JORNADA_CF_RESET		0x0001
#define JORNADA_CF_FLT  		0x0002
#define JORNADA_CF_PWAIT_EN  		0x0004
#define JORNADA_CF_SLEEPSTATE  		0x0008
#define JORNADA_CFWDCR	CF_BASE[2] /* not used */

/* PWM Control Register Locations */
#define PWM_BASE		((volatile long *)(JORNADA56X_ASIC_BASE+0x400))
#define JORNADA_PWM_CTRL	PWM_BASE[0]
#define JORNADA_PWM1_DATA	PWM_BASE[1]
#define JORNADA_PWM2_DATA	PWM_BASE[2]

/* Microwire Control Register Locations */
#define MICROWIRE_BASE	((volatile long *)(JORNADA56X_ASIC_BASE+0x500))
#define JORNADA_MWCR	MICROWIRE_BASE[0] /* Microwire Control register - mwcr */
#define JORNADA_MW_EN		0x01	/* 1=Enable Microwire Controller */
#define JORNADA_DSS_16_BIT	0x06	/* Receive data width = 16 bit */
#define JORNADA_FIFO_RST	0x08	/* 1= Reset the FIFO */
#define JORNADA_MWCR_V_SCR	8		/*Serial Clock Rate bit offset */
#define JORNADA_MWDR	MICROWIRE_BASE[1]
/* These values are for programming the AD7873 used for touchscreen
 * and battery interface.
 * See Table I of data sheet for bit values and Table II for bit positions
 */
#define JORNADA_MW_MAIN_BATTERY	0xa6	/* read main battery */
#define JORNADA_MW_BACKUP_BATTERY 0xe6	/* read backup battery */
#define JORNADA_MW_TOUCH_X	0x90	/* read touchscreen X location */
#define JORNADA_MW_TOUCH_Y	0xD0	/* read touchscreen X location */
#define JORNADA_MW_END	JORNADA_MW_TOUCH_Y /* always end reads with this command */
#define JORNADA_MWFSR	MICROWIRE_BASE[2] /* FIFO Status Register -mwfsr */
#define JORNADA_MW_TNE		0x0001	/*1=TX FIFO not empty */
#define JORNADA_MW_TOR		0x0002	/*1=TX FIFO overrun: try to write when FIFO is full */
#define JORNADA_MW_TFL		0x0004	/*1=TX FIFO is full */
#define JORNADA_MW_RNE		0x0008	/*1=RX FIFO not empty */
#define JORNADA_MW_BSY		0x0010	/*1=Busy TX or RX */
#define JORNADA_MW_TFS		0x0020	/*1=TX FIFO is <= TFL threshold request int */
#define JORNADA_MW_RFS		0x0040	/*1=RX FIFO is => RFL threshold request int */
#define JORNADA_MW_ROR		0x0080	/*1=RX FIFO overrun: FIFO full request int */
#define JORNADA_MWFER	MICROWIRE_BASE[3]
#define JORNADA_MWFTR	MICROWIRE_BASE[4] /* FIFO Control Register mwftr */
#define JORNADA_MWFTR_V_TFT		0 /*Transmit FIFO threshold bit offset */
#define JORNADA_MWFTR_V_RFT		5 /*Receive FIFO threshold bit offset */


/* L3 Control Register Locations */
#define L3_BASE			((volatile long *)(JORNADA56X_ASIC_BASE+0x600))

#define JORNADA_L3CFR	L3_BASE[0]
#define JORNADA_L3_EN			0x01
#define JORNADA_L3_READ_TRG		0x02
#define JORNADA_L3_ADDR_DONE		0x04
#define JORNADA_L3_DATA_DONE		0x08
#define JORNADA_L3_READ_DONE		0x10
#define JORNADA_L3CAR	L3_BASE[1] /* address reg */
#define JORNADA_L3CDW	L3_BASE[2] /* data write */
#define JORNADA_L3CDR	L3_BASE[3] /* data read doesn't work */

#define LED_BASE		((volatile long *)(JORNADA56X_ASIC_BASE+0x780))

#define JORNADA_LEDCR			LED_BASE[0]
#define JORNADA_BKCTRLR			LED_BASE[1]
#define JORNADA_BKCNTLR			LED_BASE[2]
#define JORNADA_BKCNTVR			LED_BASE[3]

#define JORNADA_GREEN_LED_ON			0x001	/* Bit 0	0=Disable 1=Enable */
#define	JORNADA_AMBER_LED_ON			0x002	/* Bit 1	0=Disable 1=Enable */
#define	JORNADA_BLUE_LED_ON			0x004	/* Bit 2	0=Disable 1=Enable */
#define	JORNADA_CHARGE_LED_OFF			0x008	/* Bit 3	0=Enable 1=Disable */
#define	JORNADA_GREEN_LED_BLINK			0x010	/* Bit 4	0=Disable 1=Enable */
#define	JORNADA_AMBER_LED_BLINK			0x020	/* Bit 5	0=Disable 1=Enable */
#define	JORNADA_BLUE_LED_BLINK			0x040	/* Bit 6	0=Disable 1=Enable */
#define	JORNADA_GREEN_LED_SLOW_BLINK		0x080	/* Bit 7	0=Disable 1=Enable */
#define	JORNADA_AMBER_LED_SLOW_BLINK		0x100	/* Bit 8	0=Disable 1=Enable */
#define	JORNADA_BLUE_LED_SLOW_BLINK		0x200	/* Bit 9	0=Disable 1=Enable */
#define	JORNADA_CHARGE_FULL			0x400	/* Bit 10	0=Charing or OFF, 1=Full charge */
#define JORNADA_AC_IN				0x800	/* Bit 11	AC 1=Out 0=In */


/* HP ASIC GPIO Control Register Locations */
#define JORNADA_E_GPIO_BASE		((volatile long *)(JORNADA56X_ASIC_BASE+0x700))
#define JORNADA_GPBPSR	JORNADA_E_GPIO_BASE[1]
/* B[0-3] is freq setting for audio */
/* B[6-8] are rows, B[9-13] are columns for button scan input */
/* The light sensor is a GPIO interrupt connected to the HP ASIC GPIO B14. 
 * If the GPIO is high, means the lower
 * threshold has been crossed and the front light should be turned on. */
#define JORNADA_GPCPSR	JORNADA_E_GPIO_BASE[2]
#define JORNADA_GPBPCR	JORNADA_E_GPIO_BASE[4]
#define JORNADA_GPCPCR	JORNADA_E_GPIO_BASE[5]
#define JORNADA_GPBPLR	JORNADA_E_GPIO_BASE[7]
#define JORNADA_GPCPLR	JORNADA_E_GPIO_BASE[8]
#define JORNADA_GPBPDR	JORNADA_E_GPIO_BASE[10]
#define JORNADA_GPCPDR	JORNADA_E_GPIO_BASE[11]
#define JORNADA_GPBPSDR	JORNADA_E_GPIO_BASE[13]
#define JORNADA_GPCPSDR	JORNADA_E_GPIO_BASE[14]
#define JORNADA_GPBPSLR	JORNADA_E_GPIO_BASE[16]
#define JORNADA_GPCPSLR	JORNADA_E_GPIO_BASE[17]
#define JORNADA_GPBPFDR	JORNADA_E_GPIO_BASE[19]
#define JORNADA_GPCPFDR	JORNADA_E_GPIO_BASE[20]
#define JORNADA_GPBPFLR	JORNADA_E_GPIO_BASE[22]
#define JORNADA_GPCPFLR	JORNADA_E_GPIO_BASE[23]
#define JORNADA_GPDPSR	JORNADA_E_GPIO_BASE[24]
#define JORNADA_RS232_ON	GPIO_GPIO1	/* GPIO D */
#define JORNADA_CF_POWER_OFF	GPIO_GPIO7	/* GPIO D (0 -> power on!) */
#define JORNADA_BACKLIGHT	GPIO_GPIO15	/* GPIO D (0 -> light on!) */
#define JORNADA_GPDPCR	JORNADA_E_GPIO_BASE[25]
#define JORNADA_GPDPLR	JORNADA_E_GPIO_BASE[26]
#define JORNADA_GPDPDR	JORNADA_E_GPIO_BASE[27]
#define JORNADA_GPDPSDR	JORNADA_E_GPIO_BASE[28]
#define JORNADA_GPDPSLR	JORNADA_E_GPIO_BASE[29]
#define JORNADA_GPDPFDR	JORNADA_E_GPIO_BASE[30]
#define JORNADA_GPDPFLR	JORNADA_E_GPIO_BASE[31]

/* Interrupts */
/* 16 bits for interrupts in JORNADA_INT_EN */
#define IRQ_JORNADA_MMC		(IRQ_BOARD_END + 0)
#define IRQ_JORNADA_MMC_DETECT	(IRQ_BOARD_END + 1)
#define IRQ_JORNADA_MMC_REMOVE	(IRQ_BOARD_END + 2)
#define IRQ_JORNADA_L3_DONE	(IRQ_BOARD_END + 3)
#define IRQ_JORNADA_MW_TX	(IRQ_BOARD_END + 5)
#define IRQ_JORNADA_MW_RX	(IRQ_BOARD_END + 6)
#define IRQ_JORNADA_TUR		(IRQ_BOARD_END + 7)
#define IRQ_JORNADA_TOR		(IRQ_BOARD_END + 8)
#define IRQ_JORNADA_ROR		(IRQ_BOARD_END + 9)
#define IRQ_JORNADA_ALARM_ACK	(IRQ_BOARD_END + 10)
#define IRQ_JORNADA_CF_REMOVE	(IRQ_BOARD_END + 11)
#define IRQ_JORNADA_GPIO_B	(IRQ_BOARD_END + 12)
#define IRQ_JORNADA_GPIO_C	(IRQ_BOARD_END + 13)
/* 16 bits for interrupts in JORNADA_INT_EN2 */
#define IRQ_JORNADA_UART	(IRQ_BOARD_END + 16)
#define IRQ_JORNADA_UART_RID	(IRQ_BOARD_END + 17)
#define IRQ_JORNADA_SM_DETECT	(IRQ_BOARD_END + 18)
#define IRQ_JORNADA_SM_REMOVE	(IRQ_BOARD_END + 19)
#define IRQ_JORNADA_SM_RFS	(IRQ_BOARD_END + 20)
#define IRQ_JORNADA_SM_TFS	(IRQ_BOARD_END + 21)
#define IRQ_JORNADA_SM_RID	(IRQ_BOARD_END + 22)
#define IRQ_JORNADA_SPI_TX	(IRQ_BOARD_END + 23) /*SPI_TX FIFO below threshold */
#define IRQ_JORNADA_SPI_RX	(IRQ_BOARD_END + 24) /*SPI_RX FIFO above threshold */
#define IRQ_JORNADA_SPI_TUR	(IRQ_BOARD_END + 25) /*SPI_TX FIFO under-run */
#define IRQ_JORNADA_SPI_TOR	(IRQ_BOARD_END + 26) /*SPI_TX FIFO overrun */
#define IRQ_JORNADA_SPI_ROR	(IRQ_BOARD_END + 27) /*SPI_RX FIFO overrun */
#define IRQ_JORNADA_CF	(IRQ_BOARD_END + 28)	 /*CF IREQ Interrupt */
#define IRQ_JORNADA_CF_INSERT	(IRQ_BOARD_END + 29)
#define IRQ_JORNADA_CF_STSCHG	(IRQ_BOARD_END + 30)
#define IRQ_JORNADA_CF_WAIT_ERR	(IRQ_BOARD_END + 31)
/* 16 bits for interrupts in JORNADA_GPIO B */
#define IRQ_JORNADA_GPIO_B0	(IRQ_BOARD_END + 32)
#define IRQ_JORNADA_KEY_COL0	(IRQ_BOARD_END + 32 + 9) /* GPIO B(9) */
#define IRQ_JORNADA_GPIO_B15	(IRQ_BOARD_END + 47)
/* 16 bits for interrupts in JORNADA_GPIO C */
#define IRQ_JORNADA_GPIO_C0	(IRQ_BOARD_END + 48)
#define IRQ_JORNADA_GPIO_C15	(IRQ_BOARD_END + 63)

#endif
#endif /* __ASM_ARCH_JORNADA56X_H */

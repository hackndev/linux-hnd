/*
 *
 * Definitions for the HTC ASIC3 chip found in several handheld devices 
 *
 * Copyright 2001 Compaq Computer Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
 * AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
 * FITNESS FOR ANY PARTICULAR PURPOSE.
 *
 * Author: Andrew Christian
 *
 */

#ifndef IPAQ_ASIC3_H
#define IPAQ_ASIC3_H

/****************************************************/
/* IPAQ, ASIC #3, replaces ASIC #1 */

#define IPAQ_ASIC3_OFFSET(x,y) (_IPAQ_ASIC3_ ## x ## _Base + _IPAQ_ASIC3_ ## x ## _ ## y)
#define IPAQ_ASIC3_GPIO_OFFSET(x,y) (_IPAQ_ASIC3_GPIO_ ## x ## _Base + _IPAQ_ASIC3_GPIO_ ## y)


/* All offsets below are specified with the following address bus shift */
#define ASIC3_DEFAULT_ADDR_SHIFT 2

#define _IPAQ_ASIC3_GPIO_A_Base      0x0000
#define _IPAQ_ASIC3_GPIO_B_Base      0x0100
#define _IPAQ_ASIC3_GPIO_C_Base      0x0200
#define _IPAQ_ASIC3_GPIO_D_Base      0x0300

#define _IPAQ_ASIC3_GPIO_Mask          0x00    /* R/W 0:don't mask, 1:mask interrupt */
#define _IPAQ_ASIC3_GPIO_Direction     0x04    /* R/W 0:input, 1:output              */
#define _IPAQ_ASIC3_GPIO_Out           0x08    /* R/W 0:output low, 1:output high    */
#define _IPAQ_ASIC3_GPIO_TriggerType   0x0c    /* R/W 0:level, 1:edge                */
#define _IPAQ_ASIC3_GPIO_EdgeTrigger   0x10    /* R/W 0:falling, 1:rising            */
#define _IPAQ_ASIC3_GPIO_LevelTrigger  0x14    /* R/W 0:low, 1:high level detect     */
#define _IPAQ_ASIC3_GPIO_SleepMask     0x18    /* R/W 0:don't mask, 1:mask trigger in sleep mode  */
#define _IPAQ_ASIC3_GPIO_SleepOut      0x1c    /* R/W level 0:low, 1:high in sleep mode           */
#define _IPAQ_ASIC3_GPIO_BattFaultOut  0x20    /* R/W level 0:low, 1:high in batt_fault           */
#define _IPAQ_ASIC3_GPIO_IntStatus     0x24    /* R/W 0:none, 1:detect               */
#define _IPAQ_ASIC3_GPIO_AltFunction   0x28	/* R/W 0:normal control 1:LED register control */
#define _IPAQ_ASIC3_GPIO_SleepConf     0x2c    /* R/W bit 1: autosleep 0: disable gposlpout in normal mode, enable gposlpout in sleep mode */
#define _IPAQ_ASIC3_GPIO_Status        0x30    /* R   Pin status                                  */

#define IPAQ_ASIC3_GPIO_A_MASK(_b)            IPAQ_ASIC3_GPIO( _b, u16, A, Mask )
#define IPAQ_ASIC3_GPIO_A_DIR(_b)             IPAQ_ASIC3_GPIO( _b, u16, A, Direction )       
#define IPAQ_ASIC3_GPIO_A_OUT(_b)             IPAQ_ASIC3_GPIO( _b, u16, A, Out )    
#define IPAQ_ASIC3_GPIO_A_LEVELTRI(_b)        IPAQ_ASIC3_GPIO( _b, u16, A, TriggerType )
#define IPAQ_ASIC3_GPIO_A_RISING(_b)          IPAQ_ASIC3_GPIO( _b, u16, A, EdgeTrigger )
#define IPAQ_ASIC3_GPIO_A_LEVEL(_b)           IPAQ_ASIC3_GPIO( _b, u16, A, LevelTrigger )
#define IPAQ_ASIC3_GPIO_A_SLEEP_MASK(_b)      IPAQ_ASIC3_GPIO( _b, u16, A, SleepMask )
#define IPAQ_ASIC3_GPIO_A_SLEEP_OUT(_b)       IPAQ_ASIC3_GPIO( _b, u16, A, SleepOut )
#define IPAQ_ASIC3_GPIO_A_BATT_FAULT_OUT(_b)  IPAQ_ASIC3_GPIO( _b, u16, A, BattFaultOut )
#define IPAQ_ASIC3_GPIO_A_INT_STATUS(_b)      IPAQ_ASIC3_GPIO( _b, u16, A, IntStatus )
#define IPAQ_ASIC3_GPIO_A_ALT_FUNCTION(_b)    IPAQ_ASIC3_GPIO( _b, u16, A, AltFunction )
#define IPAQ_ASIC3_GPIO_A_SLEEP_CONF(_b)      IPAQ_ASIC3_GPIO( _b, u16, A, SleepConf )
#define IPAQ_ASIC3_GPIO_A_STATUS(_b)          IPAQ_ASIC3_GPIO( _b, u16, A, Status )

#define IPAQ_ASIC3_GPIO_B_MASK(_b)            IPAQ_ASIC3_GPIO( _b, u16, B, Mask )
#define IPAQ_ASIC3_GPIO_B_DIR(_b)             IPAQ_ASIC3_GPIO( _b, u16, B, Direction )       
#define IPAQ_ASIC3_GPIO_B_OUT(_b)             IPAQ_ASIC3_GPIO( _b, u16, B, Out )    
#define IPAQ_ASIC3_GPIO_B_LEVELTRI(_b)        IPAQ_ASIC3_GPIO( _b, u16, B, TriggerType )
#define IPAQ_ASIC3_GPIO_B_RISING(_b)          IPAQ_ASIC3_GPIO( _b, u16, B, EdgeTrigger )
#define IPAQ_ASIC3_GPIO_B_LEVEL(_b)           IPAQ_ASIC3_GPIO( _b, u16, B, LevelTrigger )
#define IPAQ_ASIC3_GPIO_B_SLEEP_MASK(_b)      IPAQ_ASIC3_GPIO( _b, u16, B, SleepMask )
#define IPAQ_ASIC3_GPIO_B_SLEEP_OUT(_b)       IPAQ_ASIC3_GPIO( _b, u16, B, SleepOut )
#define IPAQ_ASIC3_GPIO_B_BATT_FAULT_OUT(_b)  IPAQ_ASIC3_GPIO( _b, u16, B, BattFaultOut )
#define IPAQ_ASIC3_GPIO_B_INT_STATUS(_b)      IPAQ_ASIC3_GPIO( _b, u16, B, IntStatus )
#define IPAQ_ASIC3_GPIO_B_ALT_FUNCTION(_b)    IPAQ_ASIC3_GPIO( _b, u16, B, AltFunction )
#define IPAQ_ASIC3_GPIO_B_SLEEP_CONF(_b)      IPAQ_ASIC3_GPIO( _b, u16, B, SleepConf )
#define IPAQ_ASIC3_GPIO_B_STATUS(_b)          IPAQ_ASIC3_GPIO( _b, u16, B, Status )

#define IPAQ_ASIC3_GPIO_C_MASK(_b)            IPAQ_ASIC3_GPIO( _b, u16, C, Mask )
#define IPAQ_ASIC3_GPIO_C_DIR(_b)             IPAQ_ASIC3_GPIO( _b, u16, C, Direction )       
#define IPAQ_ASIC3_GPIO_C_OUT(_b)             IPAQ_ASIC3_GPIO( _b, u16, C, Out )    
#define IPAQ_ASIC3_GPIO_C_LEVELTRI(_b)        IPAQ_ASIC3_GPIO( _b, u16, C, TriggerType )
#define IPAQ_ASIC3_GPIO_C_RISING(_b)          IPAQ_ASIC3_GPIO( _b, u16, C, EdgeTrigger )
#define IPAQ_ASIC3_GPIO_C_LEVEL(_b)           IPAQ_ASIC3_GPIO( _b, u16, C, LevelTrigger )
#define IPAQ_ASIC3_GPIO_C_SLEEP_MASK(_b)      IPAQ_ASIC3_GPIO( _b, u16, C, SleepMask )
#define IPAQ_ASIC3_GPIO_C_SLEEP_OUT(_b)       IPAQ_ASIC3_GPIO( _b, u16, C, SleepOut )
#define IPAQ_ASIC3_GPIO_C_BATT_FAULT_OUT(_b)  IPAQ_ASIC3_GPIO( _b, u16, C, BattFaultOut )
#define IPAQ_ASIC3_GPIO_C_INT_STATUS(_b)      IPAQ_ASIC3_GPIO( _b, u16, C, IntStatus )
#define IPAQ_ASIC3_GPIO_C_ALT_FUNCTION(_b)    IPAQ_ASIC3_GPIO( _b, u16, C, AltFunction )
#define IPAQ_ASIC3_GPIO_C_SLEEP_CONF(_b)      IPAQ_ASIC3_GPIO( _b, u16, C, SleepConf )
#define IPAQ_ASIC3_GPIO_C_STATUS(_b)          IPAQ_ASIC3_GPIO( _b, u16, C, Status )

#define IPAQ_ASIC3_GPIO_D_MASK(_b)            IPAQ_ASIC3_GPIO( _b, u16, D, Mask )
#define IPAQ_ASIC3_GPIO_D_DIR(_b)             IPAQ_ASIC3_GPIO( _b, u16, D, Direction )       
#define IPAQ_ASIC3_GPIO_D_OUT(_b)             IPAQ_ASIC3_GPIO( _b, u16, D, Out )    
#define IPAQ_ASIC3_GPIO_D_LEVELTRI(_b)        IPAQ_ASIC3_GPIO( _b, u16, D, TriggerType )
#define IPAQ_ASIC3_GPIO_D_RISING(_b)          IPAQ_ASIC3_GPIO( _b, u16, D, EdgeTrigger )
#define IPAQ_ASIC3_GPIO_D_LEVEL(_b)           IPAQ_ASIC3_GPIO( _b, u16, D, LevelTrigger )
#define IPAQ_ASIC3_GPIO_D_SLEEP_MASK(_b)      IPAQ_ASIC3_GPIO( _b, u16, D, SleepMask )
#define IPAQ_ASIC3_GPIO_D_SLEEP_OUT(_b)       IPAQ_ASIC3_GPIO( _b, u16, D, SleepOut )
#define IPAQ_ASIC3_GPIO_D_BATT_FAULT_OUT(_b)  IPAQ_ASIC3_GPIO( _b, u16, D, BattFaultOut )
#define IPAQ_ASIC3_GPIO_D_INT_STATUS(_b)      IPAQ_ASIC3_GPIO( _b, u16, D, IntStatus )
#define IPAQ_ASIC3_GPIO_D_ALT_FUNCTION(_b)    IPAQ_ASIC3_GPIO( _b, u16, D, AltFunction )
#define IPAQ_ASIC3_GPIO_D_SLEEP_CONF(_b)      IPAQ_ASIC3_GPIO( _b, u16, D, SleepConf )
#define IPAQ_ASIC3_GPIO_D_STATUS(_b)          IPAQ_ASIC3_GPIO( _b, u16, D, Status )

#define _IPAQ_ASIC3_SPI_Base		      0x0400
#define _IPAQ_ASIC3_SPI_Control               0x0000
#define _IPAQ_ASIC3_SPI_TxData                0x0004
#define _IPAQ_ASIC3_SPI_RxData                0x0008
#define _IPAQ_ASIC3_SPI_Int                   0x000c
#define _IPAQ_ASIC3_SPI_Status                0x0010

#define IPAQ_ASIC3_SPI_Control(_b)            IPAQ_ASIC3( _b, u16, SPI, Control )
#define IPAQ_ASIC3_SPI_TxData(_b)             IPAQ_ASIC3( _b, u16, SPI, TxData )
#define IPAQ_ASIC3_SPI_RxData(_b)             IPAQ_ASIC3( _b, u16, SPI, RxData )
#define IPAQ_ASIC3_SPI_Int(_b)                IPAQ_ASIC3( _b, u16, SPI, Int )
#define IPAQ_ASIC3_SPI_Status(_b)             IPAQ_ASIC3( _b, u16, SPI, Status )

#define SPI_CONTROL_SPR(clk)      ((clk) & 0x0f)  /* Clock rate */

#define _IPAQ_ASIC3_PWM_0_Base                0x0500
#define _IPAQ_ASIC3_PWM_1_Base                0x0600
#define _IPAQ_ASIC3_PWM_TimeBase              0x0000
#define _IPAQ_ASIC3_PWM_PeriodTime            0x0004
#define _IPAQ_ASIC3_PWM_DutyTime              0x0008

#define IPAQ_ASIC3_PWM_TimeBase(_b, x)        IPAQ_ASIC3_N( _b, u16, PWM, x, TimeBase )
#define IPAQ_ASIC3_PWM_PeriodTime(_b, x)      IPAQ_ASIC3_N( _b, u16, PWM, x, PeriodTime )
#define IPAQ_ASIC3_PWM_DutyTime(_b, x)        IPAQ_ASIC3_N( _b, u16, PWM, x, DutyTime )

#define PWM_TIMEBASE_VALUE(x)    ((x)&0xf)   /* Low 4 bits sets time base */
#define PWM_TIMEBASE_ENABLE     (1 << 4)   /* Enable clock */

#define _IPAQ_ASIC3_LED_0_Base                0x0700
#define _IPAQ_ASIC3_LED_1_Base                0x0800
#define _IPAQ_ASIC3_LED_2_Base 		      0x0900
#define _IPAQ_ASIC3_LED_TimeBase              0x0000    /* R/W  7 bits */
#define _IPAQ_ASIC3_LED_PeriodTime            0x0004    /* R/W 12 bits */
#define _IPAQ_ASIC3_LED_DutyTime              0x0008    /* R/W 12 bits */
#define _IPAQ_ASIC3_LED_AutoStopCount         0x000c    /* R/W 16 bits */

#define IPAQ_ASIC3_LED_TimeBase(_b, x)         IPAQ_ASIC3_N( _b,  u8, LED, x, TimeBase )
#define IPAQ_ASIC3_LED_PeriodTime(_b, x)       IPAQ_ASIC3_N( _b, u16, LED, x, PeriodTime )
#define IPAQ_ASIC3_LED_DutyTime(_b, x)         IPAQ_ASIC3_N( _b, u16, LED, x, DutyTime )
#define IPAQ_ASIC3_LED_AutoStopCount(_b, x)    IPAQ_ASIC3_N( _b, u16, LED, x, AutoStopCount )

/* LED TimeBase bits - match ASIC2 */
#define LED_TBS		0x0f		/* Low 4 bits sets time base, max = 13		*/
					/* Note: max = 5 on hx4700			*/
					/* 0: maximum time base				*/
					/* 1: maximum time base / 2			*/
					/* n: maximum time base / 2^n			*/

#define LED_EN		(1 << 4)	/* LED ON/OFF 0:off, 1:on			*/
#define LED_AUTOSTOP	(1 << 5)	/* LED ON/OFF auto stop set 0:disable, 1:enable */ 
#define LED_ALWAYS	(1 << 6)	/* LED Interrupt Mask 0:No mask, 1:mask		*/

#define _IPAQ_ASIC3_CLOCK_Base		0x0A00
#define _IPAQ_ASIC3_CLOCK_CDEX           0x00
#define _IPAQ_ASIC3_CLOCK_SEL            0x04

#define IPAQ_ASIC3_CLOCK_CDEX(_b)         IPAQ_ASIC3( _b, u16, CLOCK, CDEX )
#define IPAQ_ASIC3_CLOCK_SEL(_b)          IPAQ_ASIC3( _b, u16, CLOCK, SEL )

#define CLOCK_CDEX_SOURCE       (1 << 0)  /* 2 bits */
#define CLOCK_CDEX_SOURCE0      (1 << 0)
#define CLOCK_CDEX_SOURCE1      (1 << 1)
#define CLOCK_CDEX_SPI          (1 << 2)
#define CLOCK_CDEX_OWM          (1 << 3)
#define CLOCK_CDEX_PWM0         (1 << 4)
#define CLOCK_CDEX_PWM1         (1 << 5)
#define CLOCK_CDEX_LED0         (1 << 6)
#define CLOCK_CDEX_LED1         (1 << 7)
#define CLOCK_CDEX_LED2         (1 << 8)

#define CLOCK_CDEX_SD_HOST      (1 << 9)   /* R/W: SD host clock source 24.576M/12.288M */
#define CLOCK_CDEX_SD_BUS       (1 << 10)  /* R/W: SD bus clock source control 24.576M/12.288M */
#define CLOCK_CDEX_SMBUS        (1 << 11)
#define CLOCK_CDEX_CONTROL_CX   (1 << 12)

#define CLOCK_CDEX_EX0          (1 << 13)  /* R/W: 32.768 kHz crystal */
#define CLOCK_CDEX_EX1          (1 << 14)  /* R/W: 24.576 MHz crystal */

#define CLOCK_SEL_SD_HCLK_SEL   (1 << 0)   /* R/W: SDIO host clock select  -  1: 24.576 Mhz, 0: 12.288 MHz */
#define CLOCK_SEL_SD_BCLK_SEL   (1 << 1)   /* R/W: SDIO bus clock select -  1: 24.576 MHz, 0: 12.288 MHz */
#define CLOCK_SEL_CX            (1 << 2)   /* R/W: INT clock source control (32.768 kHz) */


#define _IPAQ_ASIC3_INTR_Base		0x0B00

#define _IPAQ_ASIC3_INTR_IntMask       0x00  /* Interrupt mask control */
#define _IPAQ_ASIC3_INTR_PIntStat      0x04  /* Peripheral interrupt status */
#define _IPAQ_ASIC3_INTR_IntCPS        0x08  /* Interrupt timer clock pre-scale */
#define _IPAQ_ASIC3_INTR_IntTBS        0x0c  /* Interrupt timer set */

#define IPAQ_ASIC3_INTR_IntMask(_b)       IPAQ_ASIC3( _b, u8, INTR, IntMask )
#define IPAQ_ASIC3_INTR_PIntStat(_b)      IPAQ_ASIC3( _b, u8, INTR, PIntStat )
#define IPAQ_ASIC3_INTR_IntCPS(_b)        IPAQ_ASIC3( _b, u8, INTR, IntCPS )
#define IPAQ_ASIC3_INTR_IntTBS(_b)        IPAQ_ASIC3( _b, u16, INTR, IntTBS )

#define ASIC3_INTMASK_GINTMASK    (1 << 0)  /* Global interrupt mask 1:enable */
#define ASIC3_INTMASK_GINTEL      (1 << 1)  /* 1: rising edge, 0: hi level */
#define ASIC3_INTMASK_MASK0       (1 << 2)
#define ASIC3_INTMASK_MASK1       (1 << 3)
#define ASIC3_INTMASK_MASK2       (1 << 4)
#define ASIC3_INTMASK_MASK3       (1 << 5)
#define ASIC3_INTMASK_MASK4       (1 << 6)
#define ASIC3_INTMASK_MASK5       (1 << 7)

#define ASIC3_INTR_PERIPHERAL_A   (1 << 0)
#define ASIC3_INTR_PERIPHERAL_B   (1 << 1)
#define ASIC3_INTR_PERIPHERAL_C   (1 << 2)
#define ASIC3_INTR_PERIPHERAL_D   (1 << 3)
#define ASIC3_INTR_LED0           (1 << 4)
#define ASIC3_INTR_LED1           (1 << 5)
#define ASIC3_INTR_LED2           (1 << 6)
#define ASIC3_INTR_SPI            (1 << 7)
#define ASIC3_INTR_SMBUS          (1 << 8)
#define ASIC3_INTR_OWM            (1 << 9)

#define ASIC3_INTR_CPS(x)         ((x)&0x0f)    /* 4 bits, max 14 */
#define ASIC3_INTR_CPS_SET        ( 1 << 4 )    /* Time base enable */


/* Basic control of the SD ASIC */
#define _IPAQ_ASIC3_SDHWCTRL_Base	0x0E00

#define _IPAQ_ASIC3_SDHWCTRL_SDConf    0x00
#define IPAQ_ASIC3_SDHWCTRL_SDConf(_b)    IPAQ_ASIC3( _b, u8, SDHWCTRL, SDConf )

#define ASIC3_SDHWCTRL_SUSPEND    (1 << 0)  /* 1=suspend all SD operations */
#define ASIC3_SDHWCTRL_CLKSEL     (1 << 1)  /* 1=SDICK, 0=HCLK */
#define ASIC3_SDHWCTRL_PCLR       (1 << 2)  /* All registers of SDIO cleared */
#define ASIC3_SDHWCTRL_LEVCD      (1 << 3)  /* Level of SD card detection: 1:high, 0:low */
#define ASIC3_SDHWCTRL_LEVWP      (1 << 4)  /* Level of SD card write protection: 1=low, 0=high */
#define ASIC3_SDHWCTRL_SDLED      (1 << 5)  /* SD card LED signal 1=enable, 0=disable */
#define ASIC3_SDHWCTRL_SDPWR      (1 << 6)  /* SD card power supply control 1=enable */


/* This is a pointer to an array of 12 u32 values - but only the lower 2 bytes matter */
/* Use it as "IPAQ_ASIC3_HWPROTECT_ARRAY[x]" */

#define _IPAQ_ASIC3_HWPROTECT_Base	0x1000
#define IPAQ_ASIC3_HWPROTECT_ARRAY  ((volatile u32*)(_IPAQ_ASIC3_Base + _IPAQ_ASIC3_HWPROTECT_Base))
#define HWPROTECT_ARRAY_LEN 12
#define HWPROTECT_ARRAY_VALUES {0x4854,0x432d,0x5344,0x494f,0x2050,0x2f4e,0x3a33,0x3048,0x3830,0x3032,0x382d,0x3030}


#define _IPAQ_ASIC3_EXTCF_Base		0x1100

#define _IPAQ_ASIC3_EXTCF_Select         0x00
#define _IPAQ_ASIC3_EXTCF_Reset          0x04

#define IPAQ_ASIC3_EXTCF_Select(_b)    IPAQ_ASIC3( _b, u16, EXTCF, Select )
#define IPAQ_ASIC3_EXTCF_Reset(_b)     IPAQ_ASIC3( _b, u16, EXTCF, Reset )

#define ASIC3_EXTCF_SMOD0	         (1 << 0)  /* slot number of mode 0 */
#define ASIC3_EXTCF_SMOD1	         (1 << 1)  /* slot number of mode 1 */
#define ASIC3_EXTCF_SMOD2	         (1 << 2)  /* slot number of mode 2 */
#define ASIC3_EXTCF_OWM_EN	         (1 << 4)  /* enable onewire module */
#define ASIC3_EXTCF_OWM_SMB	         (1 << 5)  /* OWM bus selection */
#define ASIC3_EXTCF_OWM_RESET            (1 << 6)  /* undocumented, used by OWM and CF */
#define ASIC3_EXTCF_CF0_SLEEP_MODE       (1 << 7)  /* CF0 sleep state control */
#define ASIC3_EXTCF_CF1_SLEEP_MODE       (1 << 8)  /* CF1 sleep state control */
#define ASIC3_EXTCF_CF0_PWAIT_EN         (1 << 10)  /* CF0 PWAIT_n control */
#define ASIC3_EXTCF_CF1_PWAIT_EN         (1 << 11)  /* CF1 PWAIT_n control */
#define ASIC3_EXTCF_CF0_BUF_EN           (1 << 12)  /* CF0 buffer control */
#define ASIC3_EXTCF_CF1_BUF_EN           (1 << 13)  /* CF1 buffer control */
#define ASIC3_EXTCF_SD_MEM_ENABLE        (1 << 14)
#define ASIC3_EXTCF_CF_SLEEP             (1 << 15)  /* CF sleep mode control */

/*****************************************************************************
 *  The Onewire interface registers
 *
 *  OWM_CMD
 *  OWM_DAT
 *  OWM_INTR
 *  OWM_INTEN
 *  OWM_CLKDIV
 *
 *****************************************************************************/

#define _IPAQ_ASIC3_OWM_Base		0xC00

#define _IPAQ_ASIC3_OWM_CMD         0x00
#define _IPAQ_ASIC3_OWM_DAT         0x04
#define _IPAQ_ASIC3_OWM_INTR        0x08
#define _IPAQ_ASIC3_OWM_INTEN       0x0C
#define _IPAQ_ASIC3_OWM_CLKDIV      0x10

#define ASIC3_OWM_CMD_ONEWR         (1 << 0)
#define ASIC3_OWM_CMD_SRA           (1 << 1)
#define ASIC3_OWM_CMD_DQO           (1 << 2)
#define ASIC3_OWM_CMD_DQI           (1 << 3)

#define ASIC3_OWM_INTR_PD          (1 << 0)
#define ASIC3_OWM_INTR_PDR         (1 << 1)
#define ASIC3_OWM_INTR_TBE         (1 << 2)
#define ASIC3_OWM_INTR_TEMP        (1 << 3)
#define ASIC3_OWM_INTR_RBF         (1 << 4)

#define ASIC3_OWM_INTEN_EPD        (1 << 0)
#define ASIC3_OWM_INTEN_IAS        (1 << 1)
#define ASIC3_OWM_INTEN_ETBE       (1 << 2)
#define ASIC3_OWM_INTEN_ETMT       (1 << 3)
#define ASIC3_OWM_INTEN_ERBF       (1 << 4)

#define ASIC3_OWM_CLKDIV_PRE       (3 << 0) /* two bits wide at bit position 0 */
#define ASIC3_OWM_CLKDIV_DIV       (7 << 2) /* 3 bits wide at bit position 2 */


/*****************************************************************************
 *  The SD configuration registers are at a completely different location
 *  in memory.  They are divided into three sets of registers:
 *
 *  SD_CONFIG         Core configuration register
 *  SD_CTRL           Control registers for SD operations
 *  SDIO_CTRL         Control registers for SDIO operations
 *
 *****************************************************************************/

#define IPAQ_ASIC3_SD_CONFIG(_b, s,x)   \
     (*((volatile s *) ((_b) + _IPAQ_ASIC3_SD_CONFIG_Base + (_IPAQ_ASIC3_SD_CONFIG_ ## x))))

#define _IPAQ_ASIC3_SD_CONFIG_Base            0x0400    // Assumes 32 bit addressing

#define _IPAQ_ASIC3_SD_CONFIG_Command           0x08   /* R/W: Command */
#define _IPAQ_ASIC3_SD_CONFIG_Addr0             0x20   /* [9:31] SD Control Register Base Address */
#define _IPAQ_ASIC3_SD_CONFIG_Addr1             0x24   /* [9:31] SD Control Register Base Address */
#define _IPAQ_ASIC3_SD_CONFIG_IntPin            0x78   /* R/O: interrupt assigned to pin */
#define _IPAQ_ASIC3_SD_CONFIG_ClkStop           0x80   /* Set to 0x1f to clock SD controller, 0 otherwise. */
							/* at 0x82 - Gated Clock Control */
#define _IPAQ_ASIC3_SD_CONFIG_ClockMode         0x84   /* Control clock of SD controller */
#define _IPAQ_ASIC3_SD_CONFIG_SDHC_PinStatus    0x88   /* R/0: read status of SD pins */
#define _IPAQ_ASIC3_SD_CONFIG_SDHC_Power1       0x90   /* Power1 - manual power control */
							/* Power2 is at 0x92 - auto power up after card inserted */
#define _IPAQ_ASIC3_SD_CONFIG_SDHC_Power3       0x94   /* auto power down when card removed */
#define _IPAQ_ASIC3_SD_CONFIG_SDHC_CardDetect   0x98   /* */
#define _IPAQ_ASIC3_SD_CONFIG_SDHC_Slot         0xA0   /* R/O: define support slot number */
#define _IPAQ_ASIC3_SD_CONFIG_SDHC_ExtGateClk1  0x1E0  /* Could be used for gated clock (don't use) */
#define _IPAQ_ASIC3_SD_CONFIG_SDHC_ExtGateClk2  0x1E2  /* Could be used for gated clock (don't use) */
#define _IPAQ_ASIC3_SD_CONFIG_SDHC_GPIO_OutAndEnable  0x1E8  /* GPIO Output Reg. , at 0x1EA - GPIO Output Enable Reg. */
#define _IPAQ_ASIC3_SD_CONFIG_SDHC_GPIO_Status  0x1EC  /* GPIO Status Reg. */
#define _IPAQ_ASIC3_SD_CONFIG_SDHC_ExtGateClk3  0x1F0  /* Bit 1: double buffer/single buffer */
 
#define IPAQ_ASIC3_SD_CONFIG_Command(_b)           IPAQ_ASIC3_SD_CONFIG(_b, u16, Command )
#define IPAQ_ASIC3_SD_CONFIG_Addr0(_b)             IPAQ_ASIC3_SD_CONFIG(_b, u16, Addr0 )
#define IPAQ_ASIC3_SD_CONFIG_Addr1(_b)             IPAQ_ASIC3_SD_CONFIG(_b, u16, Addr1 )
#define IPAQ_ASIC3_SD_CONFIG_IntPin(_b)            IPAQ_ASIC3_SD_CONFIG(_b, u8, IntPin )
#define IPAQ_ASIC3_SD_CONFIG_ClkStop(_b)           IPAQ_ASIC3_SD_CONFIG(_b, u8, ClkStop )
#define IPAQ_ASIC3_SD_CONFIG_ClockMode(_b)         IPAQ_ASIC3_SD_CONFIG(_b, u8, ClockMode )
#define IPAQ_ASIC3_SD_CONFIG_SDHC_PinStatus(_b)    IPAQ_ASIC3_SD_CONFIG(_b, u16, SDHC_PinStatus )
#define IPAQ_ASIC3_SD_CONFIG_SDHC_Power1(_b)       IPAQ_ASIC3_SD_CONFIG(_b, u16, SDHC_Power1 )
#define IPAQ_ASIC3_SD_CONFIG_SDHC_Power3(_b)       IPAQ_ASIC3_SD_CONFIG(_b, u16, SDHC_Power3 )
#define IPAQ_ASIC3_SD_CONFIG_SDHC_CardDetect(_b)   IPAQ_ASIC3_SD_CONFIG(_b, u16, SDHC_CardDetect )
#define IPAQ_ASIC3_SD_CONFIG_SDHC_Slot(_b)         IPAQ_ASIC3_SD_CONFIG(_b, u16, SDHC_Slot )
#define IPAQ_ASIC3_SD_CONFIG_SDHC_ExtGateClk1(_b)  IPAQ_ASIC3_SD_CONFIG(_b, u16, SDHC_ExtGateClk1 )
#define IPAQ_ASIC3_SD_CONFIG_SDHC_ExtGateClk3(_b)  IPAQ_ASIC3_SD_CONFIG(_b, u16, SDHC_ExtGateClk3 )

#define SD_CONFIG_

#define SD_CONFIG_COMMAND_MAE                (1<<1)    /* Memory access enable (set to 1 to access SD Controller) */

#define SD_CONFIG_CLK_ENABLE_ALL             0x1f

#define SD_CONFIG_POWER1_PC_33V              0x0200    /* Set for 3.3 volts */
#define SD_CONFIG_POWER1_PC_OFF              0x0000    /* Turn off power */

#define SD_CONFIG_CARDDETECTMODE_CLK           ((x)&0x3) /* two bits - number of cycles for card detection */


#define _IPAQ_ASIC3_SD_CTRL_Base            0x1000

#define IPAQ_ASIC3_SD(_b, s,x)   \
     (*((volatile s *) ((_b) + _IPAQ_ASIC3_SD_CTRL_Base + (_IPAQ_ASIC3_SD_CTRL_ ## x))))

#define _IPAQ_ASIC3_SD_CTRL_Cmd                  0x00
#define _IPAQ_ASIC3_SD_CTRL_Arg0                 0x08
#define _IPAQ_ASIC3_SD_CTRL_Arg1                 0x0C
#define _IPAQ_ASIC3_SD_CTRL_StopInternal         0x10
#define _IPAQ_ASIC3_SD_CTRL_TransferSectorCount  0x14
#define _IPAQ_ASIC3_SD_CTRL_Response0            0x18
#define _IPAQ_ASIC3_SD_CTRL_Response1            0x1C
#define _IPAQ_ASIC3_SD_CTRL_Response2            0x20
#define _IPAQ_ASIC3_SD_CTRL_Response3            0x24
#define _IPAQ_ASIC3_SD_CTRL_Response4            0x28
#define _IPAQ_ASIC3_SD_CTRL_Response5            0x2C
#define _IPAQ_ASIC3_SD_CTRL_Response6            0x30
#define _IPAQ_ASIC3_SD_CTRL_Response7            0x34
#define _IPAQ_ASIC3_SD_CTRL_CardStatus           0x38
#define _IPAQ_ASIC3_SD_CTRL_BufferCtrl           0x3C
#define _IPAQ_ASIC3_SD_CTRL_IntMaskCard          0x40
#define _IPAQ_ASIC3_SD_CTRL_IntMaskBuffer        0x44
#define _IPAQ_ASIC3_SD_CTRL_CardClockCtrl        0x48
#define _IPAQ_ASIC3_SD_CTRL_MemCardXferDataLen   0x4C
#define _IPAQ_ASIC3_SD_CTRL_MemCardOptionSetup   0x50
#define _IPAQ_ASIC3_SD_CTRL_ErrorStatus0         0x58
#define _IPAQ_ASIC3_SD_CTRL_ErrorStatus1         0x5C
#define _IPAQ_ASIC3_SD_CTRL_DataPort             0x60
#define _IPAQ_ASIC3_SD_CTRL_TransactionCtrl      0x68
#define _IPAQ_ASIC3_SD_CTRL_SoftwareReset        0x1C0

#define IPAQ_ASIC3_SD_CTRL_Cmd(_b)                  IPAQ_ASIC3_SD( _b, u16, Cmd )   /* */
#define IPAQ_ASIC3_SD_CTRL_Arg0(_b)                 IPAQ_ASIC3_SD( _b, u16, Arg0 )   /* */
#define IPAQ_ASIC3_SD_CTRL_Arg1(_b)                 IPAQ_ASIC3_SD( _b, u16, Arg1 )   /* */
#define IPAQ_ASIC3_SD_CTRL_StopInternal(_b)         IPAQ_ASIC3_SD( _b, u16, StopInternal )   /* */
#define IPAQ_ASIC3_SD_CTRL_TransferSectorCount(_b)  IPAQ_ASIC3_SD( _b, u16, TransferSectorCount )   /* */
#define IPAQ_ASIC3_SD_CTRL_Response0(_b)            IPAQ_ASIC3_SD( _b, u16, Response0 )   /* */
#define IPAQ_ASIC3_SD_CTRL_Response1(_b)            IPAQ_ASIC3_SD( _b, u16, Response1 )   /* */
#define IPAQ_ASIC3_SD_CTRL_Response2(_b)            IPAQ_ASIC3_SD( _b, u16, Response2 )   /* */
#define IPAQ_ASIC3_SD_CTRL_Response3(_b)            IPAQ_ASIC3_SD( _b, u16, Response3 )   /* */
#define IPAQ_ASIC3_SD_CTRL_Response4(_b)            IPAQ_ASIC3_SD( _b, u16, Response4 )   /* */
#define IPAQ_ASIC3_SD_CTRL_Response5(_b)            IPAQ_ASIC3_SD( _b, u16, Response5 )   /* */
#define IPAQ_ASIC3_SD_CTRL_Response6(_b)            IPAQ_ASIC3_SD( _b, u16, Response6 )   /* */
#define IPAQ_ASIC3_SD_CTRL_Response7(_b)            IPAQ_ASIC3_SD( _b, u16, Response7 )   /* */
#define IPAQ_ASIC3_SD_CTRL_CardStatus(_b)           IPAQ_ASIC3_SD( _b, u16, CardStatus )   /* */
#define IPAQ_ASIC3_SD_CTRL_BufferCtrl(_b)           IPAQ_ASIC3_SD( _b, u16, BufferCtrl )   /* and error status*/
#define IPAQ_ASIC3_SD_CTRL_IntMaskCard(_b)          IPAQ_ASIC3_SD( _b, u16, IntMaskCard )   /* */
#define IPAQ_ASIC3_SD_CTRL_IntMaskBuffer(_b)        IPAQ_ASIC3_SD( _b, u16, IntMaskBuffer )   /* */
#define IPAQ_ASIC3_SD_CTRL_CardClockCtrl(_b)        IPAQ_ASIC3_SD( _b, u16, CardClockCtrl )   /* */
#define IPAQ_ASIC3_SD_CTRL_MemCardXferDataLen(_b)   IPAQ_ASIC3_SD( _b, u16, MemCardXferDataLen )   /* */
#define IPAQ_ASIC3_SD_CTRL_MemCardOptionSetup(_b)   IPAQ_ASIC3_SD( _b, u16, MemCardOptionSetup )   /* */
#define IPAQ_ASIC3_SD_CTRL_ErrorStatus0(_b)         IPAQ_ASIC3_SD( _b, u16, ErrorStatus0 )   /* */
#define IPAQ_ASIC3_SD_CTRL_ErrorStatus1(_b)         IPAQ_ASIC3_SD( _b, u16, ErrorStatus1 )   /* */
#define IPAQ_ASIC3_SD_CTRL_DataPort(_b)             IPAQ_ASIC3_SD( _b, u16, DataPort )   /* */
#define IPAQ_ASIC3_SD_CTRL_TransactionCtrl(_b)      IPAQ_ASIC3_SD( _b, u16, TransactionCtrl )   /* */
#define IPAQ_ASIC3_SD_CTRL_SoftwareReset(_b)        IPAQ_ASIC3_SD( _b, u16, SoftwareReset )   /* */

#define SD_CTRL_SOFTWARE_RESET_CLEAR            (1<<0)

#define SD_CTRL_TRANSACTIONCONTROL_SET          (1<<8) // 0x0100

#define SD_CTRL_CARDCLOCKCONTROL_FOR_SD_CARD    (1<<15)// 0x8000
#define SD_CTRL_CARDCLOCKCONTROL_ENABLE_CLOCK   (1<<8) // 0x0100
#define SD_CTRL_CARDCLOCKCONTROL_CLK_DIV_512    (1<<7) // 0x0080
#define SD_CTRL_CARDCLOCKCONTROL_CLK_DIV_256    (1<<6) // 0x0040
#define SD_CTRL_CARDCLOCKCONTROL_CLK_DIV_128    (1<<5) // 0x0020
#define SD_CTRL_CARDCLOCKCONTROL_CLK_DIV_64     (1<<4) // 0x0010
#define SD_CTRL_CARDCLOCKCONTROL_CLK_DIV_32     (1<<3) // 0x0008
#define SD_CTRL_CARDCLOCKCONTROL_CLK_DIV_16     (1<<2) // 0x0004
#define SD_CTRL_CARDCLOCKCONTROL_CLK_DIV_8      (1<<1) // 0x0002
#define SD_CTRL_CARDCLOCKCONTROL_CLK_DIV_4      (1<<0) // 0x0001
#define SD_CTRL_CARDCLOCKCONTROL_CLK_DIV_2      (0<<0) // 0x0000

#define MEM_CARD_OPTION_REQUIRED                   0x000e
#define MEM_CARD_OPTION_DATA_RESPONSE_TIMEOUT(x)   (((x)&0x0f)<<4)      /* Four bits */
#define MEM_CARD_OPTION_C2_MODULE_NOT_PRESENT      (1<<14) // 0x4000
#define MEM_CARD_OPTION_DATA_XFR_WIDTH_1           (1<<15) // 0x8000
#define MEM_CARD_OPTION_DATA_XFR_WIDTH_4           (0<<15) //~0x8000

#define SD_CTRL_COMMAND_INDEX(x)                   ((x)&0x3f)           /* 0=CMD0, 1=CMD1, ..., 63=CMD63 */
#define SD_CTRL_COMMAND_TYPE_CMD                   (0 << 6)
#define SD_CTRL_COMMAND_TYPE_ACMD                  (1 << 6)
#define SD_CTRL_COMMAND_TYPE_AUTHENTICATION        (2 << 6)
#define SD_CTRL_COMMAND_RESPONSE_TYPE_NORMAL       (0 << 8)
#define SD_CTRL_COMMAND_RESPONSE_TYPE_EXT_R1       (4 << 8)
#define SD_CTRL_COMMAND_RESPONSE_TYPE_EXT_R1B      (5 << 8)
#define SD_CTRL_COMMAND_RESPONSE_TYPE_EXT_R2       (6 << 8)
#define SD_CTRL_COMMAND_RESPONSE_TYPE_EXT_R3       (7 << 8)
#define SD_CTRL_COMMAND_DATA_PRESENT               (1 << 11)
#define SD_CTRL_COMMAND_TRANSFER_READ              (1 << 12)
#define SD_CTRL_COMMAND_TRANSFER_WRITE             (0 << 12)
#define SD_CTRL_COMMAND_MULTI_BLOCK                (1 << 13)
#define SD_CTRL_COMMAND_SECURITY_CMD               (1 << 14)

#define SD_CTRL_STOP_INTERNAL_ISSSUE_CMD12         (1 << 0)
#define SD_CTRL_STOP_INTERNAL_AUTO_ISSUE_CMD12     (1 << 8)

#define SD_CTRL_CARDSTATUS_RESPONSE_END            (1 << 0)
#define SD_CTRL_CARDSTATUS_RW_END                  (1 << 2)
#define SD_CTRL_CARDSTATUS_CARD_REMOVED_0          (1 << 3)
#define SD_CTRL_CARDSTATUS_CARD_INSERTED_0         (1 << 4)
#define SD_CTRL_CARDSTATUS_SIGNAL_STATE_PRESENT_0  (1 << 5)
#define SD_CTRL_CARDSTATUS_WRITE_PROTECT           (1 << 7)
#define SD_CTRL_CARDSTATUS_CARD_REMOVED_3          (1 << 8)
#define SD_CTRL_CARDSTATUS_CARD_INSERTED_3         (1 << 9)
#define SD_CTRL_CARDSTATUS_SIGNAL_STATE_PRESENT_3  (1 << 10)

#define SD_CTRL_BUFFERSTATUS_CMD_INDEX_ERROR       (1 << 0) // 0x0001
#define SD_CTRL_BUFFERSTATUS_CRC_ERROR             (1 << 1) // 0x0002
#define SD_CTRL_BUFFERSTATUS_STOP_BIT_END_ERROR    (1 << 2) // 0x0004
#define SD_CTRL_BUFFERSTATUS_DATA_TIMEOUT          (1 << 3) // 0x0008
#define SD_CTRL_BUFFERSTATUS_BUFFER_OVERFLOW       (1 << 4) // 0x0010
#define SD_CTRL_BUFFERSTATUS_BUFFER_UNDERFLOW      (1 << 5) // 0x0020
#define SD_CTRL_BUFFERSTATUS_CMD_TIMEOUT           (1 << 6) // 0x0040
#define SD_CTRL_BUFFERSTATUS_UNK7                  (1 << 7) // 0x0080
#define SD_CTRL_BUFFERSTATUS_BUFFER_READ_ENABLE    (1 << 8) // 0x0100
#define SD_CTRL_BUFFERSTATUS_BUFFER_WRITE_ENABLE   (1 << 9) // 0x0200
#define SD_CTRL_BUFFERSTATUS_ILLEGAL_FUNCTION      (1 << 13)// 0x2000
#define SD_CTRL_BUFFERSTATUS_CMD_BUSY              (1 << 14)// 0x4000
#define SD_CTRL_BUFFERSTATUS_ILLEGAL_ACCESS        (1 << 15)// 0x8000

#define SD_CTRL_INTMASKCARD_RESPONSE_END           (1 << 0) // 0x0001
#define SD_CTRL_INTMASKCARD_RW_END                 (1 << 2) // 0x0004
#define SD_CTRL_INTMASKCARD_CARD_REMOVED_0         (1 << 3) // 0x0008
#define SD_CTRL_INTMASKCARD_CARD_INSERTED_0        (1 << 4) // 0x0010
#define SD_CTRL_INTMASKCARD_SIGNAL_STATE_PRESENT_0 (1 << 5) // 0x0020
#define SD_CTRL_INTMASKCARD_UNK6                   (1 << 6) // 0x0040
#define SD_CTRL_INTMASKCARD_WRITE_PROTECT          (1 << 7) // 0x0080
#define SD_CTRL_INTMASKCARD_CARD_REMOVED_3         (1 << 8) // 0x0100
#define SD_CTRL_INTMASKCARD_CARD_INSERTED_3        (1 << 9) // 0x0200
#define SD_CTRL_INTMASKCARD_SIGNAL_STATE_PRESENT_3 (1 << 10)// 0x0400

#define SD_CTRL_INTMASKBUFFER_CMD_INDEX_ERROR      (1 << 0) // 0x0001
#define SD_CTRL_INTMASKBUFFER_CRC_ERROR            (1 << 1) // 0x0002
#define SD_CTRL_INTMASKBUFFER_STOP_BIT_END_ERROR   (1 << 2) // 0x0004
#define SD_CTRL_INTMASKBUFFER_DATA_TIMEOUT         (1 << 3) // 0x0008
#define SD_CTRL_INTMASKBUFFER_BUFFER_OVERFLOW      (1 << 4) // 0x0010
#define SD_CTRL_INTMASKBUFFER_BUFFER_UNDERFLOW     (1 << 5) // 0x0020
#define SD_CTRL_INTMASKBUFFER_CMD_TIMEOUT          (1 << 6) // 0x0040
#define SD_CTRL_INTMASKBUFFER_UNK7                 (1 << 7) // 0x0080
#define SD_CTRL_INTMASKBUFFER_BUFFER_READ_ENABLE   (1 << 8) // 0x0100
#define SD_CTRL_INTMASKBUFFER_BUFFER_WRITE_ENABLE  (1 << 9) // 0x0200
#define SD_CTRL_INTMASKBUFFER_ILLEGAL_FUNCTION     (1 << 13)// 0x2000
#define SD_CTRL_INTMASKBUFFER_CMD_BUSY             (1 << 14)// 0x4000
#define SD_CTRL_INTMASKBUFFER_ILLEGAL_ACCESS       (1 << 15)// 0x8000

#define SD_CTRL_DETAIL0_RESPONSE_CMD_ERROR                   (1 << 0) // 0x0001
#define SD_CTRL_DETAIL0_END_BIT_ERROR_FOR_RESPONSE_NON_CMD12 (1 << 2) // 0x0004
#define SD_CTRL_DETAIL0_END_BIT_ERROR_FOR_RESPONSE_CMD12     (1 << 3) // 0x0008
#define SD_CTRL_DETAIL0_END_BIT_ERROR_FOR_READ_DATA          (1 << 4) // 0x0010
#define SD_CTRL_DETAIL0_END_BIT_ERROR_FOR_WRITE_CRC_STATUS   (1 << 5) // 0x0020
#define SD_CTRL_DETAIL0_CRC_ERROR_FOR_RESPONSE_NON_CMD12     (1 << 8) // 0x0100
#define SD_CTRL_DETAIL0_CRC_ERROR_FOR_RESPONSE_CMD12         (1 << 9) // 0x0200
#define SD_CTRL_DETAIL0_CRC_ERROR_FOR_READ_DATA              (1 << 10)// 0x0400
#define SD_CTRL_DETAIL0_CRC_ERROR_FOR_WRITE_CMD              (1 << 11)// 0x0800

#define SD_CTRL_DETAIL1_NO_CMD_RESPONSE                      (1 << 0) // 0x0001
#define SD_CTRL_DETAIL1_TIMEOUT_READ_DATA                    (1 << 4) // 0x0010
#define SD_CTRL_DETAIL1_TIMEOUT_CRS_STATUS                   (1 << 5) // 0x0020
#define SD_CTRL_DETAIL1_TIMEOUT_CRC_BUSY                     (1 << 6) // 0x0040

#define _IPAQ_ASIC3_SDIO_CTRL_Base          0x1200

#define IPAQ_ASIC3_SDIO(_b, s,x)   \
     (*((volatile s *) ((_b) + _IPAQ_ASIC3_SDIO_CTRL_Base + (_IPAQ_ASIC3_SDIO_CTRL_ ## x))))

#define _IPAQ_ASIC3_SDIO_CTRL_Cmd                  0x00
#define _IPAQ_ASIC3_SDIO_CTRL_CardPortSel          0x04
#define _IPAQ_ASIC3_SDIO_CTRL_Arg0                 0x08
#define _IPAQ_ASIC3_SDIO_CTRL_Arg1                 0x0C
#define _IPAQ_ASIC3_SDIO_CTRL_TransferBlockCount   0x14
#define _IPAQ_ASIC3_SDIO_CTRL_Response0            0x18
#define _IPAQ_ASIC3_SDIO_CTRL_Response1            0x1C
#define _IPAQ_ASIC3_SDIO_CTRL_Response2            0x20
#define _IPAQ_ASIC3_SDIO_CTRL_Response3            0x24
#define _IPAQ_ASIC3_SDIO_CTRL_Response4            0x28
#define _IPAQ_ASIC3_SDIO_CTRL_Response5            0x2C
#define _IPAQ_ASIC3_SDIO_CTRL_Response6            0x30
#define _IPAQ_ASIC3_SDIO_CTRL_Response7            0x34
#define _IPAQ_ASIC3_SDIO_CTRL_CardStatus           0x38
#define _IPAQ_ASIC3_SDIO_CTRL_BufferCtrl           0x3C
#define _IPAQ_ASIC3_SDIO_CTRL_IntMaskCard          0x40
#define _IPAQ_ASIC3_SDIO_CTRL_IntMaskBuffer        0x44
#define _IPAQ_ASIC3_SDIO_CTRL_CardXferDataLen      0x4C
#define _IPAQ_ASIC3_SDIO_CTRL_CardOptionSetup      0x50
#define _IPAQ_ASIC3_SDIO_CTRL_ErrorStatus0         0x54
#define _IPAQ_ASIC3_SDIO_CTRL_ErrorStatus1         0x58
#define _IPAQ_ASIC3_SDIO_CTRL_DataPort             0x60
#define _IPAQ_ASIC3_SDIO_CTRL_TransactionCtrl      0x68
#define _IPAQ_ASIC3_SDIO_CTRL_CardIntCtrl          0x6C
#define _IPAQ_ASIC3_SDIO_CTRL_ClocknWaitCtrl       0x70
#define _IPAQ_ASIC3_SDIO_CTRL_HostInformation      0x74
#define _IPAQ_ASIC3_SDIO_CTRL_ErrorCtrl            0x78
#define _IPAQ_ASIC3_SDIO_CTRL_LEDCtrl              0x7C
#define _IPAQ_ASIC3_SDIO_CTRL_SoftwareReset        0x1C0

#define IPAQ_ASIC3_SDIO_CTRL_Cmd(_b)                  IPAQ_ASIC3_SDIO( _b, u16, Cmd )   /* */
#define IPAQ_ASIC3_SDIO_CTRL_CardPortSel(_b)          IPAQ_ASIC3_SDIO( _b, u16, CardPortSel )   /* */
#define IPAQ_ASIC3_SDIO_CTRL_Arg0(_b)                 IPAQ_ASIC3_SDIO( _b, u16, Arg0 )   /* */
#define IPAQ_ASIC3_SDIO_CTRL_Arg1(_b)                 IPAQ_ASIC3_SDIO( _b, u16, Arg1 )   /* */
#define IPAQ_ASIC3_SDIO_CTRL_TransferBlockCount(_b)   IPAQ_ASIC3_SDIO( _b, u16, TransferBlockCount )   /* */
#define IPAQ_ASIC3_SDIO_CTRL_Response0(_b)            IPAQ_ASIC3_SDIO( _b, u16, Response0 )   /* */
#define IPAQ_ASIC3_SDIO_CTRL_Response1(_b)            IPAQ_ASIC3_SDIO( _b, u16, Response1 )   /* */
#define IPAQ_ASIC3_SDIO_CTRL_Response2(_b)            IPAQ_ASIC3_SDIO( _b, u16, Response2 )   /* */
#define IPAQ_ASIC3_SDIO_CTRL_Response3(_b)            IPAQ_ASIC3_SDIO( _b, u16, Response3 )   /* */
#define IPAQ_ASIC3_SDIO_CTRL_Response4(_b)            IPAQ_ASIC3_SDIO( _b, u16, Response4 )   /* */
#define IPAQ_ASIC3_SDIO_CTRL_Response5(_b)            IPAQ_ASIC3_SDIO( _b, u16, Response5 )   /* */
#define IPAQ_ASIC3_SDIO_CTRL_Response6(_b)            IPAQ_ASIC3_SDIO( _b, u16, Response6 )   /* */
#define IPAQ_ASIC3_SDIO_CTRL_Response7(_b)            IPAQ_ASIC3_SDIO( _b, u16, Response7 )   /* */
#define IPAQ_ASIC3_SDIO_CTRL_CardStatus(_b)           IPAQ_ASIC3_SDIO( _b, u16, CardStatus )   /* */
#define IPAQ_ASIC3_SDIO_CTRL_BufferCtrl(_b)           IPAQ_ASIC3_SDIO( _b, u16, BufferCtrl )   /* and error status*/
#define IPAQ_ASIC3_SDIO_CTRL_IntMaskCard(_b)          IPAQ_ASIC3_SDIO( _b, u16, IntMaskCard )   /* */
#define IPAQ_ASIC3_SDIO_CTRL_IntMaskBuffer(_b)        IPAQ_ASIC3_SDIO( _b, u16, IntMaskBuffer )   /* */
#define IPAQ_ASIC3_SDIO_CTRL_CardXferDataLen(_b)      IPAQ_ASIC3_SDIO( _b, u16, CardXferDataLen )   /* */
#define IPAQ_ASIC3_SDIO_CTRL_CardOptionSetup(_b)      IPAQ_ASIC3_SDIO( _b, u16, CardOptionSetup )   /* */
#define IPAQ_ASIC3_SDIO_CTRL_ErrorStatus0(_b)         IPAQ_ASIC3_SDIO( _b, u16, ErrorStatus0 )   /* */
#define IPAQ_ASIC3_SDIO_CTRL_ErrorStatus1(_b)         IPAQ_ASIC3_SDIO( _b, u16, ErrorStatus1 )   /* */
#define IPAQ_ASIC3_SDIO_CTRL_DataPort(_b)             IPAQ_ASIC3_SDIO( _b, u16, DataPort )   /* */
#define IPAQ_ASIC3_SDIO_CTRL_TransactionCtrl(_b)      IPAQ_ASIC3_SDIO( _b, u16, TransactionCtrl )   /* */
#define IPAQ_ASIC3_SDIO_CTRL_CardIntCtrl(_b)          IPAQ_ASIC3_SDIO( _b, u16, CardIntCtrl )   /* */
#define IPAQ_ASIC3_SDIO_CTRL_ClocknWaitCtrl(_b)       IPAQ_ASIC3_SDIO( _b, u16, ClocknWaitCtrl )   /* */
#define IPAQ_ASIC3_SDIO_CTRL_HostInformation(_b)      IPAQ_ASIC3_SDIO( _b, u16, HostInformation )   /* */
#define IPAQ_ASIC3_SDIO_CTRL_ErrorCtrl(_b)            IPAQ_ASIC3_SDIO( _b, u16, ErrorCtrl )   /* */
#define IPAQ_ASIC3_SDIO_CTRL_LEDCtrl(_b)              IPAQ_ASIC3_SDIO( _b, u16, LEDCtrl )   /* */
#define IPAQ_ASIC3_SDIO_CTRL_SoftwareReset(_b)        IPAQ_ASIC3_SDIO( _b, u16, SoftwareReset )   /* */

#define IPAQ_ASIC3_MAP_SIZE	     0x2000

#endif

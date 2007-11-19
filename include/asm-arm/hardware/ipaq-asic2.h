/*
 *
 * Definitions for IPAQ Handheld Computer
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

#ifndef IPAQ_ASIC2_H
#define IPAQ_ASIC2_H

/********************* IPAQ, ASIC #2 ********************/

#define IPAQ_ASIC2_OFFSET(x,y)					\
	(_IPAQ_ASIC2_## x ## _Base + _IPAQ_ASIC2_ ## x ## _ ## y)
#define IPAQ_ASIC2_NOFFSET(x,n,y)					\
	(_IPAQ_ASIC2_## x ## _ ## n ## _Base + _IPAQ_ASIC2_ ## x ## _ ## y)
#define IPAQ_ASIC2_ARRAY(_base,x,y,z)					\
	(_IPAQ_ASIC2_ ## x ## _0_Base + y * (_IPAQ_ASIC2_ ## x ## _1_Base - _IPAQ_ASIC2_ ## x ## _0_Base) \
	 + _IPAQ_ASIC2_ ## x ## _ ## z))

#define _IPAQ_ASIC2_GPIO_Base                 0x0000
#define _IPAQ_ASIC2_GPIO_Direction            0x0000    /* R/W, 16 bits 1:input, 0:output */
#define _IPAQ_ASIC2_GPIO_InterruptType        0x0004    /* R/W, 12 bits 1:edge, 0:level          */
#define _IPAQ_ASIC2_GPIO_InterruptEdgeType    0x0008    /* R/W, 12 bits 1:rising, 0:falling */
#define _IPAQ_ASIC2_GPIO_InterruptLevelType   0x000C    /* R/W, 12 bits 1:high, 0:low  */
#define _IPAQ_ASIC2_GPIO_InterruptClear       0x0010    /* W,   12 bits */
#define _IPAQ_ASIC2_GPIO_InterruptFlag        0x0010    /* R,   12 bits - reads int status */
#define _IPAQ_ASIC2_GPIO_Data                 0x0014    /* R/W, 16 bits */
#define _IPAQ_ASIC2_GPIO_BattFaultOut         0x0018    /* R/W, 16 bit - sets level on batt fault */
#define _IPAQ_ASIC2_GPIO_InterruptEnable      0x001c    /* R/W, 12 bits 1:enable interrupt */
#define _IPAQ_ASIC2_GPIO_Alternate            0x003c    /* R/W, 12+1 bits - set alternate functions */

#define IPAQ_ASIC2_GPIODIR       IPAQ_ASIC2_OFFSET(GPIO, Direction)
#define IPAQ_ASIC2_GPIINTTYPE    IPAQ_ASIC2_OFFSET(GPIO, InterruptType)
#define IPAQ_ASIC2_GPIINTESEL    IPAQ_ASIC2_OFFSET(GPIO, InterruptEdgeType)
#define IPAQ_ASIC2_GPIINTALSEL   IPAQ_ASIC2_OFFSET(GPIO, InterruptLevelType)
#define IPAQ_ASIC2_GPIINTCLR     IPAQ_ASIC2_OFFSET(GPIO, InterruptClear)
#define IPAQ_ASIC2_GPIINTFLAG    IPAQ_ASIC2_OFFSET(GPIO, InterruptFlag)
#define IPAQ_ASIC2_GPIOPIOD      IPAQ_ASIC2_OFFSET(GPIO, Data)
#define IPAQ_ASIC2_GPOBFSTAT     IPAQ_ASIC2_OFFSET(GPIO, BattFaultOut)
#define IPAQ_ASIC2_GPIINTSTAT    IPAQ_ASIC2_OFFSET(GPIO, InterruptEnable)
#define IPAQ_ASIC2_GPIOALT       IPAQ_ASIC2_OFFSET(GPIO, Alternate)

#define GPIO2_IN_Y1_N                       (1 << 0)   /* Output: Touchscreen Y1 */
#define GPIO2_IN_X0                         (1 << 1)   /* Output: Touchscreen X0 */
#define GPIO2_IN_Y0                         (1 << 2)   /* Output: Touchscreen Y0 */
#define GPIO2_IN_X1_N                       (1 << 3)   /* Output: Touchscreen X1 */
#define GPIO2_BT_RST                        (1 << 4)   /* Output: Bluetooth reset */
#define GPIO2_PEN_IRQ                       (1 << 5)   /* Input : Pen down        */
#define GPIO2_SD_DETECT                     (1 << 6)   /* Input : SD detect */
#define GPIO2_EAR_IN_N                      (1 << 7)   /* Input : Audio jack plug inserted */
#define GPIO2_OPT_PCM_RESET                 (1 << 8)   /* Output: Card reset (pin 2 on expansion) */
#define GPIO2_OPT_RESET                     (1 << 9)   /* Output: Option pack reset (pin 8 on expansion) */
#define GPIO2_USB_DETECT_N                  (1 << 10)  /* Input : */
#define GPIO2_SD_CON_SLT                    (1 << 11)  /* Input : */
#define GPIO2_OPT_ON                        (1 << 12)  /* Output: Option jacket power */
#define GPIO2_OPT_ON_NVRAM                  (1 << 13)  /* Output: Option jacket NVRAM power (multiplexed with PWM1 */
#define GPIO2_UART0_RXD_ENABLE              (1 << 14)  /* Output: When low, UART0 RXD -> UART0 DCD */
#define GPIO2_UART0_TXD_ENABLE              (1 << 15)  /* Output: When low, host awaken BT and reset BT */

#define _IPAQ_ASIC2_KPIO_Base                 0x0200
#define _IPAQ_ASIC2_KPIO_Direction            0x0000    /* R/W, 12 bits 1:input, 0:output */
#define _IPAQ_ASIC2_KPIO_InterruptType        0x0004    /* R/W, 12 bits 1:edge, 0:level          */
#define _IPAQ_ASIC2_KPIO_InterruptEdgeType    0x0008    /* R/W, 12 bits 1:rising, 0:falling */
#define _IPAQ_ASIC2_KPIO_InterruptLevelType   0x000C    /* R/W, 12 bits 1:high, 0:low  */
#define _IPAQ_ASIC2_KPIO_InterruptClear       0x0010    /* W,   20 bits - 8 special */
#define _IPAQ_ASIC2_KPIO_InterruptFlag        0x0010    /* R,   20 bits - 8 special - reads int status */
#define _IPAQ_ASIC2_KPIO_Data                 0x0014    /* R/W, 16 bits */
#define _IPAQ_ASIC2_KPIO_BattFaultOut         0x0018    /* R/W, 16 bit - sets level on batt fault */
#define _IPAQ_ASIC2_KPIO_InterruptEnable      0x001c    /* R/W, 20 bits - 8 special (DON'T TRY TO READ!) */
#define _IPAQ_ASIC2_KPIO_Alternate            0x003c    /* R/W, 6 bits */

#define IPAQ_ASIC2_KPIODIR       IPAQ_ASIC2_OFFSET(KPIO, Direction)
#define IPAQ_ASIC2_KPIINTTYPE    IPAQ_ASIC2_OFFSET(KPIO, InterruptType)
#define IPAQ_ASIC2_KPIINTESEL    IPAQ_ASIC2_OFFSET(KPIO, InterruptEdgeType)
#define IPAQ_ASIC2_KPIINTALSEL   IPAQ_ASIC2_OFFSET(KPIO, InterruptLevelType)
#define IPAQ_ASIC2_KPIINTCLR     IPAQ_ASIC2_OFFSET(KPIO, InterruptClear)
#define IPAQ_ASIC2_KPIINTFLAG    IPAQ_ASIC2_OFFSET(KPIO, InterruptFlag)
#define IPAQ_ASIC2_KPIOPIOD      IPAQ_ASIC2_OFFSET(KPIO, Data)
#define IPAQ_ASIC2_KPOBFSTAT     IPAQ_ASIC2_OFFSET(KPIO, BattFaultOut)
#define IPAQ_ASIC2_KPIINTSTAT    IPAQ_ASIC2_OFFSET(KPIO, InterruptEnable)
#define IPAQ_ASIC2_KPIOALT       IPAQ_ASIC2_OFFSET(KPIO, Alternate)

#define KPIO_UART_MAGIC       (1 << 14)
#define KPIO_SPI_INT          (1 << 16)
#define KPIO_OWM_INT          (1 << 17)
#define KPIO_ADC_INT          (1 << 18)
#define KPIO_UART_0_INT       (1 << 19)
#define KPIO_UART_1_INT       (1 << 20)
#define KPIO_TIMER_0_INT      (1 << 21)
#define KPIO_TIMER_1_INT      (1 << 22)
#define KPIO_TIMER_2_INT      (1 << 23)

#define KPIO_RECORD_BTN_N     (1 << 0)   /* Record button */
#define KPIO_KEY_5W1_N        (1 << 1)   /* Keypad */
#define KPIO_KEY_5W2_N        (1 << 2)   /* */
#define KPIO_KEY_5W3_N        (1 << 3)   /* */
#define KPIO_KEY_5W4_N        (1 << 4)   /* */
#define KPIO_KEY_5W5_N        (1 << 5)   /* */
#define KPIO_KEY_LEFT_N       (1 << 6)   /* */
#define KPIO_KEY_RIGHT_N      (1 << 7)   /* */
#define KPIO_KEY_AP1_N        (1 << 8)   /* Old "Calendar" */
#define KPIO_KEY_AP2_N        (1 << 9)   /* Old "Schedule" */
#define KPIO_KEY_AP3_N        (1 << 10)  /* Old "Q"        */
#define KPIO_KEY_AP4_N        (1 << 11)  /* Old "Undo"     */
#define KPIO_KEY_ALL          0x0fff

/* Alternate KPIO functions (set by default) */
#define KPIO_ALT_KEY_5W1_N        (1 << 1)   /* Action key */
#define KPIO_ALT_KEY_5W2_N        (1 << 2)   /* J1 of keypad input */
#define KPIO_ALT_KEY_5W3_N        (1 << 3)   /* J2 of keypad input */
#define KPIO_ALT_KEY_5W4_N        (1 << 4)   /* J3 of keypad input */
#define KPIO_ALT_KEY_5W5_N        (1 << 5)   /* J4 of keypad input */
#define KPIO_ALT_KEY_ALL           0x003e

#define _IPAQ_ASIC2_SPI_Base                  0x0400
#define _IPAQ_ASIC2_SPI_Control               0x0000    /* R/W 8 bits */
#define _IPAQ_ASIC2_SPI_Data                  0x0004    /* R/W 8 bits */
#define _IPAQ_ASIC2_SPI_ChipSelectDisabled    0x0008    /* W   8 bits */

#define IPAQ_ASIC2_SPI_Control             IPAQ_ASIC2_OFFSET(SPI, Control)
#define IPAQ_ASIC2_SPI_Data                IPAQ_ASIC2_OFFSET(SPI, Data)
#define IPAQ_ASIC2_SPI_ChipSelectDisabled  IPAQ_ASIC2_OFFSET(SPI, ChipSelectDisabled)

#define SPI_CONTROL_SPR(clk)      ((clk) & 0x0f)  /* Clock rate: valid from 0000 (8kHz) to 1000 (2.048 MHz) */
#define SPI_CONTROL_SPE           (1 << 4)   /* SPI Enable (1:enable, 0:disable) */
#define SPI_CONTROL_SPIE          (1 << 5)   /* SPI Interrupt enable (1:enable, 0:disable) */
#define SPI_CONTROL_SEL           (1 << 6)   /* Chip select: 1:SPI_CS1 enable, 0:SPI_CS0 enable */
#define SPI_CONTROL_SEL_CS0       (0 << 6)   /* Set CS0 low */
#define SPI_CONTROL_SEL_CS1       (1 << 6)   /* Set CS0 high */
#define SPI_CONTROL_CPOL          (1 << 7)   /* Clock polarity, 1:SCK high when idle */

#define _IPAQ_ASIC2_PWM_0_Base                0x0600
#define _IPAQ_ASIC2_PWM_1_Base                0x0700
#define _IPAQ_ASIC2_PWM_TimeBase              0x0000    /* R/W 6 bits */
#define _IPAQ_ASIC2_PWM_PeriodTime            0x0004    /* R/W 12 bits */
#define _IPAQ_ASIC2_PWM_DutyTime              0x0008    /* R/W 12 bits */

/* Note: PWM 0 is connected to the frontlight */
#define IPAQ_ASIC2_PWM_0_TimeBase          IPAQ_ASIC2_NOFFSET(PWM, 0, TimeBase)
#define IPAQ_ASIC2_PWM_0_PeriodTime        IPAQ_ASIC2_NOFFSET(PWM, 0, PeriodTime)
#define IPAQ_ASIC2_PWM_0_DutyTime          IPAQ_ASIC2_NOFFSET(PWM, 0, DutyTime)

/* Note: PWM 1 is not used - the output pin is multiplexed with GPIO 13 (turns on/off option jacket) */
#define IPAQ_ASIC2_PWM_1_TimeBase          IPAQ_ASIC2_NOFFSET(PWM, 1, TimeBase)
#define IPAQ_ASIC2_PWM_1_PeriodTime        IPAQ_ASIC2_NOFFSET(PWM, 1, PeriodTime)
#define IPAQ_ASIC2_PWM_1_DutyTime          IPAQ_ASIC2_NOFFSET(PWM, 1, DutyTime)

#define PWM_TIMEBASE_VALUE(x)    ((x)&0xf)   /* Low 4 bits sets time base, max = 8 */
#define PWM_TIMEBASE_ENABLE     (1 << 4)   /* Enable clock */
#define PWM_TIMEBASE_CLEAR      (1 << 5)   /* Clear the PWM */

#define _IPAQ_ASIC2_LED_0_Base                0x0800
#define _IPAQ_ASIC2_LED_1_Base                0x0880
#define _IPAQ_ASIC2_LED_2_Base                0x0900
#define _IPAQ_ASIC2_LED_TimeBase              0x0000    /* R/W  7 bits */
#define _IPAQ_ASIC2_LED_PeriodTime            0x0004    /* R/W 12 bits */
#define _IPAQ_ASIC2_LED_DutyTime              0x0008    /* R/W 12 bits */
#define _IPAQ_ASIC2_LED_AutoStopCount         0x000c    /* R/W 16 bits --- Doesn't seem to be changeable */

#define IPAQ_ASIC2_LED_0_TimeBase          IPAQ_ASIC2_NOFFSET(LED, 0, TimeBase)
#define IPAQ_ASIC2_LED_0_PeriodTime        IPAQ_ASIC2_NOFFSET(LED, 0, PeriodTime)
#define IPAQ_ASIC2_LED_0_DutyTime          IPAQ_ASIC2_NOFFSET(LED, 0, DutyTime)
#define IPAQ_ASIC2_LED_0_AutoStopCount     IPAQ_ASIC2_NOFFSET(LED, 0, AutoStopCount)

#define IPAQ_ASIC2_LED_1_TimeBase          IPAQ_ASIC2_NOFFSET(LED, 1, TimeBase)
#define IPAQ_ASIC2_LED_1_PeriodTime        IPAQ_ASIC2_NOFFSET(LED, 1, PeriodTime)
#define IPAQ_ASIC2_LED_1_DutyTime          IPAQ_ASIC2_NOFFSET(LED, 1, DutyTime)
#define IPAQ_ASIC2_LED_1_AutoStopCount     IPAQ_ASIC2_NOFFSET(LED, 1, AutoStopCount)

#define IPAQ_ASIC2_LED_2_TimeBase          IPAQ_ASIC2_NOFFSET(LED, 2, TimeBase)
#define IPAQ_ASIC2_LED_2_PeriodTime        IPAQ_ASIC2_NOFFSET(LED, 2, PeriodTime)
#define IPAQ_ASIC2_LED_2_DutyTime          IPAQ_ASIC2_NOFFSET(LED, 2, DutyTime)
#define IPAQ_ASIC2_LED_2_AutoStopCount     IPAQ_ASIC2_NOFFSET(LED, 2, AutoStopCount)

#define IPAQ_ASIC2_LED_TimeBase(_b,x)          IPAQ_ASIC2_ARRAY(LED, x, TimeBase)
#define IPAQ_ASIC2_LED_PeriodTime(_b,x)        IPAQ_ASIC2_ARRAY(LED, x, PeriodTime)
#define IPAQ_ASIC2_LED_DutyTime(_b,x)          IPAQ_ASIC2_ARRAY(LED, x, DutyTime)
#define IPAQ_ASIC2_LED_AutoStopCount(_b,x)     IPAQ_ASIC2_ARRAY(LED, x, AutoStopCount)

#define LED_TBS		0x0f		/* Low 4 bits sets time base, max = 13		*/
					/* 0: maximum time base				*/
					/* 1: maximum time base / 2			*/
					/* n: maximum time base / 2^n			*/

#define LED_EN		(1 << 4)	/* LED ON/OFF 0:off, 1:on			*/
#define LED_AUTOSTOP	(1 << 5)	/* LED ON/OFF auto stop set 0:disable, 1:enable */ 
#define LED_ALWAYS	(1 << 6)	/* LED Interrupt Mask 0:No mask, 1:mask		*/

/* Standard 16550A UART */
#define _IPAQ_ASIC2_UART_0_Base               0x0A00
#define _IPAQ_ASIC2_UART_1_Base               0x0C00
#define _IPAQ_ASIC2_UART_Receive              0x0000    /* R    8 bits (DLAB=0) */
#define _IPAQ_ASIC2_UART_Transmit             0x0000    /*   W  8 bits (DLAB=0) */
#define _IPAQ_ASIC2_UART_IntEnable            0x0004    /* R/W  8 bits (DLAB=0) */
#define _IPAQ_ASIC2_UART_IntIdentify          0x0008    /* R    8 bits */
#define _IPAQ_ASIC2_UART_FIFOControl          0x0008    /*   W  8 bits */
#define _IPAQ_ASIC2_UART_LineControl          0x000c    /* R/W  8 bits */
#define _IPAQ_ASIC2_UART_ModemControl         0x0010    /* R/W  8 bits */
#define _IPAQ_ASIC2_UART_LineStatus           0x0014    /* R/W  8 bits */
#define _IPAQ_ASIC2_UART_ModemStatus          0x0018    /* R/W  8 bits */
#define _IPAQ_ASIC2_UART_ScratchPad           0x001c    /* R/W  8 bits */
#define _IPAQ_ASIC2_UART_Reset                0x0020    /*   W  1 bit  */
#define _IPAQ_ASIC2_UART_DivisorLatchL        0x0000    /* R/W  8 bits (DLAB=1) */
#define _IPAQ_ASIC2_UART_DivisorLatchH        0x0004    /* R/W  8 bits (DLAB=1) */

#define IPAQ_ASIC2_UART_0_RCVR       IPAQ_ASIC2_NOFFSET(UART, 0, Receive)
#define IPAQ_ASIC2_UART_0_XMIT       IPAQ_ASIC2_NOFFSET(UART, 0, Transmit)
#define IPAQ_ASIC2_UART_0_IER        IPAQ_ASIC2_NOFFSET(UART, 0, IntEnable)
#define IPAQ_ASIC2_UART_0_IIR        IPAQ_ASIC2_NOFFSET(UART, 0, IntIdentify)
#define IPAQ_ASIC2_UART_0_FCR        IPAQ_ASIC2_NOFFSET(UART, 0, FIFOControl)
#define IPAQ_ASIC2_UART_0_LCR        IPAQ_ASIC2_NOFFSET(UART, 0, LineControl)
#define IPAQ_ASIC2_UART_0_MCR        IPAQ_ASIC2_NOFFSET(UART, 0, ModemControl)
#define IPAQ_ASIC2_UART_0_LSR        IPAQ_ASIC2_NOFFSET(UART, 0, LineStatus)
#define IPAQ_ASIC2_UART_0_MSR        IPAQ_ASIC2_NOFFSET(UART, 0, ModemStatus)
#define IPAQ_ASIC2_UART_0_SCR        IPAQ_ASIC2_NOFFSET(UART, 0, ScratchPad)
#define IPAQ_ASIC2_UART_0_RSR        IPAQ_ASIC2_NOFFSET(UART, 0, Reset)
#define IPAQ_ASIC2_UART_0_DLL        IPAQ_ASIC2_NOFFSET(UART, 0, DivisorLatchL)
#define IPAQ_ASIC2_UART_0_DLM        IPAQ_ASIC2_NOFFSET(UART, 0, DivisorLatchH)

#define IPAQ_ASIC2_UART_1_RCVR       IPAQ_ASIC2_NOFFSET(UART, 1, Receive)
#define IPAQ_ASIC2_UART_1_XMIT       IPAQ_ASIC2_NOFFSET(UART, 1, Transmit)
#define IPAQ_ASIC2_UART_1_IER        IPAQ_ASIC2_NOFFSET(UART, 1, IntEnable)
#define IPAQ_ASIC2_UART_1_IIR        IPAQ_ASIC2_NOFFSET(UART, 1, IntIdentify)
#define IPAQ_ASIC2_UART_1_FCR        IPAQ_ASIC2_NOFFSET(UART, 1, FIFOControl)
#define IPAQ_ASIC2_UART_1_LCR        IPAQ_ASIC2_NOFFSET(UART, 1, LineControl)
#define IPAQ_ASIC2_UART_1_MCR        IPAQ_ASIC2_NOFFSET(UART, 1, ModemControl)
#define IPAQ_ASIC2_UART_1_LSR        IPAQ_ASIC2_NOFFSET(UART, 1, LineStatus)
#define IPAQ_ASIC2_UART_1_MSR        IPAQ_ASIC2_NOFFSET(UART, 1, ModemStatus)
#define IPAQ_ASIC2_UART_1_SCR        IPAQ_ASIC2_NOFFSET(UART, 1, ScratchPad)
#define IPAQ_ASIC2_UART_1_RSR        IPAQ_ASIC2_NOFFSET(UART, 1, Reset)
#define IPAQ_ASIC2_UART_1_DLL        IPAQ_ASIC2_NOFFSET(UART, 1, DivisorLatchL)
#define IPAQ_ASIC2_UART_1_DLM        IPAQ_ASIC2_NOFFSET(UART, 1, DivisorLatchH)

#define _IPAQ_ASIC2_TIMER_Base                0x0E00    /* 8254-compatible timers */
#define _IPAQ_ASIC2_TIMER_Counter0            0x0000    /* R/W  8 bits */
#define _IPAQ_ASIC2_TIMER_Counter1            0x0004    /* R/W  8 bits */
#define _IPAQ_ASIC2_TIMER_Counter2            0x0008    /* R/W  8 bits */
#define _IPAQ_ASIC2_TIMER_Control             0x000a    /*   W  8 bits */
#define _IPAQ_ASIC2_TIMER_Command             0x0010    /* R/W  8 bits */

#define IPAQ_ASIC2_TIMER_Counter0          IPAQ_ASIC2_OFFSET(TIMER, Counter0)
#define IPAQ_ASIC2_TIMER_Counter1          IPAQ_ASIC2_OFFSET(TIMER, Counter1)
#define IPAQ_ASIC2_TIMER_Counter2          IPAQ_ASIC2_OFFSET(TIMER, Counter2)
#define IPAQ_ASIC2_TIMER_Control           IPAQ_ASIC2_OFFSET(TIMER, Control)
#define IPAQ_ASIC2_TIMER_Command           IPAQ_ASIC2_OFFSET(TIMER, Command)

/* These defines are likely incorrect - in particular, TIMER_CNTL_MODE_4 might
   need to be 0x04 */
#define TIMER_CNTL_SELECT(x)               (((x)&0x3)<<6)   /* Select counter */
#define TIMER_CNTL_RW(x)                   (((x)&0x3)<<4)   /* Read/write mode */
#define TIMER_CNTL_RW_LATCH                TIMER_CNTL_RW(0)
#define TIMER_CNTL_RW_LSB_MSB              TIMER_CNTL_RW(3) /* LSB first, then MSB */
#define TIMER_CNTL_MODE(x)                 (((x)&0x7)<<1)   /* Mode */
#define TIMER_CNTL_MODE_0                  TIMER_CNTL_MODE(0)  /* Supported for 0 & 1 */
#define TIMER_CNTL_MODE_2                  TIMER_CNTL_MODE(2)  /* Supported for all timers */
#define TIMER_CNTL_MODE_4                  TIMER_CNTL_MODE(4)  /* Supported for all timers */
#define TIMER_CNTL_BCD                     (1 << 0)       /* 1=Use BCD counter, 4 decades */

#define TIMER_CMD_GAT_0                    (1 << 0)    /* Gate enable, counter 0 */
#define TIMER_CMD_GAT_1                    (1 << 1)    /* Gate enable, counter 1 */
#define TIMER_CMD_GAT_2                    (1 << 2)    /* Gate enable, counter 2 */
#define TIMER_CMD_CLK_0                    (1 << 3)    /* Clock enable, counter 0 */
#define TIMER_CMD_CLK_1                    (1 << 4)    /* Clock enable, counter 1 */
#define TIMER_CMD_CLK_2                    (1 << 5)    /* Clock enable, counter 2 */
#define TIMER_CMD_MODE_0                   (1 << 6)    /* Mode 0 enable, counter 0 */
#define TIMER_CMD_MODE_1                   (1 << 7)    /* Mode 0 enable, counter 1 */

#define _IPAQ_ASIC2_CLOCK_Base                0x1000
#define _IPAQ_ASIC2_CLOCK_Enable              0x0000    /* R/W  18 bits */

#define IPAQ_ASIC2_CLOCK_Enable            IPAQ_ASIC2_OFFSET(CLOCK, Enable)

#define ASIC2_CLOCK_AUDIO_1                0x01    /* Enable 4.1 MHz clock for 8Khz and 4khz sample rate */
#define ASIC2_CLOCK_AUDIO_2                0x02    /* Enable 12.3 MHz clock for 48Khz and 32khz sample rate */
#define ASIC2_CLOCK_AUDIO_3                0x04    /* Enable 5.6 MHz clock for 11 kHZ sample rate */
#define ASIC2_CLOCK_AUDIO_4                0x08    /* Enable 11.289 MHz clock for 44 and 22 kHz sample rate */
#define ASIC2_CLOCK_AUDIO_MASK             0x0f    /* Bottom four bits are for audio */
#define ASIC2_CLOCK_ADC              (1 << 4)    /* 1.024 MHz clock to ADC (CX4) */
#define ASIC2_CLOCK_SPI              (1 << 5)    /* 4.096 MHz clock to SPI (CX5) */
#define ASIC2_CLOCK_OWM              (1 << 6)    /* 4.096 MHz clock to OWM (CX6) */
#define ASIC2_CLOCK_PWM              (1 << 7)    /* 2.048 MHz clock to PWM (CX7) */
#define ASIC2_CLOCK_UART_1           (1 << 8)    /* 24.576 MHz clock to UART1 (CX8 - turn off CX14) */
#define ASIC2_CLOCK_UART_0           (1 << 9)    /* 24.576 MHz clock to UART0 (CX9 - turn off CX13) */
#define ASIC2_CLOCK_SD_1             (1 << 10)   /* 16.934 MHz to SD */
#define ASIC2_CLOCK_SD_2             (2 << 10)   /* 24.576 MHz to SD */
#define ASIC2_CLOCK_SD_3             (3 << 10)   /* 33.869 MHz to SD */
#define ASIC2_CLOCK_SD_4             (4 << 10)   /* 49.152 MHz to SD */
#define ASIC2_CLOCK_SD_MASK              0x1c00    /* Bits 10 through 12 are for SD */
#define ASIC2_CLOCK_EX0              (1 << 13)   /* Enable 32.768 kHz (LED,Timer,Interrupt) */
#define ASIC2_CLOCK_EX1              (1 << 14)   /* Enable 24.576 MHz (ADC,PCM,SPI,PWM,UART,SD,Audio,BT) */
#define ASIC2_CLOCK_EX2              (1 << 15)   /* Enable 33.869 MHz (SD,Audio) */
#define ASIC2_CLOCK_SLOW_UART_0      (1 << 16)   /* Enable 3.686 MHz to UART0 (CX13 - turn off CX9) */
#define ASIC2_CLOCK_SLOW_UART_1      (1 << 17)   /* Enable 3.686 MHz to UART1 (CX14 - turn off CX8) */
#define ASIC2_CLOCK_UART_0_MASK      (ASIC2_CLOCK_UART_0 | ASIC2_CLOCK_SLOW_UART_0)
#define ASIC2_CLOCK_UART_1_MASK      (ASIC2_CLOCK_UART_1 | ASIC2_CLOCK_SLOW_UART_1)

#define _IPAQ_ASIC2_ADC_Base                  0x1200
#define _IPAQ_ASIC2_ADC_Multiplexer           0x0000    /* R/W 4 bits - low 3 bits set channel */
#define _IPAQ_ASIC2_ADC_ControlStatus         0x0004    /* R/W 8 bits */
#define _IPAQ_ASIC2_ADC_Data                  0x0008    /* R   10 bits */

#define IPAQ_ASIC2_ADMUX        IPAQ_ASIC2_OFFSET(ADC, Multiplexer)
#define IPAQ_ASIC2_ADCSR        IPAQ_ASIC2_OFFSET(ADC, ControlStatus)
#define IPAQ_ASIC2_ADCDR        IPAQ_ASIC2_OFFSET(ADC, Data)

#define ASIC2_ADMUX(x)                     ((x)&0x07)    /* Low 3 bits sets channel.  max = 4 */
#define ASIC2_ADMUX_MASK                         0x07
#define ASIC2_ADMUX_0_LIGHTSENSOR      ASIC2_ADMUX(0)   
#define ASIC2_ADMUX_1_IMIN             ASIC2_ADMUX(1)   
#define ASIC2_ADMUX_2_VS_MBAT          ASIC2_ADMUX(2)   
#define ASIC2_ADMUX_3_TP_X0            ASIC2_ADMUX(3)    /* Touchpanel X0 */
#define ASIC2_ADMUX_4_TP_Y1            ASIC2_ADMUX(4)    /* Touchpanel Y1 */
#define ASIC2_ADMUX_CLKEN                  (1 << 3)    /* Enable clock */
 
#define ASIC2_ADCSR_ADPS(x)                ((x)&0x0f)    /* Low 4 bits sets prescale, max = 8 */
#define ASIC2_ADCSR_FREE_RUN               (1 << 4)
#define ASIC2_ADCSR_INT_ENABLE             (1 << 5)
#define ASIC2_ADCSR_START                  (1 << 6)    /* Set to start conversion.  Goes to 0 when done */
#define ASIC2_ADCSR_ENABLE                 (1 << 7)    /* 1:power up ADC, 0:power down */


#define _IPAQ_ASIC2_INTR_Base                 0x1600
#define _IPAQ_ASIC2_INTR_MaskAndFlag          0x0000    /* R/(W) 8bits */
#define _IPAQ_ASIC2_INTR_ClockPrescale        0x0004    /* R/(W) 5bits */
#define _IPAQ_ASIC2_INTR_TimerSet             0x0008    /* R/(W) 8bits */

#define IPAQ_ASIC2_INTR_MaskAndFlag      IPAQ_ASIC2_OFFSET(INTR, MaskAndFlag)
#define IPAQ_ASIC2_INTR_ClockPrescale    IPAQ_ASIC2_OFFSET(INTR, ClockPrescale)
#define IPAQ_ASIC2_INTR_TimerSet         IPAQ_ASIC2_OFFSET(INTR, TimerSet)

#define ASIC2_INTMASK_GLOBAL               (1 << 0)    /* Global interrupt mask */
//#define ASIC2_INTR_POWER_ON_RESET        (1 << 1)    /* 01: Power on reset (bits 1 & 2) */
//#define ASIC2_INTR_EXTERNAL_RESET        (2 << 1)    /* 10: External reset (bits 1 & 2) */
#define ASIC2_INTMASK_UART_0               (1 << 4)    /* 1: mask, 0: enable */
#define ASIC2_INTMASK_UART_1               (1 << 5)    /* 1: mask, 0: enable */
#define ASIC2_INTMASK_TIMER                (1 << 6)    /* 1: mask, 0: enable */
#define ASIC2_INTMASK_OWM                  (1 << 7)    /* 0: mask, 1: enable */

#define ASIC2_INTCPS_CPS(x)                ((x)&0x0f)    /* 4 bits, max 14 */
#define ASIC2_INTCPS_SET                   (1 << 4)    /* Time base enable */

#define _IPAQ_ASIC2_OWM_Base                  0x1800
#define _IPAQ_ASIC2_OWM_Command               0x0000    /* R/W 4 bits command register */
#define _IPAQ_ASIC2_OWM_Data                  0x0004    /* R/W 8 bits, transmit / receive buffer */
#define _IPAQ_ASIC2_OWM_Interrupt             0x0008    /* R/W Command register */
#define _IPAQ_ASIC2_OWM_InterruptEnable       0x000c    /* R/W Command register */
#define _IPAQ_ASIC2_OWM_ClockDivisor          0x0010    /* R/W 5 bits of divisor and pre-scale */

#define IPAQ_ASIC2_OWM_Command            IPAQ_ASIC2_OFFSET(OWM, Command)
#define IPAQ_ASIC2_OWM_Data               IPAQ_ASIC2_OFFSET(OWM, Data)
#define IPAQ_ASIC2_OWM_Interrupt          IPAQ_ASIC2_OFFSET(OWM, Interrupt)
#define IPAQ_ASIC2_OWM_InterruptEnable    IPAQ_ASIC2_OFFSET(OWM, InterruptEnable)
#define IPAQ_ASIC2_OWM_ClockDivisor       IPAQ_ASIC2_OFFSET(OWM, ClockDivisor)

#define OWM_CMD_ONE_WIRE_RESET (1 << 0)    /* Set to force reset on 1-wire bus */
#define OWM_CMD_SRA            (1 << 1)    /* Set to switch to Search ROM accelerator mode */
#define OWM_CMD_DQ_OUTPUT      (1 << 2)    /* Write only - forces bus low */
#define OWM_CMD_DQ_INPUT       (1 << 3)    /* Read only - reflects state of bus */

#define OWM_INT_PD             (1 << 0)    /* Presence detect */
#define OWM_INT_PDR            (1 << 1)    /* Presence detect result */
#define OWM_INT_TBE            (1 << 2)    /* Transmit buffer empty */
#define OWM_INT_TEMT           (1 << 3)    /* Transmit shift register empty */
#define OWM_INT_RBF            (1 << 4)    /* Receive buffer full */

#define OWM_INTEN_EPD          (1 << 0)    /* Enable presence detect interrupt */
#define OWM_INTEN_IAS          (1 << 1)    /* INTR active state */
#define OWM_INTEN_ETBE         (1 << 2)    /* Enable transmit buffer empty interrupt */
#define OWM_INTEN_ETMT         (1 << 3)    /* Enable transmit shift register empty interrupt */
#define OWM_INTEN_ERBF         (1 << 4)    /* Enable receive buffer full interrupt */

#define _IPAQ_ASIC2_FlashCtl_Base             0x1A00

/* Write enable for the flash */

#define _IPAQ_ASIC2_FlashWP_Base         0x1f00
#define _IPAQ_ASIC2_FlashWP_VPP_ON       0x00    /* R   1: write, 0: protect */
#define IPAQ_ASIC2_FlashWP_VPP_ON        IPAQ_ASIC2_OFFSET(FlashWP, VPP_ON)

/* H3800-specific IRQs (CONFIG_SA1100_H3800) */
#define IPAQ_ASIC2_KPIO_IRQ_START    0
#define IRQ_IPAQ_ASIC2_KEY           (0)
#define IRQ_IPAQ_ASIC2_SPI           (1)
#define IRQ_IPAQ_ASIC2_OWM           (2)
#define IRQ_IPAQ_ASIC2_ADC           (3)
#define IRQ_IPAQ_ASIC2_UART_0        (4)
#define IRQ_IPAQ_ASIC2_UART_1        (5)
#define IRQ_IPAQ_ASIC2_TIMER_0       (6)
#define IRQ_IPAQ_ASIC2_TIMER_1       (7)
#define IRQ_IPAQ_ASIC2_TIMER_2       (8)
#define IPAQ_ASIC2_KPIO_IRQ_COUNT    9

#define IPAQ_ASIC2_GPIO_IRQ_START    9
#define IRQ_IPAQ_ASIC2_PEN           (9)
#define IRQ_IPAQ_ASIC2_SD_DETECT     (10)
#define IRQ_IPAQ_ASIC2_EAR_IN        (11)
#define IRQ_IPAQ_ASIC2_USB_DETECT    (12)
#define IRQ_IPAQ_ASIC2_SD_CON_SLT    (13)
#define IPAQ_ASIC2_GPIO_IRQ_COUNT    5

#define IPAQ_ASIC2_MAP_SIZE	     0x2000

#endif

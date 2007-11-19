/*
 *
 * Definitions for H3600 Handheld Computer
 *
 * Copyright 2000 Compaq Computer Corporation.
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
 * Author: Jamey Hicks.
 *
 * History:
 *
 * 2001-10-??   Andrew Christian   Added support for iPAQ H3800
 * 2005-04-02   Holger Hans Peter Freyther migrate to own file for the 2.6 port
 *
 */

#ifndef _INCLUDE_H3800_H_
#define _INCLUDE_H3800_H_

#include <asm/arch/ipaqsa.h>

#define H3800_ASIC2_VIRT     0xf0000000 /* CS#5 + 0x01000000, same as EGPIO */
#define GPIO_H3800_ASIC         	GPIO_GPIO (1)
#define GPIO_H3800_AC_IN                GPIO_GPIO (12)
#define GPIO_H3800_COM_DSR              GPIO_GPIO (13)
#define GPIO_H3800_MMC_INT              GPIO_GPIO (18)
#define GPIO_H3800_NOPT_IND             GPIO_GPIO (20)   /* Almost exactly the same as GPIO_H3600_OPT_DET */
#define GPIO_H3800_OPT_BAT_FAULT        GPIO_GPIO (22)
#define GPIO_H3800_CLK_OUT              GPIO_GPIO (27)

#define IRQ_GPIO_H3800_ASIC             IRQ_GPIO1

/* UART #1 is used for GPIO on a 3800 (use the PPC registers) */
#define PPC_H3800_COM_DTR               PPC_TXD1         /* Control the DTR line to the serial port */
#define PPC_H3800_NSD_WP                PPC_RXD1         /* SD card write protect sense line */

#if 0
/****************************************************/
/* H3800, ASIC #1
 * This ASIC is accesed through ASIC #2, and
 * mapped into the 1c00 - 1f00 region
 */
#define H3800_ASIC1_OFFSET(s,x,y)   \
     (*((volatile s *) (_IPAQ_ASIC2_GPIO_Base + _H3800_ASIC1_ ## x ## _Base + (_H3800_ASIC1_ ## x ## _ ## y))))


/****************************************************/
#define IRQ_GPIO_H3800_AC_IN            IRQ_GPIO12
#define IRQ_GPIO_H3800_MMC_INT          IRQ_GPIO18
#define IRQ_GPIO_H3800_NOPT_IND         IRQ_GPIO20 /* almost same as OPT_DET */


#define _H3800_ASIC1_MMC_Base             0x1c00

#define _H3800_ASIC1_MMC_StartStopClock     0x00    /* R/W 8bit                                  */
#define _H3800_ASIC1_MMC_Status             0x04    /* R   See below, default 0x0040             */
#define _H3800_ASIC1_MMC_ClockRate          0x08    /* R/W 8bit, low 3 bits are clock divisor    */
#define _H3800_ASIC1_MMC_Revision           0x0c    /* R/W Write to this to reset the asic!      */
#define _H3800_ASIC1_MMC_SPIRegister        0x10    /* R/W 8bit, see below                       */
#define _H3800_ASIC1_MMC_CmdDataCont        0x14    /* R/W 8bit, write to start MMC adapter      */
#define _H3800_ASIC1_MMC_ResponseTimeout    0x18    /* R/W 8bit, clocks before response timeout  */
#define _H3800_ASIC1_MMC_ReadTimeout        0x1c    /* R/W 16bit, clocks before received data timeout */
#define _H3800_ASIC1_MMC_BlockLength        0x20    /* R/W 10bit */
#define _H3800_ASIC1_MMC_NumOfBlocks        0x24    /* R/W 16bit, in block mode, number of blocks  */
#define _H3800_ASIC1_MMC_InterruptMask      0x34    /* R/W 8bit */
#define _H3800_ASIC1_MMC_CommandNumber      0x38    /* R/W 6 bits */
#define _H3800_ASIC1_MMC_ArgumentH          0x3c    /* R/W 16 bits  */
#define _H3800_ASIC1_MMC_ArgumentL          0x40    /* R/W 16 bits */
#define _H3800_ASIC1_MMC_ResFifo            0x44    /* R   8 x 16 bits - contains response FIFO */
#define _H3800_ASIC1_MMC_DataBuffer         0x4c    /* R/W 16 bits?? */
#define _H3800_ASIC1_MMC_BufferPartFull     0x50    /* R/W 8 bits */

#define H3800_ASIC1_MMC_StartStopClock    H3800_ASIC1_OFFSET(  u8, MMC, StartStopClock )
#define H3800_ASIC1_MMC_Status            H3800_ASIC1_OFFSET( u16, MMC, Status )
#define H3800_ASIC1_MMC_ClockRate         H3800_ASIC1_OFFSET(  u8, MMC, ClockRate )
#define H3800_ASIC1_MMC_Revision          H3800_ASIC1_OFFSET( u16, MMC, Revision )
#define H3800_ASIC1_MMC_SPIRegister       H3800_ASIC1_OFFSET(  u8, MMC, SPIRegister )
#define H3800_ASIC1_MMC_CmdDataCont       H3800_ASIC1_OFFSET(  u8, MMC, CmdDataCont )
#define H3800_ASIC1_MMC_ResponseTimeout   H3800_ASIC1_OFFSET(  u8, MMC, ResponseTimeout )
#define H3800_ASIC1_MMC_ReadTimeout       H3800_ASIC1_OFFSET( u16, MMC, ReadTimeout )
#define H3800_ASIC1_MMC_BlockLength       H3800_ASIC1_OFFSET( u16, MMC, BlockLength )
#define H3800_ASIC1_MMC_NumOfBlocks       H3800_ASIC1_OFFSET( u16, MMC, NumOfBlocks )
#define H3800_ASIC1_MMC_InterruptMask     H3800_ASIC1_OFFSET(  u8, MMC, InterruptMask )
#define H3800_ASIC1_MMC_CommandNumber     H3800_ASIC1_OFFSET(  u8, MMC, CommandNumber )
#define H3800_ASIC1_MMC_ArgumentH         H3800_ASIC1_OFFSET( u16, MMC, ArgumentH )
#define H3800_ASIC1_MMC_ArgumentL         H3800_ASIC1_OFFSET( u16, MMC, ArgumentL )
#define H3800_ASIC1_MMC_ResFifo           H3800_ASIC1_OFFSET( u16, MMC, ResFifo )
#define H3800_ASIC1_MMC_DataBuffer        H3800_ASIC1_OFFSET( u16, MMC, DataBuffer )
#define H3800_ASIC1_MMC_BufferPartFull    H3800_ASIC1_OFFSET(  u8, MMC, BufferPartFull )

#define MMC_STOP_CLOCK                   (1 << 0)   /* Write to "StartStopClock" register */
#define MMC_START_CLOCK                  (1 << 1)

#define MMC_STATUS_READ_TIMEOUT          (1 << 0)
#define MMC_STATUS_RESPONSE_TIMEOUT      (1 << 1)
#define MMC_STATUS_CRC_WRITE_ERROR       (1 << 2)
#define MMC_STATUS_CRC_READ_ERROR        (1 << 3)
#define MMC_STATUS_SPI_READ_ERROR        (1 << 4)  /* SPI data token error received */
#define MMC_STATUS_CRC_RESPONSE_ERROR    (1 << 5)
#define MMC_STATUS_FIFO_EMPTY            (1 << 6)
#define MMC_STATUS_FIFO_FULL             (1 << 7)
#define MMC_STATUS_CLOCK_ENABLE          (1 << 8)  /* MultiMediaCard clock stopped */
#define MMC_STATUS_WR_CRC_ERROR_CODE     (3 << 9)  /* 2 bits wide */
#define MMC_STATUS_DATA_TRANSFER_DONE    (1 << 11) /* Write operation, indicates transfer finished */
#define MMC_STATUS_END_PROGRAM           (1 << 12) /* End write and read operations */
#define MMC_STATUS_END_COMMAND_RESPONSE  (1 << 13) /* End command response */

#define MMC_CLOCK_RATE_FULL              0
#define MMC_CLOCK_RATE_DIV_2             1
#define MMC_CLOCK_RATE_DIV_4             2
#define MMC_CLOCK_RATE_DIV_8             3
#define MMC_CLOCK_RATE_DIV_16            4
#define MMC_CLOCK_RATE_DIV_32            5
#define MMC_CLOCK_RATE_DIV_64            6

#define MMC_SPI_REG_SPI_ENABLE           (1 << 0)  /* Enables SPI mode */
#define MMC_SPI_REG_CRC_ON               (1 << 1)  /* 1:turn on CRC    */
#define MMC_SPI_REG_SPI_CS_ENABLE        (1 << 2)  /* 1:turn on SPI CS */
#define MMC_SPI_REG_CS_ADDRESS_MASK      0x38      /* Bits 3,4,5 are the SPI CS relative address */

#define MMC_CMD_DATA_CONT_FORMAT_NO_RESPONSE  0x00
#define MMC_CMD_DATA_CONT_FORMAT_R1           0x01
#define MMC_CMD_DATA_CONT_FORMAT_R2           0x02
#define MMC_CMD_DATA_CONT_FORMAT_R3           0x03
#define MMC_CMD_DATA_CONT_DATA_ENABLE         (1 << 2)  /* This command contains a data transfer */
#define MMC_CMD_DATA_CONT_READ                (0 << 3)  /* This data transfer is a read */
#define MMC_CMD_DATA_CONT_WRITE               (1 << 3)  /* This data transfer is a write */
#define MMC_CMD_DATA_CONT_STREAM_BLOCK        (1 << 4)  /* This data transfer is in stream mode */
#define MMC_CMD_DATA_CONT_MULTI_BLOCK         (1 << 5)
#define MMC_CMD_DATA_CONT_BC_MODE             (1 << 6)
#define MMC_CMD_DATA_CONT_BUSY_BIT            (1 << 7)  /* Busy signal expected after current cmd */
#define MMC_CMD_DATA_CONT_INITIALIZE          (1 << 8)  /* Enables the 80 bits for initializing card */
#define MMC_CMD_DATA_CONT_NO_COMMAND          (1 << 9)
#define MMC_CMD_DATA_CONT_WIDE_BUS            (1 << 10)
#define MMC_CMD_DATA_CONT_WE_WIDE_BUS         (1 << 11)
#define MMC_CMD_DATA_CONT_NOB_ON              (1 << 12)

#define MMC_INT_MASK_DATA_TRANSFER_DONE       (1 << 0)
#define MMC_INT_MASK_PROGRAM_DONE             (1 << 1)
#define MMC_INT_MASK_END_COMMAND_RESPONSE     (1 << 2)
#define MMC_INT_MASK_BUFFER_READY             (1 << 3)
#define MMC_INT_MASK_ALL                      (0xf)

#define MMC_BUFFER_PART_FULL                  (1 << 0)
#define MMC_BUFFER_LAST_BUF                   (1 << 1)

/********* GPIO **********/

#define _H3800_ASIC1_GPIO_Base        0x1e00

#define _H3800_ASIC1_GPIO_Mask          0x60    /* R/W 0:don't mask, 1:mask interrupt */
#define _H3800_ASIC1_GPIO_Direction     0x64    /* R/W 0:input, 1:output              */
#define _H3800_ASIC1_GPIO_Out           0x68    /* R/W 0:output low, 1:output high    */
#define _H3800_ASIC1_GPIO_TriggerType   0x6c    /* R/W 0:level, 1:edge                */
#define _H3800_ASIC1_GPIO_EdgeTrigger   0x70    /* R/W 0:falling, 1:rising            */
#define _H3800_ASIC1_GPIO_LevelTrigger  0x74    /* R/W 0:low, 1:high level detect     */
#define _H3800_ASIC1_GPIO_LevelStatus   0x78    /* R/W 0:none, 1:detect               */
#define _H3800_ASIC1_GPIO_EdgeStatus    0x7c    /* R/W 0:none, 1:detect               */
#define _H3800_ASIC1_GPIO_State         0x80    /* R   See masks below  (default 0)         */
#define _H3800_ASIC1_GPIO_Reset         0x84    /* R/W See masks below  (default 0x04)      */
#define _H3800_ASIC1_GPIO_SleepMask     0x88    /* R/W 0:don't mask, 1:mask trigger in sleep mode  */
#define _H3800_ASIC1_GPIO_SleepDir      0x8c    /* R/W direction 0:input, 1:ouput in sleep mode    */
#define _H3800_ASIC1_GPIO_SleepOut      0x90    /* R/W level 0:low, 1:high in sleep mode           */
#define _H3800_ASIC1_GPIO_Status        0x94    /* R   Pin status                                  */
#define _H3800_ASIC1_GPIO_BattFaultDir  0x98    /* R/W direction 0:input, 1:output in batt_fault   */
#define _H3800_ASIC1_GPIO_BattFaultOut  0x9c    /* R/W level 0:low, 1:high in batt_fault           */

#define H3800_ASIC1_GPIO_MASK            H3800_ASIC1_OFFSET( u16, GPIO, Mask )
#define H3800_ASIC1_GPIO_DIR             H3800_ASIC1_OFFSET( u16, GPIO, Direction )
#define H3800_ASIC1_GPIO_OUT             H3800_ASIC1_OFFSET( u16, GPIO, Out )
#define H3800_ASIC1_GPIO_LEVELTRI        H3800_ASIC1_OFFSET( u16, GPIO, TriggerType )
#define H3800_ASIC1_GPIO_RISING          H3800_ASIC1_OFFSET( u16, GPIO, EdgeTrigger )
#define H3800_ASIC1_GPIO_LEVEL           H3800_ASIC1_OFFSET( u16, GPIO, LevelTrigger )
#define H3800_ASIC1_GPIO_LEVEL_STATUS    H3800_ASIC1_OFFSET( u16, GPIO, LevelStatus )
#define H3800_ASIC1_GPIO_EDGE_STATUS     H3800_ASIC1_OFFSET( u16, GPIO, EdgeStatus )
#define H3800_ASIC1_GPIO_STATE           H3800_ASIC1_OFFSET(  u8, GPIO, State )
#define H3800_ASIC1_GPIO_RESET           H3800_ASIC1_OFFSET(  u8, GPIO, Reset )
#define H3800_ASIC1_GPIO_SLEEP_MASK      H3800_ASIC1_OFFSET( u16, GPIO, SleepMask )
#define H3800_ASIC1_GPIO_SLEEP_DIR       H3800_ASIC1_OFFSET( u16, GPIO, SleepDir )
#define H3800_ASIC1_GPIO_SLEEP_OUT       H3800_ASIC1_OFFSET( u16, GPIO, SleepOut )
#define H3800_ASIC1_GPIO_STATUS          H3800_ASIC1_OFFSET( u16, GPIO, Status )
#define H3800_ASIC1_GPIO_BATT_FAULT_DIR  H3800_ASIC1_OFFSET( u16, GPIO, BattFaultDir )
#define H3800_ASIC1_GPIO_BATT_FAULT_OUT  H3800_ASIC1_OFFSET( u16, GPIO, BattFaultOut )

#define H3800_ASIC1_GPIO_STATE_MASK            (1 << 0)
#define H3800_ASIC1_GPIO_STATE_DIRECTION       (1 << 1)
#define H3800_ASIC1_GPIO_STATE_OUT             (1 << 2)
#define H3800_ASIC1_GPIO_STATE_TRIGGER_TYPE    (1 << 3)
#define H3800_ASIC1_GPIO_STATE_EDGE_TRIGGER    (1 << 4)
#define H3800_ASIC1_GPIO_STATE_LEVEL_TRIGGER   (1 << 5)

#define H3800_ASIC1_GPIO_RESET_SOFTWARE        (1 << 0)
#define H3800_ASIC1_GPIO_RESET_AUTO_SLEEP      (1 << 1)
#define H3800_ASIC1_GPIO_RESET_FIRST_PWR_ON    (1 << 2)

/* These are all outputs */
#define GPIO1_IR_ON_N          (1 << 0)   /* Apply power to the IR Module */
#define GPIO1_SD_PWR_ON        (1 << 1)   /* Secure Digital power on */
#define GPIO1_RS232_ON         (1 << 2)   /* Turn on power to the RS232 chip ? */
#define GPIO1_PULSE_GEN        (1 << 3)   /* Goes to speaker / earphone */
#define GPIO1_CH_TIMER         (1 << 4)   /* Charger */
#define GPIO1_LCD_5V_ON        (1 << 5)   /* Enables LCD_5V */
#define GPIO1_LCD_ON           (1 << 6)   /* Enables LCD_3V */
#define GPIO1_LCD_PCI          (1 << 7)   /* Connects to PDWN on LCD controller */
#define GPIO1_VGH_ON           (1 << 8)   /* Drives VGH on the LCD (+9??) */
#define GPIO1_VGL_ON           (1 << 9)   /* Drivers VGL on the LCD (-6??) */
#define GPIO1_FL_PWR_ON        (1 << 10)  /* Frontlight power on */
#define GPIO1_BT_PWR_ON        (1 << 11)  /* Bluetooth power on */
#define GPIO1_SPK_ON           (1 << 12)  /* Built-in speaker on */
#define GPIO1_EAR_ON_N         (1 << 13)  /* Headphone jack on */
#define GPIO1_AUD_PWR_ON       (1 << 14)  /* All audio power */
#endif

#endif

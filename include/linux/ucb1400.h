/*
 * ucb1400_ts.h
 *
 * Copyright (C) 2007 Marek Vasut <marek.vasut@gmail.com>, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef UCB1400_TS_H
#define UCB1400_TS_H

/*
 * Interesting UCB1400 AC-link registers
 */
  
#define UCB_IO_DATA             0x5a
#define UCB_IO_DIR              0x5c
#define UCB_IE_RIS              0x5e
#define UCB_IE_FAL              0x60
#define UCB_IE_STATUS           0x62
#define UCB_IE_CLEAR            0x62
#define UCB_IO_0                (1 << 0)
#define UCB_IO_1                (1 << 1)
#define UCB_IO_2                (1 << 2)
#define UCB_IO_3                (1 << 3)
#define UCB_IO_4                (1 << 4)
#define UCB_IO_5                (1 << 5)
#define UCB_IO_6                (1 << 6)
#define UCB_IO_7                (1 << 7)
#define UCB_IO_8                (1 << 8)
#define UCB_IO_9                (1 << 9)

#define UCB_IE_ADC              (1 << 11)
#define UCB_IE_TSPX             (1 << 12)
#define UCB_IE_TSMX             (1 << 13)
#define UCB_IE_TCLIP            (1 << 14)
#define UCB_IE_ACLIP            (1 << 15)
#define UCB_IE_IO               (0x3FF)

#define UCB_TS_CR               0x64
#define UCB_TS_CR_TSMX_POW      (1 << 0)
#define UCB_TS_CR_TSPX_POW      (1 << 1)
#define UCB_TS_CR_TSMY_POW      (1 << 2)
#define UCB_TS_CR_TSPY_POW      (1 << 3)
#define UCB_TS_CR_TSMX_GND      (1 << 4)
#define UCB_TS_CR_TSPX_GND      (1 << 5)
#define UCB_TS_CR_TSMY_GND      (1 << 6)
#define UCB_TS_CR_TSPY_GND      (1 << 7)
#define UCB_TS_CR_MODE_INT      (0 << 8)
#define UCB_TS_CR_MODE_PRES     (1 << 8)
#define UCB_TS_CR_MODE_POS      (2 << 8)
#define UCB_TS_CR_BIAS_ENA      (1 << 11)
#define UCB_TS_CR_TSPX_LOW      (1 << 12)
#define UCB_TS_CR_TSMX_LOW      (1 << 13)

#define UCB_ADC_CR              0x66
#define UCB_ADC_SYNC_ENA        (1 << 0)
#define UCB_ADC_VREFBYP_CON     (1 << 1)
#define UCB_ADC_INP_TSPX        (0 << 2)
#define UCB_ADC_INP_TSMX        (1 << 2)
#define UCB_ADC_INP_TSPY        (2 << 2)
#define UCB_ADC_INP_TSMY        (3 << 2)
#define UCB_ADC_INP_AD0         (4 << 2)
#define UCB_ADC_INP_AD1         (5 << 2)
#define UCB_ADC_INP_AD2         (6 << 2)
#define UCB_ADC_INP_AD3         (7 << 2)
#define UCB_ADC_EXT_REF         (1 << 5)
#define UCB_ADC_START           (1 << 7)
#define UCB_ADC_ENA             (1 << 15)

#define UCB_ADC_DATA            0x68
#define UCB_ADC_DAT_VALID       (1 << 15)
#define UCB_ADC_DAT_VALUE(x)    ((x) & 0x3ff)

#define UCB_CSR1                0x6a
#define UCB_CSR2                0x6c
#define UCB_IE_EXTRA            0x70

#define UCB_ID                  0x7e
#define UCB_ID_1400             0x4304

/*
 * Battery
 */

#define UCB_BATT_HIGH           560  // adc "ticks" not mV !
#define UCB_BATT_LOW            490
#define UCB_BATT_CRITICAL       480

#define UCB_BATT_DURATION       600 // battery duration in minutes
#define UCB_ADC_TIMEOUT         50

#define UCB_BATT_MIN_VOLTAGE	3550
#define UCB_BATT_MAX_VOLTAGE	4100

#endif

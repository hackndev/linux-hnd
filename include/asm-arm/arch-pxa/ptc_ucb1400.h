/*
 *  linux/drivers/mfd/ucb1400.h
 *
 *  Copyright (C) 2001 Russell King, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 */

/* ucb1400 aclink register mappings: */

#define UCB_IO_DATA	0x5a
#define UCB_IO_DIR	0x5c
#define UCB_IE_RIS	0x5e
#define UCB_IE_FAL	0x60
#define UCB_IE_STATUS	0x62
#define UCB_IE_CLEAR	0x62
#define UCB_TS_CR	0x64
#define UCB_ADC_CR	0x66
#define UCB_ADC_DATA	0x68
#define UCB_CSR1        0x6a      
#define UCB_CSR2        0x6c       
#define UCB_IE_EXTRA	0x70

#define UCB_CS_GPIO_AC97_INT (1 << 2)

#define UCB_ADC_DAT(x)		((x) & 0x3ff)

#define UCB_IO_0		(1 << 0)
#define UCB_IO_1		(1 << 1)
#define UCB_IO_2		(1 << 2)
#define UCB_IO_3		(1 << 3)
#define UCB_IO_4		(1 << 4)
#define UCB_IO_5		(1 << 5)
#define UCB_IO_6		(1 << 6)
#define UCB_IO_7		(1 << 7)
#define UCB_IO_8		(1 << 8)
#define UCB_IO_9		(1 << 9)

#define UCB_IE_ADC		(1 << 11)
#define UCB_IE_TSPX		(1 << 12)
#define UCB_IE_TSMX		(1 << 13)
#define UCB_IE_TCLIP		(1 << 14)
#define UCB_IE_ACLIP		(1 << 15)
#define UCB_IE_IO		(0x3FF)

#define UCB_TC_B_VOICE_ENA	(1 << 3)
#define UCB_TC_B_CLIP		(1 << 4)
#define UCB_TC_B_ATT		(1 << 6)
#define UCB_TC_B_SIDE_ENA	(1 << 11)
#define UCB_TC_B_MUTE		(1 << 13)
#define UCB_TC_B_IN_ENA		(1 << 14)
#define UCB_TC_B_OUT_ENA	(1 << 15)

#define UCB_AC_B_LOOP		(1 << 8)
#define UCB_AC_B_MUTE		(1 << 13)
#define UCB_AC_B_IN_ENA		(1 << 14)
#define UCB_AC_B_OUT_ENA	(1 << 15)

#define UCB_TS_CR_TSMX_POW	(1 << 0)
#define UCB_TS_CR_TSPX_POW	(1 << 1)
#define UCB_TS_CR_TSMY_POW	(1 << 2)
#define UCB_TS_CR_TSPY_POW	(1 << 3)
#define UCB_TS_CR_TSMX_GND	(1 << 4)
#define UCB_TS_CR_TSPX_GND	(1 << 5)
#define UCB_TS_CR_TSMY_GND	(1 << 6)
#define UCB_TS_CR_TSPY_GND	(1 << 7)
#define UCB_TS_CR_MODE_INT	(0 << 8)
#define UCB_TS_CR_MODE_PRES	(1 << 8)
#define UCB_TS_CR_MODE_POS	(2 << 8)
#define UCB_TS_CR_BIAS_ENA	(1 << 11)
#define UCB_TS_CR_TSPX		(1 << 12)
#define UCB_TS_CR_TSMX		(1 << 13)

#define UCB_ADC_SYNC_ENA	(1 << 0)
#define UCB_ADC_VREFBYP_CON	(1 << 1)
#define UCB_ADC_INP_TSPX	(0 << 2)
#define UCB_ADC_INP_TSMX	(1 << 2)
#define UCB_ADC_INP_TSPY	(2 << 2)
#define UCB_ADC_INP_TSMY	(3 << 2)
#define UCB_ADC_INP_AD0		(4 << 2)
#define UCB_ADC_INP_AD1		(5 << 2)
#define UCB_ADC_INP_AD2		(6 << 2)
#define UCB_ADC_INP_AD3		(7 << 2)
#define UCB_ADC_EXT_REF		(1 << 5)
#define UCB_ADC_START		(1 << 7)
#define UCB_ADC_ENA		(1 << 15)

#define UCB_ADC_FILTER_ENA	(1 << 12)

#define UCB_ADC_DATA_MASK	0x3ff
#define UCB_ADC_DATA_VALID	(1 << 15)

#define UCB_ID_1400		0x4304

#define UCB_MODE	0x0d
#define UCB_MODE_DYN_VFLAG_ENA	(1 << 12)
#define UCB_MODE_AUD_OFF_CAN		(1 << 13)

#define UCB_NOSYNC	0
#define UCB_SYNC	1

#define UCB_RISING	(1 << 0)
#define UCB_FALLING	(1 << 1)


// constants on palm tc
#define UCB_TS_X_MIN		100
#define UCB_TS_X_MAX		870
#define UCB_TS_Y_MIN		270
#define UCB_TS_Y_MAX		970
#define UCB_TS_PRESSURE_MIN	0
#define UCB_TS_PRESSURE_MAX	740
#define UCB_TS_FUZZ	15

#define UCB_BATT_HIGH		560  // adc "ticks" not mV !
#define UCB_BATT_LOW		490
#define UCB_BATT_CRITICAL	480

#define UCB_BATT_DURATION	600 // battery duration in minutes
#define UCB_ADC_TIMEOUT		50 

/*
* Driver/bus support for the system on chip (SOC)
* On-chip devices attached to on-chip buses.
*
* Copyright 2003 Hewlett-Packard Company
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
* Author:  Jamey Hicks
*          <Jamey.Hicks@hp.com>
*          July 2003
*
*/

#ifndef _SOC_DEVICE_H_
#define _SOC_DEVICE_H_

#include <linux/device.h>

typedef struct platform_device_id {
	__u32 id;
} platform_device_id;

/* device ID definitions */
#define	IPAQ_ASIC2_ADC_DEVICE_ID	0x00000001
#define	IPAQ_ASIC2_SPI_DEVICE_ID	0x00000003
#define	IPAQ_ASIC2_GPIO_DEVICE_ID	0x00000004
#define	IPAQ_ASIC2_KPIO_DEVICE_ID	0x00000005

#define	IPAQ_SAMCOP_USBH_DEVICE_ID	0x00000101
#define IPAQ_SAMCOP_SRAM_DEVICE_ID	0x00000102
#define IPAQ_SAMCOP_ONEWIRE_DEVICE_ID	0x00000103
#define IPAQ_SAMCOP_ADC_DEVICE_ID	0x00000104
#define IPAQ_SAMCOP_DMA_DEVICE_ID	0x00000105
#define IPAQ_SAMCOP_NAND_DEVICE_ID	0x00000106
#define IPAQ_SAMCOP_FSI_DEVICE_ID	0x00000107
#define IPAQ_SAMCOP_EPS_DEVICE_ID	0x00000108
#define IPAQ_SAMCOP_SDI_DEVICE_ID	0x00000109

#define IPAQ_HAMCOP_SRAM_DEVICE_ID	0x00000111
#define IPAQ_HAMCOP_NAND_DEVICE_ID	0x00000115
#define IPAQ_HAMCOP_LED_DEVICE_ID	0x00000116

#define IPAQ_DS2760_DEVICE_ID		0x00000201

/* MediaQ 1100/1132/1168/1178/1188 subdevices */
#define MEDIAQ_11XX_FB_DEVICE_ID	0x00000301
#define MEDIAQ_11XX_FP_DEVICE_ID	0x00000302
#define MEDIAQ_11XX_UDC_DEVICE_ID	0x00000303
#define MEDIAQ_11XX_UHC_DEVICE_ID	0x00000304
#define MEDIAQ_11XX_SPI_DEVICE_ID	0x00000305
#define MEDIAQ_11XX_I2S_DEVICE_ID	0x00000306

#endif /* _SOC_DEVICE_H_ */

/*
 * TI TSC2101 Structure Definitions
 *
 * Copyright 2005 Openedhand Ltd.
 *
 * Author: Richard Purdie <richard@o-hand.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/adc.h>
/*
 * see http://focus.ti.com/docs/prod/folders/print/tsc2200.html
 * it's a texas instruments TSC 2200 A-D converter with interrupts
 * and keypad interrupts and everything.
 */

#define TSC2200_REG_READ (1<<15) // read register
#define TSC2200_PAGEMASK (0xf<<11) // page of device's memory to be addressed
                                  // see table II
#define TSC2200_PAGE(x) ((x<<11) & TSC2200_PAGEMASK)

#define TSC2200_REG_ADDRMASK (0x1f<<5) // memory address mask
#define TSC2200_REG_ADDR(x) ((x<<5) & TSC2200_REG_ADDRMASK)

#define TSC2200_DATAREG_PAGEADDR (0) // datareg page address
#define TSC2200_CTRLREG_PAGEADDR (1) // control reg page address

#define TSC2200_DATAREG(x) (TSC2200_PAGE(TSC2200_DATAREG_PAGEADDR) | TSC2200_REG_ADDR(x))

// these are data registers, in page 0, so they are addressed
// using e.g. TSC2200_REG_ADDR(TSC2200_DATAREG_PAGEADDR) 

#define TSC2200_DATAREG_X TSC2200_DATAREG(0)
#define TSC2200_DATAREG_Y TSC2200_DATAREG(1)
#define TSC2200_DATAREG_Z1 TSC2200_DATAREG(2)
#define TSC2200_DATAREG_Z2 TSC2200_DATAREG(3)
#define TSC2200_DATAREG_KPDATA TSC2200_DATAREG(4)
#define TSC2200_DATAREG_BAT1 TSC2200_DATAREG(5)
#define TSC2200_DATAREG_BAT2 TSC2200_DATAREG(6)
#define TSC2200_DATAREG_AUX1 TSC2200_DATAREG(7)
#define TSC2200_DATAREG_AUX2 TSC2200_DATAREG(8)
#define TSC2200_DATAREG_TEMP1 TSC2200_DATAREG(9)
#define TSC2200_DATAREG_TEMP2 TSC2200_DATAREG(0xa)
#define TSC2200_DATAREG_DAC TSC2200_DATAREG(0xb)
#define TSC2200_DATAREG_ZERO TSC2200_DATAREG(0x10)

// these are control registers, in page 1, address using
// using e.g. TSC2200_REG_ADDR(TSC2200_CTRLREG_PAGEADDR) 

#define TSC2200_CTRLREG(x) (TSC2200_PAGE(TSC2200_CTRLREG_PAGEADDR) | TSC2200_REG_ADDR(x))

#define TSC2200_CTRLREG_ADC TSC2200_CTRLREG(0)
#define TSC2200_CTRLREG_KEY TSC2200_CTRLREG(1) 
#define TSC2200_CTRLREG_DACCTL TSC2200_CTRLREG(2)
#define TSC2200_CTRLREG_REF TSC2200_CTRLREG(3) 
#define TSC2200_CTRLREG_RESET TSC2200_CTRLREG(4)
#define TSC2200_CTRLREG_CONFIG TSC2200_CTRLREG(5)
#define TSC2200_CTRLREG_KPMASK TSC2200_CTRLREG(0x10)

// these are relevant for when you access page1's adc register
#define TSC2200_CTRLREG_ADC_AD(x) ((x<<10) & TSC2200_CTRLREG_ADC_AD_MASK )
#define TSC2200_CTRLREG_ADC_AD_MASK (0x3<<10)  
#define TSC2200_CTRLREG_ADC_PSM_TSC2200 (1<<15) // pen status mode on ctrlreg adc
#define TSC2200_CTRLREG_ADC_STS (1<<14) // stop continuous scanning.
#define TSC2200_CTRLREG_ADC_AD3 (13) // 
#define TSC2200_CTRLREG_ADC_AD2 (12) // 
#define TSC2200_CTRLREG_ADC_AD1 (11) // 
#define TSC2200_CTRLREG_ADC_AD0 (10) // 
#define TSC2200_CTRLREG_ADC_RES(x) ((x<<8) & TSC2200_CTRLREG_ADC_RES_MASK )
#define TSC2200_CTRLREG_ADC_RES_MASK (0x3<<8)  
#define TSC2200_CTRLREG_ADC_RES_12BITP (0) // 12-bit ADC resolution (default)
#define TSC2200_CTRLREG_ADC_RES_8BIT (1) // 8-bit ADC resolution
#define TSC2200_CTRLREG_ADC_RES_10BIT (2) // 10-bit ADC resolution
#define TSC2200_CTRLREG_ADC_RES_12BIT (3) // 12-bit ADC resolution

#define TSC2200_CTRLREG_ADC_AVG(x) ((x<<6) & TSC2200_CTRLREG_ADC_AVG_MASK )
#define TSC2200_CTRLREG_ADC_AVG_MASK (0x3<<6)  
#define TSC2200_CTRLREG_ADC_NOAVG (0) //  a-d does no averaging
#define TSC2200_CTRLREG_ADC_4AVG (1) //  a-d does averaging of 4 samples
#define TSC2200_CTRLREG_ADC_8AVG (2) //  a-d does averaging of 8 samples
#define TSC2200_CTRLREG_ADC_16AVG (3) //  a-d does averaging of 16 samples

#define TSC2200_CTRLREG_ADC_CL(x) ((x<<4) & TSC2200_CTRLREG_ADC_CL_MASK )
#define TSC2200_CTRLREG_ADC_CL_MASK (0x3<<4)
#define TSC2200_CTRLREG_ADC_CL_8MHZ_8BIT (0)
#define TSC2200_CTRLREG_ADC_CL_4MHZ_10BIT (1)
#define TSC2200_CTRLREG_ADC_CL_2MHZ_12BIT (2)
#define TSC2200_CTRLREG_ADC_CL_1MHZ_12BIT (3)
#define TSC2200_CTRLREG_ADC_CL0 (1<< 4) // 

#define TSC2200_CTRLREG_ADC_PV(x) ((x<<1) & TSC2200_CTRLREG_ADC_PV_MASK )
#define TSC2200_CTRLREG_ADC_PV_MASK (0x7<<1)
#define TSC2200_CTRLREG_ADC_PV_100mS (0x7) // 100ms panel voltage stabilisation
// ....
#define TSC2200_CTRLREG_ADC_PV_5mS (0x4) // 5ms panel voltage stabilisation
#define TSC2200_CTRLREG_ADC_PV_1mS (0x3) // 1ms panel voltage stabilisation
#define TSC2200_CTRLREG_ADC_PV_500uS (0x2) // 500us panel voltage stabilisation
#define TSC2200_CTRLREG_ADC_PV_100uS (0x1) // 100us panel voltage stabilisation
#define TSC2200_CTRLREG_ADC_PV_0S (0x0) // zero seconds stabilisation

#define TSC2200_CTRLREG_ADC_x (1<< 0) // don't care

#define TSC2200_CTRLREG_CONFIG_DAV (1<<6)

#define TSC2200_CTRLREG_KEY_STC (1<<15) // keypad status
#define TSC2200_CTRLREG_KEY_SCS (1<<14) // keypad scan status

/* 
 * public interface.
 *
 * the tsc2200 driver is timer-interrupt-driven and pen-irq driven.
 *
 * it only needs to know how to read and write to the NSSP, via
 * the send_cmd and recv_cmd functions in the nssp_tsc2200_struct.
 *
 */


#include <linux/input.h>

/*
 * Keyboard definitions
 */


struct tsc2200_key {
	char *name;
	int key_index;
	int keycode;
};

struct tsc2200_buttons_platform_info {
	struct tsc2200_key *keys;
	int num_keys;
	int irq;
};

struct tsc2200_buttons_data {
	struct tsc2200_buttons_platform_info *platform_info;
	struct device *tsc2200_dev;

	struct timer_list timer;
	struct input_dev *input_dev;
	int keydata;
	
};

/*
 * Definitions
 */

struct tsc2200_data {
	spinlock_t lock;
	struct tsc2200_platform_info *platform;
	struct platform_device *buttons_dev;
	struct platform_device *ts_dev;
};

struct tsc2200_platform_info {
	unsigned short (*send)(unsigned short reg, unsigned short value);
	void (*init)(void);
//	int (*suspend) (void);
//	int (*resume) (void);
	void (*exit)(void);
	
	struct tsc2200_buttons_platform_info *buttons_info;
	struct tsc2200_ts_platform_info *touchscreen_info;
	
};


/* Sensor pins available */
enum {
        TSC2200_PIN_XPOS  = 0,
        TSC2200_PIN_YPOS  = 1,
        TSC2200_PIN_Z1POS = 2,
        TSC2200_PIN_Z2POS = 3,
        TSC2200_PIN_BAT1  = 4,
        TSC2200_PIN_BAT2  = 5,
        TSC2200_PIN_AUX1  = 6,
        TSC2200_PIN_AUX2  = 7,
        TSC2200_PIN_TEMP1 = 8,
        TSC2200_PIN_TEMP2 = 9,

        TSC2200_PIN_CUSTOM_START  = 10,
};

/*
 * ADC definitions
 */

struct tsc2200_adc_data {
        /* SSP port to use */
        int port;
        /* SSP clock frequency in Hz */
        int freq;
        /* Callback to multiplex custom
           pins. Should return real pin to sample, or <0 if error. */
        int (*mux_custom)(int custom_pin);

        struct adc_classdev *custom_adcs;
        size_t num_custom_adcs;
	struct tsc2200_ts_platform_info *platform_info;
	struct device *tsc2200_dev;
};
extern void tsc2200_write(struct device *dev, unsigned short regnum, unsigned short value);
extern unsigned short tsc2200_read(struct device *dev, unsigned short reg);
extern unsigned short tsc2200_dav(struct device *dev);

extern void tsc2200_stop(struct device *dev);
extern void tsc2200_lock(struct device *dev);
extern void tsc2200_unlock(struct device *dev);




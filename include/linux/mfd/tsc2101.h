/*
 * TI TSC2101 Hardware definitions
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

/* Address constructs */
#define TSC2101_READ     (1 << 15)    /* Read Register */
#define TSC2101_WRITE    (0 << 15)    /* Write Register */
#define TSC2101_PAGE(x)  ((x & 0xf) << 11) /* Memory Page to access */
#define TSC2101_ADDR(x)  ((x & 0x3f) << 5) /* Memory Address to access */

#define TSC2101_P0_REG(x) (TSC2101_PAGE(0) | TSC2101_ADDR(x))
#define TSC2101_P1_REG(x) (TSC2101_PAGE(1) | TSC2101_ADDR(x))
#define TSC2101_P2_REG(x) (TSC2101_PAGE(2) | TSC2101_ADDR(x))
#define TSC2101_P3_REG(x) (TSC2101_PAGE(3) | TSC2101_ADDR(x))

/* Page 0 Registers */
#define TSC2101_REG_X      TSC2101_P0_REG(0x0)
#define TSC2101_REG_Y      TSC2101_P0_REG(0x1)
#define TSC2101_REG_Z1     TSC2101_P0_REG(0x2)
#define TSC2101_REG_Z2     TSC2101_P0_REG(0x3)
#define TSC2101_REG_BAT    TSC2101_P0_REG(0x5)
#define TSC2101_REG_AUX1   TSC2101_P0_REG(0x7)
#define TSC2101_REG_AUX2   TSC2101_P0_REG(0x8)
#define TSC2101_REG_TEMP1  TSC2101_P0_REG(0x9)
#define TSC2101_REG_TEMP2  TSC2101_P0_REG(0xa)

/* Page 1 Registers */
#define TSC2101_REG_ADC       TSC2101_P1_REG(0x0)
#define TSC2101_REG_STATUS    TSC2101_P1_REG(0x1)
#define TSC2101_REG_BUFMODE   TSC2101_P1_REG(0x2)
#define TSC2101_REG_REF       TSC2101_P1_REG(0x3)
#define TSC2101_REG_RESETCTL  TSC2101_P1_REG(0x4)
#define TSC2101_REG_CONFIG    TSC2101_P1_REG(0x5)
#define TSC2101_REG_TEMPMAX   TSC2101_P1_REG(0x6)
#define TSC2101_REG_TEMPMIN   TSC2101_P1_REG(0x7)
#define TSC2101_REG_AUX1MAX   TSC2101_P1_REG(0x8)
#define TSC2101_REG_AUX1MIN   TSC2101_P1_REG(0x9)
#define TSC2101_REG_AUX2MAX   TSC2101_P1_REG(0xa)
#define TSC2101_REG_AUX2MIN   TSC2101_P1_REG(0xb)
#define TSC2101_REG_MEASURE   TSC2101_P1_REG(0xc)
#define TSC2101_REG_DELAY     TSC2101_P1_REG(0xd)

/* Page 2 Registers */
#define TSC2101_REG_AUDIOCON1   TSC2101_P2_REG(0x0)
#define TSC2101_REG_HEADSETPGA  TSC2101_P2_REG(0x1)
#define TSC2101_REG_DACPGA      TSC2101_P2_REG(0x2)
#define TSC2101_REG_MIXERPGA    TSC2101_P2_REG(0x3)
#define TSC2101_REG_AUDIOCON2   TSC2101_P2_REG(0x4)
#define TSC2101_REG_PWRDOWN     TSC2101_P2_REG(0x5)
#define TSC2101_REG_AUDIOCON3   TSC2101_P2_REG(0x6)
#define TSC2101_REG_DAEFC(x)	TSC2101_P2_REG(0x7+x)
#define TSC2101_REG_PLL1        TSC2101_P2_REG(0x1b)
#define TSC2101_REG_PLL2        TSC2101_P2_REG(0x1c)
#define TSC2101_REG_AUDIOCON4   TSC2101_P2_REG(0x1d)
#define TSC2101_REG_HANDSETPGA  TSC2101_P2_REG(0x1e)
#define TSC2101_REG_BUZZPGA     TSC2101_P2_REG(0x1f)
#define TSC2101_REG_AUDIOCON5   TSC2101_P2_REG(0x20)
#define TSC2101_REG_AUDIOCON6   TSC2101_P2_REG(0x21)
#define TSC2101_REG_AUDIOCON7   TSC2101_P2_REG(0x22)
#define TSC2101_REG_GPIO        TSC2101_P2_REG(0x23)
#define TSC2101_REG_AGCCON      TSC2101_P2_REG(0x24)
#define TSC2101_REG_DRVPWRDWN   TSC2101_P2_REG(0x25)
#define TSC2101_REG_MICAGC      TSC2101_P2_REG(0x26)
#define TSC2101_REG_CELLAGC     TSC2101_P2_REG(0x27)

/* Page 3 Registers */
#define TSC2101_REG_BUFLOC(x)	TSC2101_P3_REG(x)

/* Status Register Masks */

#define TSC2101_STATUS_T2STAT   (1 << 1)
#define TSC2101_STATUS_T1STAT   (1 << 2)
#define TSC2101_STATUS_AX2STAT  (1 << 3)
#define TSC2101_STATUS_AX1STAT  (1 << 4)
#define TSC2101_STATUS_BSTAT    (1 << 6)
#define TSC2101_STATUS_Z2STAT   (1 << 7)
#define TSC2101_STATUS_Z1STAT   (1 << 8)
#define TSC2101_STATUS_YSTAT    (1 << 9)
#define TSC2101_STATUS_XSTAT    (1 << 10)
#define TSC2101_STATUS_DAVAIL   (1 << 11)     
#define TSC2101_STATUS_HCTLM    (1 << 12)
#define TSC2101_STATUS_PWRDN    (1 << 13)
#define TSC2101_STATUS_PINTDAV_SHIFT (14)
#define TSC2101_STATUS_PINTDAV_MASK  (0x03)







#define TSC2101_ADC_PSM (1<<15) // pen status mode on ctrlreg adc
#define TSC2101_ADC_STS (1<<14) // stop continuous scanning.
#define TSC2101_ADC_AD3 (1<<13) 
#define TSC2101_ADC_AD2 (1<<12) 
#define TSC2101_ADC_AD1 (1<<11) 
#define TSC2101_ADC_AD0 (1<<10) 
#define TSC2101_ADC_ADMODE(x) ((x<<10) & TSC2101_ADC_ADMODE_MASK)
#define TSC2101_ADC_ADMODE_MASK (0xf<<10)  

#define TSC2101_ADC_RES(x) ((x<<8) & TSC2101_ADC_RES_MASK )
#define TSC2101_ADC_RES_MASK (0x3<<8)  
#define TSC2101_ADC_RES_12BITP (0) // 12-bit ADC resolution (default)
#define TSC2101_ADC_RES_8BIT (1) // 8-bit ADC resolution
#define TSC2101_ADC_RES_10BIT (2) // 10-bit ADC resolution
#define TSC2101_ADC_RES_12BIT (3) // 12-bit ADC resolution

#define TSC2101_ADC_AVG(x) ((x<<6) & TSC2101_ADC_AVG_MASK )
#define TSC2101_ADC_AVG_MASK (0x3<<6)  
#define TSC2101_ADC_NOAVG (0) //  a-d does no averaging
#define TSC2101_ADC_4AVG (1) //  a-d does averaging of 4 samples
#define TSC2101_ADC_8AVG (2) //  a-d does averaging of 8 samples
#define TSC2101_ADC_16AVG (3) //  a-d does averaging of 16 samples

#define TSC2101_ADC_CL(x) ((x<<4) & TSC2101_ADC_CL_MASK )
#define TSC2101_ADC_CL_MASK (0x3<<4)
#define TSC2101_ADC_CL_8MHZ_8BIT (0)
#define TSC2101_ADC_CL_4MHZ_10BIT (1)
#define TSC2101_ADC_CL_2MHZ_12BIT (2)
#define TSC2101_ADC_CL_1MHZ_12BIT (3)
#define TSC2101_ADC_CL0 (1<< 4)  

/* ADC - Panel Voltage Stabilisation Time */
#define TSC2101_ADC_PV(x)     ((x<<1) & TSC2101_ADC_PV_MASK )
#define TSC2101_ADC_PV_MASK   (0x7<<1)
#define TSC2101_ADC_PV_100ms  (0x7)  /* 100ms */
#define TSC2101_ADC_PV_50ms   (0x6)  /* 50ms  */
#define TSC2101_ADC_PV_10ms   (0x5)  /* 10ms  */
#define TSC2101_ADC_PV_5ms    (0x4)  /* 5ms   */
#define TSC2101_ADC_PV_1ms    (0x3)  /* 1ms   */
#define TSC2101_ADC_PV_500us  (0x2)  /* 500us */
#define TSC2101_ADC_PV_100us  (0x1)  /* 100us */
#define TSC2101_ADC_PV_0s     (0x0)  /* 0s    */

#define TSC2101_ADC_AVGFILT_MEAN     (0<<0)  /* Mean Average Filter */
#define TSC2101_ADC_AVGFILT_MEDIAN   (1<<0)  /* Median Average Filter */

#define TSC2101_ADC_x (1<< 0) // don't care

#define TSC2101_CONFIG_DAV (1<<6)

#define TSC2101_KEY_STC (1<<15) // keypad status
#define TSC2101_KEY_SCS (1<<14) // keypad scan status


/* Sound */
#define TSC2101_DACFS(x)	((x & 0x07) << 3)
#define TSC2101_ADCFS(x)	(x & 0x07)
#define TSC2101_REFFS           (1 << 13)
#define TSC2101_DAXFM           (1 << 12)
#define TSC2101_SLVMS           (1 << 11)
#define TSC2101_PLL_ENABLE	(1 << 15)
#define TSC2101_PLL_QVAL(x)	(x << 11)
#define TSC2101_PLL_PVAL(x)	(x << 8)
#define TSC2101_PLL_JVAL(x)	(x << 2)
#define TSC2101_PLL2_DVAL(x)	(x << 2)

#define TSC2101_ADMUT_HED	(1 << 15)
#define TSC2101_ADPGA_HED(x)	((x & 0x7F) << 8)
#define TSC2101_ADPGA_HEDI(x)	((x >> 8) & 0x7F)

#define TSC2101_ADMUT_HND	(1 << 15)
#define TSC2101_ADPGA_HND(x)	((x & 0x7F) << 8)
#define TSC2101_ADPGA_HNDI(x)	((x >> 8) & 0x7F)

#define TSC2101_DALMU		(1 << 15)
#define TSC2101_DALVL(x)	(((x) & 0x7F) << 8)
#define TSC2101_DALVLI(x)	(((x) >> 8) & 0x7F)
#define TSC2101_DARMU		(1 << 7)
#define TSC2101_DARVL(x)	((x) & 0x7F)
#define TSC2101_DARVLI(x)	((x) & 0x7F)

#define TSC2101_ASTMU		(1 << 15)
#define TSC2101_ASTG(x)		(x << 8)
#define TSC2101_MICADC		(1 << 4)
#define TSC2101_MICSEL(x)	(x << 5)
#define TSC2101_MB_HED(x)	((x & 0x02) << 7)
#define TSC2101_MB_HND		(1 << 6)
#define TSC2101_ADSTPD		(1 << 15)
#define TSC2101_DASTPD		(1 << 14)
#define TSC2101_ASSTPD		(1 << 13)
#define TSC2101_CISTPD		(1 << 12)
#define TSC2101_BISTPD		(1 << 11)
#define TSC2101_DAC2SPK1(x)	((x & 0x03) << 13)
#define TSC2101_DAC2SPK2(x)	((x & 0x03) << 7)
#define TSC2101_AST2SPK1	(1 << 12)
#define TSC2101_AST2SPK2	(1 << 6)
#define TSC2101_KCL2SPK1	(1 << 10)
#define TSC2101_KCL2SPK2	(1 << 4)
#define TSC2101_HDSCPTC		(1 << 0)

#define TSC2101_MUTLSPK		(1 << 7)
#define TSC2101_MUTCELL 	(1 << 6)
#define TSC2101_LDSCPTC		(1 << 5)
#define TSC2101_VGNDSCPTC	(1 << 4)
#define TSC2101_CAPINTF		(1 << 3)
#define TSC2101_SPL2LSK		(1 << 15)

#define TSC2101_HDDETFL		(1 << 12)
#define TSC2101_MUTSPK1		(1 << 2)
#define TSC2101_MUTSPK2		(1 << 1)

#define TSC2101_DETECT		(1 << 15)
#define TSC2101_HDDEBNPG(x)	((x & 0x03) << 9)
#define TSC2101_DGPIO2		(1 << 4)

#define TSC2101_MBIAS_HND	(1 << 15)
#define TSC2101_MBIAS_HED	(1 << 14)
#define TSC2101_ASTPWD		(1 << 13)
#define TSC2101_SPI1PWDN	(1 << 12)
#define TSC2101_SPI2PWDN	(1 << 11)
#define TSC2101_DAPWDN		(1 << 10)
#define TSC2101_ADPWDN		(1 << 9)
#define TSC2101_VGPWDN		(1 << 8)
#define TSC2101_COPWDN		(1 << 7)
#define TSC2101_LSPWDN		(1 << 6)
#define TSC2101_EFFCTL		(1 << 1)

#define TSC2101_MMPGA(x)	((x & 0x7F) << 9)
#define TSC2101_MDEBNS(x)	((x & 0x07) << 6)
#define TSC2101_MDEBSN(x)	((x & 0x07) << 3)

#define FLAG_HEADPHONES	0x01

#define TSC2101_ADC_DEFAULT (TSC2101_ADC_RES(TSC2101_ADC_RES_12BITP) | TSC2101_ADC_AVG(TSC2101_ADC_4AVG) | TSC2101_ADC_CL(TSC2101_ADC_CL_1MHZ_12BIT) | TSC2101_ADC_PV(TSC2101_ADC_PV_500us) | TSC2101_ADC_AVGFILT_MEAN) 

#define TSC2101_TIMER	1
#define TSC2101_IRQ	0

#define TSC2101_DEV_TS		0
#define TSC2101_DEV_SND		1
#define TSC2101_DEV_MEAS	2

#define LIV_MAX		0x7F
#define VOLtoTSC(x)	((x * LIV_MAX) / 100)
#define TSCtoVOL(x)	((x * 100) / LIV_MAX)

#ifdef CONFIG_TOUCHSCREEN_TSC2101
#define X_AXIS_MAX		3800
#define X_AXIS_MIN		300
#define Y_AXIS_MAX		3900
#define Y_AXIS_MIN		140
#define PRESSURE_MIN	10
#define PRESSURE_MAX	20000
#else // hx2750
#define X_AXIS_MAX		3830
#define X_AXIS_MIN		150
#define Y_AXIS_MAX		3830
#define Y_AXIS_MIN		190
#define PRESSURE_MIN	0
#define PRESSURE_MAX	20000
#endif


#include <linux/timer.h>
#include <linux/pm.h>


struct tsc2101_ts_event {
	short p;
	short x;
	short y;
};

struct tsc2101;

struct tsc2101_driver {
	/* initialization and finalization functions */
	int	(*probe)(struct tsc2101 *);
	int	(*remove)(void);

	/* power management functions */
	int	(*suspend)(pm_message_t state);
	int	(*resume)(void);


	/* part specific reading data from tsc2101 chip */
	u8 	(*readdata)(u32);

	/* pointer to main structure */
	struct tsc2101	*tsc;
	void	*priv;
};

/* main sound driver structure */
struct tsc2101_snd {
	u8 flags;
};

/* main touchscreen driver structure */
struct tsc2101_ts {
	struct input_dev *inputdevice;
	struct timer_list ts_timer;
	int pendown;
//	void (*readdata)(u32);
};

/* main measurement driver structure */
struct tsc2101_meas {
	struct timer_list misc_timer;
	short bat;
	short aux1;
	short aux2;
	short temp1;
	short temp2;
//	void (*readdata)(u32);
};

struct tsc2101_platform_info;

/* main structure */
struct tsc2101 {
	spinlock_t lock;
	unsigned long lock_flags;
	struct tsc2101_driver *ts;
	struct tsc2101_driver *snd;
	struct tsc2101_driver *meas;
	struct tsc2101_platform_info *platform;


/*
	struct tsc2101_misc_data miscdata;*/
};


struct tsc2101_platform_info {
	/* SPI functions */
	void (*send)(int read, int command, int *values, int numval);
	void (*suspend) (void);
	void (*resume) (void);

	/* IRQ for touchscreen*/
	int irq;

	/* 'is pen down on screen?' function */
	int (*pendown) (void);
};


/*
 tsc2101
	dev - device
 Write register through SPI
 */
static inline void tsc2101_regwrite(struct tsc2101 *dev, unsigned int regnum, unsigned int val)
{
	u32 reg;
	dev->platform->send(TSC2101_READ, regnum, &reg, 1);
	reg = val;
	dev->platform->send(TSC2101_WRITE, regnum, &reg, 1);
	return;
}

static inline u32 tsc2101_regread(struct tsc2101 *dev, unsigned int regnum)
{
	u32 reg;
	dev->platform->send(TSC2101_READ, regnum, &reg, 1);
	return reg;
}

void tsc2101_lock(void);
void tsc2101_unlock(void);
void tsc2101_readdata(void);
int tsc2101_unregister(struct tsc2101_driver *drv,int dev_type);
int tsc2101_register(struct tsc2101_driver *drv,int dev_type);

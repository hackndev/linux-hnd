/*
 * Definitions for WM9712 AC97 codec
 * Based upon wm97xx.h by Liam Girdwood, Wolfson Microelectronics
 */

#ifndef _A730_AC97_H_
#define _A730_AC97_H_


#define AC97_ADD_FUNC           0x58    /* Additional functions */

/*
 * WM97xx AC97 Touchscreen registers
 */
#define AC97_WM97XX_DIGITISER1	0x76
#define AC97_WM97XX_DIGITISER2	0x78
#define AC97_WM97XX_DIGITISER_RD 0x7a
#define AC97_WM9713_DIG1		0x74
#define AC97_WM9713_DIG2		AC97_WM97XX_DIGITISER1
#define AC97_WM9713_DIG3		AC97_WM97XX_DIGITISER2

/*
 * WM97xx GPGIO states
 */
#define WM97XX_GPIO_OUT 0
#define WM97XX_GPIO_IN 1
#define WM97XX_GPIO_POL_HIGH 1
#define WM97XX_GPIO_POL_LOW 0
#define WM97XX_GPIO_STICKY 1
#define WM97XX_GPIO_NOTSTICKY 0
#define WM97XX_GPIO_WAKE 1
#define WM97XX_GPIO_NOWAKE 0

/*
 * WM97xx register bits
 */
#define WM97XX_POLL		0x8000		/* initiate a polling measurement */
#define WM97XX_ADCSEL_X		0x1000		/* x coord measurement */
#define WM97XX_ADCSEL_Y		0x2000		/* y coord measurement */
#define WM97XX_ADCSEL_PRES	0x3000		/* pressure measurement */
#define WM97XX_ADCSEL_COMP1	0x4000		/* aux1 measurement */
#define WM97XX_ADCSEL_COMP2	0x5000		/* aux2 measurement */
#define WM97XX_ADCSEL_BMON	0x6000		/* aux3 measurement */
#define WM97XX_ADCSEL_WIPER	0x7000		/* aux4 measurement */
#define WM97XX_ADCSEL_MASK	0x7000
#define WM97XX_COO		0x0800		/* enable coordinate mode */
#define WM97XX_CTC		0x0400		/* enable continuous mode */
#define WM97XX_CM_RATE_93	0x0000		/* 93.75Hz continuous rate */
#define WM97XX_CM_RATE_187	0x0100		/* 187.5Hz continuous rate */
#define WM97XX_CM_RATE_375	0x0200		/* 375Hz continuous rate */
#define WM97XX_CM_RATE_750	0x0300		/* 750Hz continuous rate */
#define WM97XX_CM_RATE_8K	0x00f0		/* 8kHz continuous rate */
#define WM97XX_CM_RATE_12K	0x01f0		/* 12kHz continuous rate */
#define WM97XX_CM_RATE_24K	0x02f0		/* 24kHz continuous rate */
#define WM97XX_CM_RATE_48K	0x03f0		/* 48kHz continuous rate */
#define WM97XX_CM_RATE_MASK	0x03f0
#define WM97XX_RATE(i)		(((i & 3) << 8) | ((i & 4) ? 0xf0 : 0))
#define WM97XX_DELAY(i)		((i << 4) & 0x00f0)	/* sample delay times */
#define WM97XX_DELAY_MASK	0x00f0
#define WM97XX_SLEN		0x0008		/* slot read back enable */
#define WM97XX_SLT(i)		((i - 5) & 0x7)	/* touchpanel slot selection (5-11) */
#define WM97XX_SLT_MASK		0x0007
#define WM97XX_PRP_DETW		0x4000		/* pen detect on, digitiser off, wake up */
#define WM97XX_PRP_DET		0x8000		/* pen detect on, digitiser off, no wake up */
#define WM97XX_PRP_DET_DIG	0xc000		/* pen detect on, digitiser on */
#define WM97XX_RPR		0x2000		/* wake up on pen down */
#define WM97XX_PEN_DOWN		0x8000		/* pen is down */
#define WM97XX_ADCSRC_MASK	0x7000		/* ADC source mask */

#define WM97XX_AUX_ID1      0x8001
#define WM97XX_AUX_ID2      0x8002
#define WM97XX_AUX_ID3      0x8003
#define WM97XX_AUX_ID4      0x8004

/* Codec GPIO's */
#define WM97XX_MAX_GPIO	16
#define WM97XX_GPIO_1	(1 << 1)
#define WM97XX_GPIO_2	(1 << 2)
#define WM97XX_GPIO_3	(1 << 3)
#define WM97XX_GPIO_4	(1 << 4)
#define WM97XX_GPIO_5	(1 << 5)
#define WM97XX_GPIO_6	(1 << 6)
#define WM97XX_GPIO_7	(1 << 7)
#define WM97XX_GPIO_8	(1 << 8)
#define WM97XX_GPIO_9	(1 << 9)
#define WM97XX_GPIO_10	(1 << 10)
#define WM97XX_GPIO_11	(1 << 11)
#define WM97XX_GPIO_12	(1 << 12)
#define WM97XX_GPIO_13	(1 << 13)
#define WM97XX_GPIO_14	(1 << 14)
#define WM97XX_GPIO_15	(1 << 15)

/* WM9712 Bits */
#define WM9712_45W                      0x1000  /* set for 5-wire touchscreen */
#define WM9712_PDEN                     0x0800  /* measure only when pen down */
#define WM9712_WAIT                     0x0200  /* wait until adc is read before next sample */
#define WM9712_PIL                      0x0100  /* current used for pressure measurement. set 400uA else 200uA */
#define WM9712_MASK_HI          0x0040  /* hi on mask pin (47) stops conversions */
#define WM9712_MASK_EDGE        0x0080  /* rising/falling edge on pin delays sample */
#define WM9712_MASK_SYNC        0x00c0  /* rising/falling edge on mask initiates sample */
#define WM9712_RPU(i)           (i&0x3f)        /* internal pull up on pen detect (64k / rpu) */
#define WM9712_PD(i)            (0x1 << i)      /* power management */
#define WM9712_ADCSEL_COMP1     0x4000          /* COMP1/AUX1 measurement (pin29) */
#define WM9712_ADCSEL_COMP2     0x5000          /* COMP2/AUX2 measurement (pin30) */
#define WM9712_ADCSEL_BMON      0x6000          /* BMON/AUX3 measurement (pin31) */
#define WM9712_ADCSEL_WIPER     0x7000          /* WIPER/AUX4 measurement (pin12) */

#endif /* _A730_AC97_H_ */

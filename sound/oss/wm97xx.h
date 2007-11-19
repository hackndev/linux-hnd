/*
 * Register bits for Wolfson WM97xx series of codecs
 */
 
#ifndef _WM97XX_H_
#define _WM97XX_H_

#include <linux/ac97_codec.h>	/* AC97 control layer */

/*
 * WM97xx AC97 Touchscreen registers
 */
#define AC97_WM97XX_DIGITISER1	0x76
#define AC97_WM97XX_DIGITISER2	0x78
#define AC97_WM97XX_DIGITISER_RD 0x7a

/*
 * WM97xx register bits
 */
#define WM97XX_POLL		0x8000		/* initiate a polling measurement */
#define WM97XX_ADCSEL_X		0x1000		/* x coord measurement */
#define WM97XX_ADCSEL_Y		0x2000		/* y coord measurement */
#define WM97XX_ADCSEL_PRES	0x3000		/* pressure measurement */
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

/* WM9712 Bits */
#define WM9712_45W		0x1000		/* set for 5-wire touchscreen */
#define WM9712_PDEN		0x0800		/* measure only when pen down */
#define WM9712_WAIT		0x0200		/* wait until adc is read before next sample */
#define WM9712_PIL		0x0100		/* current used for pressure measurement. set 400uA else 200uA */ 
#define WM9712_MASK_HI		0x0040		/* hi on mask pin (47) stops conversions */
#define WM9712_MASK_EDGE	0x0080		/* rising/falling edge on pin delays sample */
#define	WM9712_MASK_SYNC	0x00c0		/* rising/falling edge on mask initiates sample */
#define WM9712_RPU(i)		(i&0x3f)	/* internal pull up on pen detect (64k / rpu) */
#define WM9712_ADCSEL_COMP1	0x4000		/* COMP1/AUX1 measurement (pin29) */
#define WM9712_ADCSEL_COMP2	0x5000		/* COMP2/AUX2 measurement (pin30) */
#define WM9712_ADCSEL_BMON	0x6000		/* BMON/AUX3 measurement (pin31) */
#define WM9712_ADCSEL_WIPER	0x7000		/* WIPER/AUX4 measurement (pin12) */
#define WM9712_PD(i)		(0x1 << i)  /* power management */ 

/* WM9712 Registers */
#define AC97_WM9712_POWER	0x24
#define AC97_WM9712_REV		0x58

/* WM9705 Bits */
#define WM9705_PDEN		0x1000		/* measure only when pen is down */
#define WM9705_PINV		0x0800		/* inverts sense of pen down output */
#define WM9705_BSEN		0x0400		/* BUSY flag enable, pin47 is 1 when busy */
#define WM9705_BINV		0x0200		/* invert BUSY (pin47) output */
#define WM9705_WAIT		0x0100		/* wait until adc is read before next sample */
#define WM9705_PIL		0x0080		/* current used for pressure measurement. set 400uA else 200uA */ 
#define WM9705_PHIZ		0x0040		/* set PHONE and PCBEEP inputs to high impedance */
#define WM9705_MASK_HI		0x0010		/* hi on mask stops conversions */
#define WM9705_MASK_EDGE	0x0020		/* rising/falling edge on pin delays sample */
#define	WM9705_MASK_SYNC	0x0030		/* rising/falling edge on mask initiates sample */
#define WM9705_PDD(i)		(i & 0x000f)	/* pen detect comparator threshold */
#define WM9705_ADCSEL_BMON	0x4000		/* BMON measurement */
#define WM9705_ADCSEL_AUX	0x5000		/* AUX measurement */
#define WM9705_ADCSEL_PHONE	0x6000		/* PHONE measurement */
#define WM9705_ADCSEL_PCBEEP	0x7000		/* PCBEEP measurement */

/* AUX ADC ID's */
#define TS_COMP1		0x0
#define TS_COMP2		0x1
#define TS_BMON			0x2
#define TS_WIPER		0x3

/* ID numbers */
#define WM97XX_ID1		0x574d
#define WM9712_ID2		0x4c12
#define WM9705_ID2		0x4c05

#define AC97_LINK_FRAME		21		/* time in uS for AC97 link frame */

/*---------------- Return codes from sample reading functions ---------------*/

/* More data is available; call the sample gathering function again */
#define RC_AGAIN		0x00000001
/* The returned sample is valid */
#define RC_VALID		0x00000002
/* The pen is up (the first RC_VALID without RC_PENUP means pen is down) */
#define RC_PENUP		0x00000004
/* The pen is down (RC_VALID implies RC_PENDOWN, but sometimes it is helpful
   to tell the handler that the pen is down but we don't know yet his coords,
   so the handler should not sleep or wait for pendown irq) */
#define RC_PENDOWN		0x00000008

/* The structure used to return sampled data into */
typedef struct {
	int x;
	int y;
	int p;
} wm97xx_data;

/* Driver parameters (same as those which can be set from command line) */
typedef struct {
	int mode;
	int rpu;
	int pdd;
	int pil;
	int five_wire;
        int delay;
	int pen_irq;
	int cont_rate;
	int cont_slot;
	/* Set bits 0/1 to 1 if the X/Y coordinate must be inverted.
	 * This is used to bring the coordinate system of the touchscreen
	 * in accordance with coordinate system of the LCD.
	 */
        int invert;
} wm97xx_params;

/* The identifier of an auxiliary ADC conversion.
 */
typedef enum
{
	WM97XX_AC_BMON,
	WM97XX_AC_AUX1, /* WM9705: AUX     WM9712: COMP1 */
	WM97XX_AC_AUX2, /* WM9705: PHONE   WM9712: COMP2 */
	WM97XX_AC_AUX3  /* WM9705: PCBEEP  WM9712: WIPER */
} wm97xx_aux_conv;

struct _wm97xx_public;

/* A special function to be called before and after the auxiliary conversion.
 * This function can e.g. toggle some external switch via a platform specific
 * GPIO to route the required analog signal to the respective WM97xx input.
 * The 'stage' parameter is 0 before conversion, 1 after.
 */
typedef void (*wm97xx_ac_prepare) (struct _wm97xx_public *wmpub,
				   wm97xx_aux_conv ac, int stage);

/* The wm97xx driver provides a private API for writing platform-specific
 * drivers. The actual way how wm97xx codec is used is highly dependent
 * of the actual hardware architecture, and it is not possible to implement
 * everything in a hardware-independent manner. To circumvent this limit,
 * the driver provides an API that can be used to communicate with the driver
 * to both query and set driver parameters.
 */
typedef struct _wm97xx_public {
	/* A pointer to the audio codec (for example, pxa-ac97.c).
	 */
	struct ac97_codec *codec;

	/* Set all driver parameters at once. If you don't want to touch
	 * some of the parameters, set them to -1. On return all the fields
	 * will contain the current driver setup, no matter what they
	 * contained on function entry. These parameters will be applied
	 * on the next call to apply_params().
	 */
	void (*set_params) (struct _wm97xx_public *tspub, wm97xx_params *params);

	/* Query driver parameters.
	 */
	void (*get_params) (struct _wm97xx_public *tspub, wm97xx_params *params);

	/* Setup hardware according to all driver parameters.
	 */
	void (*apply_params) (struct _wm97xx_public *tspub);

	/* Enable or disable continuous mode. The driver itself doesn't
	 * support continuous mode since it is highly architecture-specific.
	 */
	void (*set_continuous_mode) (struct _wm97xx_public *tspub, int enable);

	/* Set the PDEN (pen detection enable) bit to given state.
	 */
	void (*set_pden) (struct _wm97xx_public *tspub, int enable);

	/* Start a periodic auxiliary ADC conversion with given period.
	 * Note that auxiliary conversions are performed only when the
	 * pen is not down, since they are considered slowly-changing
	 * events. Period is given in jiffies. Additionally, a function can
	 * be defined to be called before and after every conversion. This
	 * can be used e.g. to route some analog signal to the respective
         * input via platform-specific GPIOs and such.
         * Use a period of 0 to disable sampling.
	 */
	void (*setup_auxconv) (struct _wm97xx_public *tspub, wm97xx_aux_conv ac,
			       int period, wm97xx_ac_prepare func);

	/* Get the result of an auxiliary ADC conversion.
	 * If an auxiliary ADC reading is not yet available, the function
	 * returns -1. The platform-specific driver must start
	 */
	int (*get_auxconv) (struct _wm97xx_public *tspub, wm97xx_aux_conv ac);

	/* Private data for custom platform-specific routines.
	 */
	void *private;

	/* A custom routine to read touchscreen samples.
	 * This can be used to implement architecture-specific
	 * sample reading routine for continuous mode.
	 */
	int (*read_sample) (struct _wm97xx_public *tspub, wm97xx_data *data);
} wm97xx_public;

wm97xx_public *wm97xx_get_device (int index);

#endif

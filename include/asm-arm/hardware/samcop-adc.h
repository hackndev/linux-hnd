#define _SAMCOP_ADC_Control             0x00000 /* resets to 0x3fc4 */
#define _SAMCOP_ADC_TouchScreenControl  0x00004 /* resets to 0x58 */
#define _SAMCOP_ADC_Delay               0x00008 /* initializes to 0xff */
#define _SAMCOP_ADC_Data0               0x0000c /* xp conversion data register */
#define _SAMCOP_ADC_Data1               0x00010 /* yp conversion data register */

#define SAMCOP_ADC_CONTROL_FORCE_START	(1 << 0) /* enables conversion start, bit cleared on start */
#define SAMCOP_ADC_CONTROL_READ_START	(1 << 1) /* enable start by read operation */
#define SAMCOP_ADC_CONTROL_STANDBY	(1 << 2)

#define SAMCOP_ADC_CONTROL_SEL_SHIFT	3
#define SAMCOP_ADC_CONTROL_SEL_AIN0	(0 << 3)
#define SAMCOP_ADC_CONTROL_SEL_AIN1	(1 << 3)
#define SAMCOP_ADC_CONTROL_SEL_AIN2	(2 << 3)
#define SAMCOP_ADC_CONTROL_SEL_AIN3	(3 << 3)
#define SAMCOP_ADC_CONTROL_SEL_YM	(4 << 3)
#define SAMCOP_ADC_CONTROL_SEL_YP	(5 << 3)
#define SAMCOP_ADC_CONTROL_SEL_XM	(6 << 3)
#define SAMCOP_ADC_CONTROL_SEL_XP	(7 << 3)

#define SAMCOP_ADC_CONTROL_PRESCALER_SHIFT	6  
#define SAMCOP_ADC_CONTROL_PRESCALER_WIDTH	8	  
#define SAMCOP_ADC_CONTROL_PRESCALER_ENABLE	(1 << 14)
#define SAMCOP_ADC_CONTROL_CONVERSION_END	(1 << 15) /* read only */ 

/* touch screen measurement mode */
#define SAMCOP_ADC_TSC_XYPST_NONE	(0 << 0)
#define SAMCOP_ADC_TSC_XYPST_XPOS	(1 << 0)
#define SAMCOP_ADC_TSC_XYPST_YPOS	(2 << 0)
#define SAMCOP_ADC_TSC_XYPST_INTERRUPT	(3 << 0)
#define SAMCOP_ADC_TSC_AUTO_MODE	(1 << 2) /* enable auto mode */
#define SAMCOP_ADC_TSC_PULL_UP		(1 << 3) /* xp pullup disable */
#define SAMCOP_ADC_TSC_XP_SEN		(1 << 4) /* xp output driver disable */
#define SAMCOP_ADC_TSC_XM_SEN		(1 << 5) /* xm output driver enable */
#define SAMCOP_ADC_TSC_YP_SEN		(1 << 6) /* yp output driver disable */
#define SAMCOP_ADC_TSC_YM_SEN		(1 << 7) /* ym output driver enable */
#define SAMCOP_ADC_TSC_UD_SEN		(1 << 8) /* pen down (0)/up (1) interrupt? */

#define SAMCOP_ADC_DATA_DATA_MASK       0x3ff

#define SAMCOP_ADC_DATA_XPDATA		((1 << 10) - 1)
#define SAMCOP_ADC_DATA_XYPST_NONE	(0 << 12)
#define SAMCOP_ADC_DATA_XYPST_XPOS	(1 << 12)
#define SAMCOP_ADC_DATA_XYPST_YPOS	(2 << 12)
#define SAMCOP_ADC_DATA_XYPST_INTERRUPT	(3 << 12)
#define SAMCOP_ADC_DATA_AUTO_MODE	(1 << 14)
#define SAMCOP_ADC_DATA_PEN_UP		(1 << 15) /* stylus up */ 

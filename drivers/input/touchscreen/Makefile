#
# Makefile for the touchscreen drivers.
#

# Each configuration option enables a list of files.

wm97xx-ts-objs := wm97xx-core.o

obj-$(CONFIG_TOUCHSCREEN_ADS7846)	+= ads7846.o
obj-$(CONFIG_TOUCHSCREEN_BITSY)		+= h3600_ts_input.o
obj-$(CONFIG_TOUCHSCREEN_CORGI)		+= corgi_ts.o
obj-$(CONFIG_TOUCHSCREEN_GUNZE)		+= gunze.o
obj-$(CONFIG_TOUCHSCREEN_TSC2101)	+= tsc2101_ts.o
obj-$(CONFIG_SA1100_JORNADA56X)		+= jornada56x_ts.o
obj-$(CONFIG_SA1100_JORNADA720)		+= jornada720_ts.o
obj-$(CONFIG_TOUCHSCREEN_ELO)		+= elo.o
obj-$(CONFIG_TOUCHSCREEN_MTOUCH)	+= mtouch.o
obj-$(CONFIG_TOUCHSCREEN_MK712)		+= mk712.o
obj-$(CONFIG_TOUCHSCREEN_HP600)		+= hp680_ts_input.o
obj-$(CONFIG_TOUCHSCREEN_WM97XX)	+= wm97xx-ts.o

wm97xx-ts-objs := wm97xx-core.o
ifeq ($(CONFIG_TOUCHSCREEN_WM97XX_PXA),y)
wm97xx-ts-objs += pxa-wm97xx.o
endif

ifeq ($(CONFIG_TOUCHSCREEN_WM9713),y)
wm97xx-ts-objs += wm9713.o
endif

ifeq ($(CONFIG_TOUCHSCREEN_WM9712),y)
wm97xx-ts-objs += wm9712.o
endif

ifeq ($(CONFIG_TOUCHSCREEN_WM9705),y)
wm97xx-ts-objs += wm9705.o
endif

obj-$(CONFIG_TOUCHSCREEN_PENMOUNT)	+= penmount.o
obj-$(CONFIG_TOUCHSCREEN_TOUCHRIGHT)	+= touchright.o
obj-$(CONFIG_TOUCHSCREEN_TOUCHWIN)	+= touchwin.o
obj-$(CONFIG_TOUCHSCREEN_UCB1400)	+= ucb1400_ts.o
obj-$(CONFIG_TOUCHSCREEN_PALMTC)	+= palmtc_ts.o


obj-$(CONFIG_TOUCHSCREEN_S3C2410) += s3c2410_ts.o
obj-$(CONFIG_TOUCHSCREEN_ASIC2) += asic2_ts.o
obj-$(CONFIG_TOUCHSCREEN_IPAQ_MICRO)	+= micro_ts.o
#obj-$(CONFIG_TOUCHSCREEN_TSC2200)	+= tsc2200_ts.o
obj-$(CONFIG_TOUCHSCREEN_ADC)		+= ts-adc.o

#
# Makefile for toshiba e-series PDA support
#

CFLAGS += -Idrivers/pcmcia

obj-$(CONFIG_ARCH_ESERIES)	        += eseries.o eseries_keypad.o eseries_pm.o e400_pm.o #eseries_battery.o
obj-$(CONFIG_E330_LCD)                  += e330_lcd.o
obj-$(CONFIG_E740_LCD)                  += e740_lcd.o e740_bl.o
obj-$(CONFIG_E750_LCD)                  += e750_lcd.o
obj-$(CONFIG_E400_LCD)                  += e400_lcd.o
obj-$(CONFIG_E800_LCD)                  += e800_lcd.o
obj-$(CONFIG_MACH_E800)	                += angelx.o
obj-$(CONFIG_E330_TC6387XB)             += e330_tc6387xb.o
obj-$(CONFIG_E740_T7L66XB)              += e740_t7l66xb.o
obj-$(CONFIG_E400_T7L66XB)              += e400_t7l66xb.o
obj-$(CONFIG_E750_E800_TC6393XB)        += e750_tc6393xb.o
obj-$(CONFIG_ESERIES_UDC)               += eseries_udc.o
obj-$(CONFIG_E740_PCMCIA)		+= e740_pcmcia.o
obj-$(CONFIG_E750_PCMCIA)		+= e750_pcmcia.o
obj-$(CONFIG_E800_PCMCIA)		+= e800_pcmcia.o


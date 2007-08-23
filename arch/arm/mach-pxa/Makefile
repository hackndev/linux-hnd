#
# Makefile for the linux kernel.
#

# Common support (must be linked before board specific support)
obj-y += clock.o generic.o irq.o dma.o time.o
obj-$(CONFIG_PXA25x) += pxa25x.o
obj-$(CONFIG_PXA27x) += pxa27x.o

# Specific board support
obj-$(CONFIG_ARCH_LUBBOCK) += lubbock.o
obj-$(CONFIG_MACH_LOGICPD_PXA270) += lpd270.o
obj-$(CONFIG_MACH_MAINSTONE) += mainstone.o
obj-$(CONFIG_ARCH_PXA_IDP) += idp.o
obj-$(CONFIG_MACH_TRIZEPS4)	+= trizeps4.o
obj-$(CONFIG_PXA_SHARP_C7xx)	+= corgi.o corgi_ssp.o corgi_lcd.o sharpsl_pm.o corgi_pm.o
obj-$(CONFIG_PXA_SHARP_Cxx00)	+= spitz.o corgi_ssp.o corgi_lcd.o sharpsl_pm.o spitz_pm.o
obj-$(CONFIG_MACH_AKITA)	+= akita-ioexp.o
obj-$(CONFIG_MACH_POODLE)	+= poodle.o corgi_ssp.o
obj-$(CONFIG_MACH_TOSA)         += tosa.o
obj-$(CONFIG_MACH_GHI270)       += ghi270.o ghi270_power.o ghi270hg_barometer.o ghi270_pbto.o
obj-$(CONFIG_ARCH_ESERIES)      += eseries/
obj-$(CONFIG_MACH_A620)         += asus620/
obj-$(CONFIG_MACH_A716)         += ssp.o asus716/
obj-$(CONFIG_MACH_A730)         += asus730/
obj-$(CONFIG_ARCH_H1900)        += ssp.o h1900/
obj-$(CONFIG_ARCH_H2200)        += h2200/
obj-$(CONFIG_MACH_H3900)        += h3900/ 
obj-$(CONFIG_MACH_H4000)        += h4000/
obj-$(CONFIG_MACH_H4700)        += ssp.o hx4700/
obj-$(CONFIG_MACH_HX2750)       += ssp.o hx2750/
obj-$(CONFIG_ARCH_H5400)        += h5400/
obj-$(CONFIG_MACH_HIMALAYA)     += htchimalaya/
obj-$(CONFIG_MACH_HTCUNIVERSAL)	+= htcuniversal/
obj-$(CONFIG_MACH_HTCALPINE)	+= htcalpine/
obj-$(CONFIG_MACH_MAGICIAN)	+= magician/
obj-$(CONFIG_MACH_HTCAPACHE)	+= htcapache/
obj-$(CONFIG_MACH_BLUEANGEL)    += htcblueangel/
obj-$(CONFIG_MACH_HTCBEETLES)	+= htcsable/
obj-$(CONFIG_MACH_HTCATHENA)	+= htcathena/
obj-$(CONFIG_MACH_HW6900)	+= htcsable/
obj-$(CONFIG_ARCH_ROVERP1)      += rover/
obj-y                           += aximx3/
obj-y                           += aximx5/
obj-y				+= aximx30/
obj-y                           += rover/
obj-$(CONFIG_MACH_RECON)	+= plat-recon/
obj-$(CONFIG_MACH_LOOXC550)	+= looxc550/
obj-$(CONFIG_MACH_X50)			+= aximx50/
obj-$(CONFIG_MACH_TUNGE2)		+= palmte2/
obj-$(CONFIG_MACH_OMAP_PALMTC)		+= ssp.o palmtc/
obj-$(CONFIG_MACH_T3XSCALE)		+= ssp.o palmtt3/
obj-$(CONFIG_MACH_XSCALE_PALMTT5)	+= palmtt5/
obj-$(CONFIG_MACH_XSCALE_PALMTX)	+= palmtx/
obj-$(CONFIG_MACH_XSCALE_PALMLD)	+= palmld/
obj-$(CONFIG_MACH_ZIRE31)		+= palmz31/
obj-$(CONFIG_MACH_PALMZ72)		+= palmz72/
obj-$(CONFIG_MACH_XSCALE_PALMTREO650)	+= palmt650/
obj-$(CONFIG_MACH_XSCALE_TREO680)	+= palmt680/
obj-$(CONFIG_MACH_T700WX)		+= palmt700wx/

# Support for blinky lights
led-y := leds.o
led-$(CONFIG_ARCH_LUBBOCK) += leds-lubbock.o
led-$(CONFIG_MACH_MAINSTONE) += leds-mainstone.o
led-$(CONFIG_ARCH_PXA_IDP) += leds-idp.o
led-$(CONFIG_MACH_TRIZEPS4) += leds-trizeps4.o

obj-$(CONFIG_LEDS) += $(led-y)

# CPU Frequency scaling
obj-$(CONFIG_CPU_FREQ_PXA) += cpu-pxa.o

# Misc features
obj-$(CONFIG_PM) += pm.o sleep.o
obj-$(CONFIG_PXA_SSP) += ssp.o

ifeq ($(CONFIG_PXA27x),y)
obj-$(CONFIG_PM) += standby.o
endif

obj-$(CONFIG_DMABOUNCE) += dma_needs_bounce.o

obj-$(CONFIG_PXA27x_VOLTAGE)            += pxa27x_voltage.o
obj-$(CONFIG_PXA27x_VOLTAGE_MAX158xx)   += pxa27x_voltage_max158xx.o

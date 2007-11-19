#
# Makefile for ALSA
#

snd-pxa2xx-i2sound-objs := pxa2xx-i2sound.o
snd-recon_audio-objs := recon_audio.o
snd-a620_audio-objs := a620_audio.o
snd-a716_audio-objs := a716_audio.o
snd-h1910_audio-objs := h1910_audio.o
snd-h2200_audio-objs := h2200_audio.o
snd-h4000_audio-objs := h4000_audio.o
snd-hx4700_audio-objs := hx4700_audio.o
snd-htcapache_audio-objs := htcapache_audio.o
snd-htcuniversal_audio-objs := htcuniversal_audio.o
snd-htcsable_audio-objs := htcsable_audio.o
snd-sa11xx-uda1341-objs := sa11xx-uda1341.o
snd-sa11x0-uda1341-objs := sa11x0-uda1341.o
snd-sa11x0-ssp-objs := sa11x0-ssp.o
snd-uda1341-objs := uda1341.o
snd-h5xxx-ak4535-objs := h5xxx-ak4535.o
snd-pxa-i2s-objs := pxa-i2s.o
snd-aaci-objs			:= aaci.o devdma.o
snd-pxa2xx-pcm-objs := pxa2xx-pcm.o
snd-pxa2xx-ac97-objs := pxa2xx-ac97.o

# Toplevel Module Dependency
obj-$(CONFIG_SND_A620)		 += snd-a620_audio.o
obj-$(CONFIG_SND_A716)		 += snd-a716_audio.o
obj-$(CONFIG_SND_H1910)		 += snd-h1910_audio.o
obj-$(CONFIG_SND_H2200)		 += snd-h2200_audio.o
obj-$(CONFIG_SND_H4000)		 += snd-h4000_audio.o
obj-$(CONFIG_SND_SA11XX_UDA1341) += snd-sa11xx-uda1341.o 
obj-$(CONFIG_SND_UDA1341)	 += snd-uda1341.o
obj-$(CONFIG_SND_SA11X0_SSP)	 += snd-sa11x0-ssp.o
obj-$(CONFIG_SND_SA11X0_UDA1341) += snd-sa11x0-uda1341.o
obj-$(CONFIG_SND_H5XXX_AK4535)   += snd-h5xxx-ak4535.o snd-pxa-i2s.o
obj-$(CONFIG_SND_HX4700)	 += snd-hx4700_audio.o
obj-$(CONFIG_SND_HTCAPACHE)	 += snd-htcapache_audio.o
obj-$(CONFIG_SND_HTCUNIVERSAL)	 += snd-htcuniversal_audio.o
obj-$(CONFIG_SND_HTCSABLE)	 += snd-htcsable_audio.o
obj-$(CONFIG_SND_ARMAACI)	+= snd-aaci.o
obj-$(CONFIG_SND_PXA2xx_I2SOUND) += snd-pxa2xx-i2sound.o
obj-$(CONFIG_SND_PXA2XX_PCM) += snd-pxa2xx-pcm.o
obj-$(CONFIG_SND_PXA2XX_AC97) += snd-pxa2xx-ac97.o
obj-$(CONFIG_SND_S3C2410)	 += s3c24xx-iis.o
obj-$(CONFIG_SND_RECON)         += snd-recon_audio.o

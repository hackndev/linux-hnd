#obj-m += acx.o

#acx-obj-y += pci.o
#acx-obj-y += usb.o

#acx-objs := wlan.o conv.o ioctl.o common.o $(acx-obj-y)

# Use this if you have proper Kconfig integration:

obj-$(CONFIG_ACX) += acx.o
obj-$(CONFIG_ACX_HX4700) += hx4700_acx.o
obj-$(CONFIG_ACX_HTCUNIVERSAL) += htcuniversal_acx.o
obj-$(CONFIG_ACX_HTCSABLE) += htcsable_acx.o
obj-$(CONFIG_ACX_RX3000) += rx3000_acx.o
#
acx-obj-$(CONFIG_ACX_PCI) += pci.o
acx-obj-$(CONFIG_ACX_USB) += usb.o
acx-obj-$(CONFIG_ACX_MEM) += mem.o
acx-obj-$(CONFIG_ACX_CS) += cs.o
#
acx-objs := wlan.o conv.o ioctl.o common.o $(acx-obj-y)

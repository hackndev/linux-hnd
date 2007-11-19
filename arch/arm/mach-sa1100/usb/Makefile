#
# Makefile for the USB client
#

usbdevcore-objs	:= buffer.o control.o strings.o usb_ctl.o

sa1100-objs := sa1100usb.o usb_recv.o usb_send.o

obj-$(CONFIG_SA1100_USB)	 += usbdevcore.o sa1100.o
obj-$(CONFIG_SA1100_USB_NETLINK) += usb-eth.o
obj-$(CONFIG_SA1100_USB_CHAR)	 += usb-char.o


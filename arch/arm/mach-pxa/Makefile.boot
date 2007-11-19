   zreladdr-y	:= 0xa0008000

ifeq ($(CONFIG_ARCH_PXA),y)
   zreladdr-$(CONFIG_MACH_X50)		:= 0xa8008000
endif

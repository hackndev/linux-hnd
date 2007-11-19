#
# Makefile for multifunction miscellaneous devices
#

obj-$(CONFIG_MFD_SM501)		+= sm501.o

obj-$(CONFIG_HTC_ASIC2)		+= asic2_base.o soc-core.o
obj-$(CONFIG_HTC_ASIC3)		+= asic3_base.o soc-core.o
obj-$(CONFIG_HTC_PASIC3)	+= pasic3_base.o

obj-$(CONFIG_HTC_EGPIO)		+= htc-egpio.o

obj-$(CONFIG_SOC_SAMCOP) 	+= samcop_base.o
obj-$(CONFIG_SOC_HAMCOP) 	+= hamcop_base.o
obj-$(CONFIG_SOC_SAMCOP_DMA)	+= samcop_dma.o

obj-$(CONFIG_SOC_MQ11XX)        += mq11xx_base.o

obj-$(CONFIG_IPAQ_MICRO) 	+=  micro.o

obj-$(CONFIG_SOC_TC6387XB)	+= tc6387xb.o soc-core.o
obj-$(CONFIG_SOC_T7L66XB)       += t7l66xb.o soc-core.o
obj-$(CONFIG_SOC_TC6393XB)      += tc6393xb.o soc-core.o

obj-$(CONFIG_SOC_TSC2101)      += tsc2101.o
obj-$(CONFIG_SOC_TSC2200)      += tsc2200.o

obj-$(CONFIG_MCP)		+= mcp-core.o
obj-$(CONFIG_MCP_SA11X0)	+= mcp-sa11x0.o
obj-$(CONFIG_MCP_UCB1200)	+= ucb1x00-core.o
obj-$(CONFIG_MCP_UCB1200_TS)	+= ucb1x00-ts.o

ifeq ($(CONFIG_SA1100_ASSABET),y)
obj-$(CONFIG_MCP_UCB1200)	+= ucb1x00-assabet.o
endif

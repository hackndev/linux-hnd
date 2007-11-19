/*
 * drivers/misc/soc/mq11xx.h
 *
 * Copyright (C) 2003 Andrew Zabolotny <anpaza@mail.ru>
 * Portions copyright (C) 2003 Keith Packard
 *
 * This file contains some definitions for the MediaQ 1100/1132.
 * These definitions are made public because drivers for
 * MediaQ subdevices need them.
 */

#ifndef _PLATFORM_MEDIAQ11XX
#define _PLATFORM_MEDIAQ11XX

#define MQ11xx_REG_SIZE		(8*1024)
#define MQ11xx_FB_SIZE		(256*1024)
#define MQ11xx_NUMIRQS		20

/* Interrupt handling for the MediaQ chip is quite tricky.
 * The chip has 20 internal interrupt sources (numbered from 0 to 19,
 * see the constants below) which are demultiplexed to a single output
 * pin which is usually connected to a single IRQ on the mainboard.
 * Thus the driver has to multiplex it somehow again into twenty
 * different IRQs.
 *
 * Unfortunately, the mechanisms required for this are missing (as of today)
 * on all platforms except ARM. Let's hope one day they will be supported on
 * all platforms; for now no platforms except ARM supports MediaQ interrupts.
 *
 * The following contants are IRQ offsets relative to the base IRQ number
 * (driver allocates a range of IRQs).
 */

#if defined CONFIG_ARM
#  define MQ_IRQ_MULTIPLEX
#endif

#define IRQ_MQ_VSYNC_RISING			0
#define IRQ_MQ_VSYNC_FALLING			1
#define IRQ_MQ_VENABLE_RISING			2
#define IRQ_MQ_VENABLE_FALLING			3
#define IRQ_MQ_BUS_CYCLE_ABORT			4
#define IRQ_MQ_GPIO_0				5
#define IRQ_MQ_GPIO_1				6
#define IRQ_MQ_GPIO_2				7
#define IRQ_MQ_COMMAND_FIFO_HALF_EMPTY    	8
#define IRQ_MQ_COMMAND_FIFO_EMPTY		9
#define IRQ_MQ_SOURCE_FIFO_HALF_EMPTY		10
#define IRQ_MQ_SOURCE_FIFO_EMPTY		11
#define IRQ_MQ_GRAPHICS_ENGINE_IDLE		12
#define IRQ_MQ_UHC_GLOBAL_SUSPEND_MODE		13
#define IRQ_MQ_UHC_REMOTE_WAKE_UP		14
#define IRQ_MQ_UHC				15
#define IRQ_MQ_UDC				16
#define IRQ_MQ_I2S				17
#define IRQ_MQ_SPI				18
#define IRQ_MQ_UDC_WAKE_UP			19

/* This union uses unnamed structs. This is a gcc extension, but what the
 * hell, the kernel is not compilable by anything else anyway...
 */
struct mediaq11xx_regs {
	/*
	 * CPU Interface (CC) Module	    0x000
	 */
	union {
		struct {
			u32 cpu_control;
#define MQ_CPU_CONTROL_MIU_READ_REQUEST_GENERATOR_ON	(1 << 0)
#define MQ_CPU_CONTROL_DISBABLE_FB_READ_CACHE		(1 << 1)
#define MQ_CPU_CONTROL_ENABLE_CLKRUN			(1 << 3)
#define MQ_CPU_CONTROL_GPIO_SOURCE_DATA_SELECT		(3 << 4)
#define MQ_CPU_CONTROL_CPU_INTERFACE_GPIO_DATA		(3 << 6)
			u32 fifo_status;
#define MQ_CPU_COMMAND_FIFO(s)				(((s) >> 0) & 0x1f)
#define MQ_CPU_SOURCE_FIFO(s)				(((s) >> 8) & 0x1f)
#define MQ_CPU_GE_BUSY					(1 << 16)
			u32 gpio_control_0;
			u32 gpio_control_1;
			u32 cpu_test_mode;
		};
		u32 a[6];
		u8 size[128];
	} CC;
	/*
	 * Memory Interface Unit Controller    0x080
	 */
	union {
		struct {
			u32	miu_0;
#define MQ_MIU_ENABLE					(1 << 0)
#define MQ_MIU_RESET_ENABLE				(1 << 1)
			u32	miu_1;
#define MQ_MIU_MEMORY_CLOCK_SOURCE_BUS			(1 << 0)
#define MQ_MIU_MEMORY_CLOCK_DIVIDER			(7 << 2)
#define MQ_MIU_MEMORY_CLOCK_DIVIDER_1			(0 << 2)
#define MQ_MIU_MEMORY_CLOCK_DIVIDER_1_5			(1 << 2)
#define MQ_MIU_MEMORY_CLOCK_DIVIDER_2			(2 << 2)
#define MQ_MIU_MEMORY_CLOCK_DIVIDER_2_5			(3 << 2)
#define MQ_MIU_MEMORY_CLOCK_DIVIDER_3			(4 << 2)
#define MQ_MIU_MEMORY_CLOCK_DIVIDER_4			(5 << 2)
#define MQ_MIU_MEMORY_CLOCK_DIVIDER_5			(6 << 2)
#define MQ_MIU_MEMORY_CLOCK_DIVIDER_6			(7 << 2)
#define MQ_MIU_DISPLAY_BURST_MASK			(3 << 5)
#define MQ_MIU_DISPLAY_BURST_COUNT_2			(0 << 5)
#define MQ_MIU_DISPLAY_BURST_COUNT_4			(1 << 5)
#define MQ_MIU_DISPLAY_BURST_COUNT_6			(2 << 5)
#define MQ_MIU_DISPLAY_BURST_COUNT_8			(3 << 5)
#define MQ_MIU_GRAPHICS_ENGINE_BURST_MASK	    	(3 << 7)
#define MQ_MIU_GRAPHICS_ENGINE_BURST_COUNT_2	    	(0 << 7)
#define MQ_MIU_GRAPHICS_ENGINE_BURST_COUNT_4	    	(1 << 7)
#define MQ_MIU_GRAPHICS_ENGINE_BURST_COUNT_6	    	(2 << 7)
#define MQ_MIU_GRAPHICS_ENGINE_BURST_COUNT_8	    	(3 << 7)
#define MQ_MIU_CPU_BURST_MASK				(3 << 9)
#define MQ_MIU_CPU_BURST_COUNT_2	    		(0 << 9)
#define MQ_MIU_CPU_BURST_COUNT_4	    		(1 << 9)
#define MQ_MIU_CPU_BURST_COUNT_6	    		(2 << 9)
#define MQ_MIU_CPU_BURST_COUNT_8	    		(3 << 9)
#define MQ_MIU_I2S_BURST_MASK				(3 << 11)
#define MQ_MIU_I2S_BURST_COUNT_2    			(0 << 11)
#define MQ_MIU_I2S_BURST_COUNT_4    			(1 << 11)
#define MQ_MIU_I2S_BURST_COUNT_6    			(2 << 11)
#define MQ_MIU_I2S_BURST_COUNT_8    			(3 << 11)
#define MQ_MIU_UDC_BURST_MASK				(3 << 13)
#define MQ_MIU_UDC_BURST_COUNT_2    			(0 << 13)
#define MQ_MIU_UDC_BURST_COUNT_4    			(1 << 13)
#define MQ_MIU_UDC_BURST_COUNT_6    			(2 << 13)
#define MQ_MIU_UDC_BURST_COUNT_8    			(3 << 13)
#define MQ_MIU_GCI_FIFO_THRESHOLD			(0xf << 16)
#define MQ_MIU_GCI_FIFO_THRESHOLD_(v)			((v) << 16)
#define MQ_MIU_GRAPHICS_ENGINE_SRC_READ_THRESHOLD	(7 << 20)
#define MQ_MIU_GRAPHICS_ENGINE_SRC_READ_THRESHOLD_(v)	((v) << 20)
#define MQ_MIU_GRAPHICS_ENGINE_DST_READ_THRESHOLD	(7 << 23)
#define MQ_MIU_GRAPHICS_ENGINE_DST_READ_THRESHOLD_(v)	((v) << 23)
#define MQ_MIU_I2S_TRANSMIT_THRESHOLD			(7 << 26)
#define MQ_MIU_I2S_TRANSMIT_THRESHOLD_(v)    		((v) << 26)
			u32	miu_test_control;
			u32	mq1178_unknown1;
			u32	mq1178_unknown2;
		};
		u32 a[8];
		u8 size[128];
	} MIU;
	/*
	 * Interrupt Controller	    0x100
	 */
	union {
		struct {
			u32 control;
#define MQ_INTERRUPT_CONTROL_INTERRUPT_ENABLE		(1 << 0)
#define MQ_INTERRUPT_CONTROL_INTERRUPT_POLARITY		(1 << 1)
#define MQ_INTERRUPT_CONTROL_GPIO_0_INTERRUPT_POLARITY	(1 << 2)
#define MQ_INTERRUPT_CONTROL_GPIO_1_INTERRUPT_POLARITY	(1 << 3)
#define MQ_INTERRUPT_CONTROL_GPIO_2_INTERRUPT_POLARITY	(1 << 4)
			u32 interrupt_mask;
#define MQ_INTERRUPT_MASK_VSYNC_RISING			(1 << 0)
#define MQ_INTERRUPT_MASK_VSYNC_FALLING			(1 << 1)
#define MQ_INTERRUPT_MASK_VENABLE_RISING		(1 << 2)
#define MQ_INTERRUPT_MASK_VENABLE_FALLING		(1 << 3)
#define MQ_INTERRUPT_MASK_BUS_CYCLE_ABORT		(1 << 4)
#define MQ_INTERRUPT_MASK_GPIO_0			(1 << 5)
#define MQ_INTERRUPT_MASK_GPIO_1			(1 << 6)
#define MQ_INTERRUPT_MASK_GPIO_2			(1 << 7)
#define MQ_INTERRUPT_MASK_COMMAND_FIFO_HALF_EMPTY    	(1 << 8)
#define MQ_INTERRUPT_MASK_COMMAND_FIFO_EMPTY		(1 << 9)
#define MQ_INTERRUPT_MASK_SOURCE_FIFO_HALF_EMPTY	(1 << 10)
#define MQ_INTERRUPT_MASK_SOURCE_FIFO_EMPTY		(1 << 11)
#define MQ_INTERRUPT_MASK_GRAPHICS_ENGINE_IDLE		(1 << 12)
#define MQ_INTERRUPT_MASK_UHC_GLOBAL_SUSPEND_MODE	(1 << 13)
#define MQ_INTERRUPT_MASK_UHC_REMOTE_WAKE_UP    	(1 << 14)
#define MQ_INTERRUPT_MASK_UHC				(1 << 15)
#define MQ_INTERRUPT_MASK_UDC				(1 << 16)
#define MQ_INTERRUPT_MASK_I2S				(1 << 17)
#define MQ_INTERRUPT_MASK_SPI				(1 << 18)
#define MQ_INTERRUPT_MASK_UDC_WAKE_UP			(1 << 19)
			u32 interrupt_status;
#define MQ_INTERRUPT_STATUS_VSYNC_RISING		(1 << 0)
#define MQ_INTERRUPT_STATUS_VSYNC_FALLING		(1 << 1)
#define MQ_INTERRUPT_STATUS_VENABLE_RISING		(1 << 2)
#define MQ_INTERRUPT_STATUS_VENABLE_FALLING		(1 << 3)
#define MQ_INTERRUPT_STATUS_BUS_CYCLE_ABORT		(1 << 4)
#define MQ_INTERRUPT_STATUS_GPIO_0			(1 << 5)
#define MQ_INTERRUPT_STATUS_GPIO_1			(1 << 6)
#define MQ_INTERRUPT_STATUS_GPIO_2			(1 << 7)
#define MQ_INTERRUPT_STATUS_COMMAND_FIFO_HALF_EMPTY    	(1 << 8)
#define MQ_INTERRUPT_STATUS_COMMAND_FIFO_EMPTY		(1 << 9)
#define MQ_INTERRUPT_STATUS_SOURCE_FIFO_HALF_EMPTY	(1 << 10)
#define MQ_INTERRUPT_STATUS_SOURCE_FIFO_EMPTY		(1 << 11)
#define MQ_INTERRUPT_STATUS_GRAPHICS_ENGINE_IDLE	(1 << 12)
#define MQ_INTERRUPT_STATUS_UHC_GLOBAL_SUSPEND_MODE	(1 << 13)
#define MQ_INTERRUPT_STATUS_UHC_REMOTE_WAKE_UP    	(1 << 14)
#define MQ_INTERRUPT_STATUS_UHC				(1 << 15)
#define MQ_INTERRUPT_STATUS_UDC				(1 << 16)
#define MQ_INTERRUPT_STATUS_I2S				(1 << 17)
#define MQ_INTERRUPT_STATUS_SPI				(1 << 18)
#define MQ_INTERRUPT_STATUS_UDC_WAKE_UP			(1 << 19)
			u32 interrupt_raw_status;
#define MQ_INTERRUPT_RAWSTATUS_VSYNC			(1 << 0)
#define MQ_INTERRUPT_RAWSTATUS_VENABLE			(1 << 1)
#define MQ_INTERRUPT_RAWSTATUS_SCC			(1 << 2)
#define MQ_INTERRUPT_RAWSTATUS_SPI			(1 << 3)
#define MQ_INTERRUPT_RAWSTATUS_GPIO_0			(1 << 4)
#define MQ_INTERRUPT_RAWSTATUS_GPIO_1			(1 << 5)
#define MQ_INTERRUPT_RAWSTATUS_GPIO_2			(1 << 6)
#define MQ_INTERRUPT_RAWSTATUS_GRAPHICS_ENGINE_BUSY	(1 << 8)
#define MQ_INTERRUPT_RAWSTATUS_SOURCE_FIFO_EMPTY	(1 << 9)
#define MQ_INTERRUPT_RAWSTATUS_SOURCE_FIFO_HALF_EMPTY	(1 << 10)
#define MQ_INTERRUPT_RAWSTATUS_COMMAND_FIFO_EMPTY	(1 << 11)
#define MQ_INTERRUPT_RAWSTATUS_COMMAND_FIFO_HALF_EMPTY	(1 << 12)
#define MQ_INTERRUPT_RAWSTATUS_UHC			(1 << 13)
#define MQ_INTERRUPT_RAWSTATUS_UHC_GLOBAL_SUSPEND	(1 << 14)
#define MQ_INTERRUPT_RAWSTATUS_UHC_REMOTE_WAKE_UP	(1 << 15)
#define MQ_INTERRUPT_RAWSTATUS_UDC			(1 << 16)
#define MQ_INTERRUPT_RAWSTATUS_UDC_WAKE_UP		(1 << 17)
		};
		u32 a[4];
		u8 size[128];
	} IC;
	/*
	 * Graphics Controller	    0x180
	 */
	union {
		struct {
			u32 control;
#define MQ_GC_CONTROL_ENABLE				(1 << 0)
#define MQ_GC_HORIZONTAL_COUNTER_RESET			(1 << 1)
#define MQ_GC_VERTICAL_COUNTER_RESET			(1 << 2)
#define MQ_GC_IMAGE_WINDOW_ENABLE			(1 << 3)
#define MQ_GC_DEPTH					(0xf << 4)
#define MQ_GC_DEPTH_PSEUDO_1				(0x0 << 4)
#define MQ_GC_DEPTH_PSEUDO_2				(0x1 << 4)
#define MQ_GC_DEPTH_PSEUDO_4				(0x2 << 4)
#define MQ_GC_DEPTH_PSEUDO_8				(0x3 << 4)
#define MQ_GC_DEPTH_GRAY_1				(0x8 << 4)
#define MQ_GC_DEPTH_GRAY_2				(0x9 << 4)
#define MQ_GC_DEPTH_GRAY_4				(0xa << 4)
#define MQ_GC_DEPTH_GRAY_8				(0xb << 4)
#define MQ_GC_DEPTH_TRUE_16				(0xc << 4)
#define MQ_GC_HARDWARE_CURSOR_ENABLE			(1 << 8)
#define MQ_GC_DOUBLE_BUFFER_CONTROL			(3 << 10)
#define MQ_GC_X_SCANNING_DIRECTION			(1 << 12)
#define MQ_GC_LINE_SCANNING_DIRECTION			(1 << 13)
#define MQ_GC_HORIZONTAL_DOUBLING			(1 << 14)
#define MQ_GC_VERTICAL_DOUBLING				(1 << 15)
#define MQ_GC_GRCLK_SOURCE				(3 << 16)
#define MQ_GC_GRCLK_SOURCE_BUS				(0 << 16)
#define MQ_GC_GRCLK_SOURCE_FIRST    			(1 << 16)
#define MQ_GC_GRCLK_SOURCE_SECON    			(2 << 16)
#define MQ_GC_GRCLK_SOURCE_THIRD    			(3 << 16)
#define MQ_GC_ENABLE_TEST_MODE				(1 << 18)
#define MQ_GC_ENABLE_POLY_SI_TFT			(1 << 19)
#define MQ_GC_GMCLK_FIRST_DIVISOR			(7 << 20)
#define MQ_GC_GMCLK_FIRST_DIVISOR_1			(0 << 20)
#define MQ_GC_GMCLK_FIRST_DIVISOR_1_5			(1 << 20)
#define MQ_GC_GMCLK_FIRST_DIVISOR_2_5			(2 << 20)
#define MQ_GC_GMCLK_FIRST_DIVISOR_3_5			(3 << 20)
#define MQ_GC_GMCLK_FIRST_DIVISOR_4_5			(4 << 20)
#define MQ_GC_GMCLK_FIRST_DIVISOR_5_5			(5 << 20)
#define MQ_GC_GMCLK_FIRST_DIVISOR_6_5			(6 << 20)
#define MQ_GC_GMCLK_SECOND_DIVISOR			(0xf << 24)
#define MQ_GC_GMCLK_SECOND_DIVISOR_(v)			((v) << 24)
#define MQ_GC_SHARP_160x160_HR_TFT_ENABLE    		(1 << 31)
			u32 power_sequencing;
#define MQ_GC_POWER_UP_INTERVAL				(7 << 0)
#define MQ_GC_POWER_UP_INTERVAL_1	    		(0 << 0)
#define MQ_GC_POWER_UP_INTERVAL_2	    		(1 << 0)
#define MQ_GC_POWER_UP_INTERVAL_4	    		(2 << 0)
#define MQ_GC_POWER_UP_INTERVAL_8	    		(3 << 0)
#define MQ_GC_POWER_UP_INTERVAL_16    			(4 << 0)
#define MQ_GC_POWER_UP_INTERVAL_32    			(5 << 0)
#define MQ_GC_POWER_UP_INTERVAL_48    			(6 << 0)
#define MQ_GC_POWER_UP_INTERVAL_64    			(7 << 0)
#define MQ_GC_FAST_POWER_UP				(1 << 3)
#define MQ_GC_POWER_DOWN_INTERVAL			(1 << 4)
#define MQ_GC_POWER_DOWN_INTERVAL_1	    		(0 << 4)
#define MQ_GC_POWER_DOWN_INTERVAL_2	    		(1 << 4)
#define MQ_GC_POWER_DOWN_INTERVAL_4	    		(2 << 4)
#define MQ_GC_POWER_DOWN_INTERVAL_8	    		(3 << 4)
#define MQ_GC_POWER_DOWN_INTERVAL_16    		(4 << 4)
#define MQ_GC_POWER_DOWN_INTERVAL_32    		(5 << 4)
#define MQ_GC_POWER_DOWN_INTERVAL_48    		(6 << 4)
#define MQ_GC_POWER_DOWN_INTERVAL_64    		(7 << 4)
#define MQ_GC_FAST_POWER_DOWN				(1 << 7)
			u32 horizontal_display;
#define MQ_GC_HORIZONTAL_DISPLAY_TOTAL			(0x7ff << 0)
#define MQ_GC_HORIZONTAL_DISPLAY_TOTAL_(s)    		((s) << 0)
#define MQ_GC_HORIZONTAL_DISPLAY_END			(0x7ff << 16)
#define MQ_GC_HORIZONTAL_DISPLAY_END_(s)    		((s) << 16)
			u32 vertical_display;
#define MQ_GC_VERTICAL_DISPLAY_TOTAL			(0x3ff << 0)
#define MQ_GC_VERTICAL_DISPLAY_TOTAL_(s)		((s) << 0)
#define MQ_GC_VERTICAL_DISPLAY_END			(0x3ff << 16)
#define MQ_GC_VERTICAL_DISPLAY_END_(s)			((s) << 16)
			u32 horizontal_sync;
#define MQ_GC_HORIZONTAL_SYNC_START			(0x7ff << 0)
#define MQ_GC_HORIZONTAL_SYNC_START_(s)			((s) << 0)
#define MQ_GC_HORIZONTAL_SYNC_END			(0x7ff << 16)
#define MQ_GC_HORIZONTAL_SYNC_END_(s)			((s) << 16)
			u32 vertical_sync;
#define MQ_GC_VERTICAL_SYNC_START			(0x3ff << 0)
#define MQ_GC_VERTICAL_SYNC_START_(s)			((s) << 0)
#define MQ_GC_VERTICAL_SYNC_END				(0x3ff << 16)
#define MQ_GC_VERTICAL_SYNC_END_(s)			((s) << 16)
			u32 horizontal_counter_init;	/* set to 0 */
			u32 vertical_counter_init;	/* set to 0 */
			u32 horizontal_window;
#define MQ_GC_HORIZONTAL_WINDOW_START			(0x7ff << 0)
#define MQ_GC_HORIZONTAL_WINDOW_START_(s)		((s) << 0)
#define MQ_GC_HORIZONTAL_WINDOW_WIDTH			(0x7ff << 16)
#define MQ_GC_HORIZONTAL_WINDOW_WIDTH_(s)		((s) << 16)
			u32 vertical_window;
#define MQ_GC_VERTICAL_WINDOW_START			(0x3ff << 0)
#define MQ_GC_VERTICAL_WINDOW_START_(s)			((s) << 0)
#define MQ_GC_VERTICAL_WINDOW_HEIGHT			(0x3ff << 16)
#define MQ_GC_VERTICAL_WINDOW_HEIGHT_(s)		((s) << 16)
			u32 reserved_28;
			u32 line_clock;
#define MQ_GC_LINE_CLOCK_START				(0x7ff << 0)
#define MQ_GC_LINE_CLOCK_START_(s)			((s) << 0)
#define MQ_GC_LINE_CLOCK_END				(0x7ff << 16)
#define MQ_GC_LINE_CLOCK_END_(s)			((s) << 16)
			u32 window_start_address;
			u32 alternate_window_start_address;
			u32 window_stride;
			u32 reserved_3c;
			u32 cursor_position;
#define MQ_GC_HORIZONTAL_CURSOR_START			(0x7ff << 0)
#define MQ_GC_HORIZONTAL_CURSOR_START_(s)		((s) << 0)
#define MQ_GC_VERTICAL_CURSOR_START			(0x3ff << 16)
#define MQ_GC_VERTICAL_CURSOR_START_(s)			((s) << 16)
			u32 cursor_start_address;
#define MQ_GC_CURSOR_START_ADDRESS			(0xff << 0)
#define MQ_GC_CURSOR_START_ADDRESS_(s)			((s) << 0)
#define MQ_GC_HORIZONTAL_CURSOR_OFFSET			(0x3f << 16)
#define MQ_GC_HORIZONTAL_CURSOR_OFFSET_(s)		((s) << 16)
#define MQ_GC_VERTICAL_CURSOR_OFFSET			(0x3f << 24)
#define MQ_GC_VERTICAL_CURSOR_OFFSET_(s)		((s) << 24)
			u32 cursor_foreground;
			u32 cursor_background;
			u32 reserved_50_64[6];
			u32 frame_clock_control;
#define MQ_GC_FRAME_CLOCK_START				(0x3ff << 0)
#define MQ_GC_FRAME_CLOCK_START_(s)	    		((s) << 0)
#define MQ_GC_FRAME_CLOCK_END				(0x3ff << 16)
#define MQ_GC_FRAME_CLOCK_END_(s)	    		((s) << 16)
			u32 misc_signals;
			u32 horizonal_parameter;
			u32 vertical_parameter;
			u32 window_line_start_address;
			u32 cursor_line_start_address;
		};
		u32 a[0x20];
		u8 size[128];
	} GC;
	/*
	 * Graphics Engine			0x200
	 */
	union {
		u32 a[0x14];
		u8 size[128];
	} GE;
	/*
	 * Synchronous Serial Controller	0x280
	 */
	union {
		u32 a[16];
		u8 size[128];
	} SSC;
	/*
	 * Serial Peripheral Interface		0x300
	 */
	union {
		struct {
			u32 control;
			u32 status;
			u32 data;
			u32 time_gap;
			u32 gpio_mode;
			u32 count;
			u32 fifo_threshold;
			u32 led_control;
			u32 blue_gpio_mode;
		};
		u32 a[9];
		u8 size[128];
	} SPI;
	/*
	 * Device Configuration Space		0x380
	 */
	union {
		struct {
			u32 config_0;
#define MQ_CONFIG_LITTLE_ENDIAN_ENABLE			(1 << 0)
#define MQ_CONFIG_BYTE_SWAPPING				(1 << 1)
			u32 config_1;
#define MQ_CONFIG_18_OSCILLATOR				(3 << 0)
#define MQ_CONFIG_18_OSCILLATOR_DISABLED    		(0 << 0)
#define MQ_CONFIG_18_OSCILLATOR_OSCFO			(1 << 0)
#define MQ_CONFIG_18_OSCILLATOR_INTERNAL    		(3 << 0)
#define MQ_CONFIG_CPU_CLOCK_DIVISOR			(1 << 8)
#define MQ_CONFIG_DTACK_CONTROL				(1 << 9)
#define MQ_CONFIG_INTERFACE_SYNCHRONIZER_CONTROL    	(1 << 10)
#define MQ_CONFIG_WRITE_DATA_LATCH    			(1 << 11)
#define MQ_CONFIG_CPU_TEST_MODE				(1 << 12)
#define MQ_CONFIG_SOFTWARE_CHIP_RESET			(1 << 16)
#define MQ_CONFIG_WEAK_PULL_DOWN_FMOD			(1 << 28)
#define MQ_CONFIG_WEAK_PULL_DOWN_FLCLK			(1 << 29)
#define MQ_CONFIG_WEAK_PULL_DOWN_PWM0			(1 << 30)
#define MQ_CONFIG_WEAK_PULL_DOWN_PWM1			(1 << 31)
			u32 config_2;
#define MQ_CONFIG_CC_MODULE_ENABLE			(1 << 0)
			u32 config_3;
#define MQ_CONFIG_BUS_INTERFACE_MODE			(0x7f << 0)
#define MQ_CONFIG_BUS_INTERFACE_MODE_SH7750    		(0x01 << 0)
#define MQ_CONFIG_BUS_INTERFACE_MODE_SH7709    		(0x02 << 0)
#define MQ_CONFIG_BUS_INTERFACE_MODE_VR4111    		(0x04 << 0)
#define MQ_CONFIG_BUS_INTERFACE_MODE_SA1110    		(0x08 << 0)
#define MQ_CONFIG_BUS_INTERFACE_MODE_PCI    		(0x20 << 0)
#define MQ_CONFIG_BUS_INTERFACE_MODE_DRAGONBALL_EZ    	(0x40 << 0)
			u32 config_4;
#define MQ_CONFIG_GE_FORCE_BUSY_GLOBAL			(1 << 0)
#define MQ_CONFIG_GE_FORCE_BUSY_LOCAL			(1 << 1)
#define MQ_CONFIG_GE_CLOCK_SELECT    			(1 << 2)
#define MQ_CONFIG_GE_CLOCK_DIVIDER    			(7 << 3)
#define MQ_CONFIG_GE_CLOCK_DIVIDER_1			(0 << 3)
#define MQ_CONFIG_GE_CLOCK_DIVIDER_1_5			(1 << 3)
#define MQ_CONFIG_GE_CLOCK_DIVIDER_2			(2 << 3)
#define MQ_CONFIG_GE_CLOCK_DIVIDER_2_5			(3 << 3)
#define MQ_CONFIG_GE_CLOCK_DIVIDER_3			(4 << 3)
#define MQ_CONFIG_GE_CLOCK_DIVIDER_4			(5 << 3)
#define MQ_CONFIG_GE_CLOCK_DIVIDER_5			(6 << 3)
#define MQ_CONFIG_GE_CLOCK_DIVIDER_6			(7 << 3)
#define MQ_CONFIG_GE_FIFO_RESET				(1 << 6)
#define MQ_CONFIG_GE_SOURCE_FIFO_RESET			(1 << 7)
#define MQ_CONFIG_USB_SE0_DETECT    			(1 << 8)
#define MQ_CONFIG_UHC_DYNAMIC_POWER 	   		(1 << 9)
#define MQ_CONFIG_USB_COUNTER_SCALE_ENABLE    		(1 << 10)
#define MQ_CONFIG_UHC_READ_TEST_ENABLE    		(1 << 11)
#define MQ_CONFIG_UHC_TEST_MODE_DATA    		(0xf << 12)
#define MQ_CONFIG_UHC_TRANSCEIVER_TEST_ENABLE		(1 << 16)
#define MQ_CONFIG_UHC_OVER_CURRENT_DETECT  	  	(1 << 17)
#define MQ_CONFIG_UDC_DYNAMIC_POWER_ENABLE    		(1 << 18)
#define MQ_CONFIG_UHC_TRANSCEIVER_ENABLE    		(1 << 19)
#define MQ_CONFIG_UDC_TRANSCEIVER_ENABLE		(1 << 20)
#define MQ_CONFIG_USB_TEST_VECTOR_GENERATION_ENABLE    	(1 << 21)
			u32 config_5;
#define MQ_CONFIG_GE_ENABLE				(1 << 0)
#define MQ_CONFIG_UHC_CLOCK_ENABLE			(1 << 1)
#define MQ_CONFIG_UDC_CLOCK_ENABLE   	 		(1 << 2)
			u32 config_6;
			u32 config_7;
			u32 config_8;
		};
		u32 a[9];
		u8 size[128];
	} DC;
	/*
	 * PCI Configuration Header
	 */
	union {
		/* little endian */
		struct {
			u16 vendor_id;
#define MQ_PCI_VENDORID					0x4d51
			u16 device_id;
// This is a wild guess
#define MQ_PCI_DEVICEID_1100				0x0100
#define MQ_PCI_DEVICEID_1132				0x0120
// No confirmation of this IDs yet
#define MQ_PCI_DEVICEID_1168				0x0168
// No confirmation of this IDs yet
#define MQ_PCI_DEVICEID_1178				0x0178
#define MQ_PCI_DEVICEID_1188				0x0188
			u16 command;
			u16 status;
			u32 revision_class;
			u8 cache_line_size;
			u8 latency_timer;
			u8 header_type;
			u8 BIST;
			u32 bar0;
			u32 bar1;
			u32 pad0[4];
			u32 cardbus_cis;
			u16 subsystem_vendor_id;
			u16 subsystem_id;
			u32 expansion_rom;
			u8 cap_ptr;
			u8 pad1[3];
			u8 interrupt_line;
			u8 interrupt_pin;
			u8 min_gnt;
			u8 max_lat;
			u8 capability_id;
			u8 next_item_ptr;
			u16 power_management_capabilities;
			u16 power_management_control_status;
			u8 PMCSR_BSE_bridge;
			u8 data;
		};
		u8 size[256];
	} PCI;
	/*
	 * USB Host Controller (0x500)
	 */
	union {
		u32 a[23];
		u8 size[256];
	} UHC;
	/*
	 * Flat Panel Controller (0x600)
	 */
	union {
		struct {
			u32 control;
#define MQ_FP_PANEL_TYPE				(0xf << 0)
#define MQ_FP_PANEL_TYPE_TFT				(0x0 << 0)
#define MQ_FP_PANEL_TYPE_STN				(0x4 << 0)
#define MQ_FP_MONOCHROME_SELECT				(1 << 4)
#define MQ_FP_FLAT_PANEL_INTERFACE    			(7 << 5)
#define MQ_FP_FLAT_PANEL_INTERFACE_4_BIT    		(0 << 5)
#define MQ_FP_FLAT_PANEL_INTERFACE_6_BIT    		(1 << 5)
#define MQ_FP_FLAT_PANEL_INTERFACE_8_BIT    		(2 << 5)
#define MQ_FP_FLAT_PANEL_INTERFACE_16_BIT    		(3 << 5)
#define MQ_FP_DITHER_PATTERN				(3 << 8)
#define MQ_FP_DITHER_PATTERN_1				(1 << 8)
#define MQ_FP_DITHER_BASE_COLOR				(7 << 12)
#define MQ_FP_DITHER_BASE_COLOR_DISABLE			(7 << 12)
#define MQ_FP_DITHER_BASE_COLOR_3_BITS			(3 << 12)
#define MQ_FP_DITHER_BASE_COLOR_4_BITS			(4 << 12)
#define MQ_FP_DITHER_BASE_COLOR_5_BITS			(5 << 12)
#define MQ_FP_DITHER_BASE_COLOR_6_BITS			(6 << 12)
#define MQ_FP_ALTERNATE_WINDOW_CONTROL			(1 << 15)
#define MQ_FP_FRC_CONTROL				(3 << 16)
#define MQ_FP_FRC_CONTROL_2_LEVEL    			(0 << 16)
#define MQ_FP_FRC_CONTROL_4_LEVEL    			(1 << 16)
#define MQ_FP_FRC_CONTROL_8_LEVEL    			(2 << 16)
#define MQ_FP_FRC_CONTROL_16_LEVEL    			(3 << 16)
#define MQ_FP_POLY_SI_TFT_ENABLE    			(1 << 19)
#define MQ_FP_POLY_SI_TFT_FIRST_LINE			(1 << 20)
#define MQ_FP_POLY_SI_TFT_DISPLAY_DATA_CONTROL		(1 << 21)
#define MQ_FP_APP_NOTE_SAYS_SET_THIS			(1 << 22)
			u32 pin_control_1;
#define MQ_FP_DISABLE_FLAT_PANEL_PINS			(1 << 0)
#define MQ_FP_DISPLAY_ENABLE				(1 << 2)
#define MQ_FP_AC_MODULATION_ENABLE    			(1 << 3)
#define MQ_FP_PWM_CLOCK_ENABLE				(1 << 5)
#define MQ_FP_TFT_SHIFT_CLOCK_SELECT			(1 << 6)
#define MQ_FP_SHIFT_CLOCK_MASK				(1 << 7)
#define MQ_FP_FHSYNC_CONTROL				(1 << 8)
#define MQ_FP_STN_SHIFT_CLOCK_CONTROL			(1 << 9)
#define MQ_FP_STN_EXTRA_LP_ENABLE    			(1 << 10)
#define MQ_FP_TFT_DISPLAY_ENABLE_SELECT			(3 << 12)
#define MQ_FP_TFT_DISPLAY_ENABLE_SELECT_00    		(0 << 12)
#define MQ_FP_TFT_DISPLAY_ENABLE_SELECT_01    		(1 << 12)
#define MQ_FP_TFT_DISPLAY_ENABLE_SELECT_10    		(2 << 12)
#define MQ_FP_TFT_DISPLAY_ENABLE_SELECT_11    		(3 << 12)
#define MQ_FP_TFT_HORIZONTAL_SYNC_SELECT    		(3 << 14)
#define MQ_FP_TFT_HORIZONTAL_SYNC_SELECT_00    		(0 << 14)
#define MQ_FP_TFT_HORIZONTAL_SYNC_SELECT_01    		(1 << 14)
#define MQ_FP_TFT_HORIZONTAL_SYNC_SELECT_10    		(2 << 14)
#define MQ_FP_TFT_HORIZONTAL_SYNC_SELECT_11    		(3 << 14)
#define MQ_FP_TFT_VERTICAL_SYNC_SELECT    		(3 << 16)
#define MQ_FP_TFT_VERTICAL_SYNC_SELECT_00    		(0 << 16)
#define MQ_FP_TFT_VERTICAL_SYNC_SELECT_01    		(1 << 16)
#define MQ_FP_TFT_VERTICAL_SYNC_SELECT_10    		(2 << 16)
#define MQ_FP_TFT_VERTICAL_SYNC_SELECT_11    		(3 << 16)
#define MQ_FP_LINE_CLOCK_CONTROL    			(1 << 18)
#define MQ_FP_ALTERNATE_LINE_CLOCK_CONTROL    		(1 << 19)
#define MQ_FP_FMOD_CLOCK_CONTROL    			(1 << 20)
#define MQ_FP_FMOD_FRAME_INVERSION    			(1 << 21)
#define MQ_FP_FMOD_FREQUENCY_CONTROL 			(1 << 22)
#define MQ_FP_FMOD_SYNCHRONOUS_RESET  			(1 << 23)
#define MQ_FP_SHIFT_CLOCK_DELAY   			(7 << 24)
#define MQ_FP_EXTENDED_LINE_CLOCK_CONTROL    		(1 << 27)
			u32 output_control;
			u32 input_control;
#define MQ_FP_ENVDD					(1 << 0)
#define MQ_FP_ENVEE					(1 << 1)
#define MQ_FP_FD2					(1 << 2)
#define MQ_FP_FD3					(1 << 3)
#define MQ_FP_FD4					(1 << 4)
#define MQ_FP_FD5					(1 << 5)
#define MQ_FP_FD6					(1 << 6)
#define MQ_FP_FD7					(1 << 7)
#define MQ_FP_FD10					(1 << 10)
#define MQ_FP_FD11					(1 << 11)
#define MQ_FP_FD12					(1 << 12)
#define MQ_FP_FD13					(1 << 13)
#define MQ_FP_FD14					(1 << 14)
#define MQ_FP_FD15					(1 << 15)
#define MQ_FP_FD18					(1 << 18)
#define MQ_FP_FD19					(1 << 19)
#define MQ_FP_FD20					(1 << 20)
#define MQ_FP_FD21					(1 << 21)
#define MQ_FP_FD22					(1 << 22)
#define MQ_FP_FD23					(1 << 23)
#define MQ_FP_FSCLK					(1 << 24)
#define MQ_FP_FDE					(1 << 25)
#define MQ_FP_FHSYNC					(1 << 26)
#define MQ_FP_FVSYNC					(1 << 27)
#define MQ_FP_FMOD					(1 << 28)
#define MQ_FP_FLCLK					(1 << 29)
#define MQ_FP_PWM0					(1 << 30)
#define MQ_FP_PWM1					(1 << 31)
			u32 stn_panel_control;
			u32 polarity_control;
			u32 pin_output_select_0;
			u32 pin_output_select_1;
			u32 pin_output_data;
			u32 pin_input_data;
			u32 pin_weak_pull_down;
			u32 additional_pin_output_select;
			u32 dummy1;
			u32 dummy2;
			u32 test_control;
			u32 pulse_width_mod_control;
			u32 frc_pattern[32];
			u32 frc_weight;
			u32 pwm_clock_selector_c8;
			u32 pwm_clock_selector_cc;
			u32 pwm_clock_selector_d0;
			u32 pwm_clock_selector_d4;
			u32 frc_weight_d8;
			u32 frc_weight_dc;
		};
		u32 a[0x80];
		u8 size[512];
	} FP;
	/*
	 * Color Palette (0x800)
	 */
	union {
		struct {
			u32 palette[256];
		};
		u32 a[256];
		u8 size[1024];
	} CMAP;
	/*
	 * Source FIFO Space (0xc00)
	 */
	union {
		u32 a[256];
		u8 size[1024];
	} FIFO;
	/*
	 * USB Device Controller (0x1000)
	 */
	union {
		struct {
			u32 control;
#define MQ_UDC_SUSPEND_ENABLE				(1 << 0)
#define MQ_UDC_REMOTEHOST_WAKEUP_ENABLE			(1 << 3)
#define MQ_UDC_EP2_ISOCHRONOUS				(1 << 5)
#define MQ_UDC_EP3_ISOCHRONOUS				(1 << 6)
#define MQ_UDC_WAKEUP_USBHOST				(1 << 8)
#define MQ_UDC_IEN_SOF					(1 << 16)
#define MQ_UDC_IEN_EP0_TX				(1 << 17)
#define MQ_UDC_IEN_EP0_RX				(1 << 18)
#define MQ_UDC_IEN_EP1_TX				(1 << 19)
#define MQ_UDC_IEN_EP2_TX_EOT				(1 << 20)
#define MQ_UDC_IEN_EP3_RX_EOT				(1 << 21)
#define MQ_UDC_IEN_DMA_TX				(1 << 22)
#define MQ_UDC_IEN_DMA_RX				(1 << 23)
#define MQ_UDC_IEN_DMA_RX_EOT				(1 << 24)
#define MQ_UDC_IEN_GLOBAL_SUSPEND			(1 << 25)
#define MQ_UDC_IEN_WAKEUP				(1 << 26)
#define MQ_UDC_IEN_RESET				(1 << 27)
			u32 address;
#define MQ_UDC_ADDRESS_MASK				(127)
			u32 frame_number;
#define MQ_UDC_FRAME_MASK				(2047)
#define MQ_UDC_FRAME_CORRUPTED				(1 << 11)
#define MQ_UDC_FRAME_NEW				(1 << 12)
#define MQ_UDC_FRAME_MISSING				(1 << 13)
#define MQ_UDC_EP2_DATA_MISSING				(1 << 14)
#define MQ_UDC_EP3_DATA_MISSING				(1 << 15)
			u32 ep0txcontrol;
#define MQ_UDC_TX_EDT					(1 << 1)
#define MQ_UDC_STALL					(1 << 2)
#define MQ_UDC_TX_PID_DATA1				(1 << 3)
#define MQ_UDC_TX_LAST_ENABLE_SHIFT			(4)
#define MQ_UDC_TX_LAST_ENABLE_MASK			(15 << MQ_UDC_TX_LAST_ENABLE_SHIFT)
#define MQ_UDC_TX_LAST_ENABLE(x)  			(((1 << (1 + (((x) - 1) & 3))) - 1) << MQ_UDC_TX_LAST_ENABLE_SHIFT)
#define MQ_UDC_CLEAR_FIFO				(1 << 8)
			u32 ep0txstatus;
#define MQ_UDC_ACK					(1 << 0)
#define MQ_UDC_ERR					(1 << 1)
#define MQ_UDC_TIMEOUT					(1 << 2)
#define MQ_UDC_EOT					(1 << 3)
#define MQ_UDC_FIFO_OVERRUN				(1 << 6)
#define MQ_UDC_NAK					(1 << 7)
#define MQ_UDC_FIFO_SHIFT				(9)
#define MQ_UDC_FIFO_MASK				(7)
#define MQ_UDC_FIFO(x)					(((x) >> MQ_UDC_FIFO_SHIFT) & MQ_UDC_FIFO_MASK)
#define MQ_UDC_FIFO_DEPTH				(4)
			u32 ep0rxcontrol;
			/* Most bitmasks shared with txstatus */
			u32 ep0rxstatus;
#define MQ_UDC_RX_PID_DATA1				(1 << 4)
#define MQ_UDC_RX_TOKEN_SETUP				(1 << 5)
#define MQ_UDC_RX_VALID_BYTES_SHIFT			(12)
#define MQ_UDC_RX_VALID_BYTES_MASK			(3)
#define MQ_UDC_RX_VALID_BYTES(x)			(((x) >> MQ_UDC_RX_VALID_BYTES_SHIFT) & MQ_UDC_RX_VALID_BYTES_MASK)
			u32 ep0rxfifo;
			u32 ep0txfifo;
			/* Most bitmasks shared with ep0txcontrol */
			u32 ep1control;
#define MQ_UDC_EP_ENABLE				(1 << 0)
			/* Most bitmasks shared with ep0txstatus */
			u32 ep1status;
			u32 ep1fifo;
			/* Most bitmasks shared with ep0txcontrol */
			u32 ep2control;
#define MQ_UDC_FORCE_DATA0				(1 << 3)
#define MQ_UDC_FIFO_THRESHOLD_SHIFT			(9)
#define MQ_UDC_FIFO_THRESHOLD_MASK			(7 << MQ_UDC_FIFO_THRESHOLD_SHIFT)
#define MQ_UDC_FIFO_THRESHOLD(x)			((x) << MQ_UDC_FIFO_THRESHOLD_SHIFT)
			/* Most bitmasks shared with ep0txstatus */
			u32 ep2status;
			/* Most bitmasks shared with ep0rxcontrol */
			u32 ep3control;
			/* Most bitmasks shared with ep0rxstatus */
			u32 ep3status;
			u32 intstatus;
#define MQ_UDC_INT_SOF					(1 << 0)
#define MQ_UDC_INT_EP0_TX				(1 << 1)
#define MQ_UDC_INT_EP0_RX				(1 << 2)
#define MQ_UDC_INT_EP1_TX				(1 << 3)
#define MQ_UDC_INT_EP2_TX_EOT				(1 << 4)
#define MQ_UDC_INT_EP3_RX_EOT				(1 << 5)
#define MQ_UDC_INT_DMA_TX				(1 << 6)
#define MQ_UDC_INT_DMA_RX				(1 << 7)
#define MQ_UDC_INT_DMA_RX_EOT				(1 << 8)
#define MQ_UDC_INT_GLOBAL_SUSPEND			(1 << 9)
#define MQ_UDC_INT_WAKEUP				(1 << 10)
#define MQ_UDC_INT_RESET				(1 << 11)
#define MQ_UDC_INT_EP0		(MQ_UDC_INT_EP0_TX | MQ_UDC_INT_EP0_RX)
#define MQ_UDC_INT_EP1		(MQ_UDC_INT_EP1_TX)
#define MQ_UDC_INT_EP2		(MQ_UDC_INT_EP2_TX_EOT | MQ_UDC_INT_DMA_TX)
#define MQ_UDC_INT_EP3		(MQ_UDC_INT_EP3_RX_EOT | MQ_UDC_INT_DMA_RX | \
				 MQ_UDC_INT_DMA_RX_EOT)
			u32 testcontrol1;
			u32 testcontrol2;
			u32 unused [5];
			/* This is for endpoint 2 */
			u32 dmatxcontrol;
#define MQ_UDC_DMA_ENABLE				(1 << 0)
#define MQ_UDC_DMA_PINGPONG				(1 << 1)
#define MQ_UDC_DMA_NUMBUFF_SHIFT			(2)
#define MQ_UDC_DMA_NUMBUFF_MASK				(3 << MQ_UDC_DMA_NUMBUFF_SHIFT)
#define MQ_UDC_DMA_NUMBUFF(x)				((x - 1) << MQ_UDC_DMA_NUMBUFF_SHIFT)
#define MQ_UDC_DMA_BUFF1_OWNER				(1 << 8)
#define MQ_UDC_DMA_BUFF2_OWNER				(1 << 9)
#define MQ_UDC_DMA_BUFF1_EOT				(1 << 10)
#define MQ_UDC_DMA_BUFF2_EOT				(1 << 11)
			u32 dmatxdesc1;
#define MQ_UDC_DMA_BUFFER_ADDR_SHIFT			(0)
#define MQ_UDC_DMA_BUFFER_ADDR_MASK			(32767)
#define MQ_UDC_DMA_BUFFER_ADDR(x)			(((x) >> 3) << MQ_UDC_DMA_BUFFER_ADDR_SHIFT)
#define MQ_UDC_DMA_BUFFER_SIZE_SHIFT			(16)
#define MQ_UDC_DMA_BUFFER_SIZE_MASK			(4095)
#define MQ_UDC_DMA_BUFFER_SIZE(x)			((x - 1) << MQ_UDC_DMA_BUFFER_SIZE_SHIFT)
#define MQ_UDC_DMA_BUFFER_LAST				(1 << 28)
			u32 dmatxdesc2;
			/* This shares bitmasks with dmatxcontrol, but refers to endpoint 3 */
			u32 dmarxcontrol;
#define MQ_UDC_ISO_TRANSFER_END				(1 << 16)
			u32 dmarxdesc1;
#define MQ_UDC_DMA_BUFFER_EADDR_SHIFT			(16)
#define MQ_UDC_DMA_BUFFER_EADDR_MASK			(32767)
#define MQ_UDC_DMA_BUFFER_EADDR(x)			(((x - 1) >> 3) << MQ_UDC_DMA_BUFFER_EADDR_SHIFT)
			u32 dmarxdesc2;
			u32 dmabuff1size;
			u32 dmabuff2size;
		};
		u32 a[19];
		u8 size[128];
	} UDC;
};

/* Public structure provided by the platform device */
struct mq11xx_udc_mach_info {
	int  (*udc_is_connected)(void);         /* do we see host? */
	void (*udc_command)(int cmd);
#define MQ11XX_UDC_CMD_CONNECT          0       /* let host see us */
#define MQ11XX_UDC_CMD_DISCONNECT       1       /* so host won't see us */
};

/*
 * Instead of attempting to figure out how to program this device,
 * just use canned values for the known display types.
 */
struct mediaq11xx_init_data {
	/* Standard MFD properties */
	int irq_base;
	struct platform_device **child_devs;
	int num_child_devs;

	u32 DC[0x6];
	u32 CC[0x5];
	u32 MIU[0x7];
	u32 GC[0x1b];
	u32 FP[0x80];
	u32 GE[0x14];
	u32 SPI[0x9];
	void (*set_power) (int on);
	struct mq11xx_udc_mach_info *udc_info;
};

/* Chip flavour */
enum
{
	CHIP_MEDIAQ_1100,
	CHIP_MEDIAQ_1132,
	CHIP_MEDIAQ_1168,
	CHIP_MEDIAQ_1178,
	CHIP_MEDIAQ_1188
};

/* This structure is used by the basic MediaQ SoC driver to allow client
 * drivers to use its functionality. Rather than doing a bunch of
 * EXPORT_SYMBOL's we're storing a pointer to this structure in every
 * device->platform_data, and the device_driver receives it with
 * every call.
 */
struct mediaq11xx_base {
	/* Memory that is serialized between CPU and gfx engine */
	u8 *gfxram;
	/* Memory that is not synchronized between CPU and GE.
	 * WARNING: NEVER TOUCH MEMORY ALLOCATED WITH THE 'gfx' FLAG
	 * (SEE BELOW) VIA THE "ram" POINTER!!! SUCH ADDRESSES CAN BE
	 * BOGUS AND POSSIBLY BELONG TO OTHER PROCESS OR DRIVER!!!
	 */
	volatile u8 *ram;
	/* MediaQ registers */
	volatile struct mediaq11xx_regs *regs;
	/* Chip flavour */
	int chip;
	/* Chip name (1132, 1178 etc) */
	const char *chipname;
	/* The physical address of MediaQ registers */
	u32 paddr_regs;
	/* Physical address of serialized & non-serialized RAM */
	u32 paddr_gfxram, paddr_ram;
#ifdef MQ_IRQ_MULTIPLEX
	/* Base IRQ number of our allocated interval of IRQs */
	int irq_base;
#endif

	/* Set the MediaQ GPIO to given state. Of course only those GPIOs
	 * are supported that exist (see MediaQ specs).
	 */
	int (*set_GPIO) (struct mediaq11xx_base *zis, int num, int state);
/* Change GPIO input/output mode */
#define MQ_GPIO_CHMODE	0x00000010
/* Disable GPIO pin */
#define MQ_GPIO_NONE	0x00000000
/* Enable GPIO input */
#define MQ_GPIO_IN	0x00000001
/* Enable GPIO output */
#define MQ_GPIO_OUT	0x00000002
/* Output a '0' on the GPIO pin (if OE is set) */
#define MQ_GPIO_0	0x00000020
/* Output a '1' on the GPIO pin (if OE is set) */
#define MQ_GPIO_1	0x00000040
/* Enable the pullup register (works only for GPIOs 50-53) */
#define MQ_GPIO_PULLUP	0x00000080
/* Handy shortcut - set some GPIO pin to output a '0' */
#define MQ_GPIO_OUT0	(MQ_GPIO_CHMODE | MQ_GPIO_OUT | MQ_GPIO_0)
/* Handy shortcut - set some GPIO pin to output an '1' */
#define MQ_GPIO_OUT1	(MQ_GPIO_CHMODE | MQ_GPIO_OUT | MQ_GPIO_1)
/* Set the MediaQ GPIO to input mode */
#define MQ_GPIO_INPUT	(MQ_GPIO_CHMODE | MQ_GPIO_IN)

	/* Get the state of a GPIO pin. If pin is in output mode, it
	 * returns the pin output value; if pin is in input mode it
	 * reads the logical level on the pin.
	 */
	int (*get_GPIO) (struct mediaq11xx_base *zis, int num);

	/* Apply/remove power to a subdevice.
	 * The base SoC driver keeps a count poweron requests for every
	 * subdevice; when a specific counter reaches zero the subdevice
	 * is powered off. For a list of subdev_id's see the MEDIAQ_11XX_XXX
	 * constants in soc-old.h. When all counts for all subdevices
	 * reaches zero, the MediaQ chip is totally powered off.
	 */
	void (*set_power) (struct mediaq11xx_base *zis, int subdev_id, int enable);

	/* Query the power on count of a subdevice (0 - off, >0 - on) */
	int (*get_power) (struct mediaq11xx_base *zis, int subdev_id);

	/* Allocate a portion of MediaQ RAM. The returned pointer is a
	 * a memory offset from the start of MediaQ RAM; to get a virtual
	 * address you must add it to either "gfxram" or "ram" pointers.
	 * If there is not enough free memory, function returns (u32)-1.
	 * Memory allocated with the 'gfx=1' flag should be always accessed
	 * via the 'gfxram' pointer; however the reverse (accessing 'gfx=0'
	 * memory via the 'gfxram' pointer) is allowed. This is a MediaQ 11xx
	 * limitation: it doesn't allow access to first 8K of RAM via the
	 * graphics-engine-unsynchronized window.
	 */
	u32 (*alloc) (struct mediaq11xx_base *zis, unsigned bytes, int gfx);

	/* Free a portion of MediaQ RAM, previously allocated by malloc() */
	void (*free) (struct mediaq11xx_base *zis, u32 addr, unsigned bytes);
};

/**
 * Increment the reference counter of the MediaQ base driver module.
 * You must call this before enumerating MediaQ drivers; otherwise you
 * can end with a pointer that points to nowhere in the case when driver
 * is unloaded between mq_driver_enum() and when you use it.
 */
extern int mq_driver_get (void);

/**
 * Decrement mediaq base SoC driver reference counter.
 */
extern void mq_driver_put (void);

/**
 * Query a list of all MediaQ device drivers.
 * You must lock the driver in memory by calling mq_driver_get() before
 * calling this function.
 */
extern int mq_device_enum (struct mediaq11xx_base **list, int list_size);

#endif  /* _PLATFORM_MEDIAQ11XX */

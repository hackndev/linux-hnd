/*
 * Machine structure definition macro.
 * Copyright (c) 2004 Andrew Zabolotny
 *
 * This file is included multiple times, once for every supported
 * machine type. It defines a special table with platform-specific
 * constants, routine addresses and other data.
 */

@ The first word is the length of this arch-specific table (without the first word)
		.long	CONCAT(End_, ARCH) - CONCAT(Arch_, ARCH)

@ Architecture-specific structure (see the ARCH_XXX offsets into this struct)
CONCAT(Arch_, ARCH):
		.long	CONCAT(Check_, ARCH) - bootldr_start
		.long	CONCAT(Init_, ARCH) - bootldr_start
		.long	CONCAT(GPIO_Table_, ARCH) - bootldr_start
		.long	CONCAT(MACH_TYPE_, ARCH)
#ifdef INIT_MDREFR
		.long	INIT_MDREFR
#else
		.long	(MDREFR_K1RUN | MDREFR_K2RUN | \
			 MDREFR_E1PIN | MDREFR_K0RUN | MDREFR_E0PIN | 0x18)
		@ os guide pg 13 part 0 running and enabled at full speed 
		@ refresh rate = 0x18
#endif
#ifdef INIT_MDCNFG
		.long	INIT_MDCNFG
#else
		.long	(MDCNFG_DE0 | MDCNFG_DE1 | MDCNFG_DE2 | MDCNFG_DE3 | \
			 MDCNFG_DCAC0 | (2*MDCNFG_DRAC0) | \
			 MDCNFG_DNB0 | MDCNFG_DTC0 | MDCNFG_DLATCH0 | \
			 MDCNFG_DCAC2 | (2*MDCNFG_DRAC2) | \
			 MDCNFG_DNB2 | MDCNFG_DTC2 | MDCNFG_DLATCH2)
#endif
#ifdef INIT_CMDLINE
		.asciz	INIT_CMDLINE
#else
		.byte	0
#endif
		.align	2

@ GPIO and MSC initial state
CONCAT(GPIO_Table_, ARCH):
		.long	GPSRx_InitValue
		.long	GPSRy_InitValue
		.long	GPSRz_InitValue
#ifdef GPCRx_InitValue
		.long	GPCRx_InitValue
		.long	GPCRy_InitValue
		.long	GPCRz_InitValue
#else
		.long	0,0,0
#endif
		.long	GPDRx_InitValue
		.long	GPDRy_InitValue
		.long	GPDRz_InitValue
		.long	GAFR0x_InitValue
		.long	GAFR1x_InitValue
		.long	GAFR0y_InitValue
		.long	GAFR1y_InitValue
		.long	GAFR0z_InitValue
		.long	GAFR1z_InitValue
		.long	INIT_MSC0
		.long	INIT_MSC1
		.long	INIT_MSC2

@ Now undefine everything that's defined in the mach-init.h file because
@ the compiler will barf if we'll try to include more than one such file

#undef ARCH

#undef GPIO00_Dir
#undef GPIO01_Dir
#undef GPIO02_Dir
#undef GPIO03_Dir
#undef GPIO04_Dir
#undef GPIO05_Dir
#undef GPIO06_Dir
#undef GPIO07_Dir

#undef GPIO08_Dir
#undef GPIO09_Dir
#undef GPIO10_Dir
#undef GPIO11_Dir
#undef GPIO12_Dir
#undef GPIO13_Dir
#undef GPIO14_Dir
#undef GPIO15_Dir

#undef GPIO16_Dir
#undef GPIO17_Dir
#undef GPIO18_Dir
#undef GPIO19_Dir
#undef GPIO20_Dir
#undef GPIO21_Dir
#undef GPIO22_Dir
#undef GPIO23_Dir

#undef GPIO24_Dir
#undef GPIO25_Dir
#undef GPIO26_Dir
#undef GPIO27_Dir
#undef GPIO28_Dir
#undef GPIO29_Dir
#undef GPIO30_Dir
#undef GPIO31_Dir

#undef GPIO32_Dir
#undef GPIO33_Dir
#undef GPIO34_Dir
#undef GPIO35_Dir
#undef GPIO36_Dir
#undef GPIO37_Dir
#undef GPIO38_Dir
#undef GPIO39_Dir

#undef GPIO40_Dir
#undef GPIO41_Dir
#undef GPIO42_Dir
#undef GPIO43_Dir
#undef GPIO44_Dir
#undef GPIO45_Dir
#undef GPIO46_Dir
#undef GPIO47_Dir

#undef GPIO48_Dir
#undef GPIO49_Dir
#undef GPIO50_Dir
#undef GPIO51_Dir
#undef GPIO52_Dir
#undef GPIO53_Dir
#undef GPIO54_Dir
#undef GPIO55_Dir

#undef GPIO56_Dir
#undef GPIO57_Dir
#undef GPIO58_Dir
#undef GPIO59_Dir
#undef GPIO60_Dir
#undef GPIO61_Dir
#undef GPIO62_Dir
#undef GPIO63_Dir

#undef GPIO64_Dir
#undef GPIO65_Dir
#undef GPIO66_Dir
#undef GPIO67_Dir
#undef GPIO68_Dir
#undef GPIO69_Dir
#undef GPIO70_Dir
#undef GPIO71_Dir

#undef GPIO72_Dir
#undef GPIO73_Dir
#undef GPIO74_Dir
#undef GPIO75_Dir
#undef GPIO76_Dir
#undef GPIO77_Dir
#undef GPIO78_Dir
#undef GPIO79_Dir

#undef GPIO80_Dir

#undef GPIO00_AltFunc
#undef GPIO01_AltFunc
#undef GPIO02_AltFunc
#undef GPIO03_AltFunc
#undef GPIO04_AltFunc
#undef GPIO05_AltFunc
#undef GPIO06_AltFunc
#undef GPIO07_AltFunc
#undef GPIO08_AltFunc
#undef GPIO09_AltFunc
#undef GPIO10_AltFunc
#undef GPIO11_AltFunc
#undef GPIO12_AltFunc
#undef GPIO13_AltFunc
#undef GPIO14_AltFunc
#undef GPIO15_AltFunc
#undef GPIO16_AltFunc
#undef GPIO17_AltFunc
#undef GPIO18_AltFunc
#undef GPIO19_AltFunc
#undef GPIO20_AltFunc
#undef GPIO21_AltFunc
#undef GPIO22_AltFunc
#undef GPIO23_AltFunc
#undef GPIO24_AltFunc
#undef GPIO25_AltFunc
#undef GPIO26_AltFunc
#undef GPIO27_AltFunc
#undef GPIO28_AltFunc
#undef GPIO29_AltFunc
#undef GPIO30_AltFunc
#undef GPIO31_AltFunc
#undef GPIO32_AltFunc
#undef GPIO33_AltFunc
#undef GPIO34_AltFunc
#undef GPIO35_AltFunc
#undef GPIO36_AltFunc
#undef GPIO37_AltFunc
#undef GPIO38_AltFunc
#undef GPIO39_AltFunc
#undef GPIO40_AltFunc
#undef GPIO41_AltFunc
#undef GPIO42_AltFunc
#undef GPIO43_AltFunc
#undef GPIO44_AltFunc
#undef GPIO45_AltFunc
#undef GPIO46_AltFunc
#undef GPIO47_AltFunc
#undef GPIO48_AltFunc
#undef GPIO49_AltFunc
#undef GPIO50_AltFunc
#undef GPIO51_AltFunc
#undef GPIO52_AltFunc
#undef GPIO53_AltFunc
#undef GPIO54_AltFunc
#undef GPIO55_AltFunc
#undef GPIO56_AltFunc
#undef GPIO57_AltFunc
#undef GPIO58_AltFunc
#undef GPIO59_AltFunc
#undef GPIO60_AltFunc
#undef GPIO61_AltFunc
#undef GPIO62_AltFunc
#undef GPIO63_AltFunc
#undef GPIO64_AltFunc
#undef GPIO65_AltFunc
#undef GPIO66_AltFunc
#undef GPIO67_AltFunc
#undef GPIO68_AltFunc
#undef GPIO69_AltFunc
#undef GPIO70_AltFunc
#undef GPIO71_AltFunc
#undef GPIO72_AltFunc
#undef GPIO73_AltFunc
#undef GPIO74_AltFunc
#undef GPIO75_AltFunc
#undef GPIO76_AltFunc
#undef GPIO77_AltFunc
#undef GPIO78_AltFunc
#undef GPIO79_AltFunc
#undef GPIO80_AltFunc

#undef GPIO00_Level
#undef GPIO01_Level
#undef GPIO02_Level
#undef GPIO03_Level
#undef GPIO04_Level
#undef GPIO05_Level
#undef GPIO06_Level
#undef GPIO07_Level
#undef GPIO08_Level
#undef GPIO09_Level
#undef GPIO10_Level
#undef GPIO11_Level
#undef GPIO12_Level
#undef GPIO13_Level
#undef GPIO14_Level
#undef GPIO15_Level
#undef GPIO16_Level
#undef GPIO17_Level
#undef GPIO18_Level
#undef GPIO19_Level
#undef GPIO20_Level
#undef GPIO21_Level
#undef GPIO22_Level
#undef GPIO23_Level
#undef GPIO24_Level
#undef GPIO25_Level
#undef GPIO26_Level
#undef GPIO27_Level
#undef GPIO28_Level
#undef GPIO29_Level
#undef GPIO30_Level
#undef GPIO31_Level
#undef GPIO32_Level
#undef GPIO33_Level
#undef GPIO34_Level
#undef GPIO35_Level
#undef GPIO36_Level
#undef GPIO37_Level
#undef GPIO38_Level
#undef GPIO39_Level
#undef GPIO40_Level
#undef GPIO41_Level
#undef GPIO42_Level
#undef GPIO43_Level
#undef GPIO44_Level
#undef GPIO45_Level
#undef GPIO46_Level
#undef GPIO47_Level
#undef GPIO48_Level
#undef GPIO49_Level
#undef GPIO50_Level
#undef GPIO51_Level
#undef GPIO52_Level
#undef GPIO53_Level
#undef GPIO54_Level
#undef GPIO55_Level
#undef GPIO56_Level
#undef GPIO57_Level
#undef GPIO58_Level
#undef GPIO59_Level
#undef GPIO60_Level
#undef GPIO61_Level
#undef GPIO62_Level
#undef GPIO63_Level
#undef GPIO64_Level
#undef GPIO65_Level
#undef GPIO66_Level
#undef GPIO67_Level
#undef GPIO68_Level
#undef GPIO69_Level
#undef GPIO70_Level
#undef GPIO71_Level
#undef GPIO72_Level
#undef GPIO73_Level
#undef GPIO74_Level
#undef GPIO75_Level
#undef GPIO76_Level
#undef GPIO77_Level
#undef GPIO78_Level
#undef GPIO79_Level
#undef GPIO80_Level

#undef GPIO00_Sleep_Level
#undef GPIO01_Sleep_Level
#undef GPIO02_Sleep_Level
#undef GPIO03_Sleep_Level
#undef GPIO04_Sleep_Level
#undef GPIO05_Sleep_Level
#undef GPIO06_Sleep_Level
#undef GPIO07_Sleep_Level
#undef GPIO08_Sleep_Level
#undef GPIO09_Sleep_Level
#undef GPIO10_Sleep_Level
#undef GPIO11_Sleep_Level
#undef GPIO12_Sleep_Level
#undef GPIO13_Sleep_Level
#undef GPIO14_Sleep_Level
#undef GPIO15_Sleep_Level
#undef GPIO16_Sleep_Level
#undef GPIO17_Sleep_Level
#undef GPIO18_Sleep_Level
#undef GPIO19_Sleep_Level
#undef GPIO20_Sleep_Level
#undef GPIO21_Sleep_Level
#undef GPIO22_Sleep_Level
#undef GPIO23_Sleep_Level
#undef GPIO24_Sleep_Level
#undef GPIO25_Sleep_Level
#undef GPIO26_Sleep_Level
#undef GPIO27_Sleep_Level
#undef GPIO28_Sleep_Level
#undef GPIO29_Sleep_Level
#undef GPIO30_Sleep_Level
#undef GPIO31_Sleep_Level
#undef GPIO32_Sleep_Level
#undef GPIO33_Sleep_Level
#undef GPIO34_Sleep_Level
#undef GPIO35_Sleep_Level
#undef GPIO36_Sleep_Level
#undef GPIO37_Sleep_Level
#undef GPIO38_Sleep_Level
#undef GPIO39_Sleep_Level
#undef GPIO40_Sleep_Level
#undef GPIO41_Sleep_Level
#undef GPIO42_Sleep_Level
#undef GPIO43_Sleep_Level
#undef GPIO44_Sleep_Level
#undef GPIO45_Sleep_Level
#undef GPIO46_Sleep_Level
#undef GPIO47_Sleep_Level
#undef GPIO48_Sleep_Level
#undef GPIO49_Sleep_Level
#undef GPIO50_Sleep_Level
#undef GPIO51_Sleep_Level
#undef GPIO52_Sleep_Level
#undef GPIO53_Sleep_Level
#undef GPIO54_Sleep_Level
#undef GPIO55_Sleep_Level
#undef GPIO56_Sleep_Level
#undef GPIO57_Sleep_Level
#undef GPIO58_Sleep_Level
#undef GPIO59_Sleep_Level
#undef GPIO60_Sleep_Level
#undef GPIO61_Sleep_Level
#undef GPIO62_Sleep_Level
#undef GPIO63_Sleep_Level
#undef GPIO64_Sleep_Level
#undef GPIO65_Sleep_Level
#undef GPIO66_Sleep_Level
#undef GPIO67_Sleep_Level
#undef GPIO68_Sleep_Level
#undef GPIO69_Sleep_Level
#undef GPIO70_Sleep_Level
#undef GPIO71_Sleep_Level
#undef GPIO72_Sleep_Level
#undef GPIO73_Sleep_Level
#undef GPIO74_Sleep_Level
#undef GPIO75_Sleep_Level
#undef GPIO76_Sleep_Level
#undef GPIO77_Sleep_Level
#undef GPIO78_Sleep_Level
#undef GPIO79_Sleep_Level
#undef GPIO80_Sleep_Level

#undef GPDRx_i0
#undef GPDRx_i1
#undef GPDRx_i2
#undef GPDRx_i3
#undef GPDRx_i4
#undef GPDRx_i5
#undef GPDRx_i6
#undef GPDRx_i7
#undef GPDRx_InitValue

#undef GPDRy_i0
#undef GPDRy_i1
#undef GPDRy_i2
#undef GPDRy_i3
#undef GPDRy_i4
#undef GPDRy_i5
#undef GPDRy_i6
#undef GPDRy_i7
#undef GPDRy_InitValue

#undef GPDRz_i0
#undef GPDRz_i1
#undef GPDRz_i2
#undef GPDRz_i3
#undef GPDRz_i4
#undef GPDRz_InitValue

#undef GAFR0x_i0
#undef GAFR0x_i1
#undef GAFR0x_i2
#undef GAFR0x_i3
#undef GAFR0x_InitValue

#undef GAFR1x_i0
#undef GAFR1x_i1
#undef GAFR1x_i2
#undef GAFR1x_i3
#undef GAFR1x_InitValue

#undef GAFR0y_i0
#undef GAFR0y_i1
#undef GAFR0y_i2
#undef GAFR0y_i3
#undef GAFR0y_InitValue

#undef GAFR1y_i0
#undef GAFR1y_i1
#undef GAFR1y_i2
#undef GAFR1y_i3
#undef GAFR1y_InitValue

#undef GAFR0z_i0
#undef GAFR0z_i1
#undef GAFR0z_i2
#undef GAFR0z_i3
#undef GAFR0z_InitValue

#undef GAFR1z_InitValue

#undef GPSRx_i0
#undef GPSRx_i1
#undef GPSRx_i2
#undef GPSRx_i3
#undef GPSRx_i4
#undef GPSRx_i5
#undef GPSRx_i6
#undef GPSRx_i7
#undef GPSRx_InitValue

#undef GPSRy_i0
#undef GPSRy_i1
#undef GPSRy_i2
#undef GPSRy_i3
#undef GPSRy_i4
#undef GPSRy_i5
#undef GPSRy_i6
#undef GPSRy_i7
#undef GPSRy_InitValue

#undef GPSRz_i0
#undef GPSRz_i1
#undef GPSRz_i2
#undef GPSRz_i3
#undef GPSRz_i4
#undef GPSRz_InitValue

#undef GPSRx_si0
#undef GPSRx_si1
#undef GPSRx_si2
#undef GPSRx_si3
#undef GPSRx_si4
#undef GPSRx_si5
#undef GPSRx_si6
#undef GPSRx_si7
#undef GPSRx_SleepValue

#undef GPSRy_si0
#undef GPSRy_si1
#undef GPSRy_si2
#undef GPSRy_si3
#undef GPSRy_si4
#undef GPSRy_si5
#undef GPSRy_si6
#undef GPSRy_si7
#undef GPSRy_SleepValue

#undef GPSRz_si0
#undef GPSRz_si1
#undef GPSRz_si2
#undef GPSRz_si3
#undef GPSRz_si4
#undef GPSRz_SleepValue

#undef GPCRx_InitValue
#undef GPCRy_InitValue
#undef GPCRz_InitValue

#undef INIT_MSC0
#undef INIT_MSC1
#undef INIT_MSC2

#undef INIT_MDREFR
#undef INIT_MDCNFG

#undef INIT_CMDLINE

/*
 * Dell Axim X3 initial GPIO machinery state.
 *
 */

#ifndef _AXIMX3_INIT_H_
#define _AXIMX3_INIT_H_

// commented out until someone with an Axim X3 saves the state of all GPIOs
// with HARET's DUMP GPIOST command... or writes them down manually, heh.
#if 0

//****************************************************
// Init Value base on Cotulla CPU Register definition
//****************************************************
#define GPDRx_i0		(GPIO00_Dir << 0) + (GPIO01_Dir << 1) + (GPIO02_Dir << 2) + (GPIO03_Dir << 3)
#define GPDRx_i1		(GPIO04_Dir << 4) + (GPIO05_Dir << 5) + (GPIO06_Dir << 6) + (GPIO07_Dir << 7)
#define GPDRx_i2		(GPIO08_Dir << 8) + (GPIO09_Dir << 9) + (GPIO10_Dir << 10) + (GPIO11_Dir << 11)
#define GPDRx_i3		(GPIO12_Dir << 12) + (GPIO13_Dir << 13) + (GPIO14_Dir << 14) + (GPIO15_Dir << 15)
#define GPDRx_i4		(GPIO16_Dir << 16) + (GPIO17_Dir << 17) + (GPIO18_Dir << 18) + (GPIO19_Dir << 19)
#define GPDRx_i5		(GPIO20_Dir << 20) + (GPIO21_Dir << 21) + (GPIO22_Dir << 22) + (GPIO23_Dir << 23)
#define GPDRx_i6		(GPIO24_Dir << 24) + (GPIO25_Dir << 25) + (GPIO26_Dir << 26) + (GPIO27_Dir << 27)
#define GPDRx_i7		(GPIO28_Dir << 28) + (GPIO29_Dir << 29) + (GPIO30_Dir << 30) + (GPIO31_Dir << 31)
#define GPDRx_InitValue		(GPDRx_i0+GPDRx_i1+GPDRx_i2+GPDRx_i3+GPDRx_i4+GPDRx_i5+GPDRx_i6+GPDRx_i7)

#define GPDRy_i0		(GPIO32_Dir << 0) + (GPIO33_Dir << 1) + (GPIO34_Dir << 2) + (GPIO35_Dir << 3)
#define GPDRy_i1		(GPIO36_Dir << 4) + (GPIO37_Dir << 5) + (GPIO38_Dir << 6) + (GPIO39_Dir << 7)
#define GPDRy_i2		(GPIO40_Dir << 8) + (GPIO41_Dir << 9) + (GPIO42_Dir << 10) + (GPIO43_Dir << 11)
#define GPDRy_i3		(GPIO44_Dir << 12) + (GPIO45_Dir << 13) + (GPIO46_Dir << 14) + (GPIO47_Dir << 15)
#define GPDRy_i4		(GPIO48_Dir << 16) + (GPIO49_Dir << 17) + (GPIO50_Dir << 18) + (GPIO51_Dir << 19)
#define GPDRy_i5		(GPIO52_Dir << 20) + (GPIO53_Dir << 21) + (GPIO54_Dir << 22) + (GPIO55_Dir << 23)
#define GPDRy_i6		(GPIO56_Dir << 24) + (GPIO57_Dir << 25) + (GPIO58_Dir << 26) + (GPIO59_Dir << 27)
#define GPDRy_i7		(GPIO60_Dir << 28) + (GPIO61_Dir << 29) + (GPIO62_Dir << 30) + (GPIO63_Dir << 31)
#define GPDRy_InitValue		(GPDRy_i0+GPDRy_i1+GPDRy_i2+GPDRy_i3+GPDRy_i4+GPDRy_i5+GPDRy_i6+GPDRy_i7)

#define GPDRz_i0		(GPIO64_Dir << 0) + (GPIO65_Dir << 1) + (GPIO66_Dir << 2) + (GPIO67_Dir << 3)
#define GPDRz_i1		(GPIO68_Dir << 4) + (GPIO69_Dir << 5) + (GPIO70_Dir << 6) + (GPIO71_Dir << 7)
#define GPDRz_i2		(GPIO72_Dir << 8) + (GPIO73_Dir << 9) + (GPIO74_Dir << 10) + (GPIO75_Dir << 11)
#define GPDRz_i3		(GPIO76_Dir << 12) + (GPIO77_Dir << 13) + (GPIO78_Dir << 14) + (GPIO79_Dir << 15)
#define GPDRz_i4		(GPIO80_Dir << 16)
#define GPDRz_InitValue		(GPDRz_i0+GPDRz_i1+GPDRz_i2+GPDRz_i3+GPDRz_i4)

#define GAFR0x_i0		(GPIO00_AltFunc	<< 0) + (GPIO01_AltFunc	<< 2) + (GPIO02_AltFunc	<< 4) + (GPIO03_AltFunc	<< 6)
#define GAFR0x_i1		(GPIO04_AltFunc	<< 8) + (GPIO05_AltFunc	<< 10) + (GPIO06_AltFunc << 12) + (GPIO07_AltFunc << 14)
#define GAFR0x_i2		(GPIO08_AltFunc	<< 16) + (GPIO09_AltFunc << 18) + (GPIO10_AltFunc << 20) + (GPIO11_AltFunc << 22)
#define GAFR0x_i3		(GPIO12_AltFunc	<< 24) + (GPIO13_AltFunc << 26) + (GPIO14_AltFunc << 28) + (GPIO15_AltFunc << 30)
#define GAFR0x_InitValue	(GAFR0x_i0+GAFR0x_i1+GAFR0x_i2+GAFR0x_i3)

#define GAFR1x_i0		(GPIO16_AltFunc	<< 0) + (GPIO17_AltFunc	<< 2) + (GPIO18_AltFunc	<< 4) + (GPIO19_AltFunc	<< 6)
#define GAFR1x_i1		(GPIO20_AltFunc	<< 8) + (GPIO21_AltFunc	<< 10) + (GPIO22_AltFunc << 12) + (GPIO23_AltFunc << 14)
#define GAFR1x_i2		(GPIO24_AltFunc	<< 16) + (GPIO25_AltFunc << 18) + (GPIO26_AltFunc << 20) + (GPIO27_AltFunc << 22)
#define GAFR1x_i3		(GPIO28_AltFunc	<< 24) + (GPIO29_AltFunc << 26) + (GPIO30_AltFunc << 28) + (GPIO31_AltFunc << 30)
#define GAFR1x_InitValue	(GAFR1x_i0+GAFR1x_i1+GAFR1x_i2+GAFR1x_i3)

#define GAFR0y_i0		(GPIO32_AltFunc	<< 0) + (GPIO33_AltFunc	<< 2) + (GPIO34_AltFunc	<< 4) + (GPIO35_AltFunc	<< 6)
#define GAFR0y_i1		(GPIO36_AltFunc	<< 8) + (GPIO37_AltFunc	<< 10) + (GPIO38_AltFunc << 12) + (GPIO39_AltFunc << 14)
#define GAFR0y_i2		(GPIO40_AltFunc	<< 16) + (GPIO41_AltFunc << 18) + (GPIO42_AltFunc << 20) + (GPIO43_AltFunc << 22)
#define GAFR0y_i3		(GPIO44_AltFunc	<< 24) + (GPIO45_AltFunc << 26) + (GPIO46_AltFunc << 28) + (GPIO47_AltFunc << 30)
#define GAFR0y_InitValue	(GAFR0y_i0+GAFR0y_i1+GAFR0y_i2+GAFR0y_i3)

#define GAFR1y_i0		(GPIO48_AltFunc	<< 0) + (GPIO49_AltFunc	<< 2) + (GPIO50_AltFunc	<< 4) + (GPIO51_AltFunc	<< 6)
#define GAFR1y_i1		(GPIO52_AltFunc	<< 8) + (GPIO53_AltFunc	<< 10) + (GPIO54_AltFunc << 12) + (GPIO55_AltFunc << 14)
#define GAFR1y_i2		(GPIO56_AltFunc	<< 16) + (GPIO57_AltFunc << 18) + (GPIO58_AltFunc << 20) + (GPIO59_AltFunc << 22)
#define GAFR1y_i3		(GPIO60_AltFunc	<< 24) + (GPIO61_AltFunc << 26) + (GPIO62_AltFunc << 28) + (GPIO63_AltFunc << 30)
#define GAFR1y_InitValue	(GAFR1y_i0+GAFR1y_i1+GAFR1y_i2+GAFR1y_i3)

#define GAFR0z_i0		(GPIO64_AltFunc	<< 0) + (GPIO65_AltFunc	<< 2) + (GPIO66_AltFunc	<< 4) + (GPIO67_AltFunc	<< 6)
#define GAFR0z_i1		(GPIO68_AltFunc	<< 8) + (GPIO69_AltFunc	<< 10) + (GPIO70_AltFunc << 12) + (GPIO71_AltFunc << 14)
#define GAFR0z_i2		(GPIO72_AltFunc	<< 16) + (GPIO73_AltFunc << 18) + (GPIO74_AltFunc << 20) + (GPIO75_AltFunc << 22)
#define GAFR0z_i3		(GPIO76_AltFunc	<< 24) + (GPIO77_AltFunc << 26) + (GPIO78_AltFunc << 28) + (GPIO79_AltFunc << 30)
#define GAFR0z_InitValue	(GAFR0z_i0+GAFR0z_i1+GAFR0z_i2+GAFR0z_i3)

#define GAFR1z_InitValue	(GPIO80_AltFunc	<< 0)

#define GPSRx_i0		(GPIO00_Level << 0) + (GPIO01_Level << 1) + (GPIO02_Level << 2) + (GPIO03_Level << 3)
#define GPSRx_i1		(GPIO04_Level << 4) + (GPIO05_Level << 5) + (GPIO06_Level << 6) + (GPIO07_Level << 7)
#define GPSRx_i2		(GPIO08_Level << 8) + (GPIO09_Level << 9) + (GPIO10_Level << 10) + (GPIO11_Level << 11)
#define GPSRx_i3		(GPIO12_Level << 12) + (GPIO13_Level << 13) + (GPIO14_Level << 14) + (GPIO15_Level << 15)
#define GPSRx_i4		(GPIO16_Level << 16) + (GPIO17_Level << 17) + (GPIO18_Level << 18) + (GPIO19_Level << 19)
#define GPSRx_i5		(GPIO20_Level << 20) + (GPIO21_Level << 21) + (GPIO22_Level << 22) + (GPIO23_Level << 23)
#define GPSRx_i6		(GPIO24_Level << 24) + (GPIO25_Level << 25) + (GPIO26_Level << 26) + (GPIO27_Level << 27)
#define GPSRx_i7		(GPIO28_Level << 28) + (GPIO29_Level << 29) + (GPIO30_Level << 30) + (GPIO31_Level << 31)
#define GPSRx_InitValue		(GPSRx_i0+GPSRx_i1+GPSRx_i2+GPSRx_i3+GPSRx_i4+GPSRx_i5+GPSRx_i6+GPSRx_i7)

#define GPSRy_i0		(GPIO32_Level << 0) + (GPIO33_Level << 1) + (GPIO34_Level << 2) + (GPIO35_Level << 3)
#define GPSRy_i1		(GPIO36_Level << 4) + (GPIO37_Level << 5) + (GPIO38_Level << 6) + (GPIO39_Level << 7)
#define GPSRy_i2		(GPIO40_Level << 8) + (GPIO41_Level << 9) + (GPIO42_Level << 10) + (GPIO43_Level << 11)
#define GPSRy_i3		(GPIO44_Level << 12) + (GPIO45_Level << 13) + (GPIO46_Level << 14) + (GPIO47_Level << 15)
#define GPSRy_i4		(GPIO48_Level << 16) + (GPIO49_Level << 17) + (GPIO50_Level << 18) + (GPIO51_Level << 19)
#define GPSRy_i5		(GPIO52_Level << 20) + (GPIO53_Level << 21) + (GPIO54_Level << 22) + (GPIO55_Level << 23)
#define GPSRy_i6		(GPIO56_Level << 24) + (GPIO57_Level << 25) + (GPIO58_Level << 26) + (GPIO59_Level << 27)
#define GPSRy_i7		(GPIO60_Level << 28) + (GPIO61_Level << 29) + (GPIO62_Level << 30) + (GPIO63_Level << 31)
#define GPSRy_InitValue		(GPSRy_i0+GPSRy_i1+GPSRy_i2+GPSRy_i3+GPSRy_i4+GPSRy_i5+GPSRy_i6+GPSRy_i7)

#define GPSRz_i0		(GPIO64_Level << 0) + (GPIO65_Level << 1) + (GPIO66_Level << 2) + (GPIO67_Level << 3)
#define GPSRz_i1		(GPIO68_Level << 4) + (GPIO69_Level << 5) + (GPIO70_Level << 6) + (GPIO71_Level << 7)
#define GPSRz_i2		(GPIO72_Level << 8) + (GPIO73_Level << 9) + (GPIO74_Level << 10) + (GPIO75_Level << 11)
#define GPSRz_i3		(GPIO76_Level << 12) + (GPIO77_Level << 13) + (GPIO78_Level << 14) + (GPIO79_Level << 15)
#define GPSRz_i4		(GPIO80_Level << 16)
#define GPSRz_InitValue		(GPSRz_i0+GPSRz_i1+GPSRz_i2+GPSRz_i3+GPSRz_i4)

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//// value for set when sleep

#define GPSRx_si0		(GPIO00_Sleep_Level <<  0) + (GPIO01_Sleep_Level <<  1) + (GPIO02_Sleep_Level <<  2) + (GPIO03_Sleep_Level <<  3)
#define GPSRx_si1		(GPIO04_Sleep_Level <<  4) + (GPIO05_Sleep_Level <<  5) + (GPIO06_Sleep_Level <<  6) + (GPIO07_Sleep_Level <<  7)
#define GPSRx_si2		(GPIO08_Sleep_Level <<  8) + (GPIO09_Sleep_Level <<  9) + (GPIO10_Sleep_Level << 10) + (GPIO11_Sleep_Level << 11)
#define GPSRx_si3		(GPIO12_Sleep_Level << 12) + (GPIO13_Sleep_Level << 13) + (GPIO14_Sleep_Level << 14) + (GPIO15_Sleep_Level << 15)
#define GPSRx_si4		(GPIO16_Sleep_Level << 16) + (GPIO17_Sleep_Level << 17) + (GPIO18_Sleep_Level << 18) + (GPIO19_Sleep_Level << 19)
#define GPSRx_si5		(GPIO20_Sleep_Level << 20) + (GPIO21_Sleep_Level << 21) + (GPIO22_Sleep_Level << 22) + (GPIO23_Sleep_Level << 23)
#define GPSRx_si6		(GPIO24_Sleep_Level << 24) + (GPIO25_Sleep_Level << 25) + (GPIO26_Sleep_Level << 26) + (GPIO27_Sleep_Level << 27)
#define GPSRx_si7		(GPIO28_Sleep_Level << 28) + (GPIO29_Sleep_Level << 29) + (GPIO30_Sleep_Level << 30) + (GPIO31_Sleep_Level << 31)
#define GPSRx_SleepValue	(GPSRx_si0+GPSRx_si1+GPSRx_si2+GPSRx_si3+GPSRx_si4+GPSRx_si5+GPSRx_si6+GPSRx_si7)

#define GPSRy_si0		(GPIO32_Sleep_Level <<  0) + (GPIO33_Sleep_Level <<  1) + (GPIO34_Sleep_Level <<  2) + (GPIO35_Sleep_Level << 3)
#define GPSRy_si1		(GPIO36_Sleep_Level <<  4) + (GPIO37_Sleep_Level <<  5) + (GPIO38_Sleep_Level <<  6) + (GPIO39_Sleep_Level << 7)
#define GPSRy_si2		(GPIO40_Sleep_Level <<  8) + (GPIO41_Sleep_Level <<  9) + (GPIO42_Sleep_Level << 10) + (GPIO43_Sleep_Level << 11)
#define GPSRy_si3		(GPIO44_Sleep_Level << 12) + (GPIO45_Sleep_Level << 13) + (GPIO46_Sleep_Level << 14) + (GPIO47_Sleep_Level << 15)
#define GPSRy_si4		(GPIO48_Sleep_Level << 16) + (GPIO49_Sleep_Level << 17) + (GPIO50_Sleep_Level << 18) + (GPIO51_Sleep_Level << 19)
#define GPSRy_si5		(GPIO52_Sleep_Level << 20) + (GPIO53_Sleep_Level << 21) + (GPIO54_Sleep_Level << 22) + (GPIO55_Sleep_Level << 23)
#define GPSRy_si6		(GPIO56_Sleep_Level << 24) + (GPIO57_Sleep_Level << 25) + (GPIO58_Sleep_Level << 26) + (GPIO59_Sleep_Level << 27)
#define GPSRy_si7		(GPIO60_Sleep_Level << 28) + (GPIO61_Sleep_Level << 29) + (GPIO62_Sleep_Level << 30) + (GPIO63_Sleep_Level << 31)
#define GPSRy_SleepValue	(GPSRy_si0+GPSRy_si1+GPSRy_si2+GPSRy_si3+GPSRy_si4+GPSRy_si5+GPSRy_si6+GPSRy_si7)

#define GPSRz_si0		(GPIO64_Sleep_Level <<  0) + (GPIO65_Sleep_Level <<  1) + (GPIO66_Sleep_Level <<  2) + (GPIO67_Sleep_Level << 3)
#define GPSRz_si1		(GPIO68_Sleep_Level <<  4) + (GPIO69_Sleep_Level <<  5) + (GPIO70_Sleep_Level <<  6) + (GPIO71_Sleep_Level << 7)
#define GPSRz_si2		(GPIO72_Sleep_Level <<  8) + (GPIO73_Sleep_Level <<  9) + (GPIO74_Sleep_Level << 10) + (GPIO75_Sleep_Level << 11)
#define GPSRz_si3		(GPIO76_Sleep_Level << 12) + (GPIO77_Sleep_Level << 13) + (GPIO78_Sleep_Level << 14) + (GPIO79_Sleep_Level << 15)
#define GPSRz_si4		(GPIO80_Sleep_Level << 16)
#define GPSRz_SleepValue	(GPSRz_si0+GPSRz_si1+GPSRz_si2+GPSRz_si3+GPSRz_si4)

#endif

#endif /* _AXIMX3_INIT_H_ */

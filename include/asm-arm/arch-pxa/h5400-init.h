/*****************************************************
   GPIO Pin Sleep Status (0 is low, 1 is high)
*****************************************************/
#define GPIO00_Sleep_Level		1
#define GPIO01_Sleep_Level		1
#define GPIO02_Sleep_Level		1
#define GPIO03_Sleep_Level		0
#define GPIO04_Sleep_Level		1
#define GPIO05_Sleep_Level		1
#define GPIO06_Sleep_Level		1
#define GPIO07_Sleep_Level		1
#define GPIO08_Sleep_Level		1
#define GPIO09_Sleep_Level		1
#define GPIO10_Sleep_Level		1
#define GPIO11_Sleep_Level		0
#define GPIO12_Sleep_Level		1
#define GPIO13_Sleep_Level		0
#define GPIO14_Sleep_Level		1
#define GPIO15_Sleep_Level		1
#define GPIO16_Sleep_Level		0
#define GPIO17_Sleep_Level		0
#define GPIO18_Sleep_Level		1
#define GPIO19_Sleep_Level		0
#define GPIO20_Sleep_Level		0
#define GPIO21_Sleep_Level		0
#define GPIO22_Sleep_Level		0
#define GPIO23_Sleep_Level		0
#define GPIO24_Sleep_Level		1
#define GPIO25_Sleep_Level		0
#define GPIO26_Sleep_Level		0
#define GPIO27_Sleep_Level		0
#define GPIO28_Sleep_Level		0
#define GPIO29_Sleep_Level		0
#define GPIO30_Sleep_Level		0
#define GPIO31_Sleep_Level		0
#define GPIO32_Sleep_Level		0
#define GPIO33_Sleep_Level		1
#define GPIO34_Sleep_Level		0
#define GPIO35_Sleep_Level		0
#define GPIO36_Sleep_Level		1
#define GPIO37_Sleep_Level		1
#define GPIO38_Sleep_Level		0
#define GPIO39_Sleep_Level		0
#define GPIO40_Sleep_Level		1
#define GPIO41_Sleep_Level		0
#define GPIO42_Sleep_Level		0
#define GPIO43_Sleep_Level		0
#define GPIO44_Sleep_Level		0
#define GPIO45_Sleep_Level		0
#define GPIO46_Sleep_Level		0
#define GPIO47_Sleep_Level		0
#define GPIO48_Sleep_Level		1
#define GPIO49_Sleep_Level		1
#define GPIO50_Sleep_Level		1
#define GPIO51_Sleep_Level		1
#define GPIO52_Sleep_Level		1
#define GPIO53_Sleep_Level		1
#define GPIO54_Sleep_Level		0
#define GPIO55_Sleep_Level		1
#define GPIO56_Sleep_Level		1
#define GPIO57_Sleep_Level		1
#define GPIO58_Sleep_Level		0
#define GPIO59_Sleep_Level		0
#define GPIO60_Sleep_Level		0
#define GPIO61_Sleep_Level		1
#define GPIO62_Sleep_Level		1
#define GPIO63_Sleep_Level		0
#define GPIO64_Sleep_Level		0
#define GPIO65_Sleep_Level		0
#define GPIO66_Sleep_Level		0
#define GPIO67_Sleep_Level		0
#define GPIO68_Sleep_Level		0
#define GPIO69_Sleep_Level		0
#define GPIO70_Sleep_Level		0
#define GPIO71_Sleep_Level		1
#define GPIO72_Sleep_Level		0
#define GPIO73_Sleep_Level		0
#define GPIO74_Sleep_Level		0
#define GPIO75_Sleep_Level		1
#define GPIO76_Sleep_Level		0
#define GPIO77_Sleep_Level		0
#define GPIO78_Sleep_Level		1
#define GPIO79_Sleep_Level		1
#define GPIO80_Sleep_Level		1


/* value for set when sleep */

#define GPSRx_si0		(GPIO00_Sleep_Level <<  0) + (GPIO01_Sleep_Level <<  1) + (GPIO02_Sleep_Level <<  2) + (GPIO03_Sleep_Level <<  3)
#define GPSRx_si1		(GPIO04_Sleep_Level <<  4) + (GPIO05_Sleep_Level <<  5) + (GPIO06_Sleep_Level <<  6) + (GPIO07_Sleep_Level <<  7)
#define GPSRx_si2		(GPIO08_Sleep_Level <<  8) + (GPIO09_Sleep_Level <<  9) + (GPIO10_Sleep_Level << 10) + (GPIO11_Sleep_Level << 11)
#define GPSRx_si3		(GPIO12_Sleep_Level << 12) + (GPIO13_Sleep_Level << 13) + (GPIO14_Sleep_Level << 14) + (GPIO15_Sleep_Level << 15)
#define GPSRx_si4		(GPIO16_Sleep_Level << 16) + (GPIO17_Sleep_Level << 17) + (GPIO18_Sleep_Level << 18) + (GPIO19_Sleep_Level << 19)
#define GPSRx_si5		(GPIO20_Sleep_Level << 20) + (GPIO21_Sleep_Level << 21) + (GPIO22_Sleep_Level << 22) + (GPIO23_Sleep_Level << 23)
#define GPSRx_si6		(GPIO24_Sleep_Level << 24) + (GPIO25_Sleep_Level << 25) + (GPIO26_Sleep_Level << 26) + (GPIO27_Sleep_Level << 27)
#define GPSRx_si7		(GPIO28_Sleep_Level << 28) + (GPIO29_Sleep_Level << 29) + (GPIO30_Sleep_Level << 30) + (GPIO31_Sleep_Level << 31)
#define GPSRx_SleepValue		GPSRx_si0+GPSRx_si1+GPSRx_si2+GPSRx_si3+GPSRx_si4+GPSRx_si5+GPSRx_si6+GPSRx_si7

#define GPSRy_si0		(GPIO32_Sleep_Level <<  0) + (GPIO33_Sleep_Level <<  1) + (GPIO34_Sleep_Level <<  2) + (GPIO35_Sleep_Level << 3)
#define GPSRy_si1		(GPIO36_Sleep_Level <<  4) + (GPIO37_Sleep_Level <<  5) + (GPIO38_Sleep_Level <<  6) + (GPIO39_Sleep_Level << 7)
#define GPSRy_si2		(GPIO40_Sleep_Level <<  8) + (GPIO41_Sleep_Level <<  9) + (GPIO42_Sleep_Level << 10) + (GPIO43_Sleep_Level << 11)
#define GPSRy_si3		(GPIO44_Sleep_Level << 12) + (GPIO45_Sleep_Level << 13) + (GPIO46_Sleep_Level << 14) + (GPIO47_Sleep_Level << 15)
#define GPSRy_si4		(GPIO48_Sleep_Level << 16) + (GPIO49_Sleep_Level << 17) + (GPIO50_Sleep_Level << 18) + (GPIO51_Sleep_Level << 19)
#define GPSRy_si5		(GPIO52_Sleep_Level << 20) + (GPIO53_Sleep_Level << 21) + (GPIO54_Sleep_Level << 22) + (GPIO55_Sleep_Level << 23)
#define GPSRy_si6		(GPIO56_Sleep_Level << 24) + (GPIO57_Sleep_Level << 25) + (GPIO58_Sleep_Level << 26) + (GPIO59_Sleep_Level << 27)
#define GPSRy_si7		(GPIO60_Sleep_Level << 28) + (GPIO61_Sleep_Level << 29) + (GPIO62_Sleep_Level << 30) + (GPIO63_Sleep_Level << 31)
#define GPSRy_SleepValue		GPSRy_si0+GPSRy_si1+GPSRy_si2+GPSRy_si3+GPSRy_si4+GPSRy_si5+GPSRy_si6+GPSRy_si7

#define GPSRz_si0		(GPIO64_Sleep_Level <<  0) + (GPIO65_Sleep_Level <<  1) + (GPIO66_Sleep_Level <<  2) + (GPIO67_Sleep_Level << 3)
#define GPSRz_si1		(GPIO68_Sleep_Level <<  4) + (GPIO69_Sleep_Level <<  5) + (GPIO70_Sleep_Level <<  6) + (GPIO71_Sleep_Level << 7)
#define GPSRz_si2		(GPIO72_Sleep_Level <<  8) + (GPIO73_Sleep_Level <<  9) + (GPIO74_Sleep_Level << 10) + (GPIO75_Sleep_Level << 11)
#define GPSRz_si3		(GPIO76_Sleep_Level << 12) + (GPIO77_Sleep_Level << 13) + (GPIO78_Sleep_Level << 14) + (GPIO79_Sleep_Level << 15)
#define GPSRz_si4		(GPIO80_Sleep_Level << 16)
#define GPSRz_SleepValue		GPSRz_si0+GPSRz_si1+GPSRz_si2+GPSRz_si3+GPSRz_si4

/* The following values are used so far only by LAB */
#define GAFR0x_InitValue	0x80000001
#define GAFR1x_InitValue	0x00000010
#define GAFR0y_InitValue	0x999A8558
#define GAFR1y_InitValue	0xAAA5AAAA
#define GAFR0z_InitValue	0xA0000800
#define GAFR1z_InitValue	0x00000002
#define GPDRx_InitValue		0xd3e3b940
#define GPDRy_InitValue		0x7cffab83
#define GPDRz_InitValue		0x0001ebdf
#define GPSRx_InitValue		0xD320A800
#define GPSRy_InitValue		0x64FFAB83
#define GPSRz_InitValue		0x0001C8C9
#define GPCRx_InitValue		0x00C31140
#define GPCRy_InitValue		0x98000000
#define GPCRz_InitValue		0x00002336

/** Initial GPIO register setup for the iPAQ h4xxx. */

#ifndef _H4000_INIT_H_
#define _H4000_INIT_H_

/************ GPIO pin direction setup; 0 = input, 1 = output *****************/
/* 1111 0011 1000 0011 1101 0000 0000 0000 */
#define GPDR0_InitValue        0xf383d000 
/* 1111 1100 1111 1111 1011 1111 1111 1111 */
#define GPDR1_InitValue        0xfcffbfff
/* xxxx xxxx xxx0 0001 1011 1111 1111 1111 */
#define GPDR2_InitValue        0x0001bfff

/************ GPIO Alternate Function (Select Function 0 ~ 3) *****************/
/* 0000 0000 0000 0000 0000 0000 0000 0100 */
#define GAFR0_L_InitValue 0x00000004
/* 0000 0000 0001 1010 1000 0000 0001 0010 */
#define GAFR0_U_InitValue 0x001a8012
/* 0110 0000 0000 0000 0000 0000 0000 1000 */
#define GAFR1_L_InitValue (0x60000008 | 0xa9450) /* 0xa9450 connects FFUART*/
/* 1010 1010 1010 0101 1000 1010 0000 1010 */
#define GAFR1_U_InitValue 0xaaa58a0a
/* 1000 1010 0000 1010 1010 1010 1010 1010 */
#define GAFR2_L_InitValue 0x8a0aaaaa
/* 0000 0000 0000 0000 0000 0000 0000 0010 */
#define GAFR2_U_InitValue 0x00000002

/************ GPIO Pin Sleep Level ********************************************/
/* 0000 0001 0000 0010 0000 0000 0000 0000 */
#define PGSR0_SleepValue 0x01020000
/* 0000 0000 1111 0011 0000 0011 0001 0010 */
#define PGSR1_SleepValue 0x00f30312
/* xxxx xxxx xxx0 0001 1000 0000 0000 0000 */
#define PGSR2_SleepValue 0x00018000


/************ GPIO Pin Init State *********************************************/
/*      v-----------------------------------27 PEN_IRQ */
/*      |  v-------------------------------=24 AF2o SSP Frame */
/*      |  .  v-----------------------------22 DOWN_BUTTON h4150 */
/*      |  .  |v----------------------------21 ACTION_BUTTON h4150 */
/*      |  .  ||v---------------------------20 LEFT_BUTTON h4150 */
/*      |  .  ||| v-------------------------19 UP_BUTTON h4150 */
/*      |  .  ||| |v------------------------18 AF1i Ext. Bus Ready */
/*      |  .  ||| ||v-----------------------17  */
/*      |  .  ||| |||       v---------------11 (802.11b) */
/*      |  .  ||| |||       |v--------------10 USB_DETECT */
/*      |  .  ||| |||       || v------------8 */
/*      |  .  ||| |||       || | v----------7 */
/*      |  .  ||| |||       || | |v---------6 */
/*      |  .  ||| |||       || | || v-------4  AC_IN */
/*      |  .  ||| |||       || | || |   v---1  AF1i Active low GP_reset */
/*      |  .  ||| |||       || | || |   |v--0  POWER_BUTTON */
/*         s        s                      */
/*      ii    iii ii     i  iiii iiii iiii */
/*       AAA A     A A                  A  */
/* 0000 0001 0000 0010 0000 0000 0000 0000 */
#define GPSR0_InitValue    0x1020000

/*         v-------------------------------56 AF1i Wait signal for Card Space */
/*         | v----------------------------=55 AF2o Card Address bit 26 */
/*         | |v----------------------------54 */
/*         | ||v--------------------------=53 AF2o Card Enable for Card Space */
/*         | |||v-------------------------=52 AF2o Card Enable for Card Space */
/*         | ||||   v---------------------=49 AF2o Write Enable for Card Space */
/*         | ||||   |v--------------------=48 AF2o Output Enable for Card Space*/
/*         | ||||   || v------------------=47 */
/*         | ||||   || |v-----------------=46 */
/*         | ||||   || ||     v-----------=41 AF2o FFUART request to send */
/*         | ||||   || ||     .v----------=40 AF2o FFUART data terminal Ready */
/*         | ||||   || ||     ..    v------36 AF1i FFUART Data carrier detect */
/*         | ||||   || ||     ..    |   v--33 AF2o Active low chip select 5  */
/*           ssss   ss        ss    s      */
/*        ii            i                  */
/* AAAA AAAA A AA   AA AA               A  */
/* 0000 0000 1111 0011 1000 0000 0000 0010 */
#define GPSR1_InitValue  0xf38002

/*                   v----------------------80 AF2o Active low chip select 4 */
/*                   | v--------------------79 AF2o Active low chip select 3 */
/*                   | |v-------------------78 RIGHT_BUTTON h4150 */
/*                   | ||v------------------77 AF2o LCD AC Bias */
/* xxxx xxxx xxx     s s                   */
/* xxxx xxxx xxxi iii   i                  */
/* xxxx xxxx xxx     A A AA   AA AAAA AAAA */
/*              0 0001 1010 0000 0000 0000 */
#define GPSR2_InitValue   0x1a000

#endif /* _H4000_INIT_H_ */

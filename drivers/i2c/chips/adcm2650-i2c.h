/* 
    i2c-adcm2650 - ADCM 2650 CMOS sensor I2C client driver

    Copyright (C) 2003, Intel Corporation

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/


#ifndef  _I2C_ADCM2650_H_
#define  _I2C_ADCM2650_H_
/* Calculating the Module Block Number */
#define BLOCK(a)    (u8)((a) >> 7)      /* register's module block address. */
#define OFFSET(a)   (u8)((a) & 0x7F )   /* register's offset to this block. */

/*  Update the block address.*/
#define BLOCK_SWITCH_CMD    0xFE
#define PIPE_SLAVE_ADDR     0x0052
#define SENSOR_SLAVE_ADDR   0x0055

//#define DEBUG
#define I2C_DRIVERID_ADCM2650   I2C_DRIVERID_EXP1
/* Register definitions in ADCM2650's chip. */
#define REV     0x0

struct adcm2650_data {
    /*
     *  Because the i2c bus is slow, it is often useful to cache the read
     *  information of a chip for some time (for example, 1 or 2 seconds).
     *  It depends of course on the device whether this is really worthwhile
     *  or even sensible.
     */
    struct semaphore update_lock; /* When we are reading lots of information,
                                     another process should not update the
                                     below information */
    char valid;                   /* != 0 if the following fields are valid. */
    int  blockaddr;                  /*  current using block address.    */
    unsigned long last_updated;   /* In jiffies */
};

#endif

#ifndef  __I2C_OV96xx_H__
#define  __I2C_OV96xx_H__

#ifndef DEBUG
#define DEBUG
#endif

#define OV96xx_SLAVE_ADDR (0x60>>1)	/* 60 for write , 61 for read */
// #define SENSOR_SLAVE_ADDR   0x0055      /* tbd: */

struct ov96xx_data {
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
    int  blockaddr;               /* current using block address.    */
    unsigned long last_updated;   /* In jiffies */
};

#endif


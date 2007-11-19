#ifndef  __I2C_OV9640_H__
#define  __I2C_OV9640_H__

#ifndef DEBUG
#define DEBUG
#endif

/* Calculating the Module Block Number */
#define BLOCK(a)    (u8)((a) >> 7)      /* register's module block address. */
#define OFFSET(a)   (u8)((a) & 0x7F )   /* register's offset to this block. */

/*  Update the block address.*/
#define BLOCK_SWITCH_CMD    0xFE

#define OV9640_SLAVE_ADDR (0x60>>1)	/* 60 for write , 61 for read */
// #define SENSOR_SLAVE_ADDR   0x0055      /* tbd: */


#define I2C_DRIVERID_OV9640   I2C_DRIVERID_EXP2

/*ov9640 chip id*/
#define OV9640_CHIP_ID  0x9648

/* Register definitions in OV9640's chip. */
#define PID	0xA
#define REV     0xA

struct ov9640_data {
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
void ov9640_gpio_init(void);
void ov9640_set_powerdown_gpio();
void ov9640_clear_powerdown_gpio();
#endif


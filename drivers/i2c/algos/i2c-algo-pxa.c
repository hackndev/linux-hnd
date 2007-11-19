/*
 *  i2c-algo-pxa.c
 *
 *  I2C algorithm for the PXA I2C bus access.
 *  Byte driven algorithm similar to pcf.
 *
 *  Copyright (C) 2002 Intrinsyc Software Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  History:
 *  Apr 2002: Initial version [CS]
 *  Jun 2002: Properly seperated algo/adap [FB]
 *  Jan 2003: added limited signal handling [Kai-Uwe Bloem]
 *  Jan 2003: allow SMBUS_QUICK as valid msg [FB]
 *  Jun 2003: updated for 2.5 [Dustin McIntire]
 *  ...: more updates for 2.6 [Holger Schurig]
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/init.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/i2c.h>		/* struct i2c_msg and others */
#include <linux/i2c-id.h>

#include <asm/arch/i2c-pxa.h>

/*
 * Set this to zero to remove all the debug statements via dead code elimination.
 */
//#define DEBUG	999	

static int pxa_scan = 1;

static int i2c_pxa_readbytes(struct i2c_adapter *i2c_adap, char *buf,
			int count, int last)
{

	int i, timeout=0;
	struct i2c_algo_pxa_data *adap = i2c_adap->algo_data;
	
	/* increment number of bytes to read by one -- read dummy byte */
	for (i = 0; i <= count; i++) {
		if (i!=0) {
			/* set ACK to NAK for last received byte ICR[ACKNAK] = 1
			   only if not a repeated start */

			if ((i == count) && last) {
				adap->transfer( last, I2C_RECEIVE, 0);
			} else {
				adap->transfer( 0, I2C_RECEIVE, 1);
			}

			timeout = adap->wait_for_interrupt(I2C_RECEIVE);

#ifdef DEBUG
			if (timeout==BUS_ERROR) {
				dev_dbg(&i2c_adap->dev, "read bus error, forcing reset\n");
				adap->reset();
				return I2C_RETRY;
			} else
#endif
			if (timeout == -ERESTARTSYS) {
				adap->abort();
				return timeout;
			} else
			if (timeout) {
				dev_dbg(&i2c_adap->dev, "read timeout, forcing reset\n");
				adap->reset();
				return I2C_RETRY;
			}

		}

		if (i) {
			buf[i - 1] = adap->read_byte();
		} else {
			adap->read_byte(); /* dummy read */
		}
	}
	return (i - 1);
}

static int i2c_pxa_sendbytes(struct i2c_adapter *i2c_adap, const char *buf,
			 int count, int last)
{

	struct i2c_algo_pxa_data *adap = i2c_adap->algo_data;
	int wrcount, timeout;
	
	for (wrcount=0; wrcount<count; ++wrcount) {

		adap->write_byte(buf[wrcount]);
		if ((wrcount==(count-1)) && last) {
			adap->transfer( last, I2C_TRANSMIT, 0);
		} else {
			adap->transfer( 0, I2C_TRANSMIT, 1);
		}

		timeout = adap->wait_for_interrupt(I2C_TRANSMIT);

#ifdef DEBUG
		if (timeout==BUS_ERROR) {
			dev_dbg(&i2c_adap->dev, "send bus error, forcing reset\n");
			adap->reset();
			return I2C_RETRY;
		} else
#endif
		if (timeout == -ERESTARTSYS) {
			adap->abort();
			return timeout;
		} else
		if (timeout) {
			dev_dbg(&i2c_adap->dev, "send timeout, forcing reset\n");
			adap->reset();
			return I2C_RETRY;
		}
	}
	return (wrcount);
}


static inline int i2c_pxa_set_ctrl_byte(struct i2c_algo_pxa_data * adap, struct i2c_msg *msg)
{
	u16 flags = msg->flags;
	u8 addr;
		
	addr = (u8) ( (0x7f & msg->addr) << 1 );
	if (flags & I2C_M_RD )
		addr |= 1;
	if (flags & I2C_M_REV_DIR_ADDR )
		addr ^= 1;
	adap->write_byte(addr);
	return 0;
}

static int i2c_pxa_do_xfer(struct i2c_adapter *i2c_adap, struct i2c_msg msgs[], int num)
{
	struct i2c_algo_pxa_data * adap;
	struct i2c_msg *pmsg=NULL;
	int i;
	int ret=0, timeout;
	
	adap = i2c_adap->algo_data;

	timeout = adap->wait_bus_not_busy();

	if (timeout) {
		return I2C_RETRY;
	}

	for (i = 0;ret >= 0 && i < num; i++) {
		int last = i + 1 == num;
		pmsg = &msgs[i];

		ret = i2c_pxa_set_ctrl_byte(adap,pmsg);

		/* Send START */
		if (i == 0) {
			adap->start();
		} else {
			adap->repeat_start();
		}

		adap->transfer(0, I2C_TRANSMIT, 0);

		/* Wait for ITE (transmit empty) */
		timeout = adap->wait_for_interrupt(I2C_TRANSMIT);

#ifdef DEBUG
		/* Check for ACK (bus error) */
		if (timeout==BUS_ERROR) {
			dev_dbg(&i2c_adap->dev, "xfer bus error, forcing reset\n");
			adap->reset();
			return I2C_RETRY;
		} else
#endif
		if (timeout == -ERESTARTSYS) {
			adap->abort();
			return timeout;
		} else
		if (timeout) {
			dev_dbg(&i2c_adap->dev, "xfer timeout, forcing reset\n");
			adap->reset();
			return I2C_RETRY;
		}
/* FIXME: handle arbitration... */
#if 0
		/* Check for bus arbitration loss */
		if (adap->arbitration_loss()) {
			printk("Arbitration loss detected \n");
			adap->reset();
			return I2C_RETRY;
		}
#endif

		/* Read */
		if (pmsg->flags & I2C_M_RD) {
			/* read bytes into buffer*/
			ret = i2c_pxa_readbytes(i2c_adap, pmsg->buf, pmsg->len, last);
#if defined(DEBUG) && (DEBUG > 2)
			if (ret != pmsg->len) {
				int i=0;
				dev_dbg(&i2c_adap->dev, "i2c_pxa_do_xfer read %d/%d bytes:",
					ret, pmsg->len);
                                for (i=0; i < pmsg->len; i++)
                                        printk("0x%x ", pmsg->buf[i]);
                                printk("\n");
			} else {
				int i=0;
				dev_dbg(&i2c_adap->dev, "i2c_pxa_do_xfer: read %d bytes:",ret);
                                for (i=0; i < pmsg->len; i++)
		                        printk("0x%x ", pmsg->buf[i]);
                                printk("\n");
			}
#endif
		} else { /* Write */
			ret = i2c_pxa_sendbytes(i2c_adap, pmsg->buf, pmsg->len, last);
#if defined(DEBUG) && (DEBUG > 2)
			if (ret != pmsg->len) {
				int i=0;
				dev_dbg(&i2c_adap->dev, "i2c_pxa_do_xfer wrote %d/%d bytes\n",
					ret, pmsg->len);
                                for (i=0; i < pmsg->len; i++)
                                        printk("0x%x ", pmsg->buf[i]);
                                printk("\n");
			} else {
				int i=0;
				dev_info(&i2c_adap->dev, "i2c_pxa_do_xfer wrote %d bytes\n",ret);
                                for (i=0; i < pmsg->len; i++)
                                        printk("0x%x ", pmsg->buf[i]);
                                printk("\n");

			}
#endif
		}
	}

	if (ret<0) {
		return ret;
	} else {
		return i;
	}
}

static int i2c_pxa_valid_messages( struct device *dev, struct i2c_msg msgs[], int num)
{
	int i;
	if (num < 1 || num > MAX_MESSAGES) {
		dev_dbg(dev, "%d messages of max %d\n", num, MAX_MESSAGES);
		return -EINVAL;
	}

	/* check consistency of our messages */
	for (i=0; i<num; i++) {
		if (&msgs[i]==NULL) {
			dev_dbg(dev, "msgs is NULL\n");
			return -EINVAL;
		} else {
			if (msgs[i].buf == NULL) {
				dev_dbg(dev, "length less than zero");
				return -EINVAL;
			}
		}
	}

	return 1;
}

static int i2c_pxa_xfer(struct i2c_adapter *i2c_adap, struct i2c_msg msgs[], int num)
{
	int retval = i2c_pxa_valid_messages(&i2c_adap->dev, msgs, num);
	
	if (retval > 0)
	{
		int i;
		for (i=i2c_adap->retries; i>=0; i--) {
			int retval = i2c_pxa_do_xfer(i2c_adap,msgs,num);
			if (retval != I2C_RETRY) {
				return retval;
			}
			dev_dbg(&i2c_adap->dev, "retrying transmission\n");
			udelay(100);
		}
		dev_dbg(&i2c_adap->dev, "retried %i times\n", i2c_adap->retries);
		return -EREMOTEIO;

	}
	return retval;
}

static u32 i2c_pxa_functionality(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL | I2C_FUNC_PROTOCOL_MANGLING; 
}



struct i2c_algorithm i2c_pxa_algorithm  = {
	.master_xfer   = i2c_pxa_xfer,
	.functionality = i2c_pxa_functionality,
};

/*
 * registering functions to load algorithms at runtime
 */
int i2c_pxa_add_bus(struct i2c_adapter *i2c_adap)
{
	struct i2c_algo_pxa_data *adap = i2c_adap->algo_data;
	
	i2c_adap->algo = &i2c_pxa_algorithm;

	/* register new adapter to i2c module... */
	i2c_add_adapter(i2c_adap);

	adap->reset();

	/* scan bus */
	if (pxa_scan) {
		int i;
		for (i = 0x02; i < 0xff; i+=2) {
			if (i==(I2C_PXA_SLAVE_ADDR<<1)) continue;

			if (adap->wait_bus_not_busy()) {
				dev_err(&i2c_adap->dev, "TIMEOUT while scanning bus\n");
				return -EIO;
			}
			adap->write_byte(i);
			adap->start();
			adap->transfer(0, I2C_TRANSMIT, 0);

			if ((adap->wait_for_interrupt(I2C_TRANSMIT) != BUS_ERROR)) {
				dev_info(&i2c_adap->dev, "found device 0x%02x\n", i>>1);
				adap->abort();
			} else {
				adap->stop();
			}
			udelay(adap->udelay);
		}
	}
	return 0;
}

int i2c_pxa_del_bus(struct i2c_adapter *i2c_adap)
{
	return i2c_del_adapter(i2c_adap);
}
EXPORT_SYMBOL(i2c_pxa_add_bus);
EXPORT_SYMBOL(i2c_pxa_del_bus);

module_param(pxa_scan, bool, 0);
MODULE_PARM_DESC(pxa_scan, "Scan for active chips on the bus");

MODULE_AUTHOR("Intrinsyc Software Inc.");
MODULE_LICENSE("GPL");

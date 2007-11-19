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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <asm/hardware.h>
#include <asm/types.h>
//#include <linux/i2c-sensor.h>
#include "adcm2650-i2c.h"

#define I2C_NAME(s) (s)->name

int i2c_adcm2650_probe(struct i2c_adapter *adapter);
extern int i2c_adapter_id(struct i2c_adapter *adap);
int i2c_adcm2650_attach_client(struct i2c_adapter *adapter, int address, int kind);
void i2c_adcm2650_cleanup(void);
int i2c_adcm2650_detach(struct i2c_client *client);

static struct i2c_driver adcm2650_driver  = 
{
	.owner          		= THIS_MODULE,
	.name =				"adcm2650 driver",		/* name           */
	.id =				I2C_DRIVERID_ADCM2650, 		/* id             */
	.flags =			I2C_DF_NOTIFY,        		/* flags          */
	.attach_adapter =		&i2c_adcm2650_probe,   /* attach_adapter */
	.detach_client =		&i2c_adcm2650_detach,    /* detach_client  */
	.command =			NULL,
};

extern  struct i2c_adapter *i2cdev_adaps[];
/* Unique ID allocation */
static int adcm2650_id = 0;
static struct i2c_client *g_client;
static unsigned short normal_i2c[] = { 0x52,I2C_CLIENT_END };
static unsigned short normal_i2c_range[] = { I2C_CLIENT_END }; 
static unsigned short probe[2] = { I2C_CLIENT_END , I2C_CLIENT_END };
static unsigned short probe_range[2] = { I2C_CLIENT_END , I2C_CLIENT_END };
static unsigned short ignore[2] = { I2C_CLIENT_END , I2C_CLIENT_END };
static unsigned short ignore_range[2] = { I2C_CLIENT_END, I2C_CLIENT_END };
static unsigned short force[2] = { I2C_CLIENT_END , I2C_CLIENT_END };

//I2C_CLIENT_INSMOD;
static struct i2c_client_address_data addr_data = {
        .normal_i2c             = normal_i2c,
        .probe                  = probe,
        .ignore                 = ignore,
};

static int ChgBlockAddr(u8 block)
{
	struct adcm2650_data *p;
	int	res;
	p = i2c_get_clientdata(g_client);
	
	down(&p->update_lock);
	/*	FIXME:	Shall we change the g_client->addr?	*/
	g_client->addr = PIPE_SLAVE_ADDR;	
	res = i2c_smbus_write_byte_data(g_client, BLOCK_SWITCH_CMD, block);
	p->blockaddr = block;
	up( &p->update_lock);
	return res;
}

int adcm2650_read(u16 addr, u16 *pvalue)
{
	int	res=0;
	struct 	adcm2650_data *p;
	u8	blockaddr = BLOCK(addr);
	u8	offset;
	
	if( g_client == NULL )	/*	No global client pointer?	*/
		return -1;

	p = i2c_get_clientdata(g_client);

	//if( p->blockaddr != blockaddr )
	res = ChgBlockAddr( blockaddr);
	
	if( res !=0 )  {
		printk("Change block address failed. block = %2x \n", blockaddr);
		return -1;
	}
	offset = (addr << 1) & 0xff;
	res = i2c_smbus_read_word_data(g_client, offset);
	printk("adcm2650_read: read 0x%x at offset 0x%x\n", res, offset);
	*pvalue = (u16)res;
	return res;
}	

int adcm2650_write(u16 addr, u16 value)
{
	int	res=0;
	struct adcm2650_data *p;
	u8	blockaddr = BLOCK(addr);
	u8 	offset;
	
	if( g_client == NULL )	/*	No global client pointer?	*/
		return -1;

	p = i2c_get_clientdata(g_client);
	//if( p->blockaddr != blockaddr )
	res = ChgBlockAddr( blockaddr );
	
	if( res !=0 )  {
		printk("Change block address failed. block = %2x \n", blockaddr);
		return -1;
	}
	offset = (addr << 1) & 0xff;
	printk("adcm2650_write: writing 0x%x at offset 0x%x\n", value, offset);
	return i2c_smbus_write_word_data(g_client, offset, value );
}	


int i2c_adcm2650_read(struct i2c_client *client, u8 reg)
{
	return i2c_smbus_read_word_data(client,reg);
	//  return i2c_smbus_read_byte_data(client,reg);
}

int i2c_adcm2650_write(struct i2c_client *client, u8 reg, u16 value)
{
	return i2c_smbus_write_word_data(client,reg,value);
}


int i2c_adcm2650_detect_client(struct i2c_adapter *adapter, int address, int kind)
{
	struct i2c_client *new_client;
	int err = 0;
	struct adcm2650_data *data;
	printk("detected ADCM2650 at addr 0x%x i2c-bus %s\n",
			                address, adapter->name);
	/* Let's see whether this adapter can support what we need.
          Please substitute the things you need here!  */
	if ( !i2c_check_functionality(adapter,I2C_FUNC_SMBUS_WORD_DATA) ) {
		printk(KERN_INFO "Word op is not permited.\n");
		goto ERROR0;
	}

	/* OK. For now, we presume we have a valid client. We now create the
          client structure, even though we cannot fill it completely yet.
          But it allows us to access several i2c functions safely */
    
	/* Note that we reserve some space for adcm2650_data too. If you don't
          need it, remove it. We do it here to help to lessen memory
          fragmentation. */

	new_client=kmalloc(sizeof(struct i2c_client), GFP_KERNEL );

	if ( !new_client )  {
       	err = -ENOMEM;
	        goto ERROR0;
       }
	memset(new_client, 0, sizeof(struct i2c_client));
	data = kmalloc(sizeof(struct adcm2650_data), GFP_KERNEL);
	if (data == NULL) {
		kfree(new_client);
		return -ENOMEM;
	}
	memset(data, 0, sizeof(struct adcm2650_data));
	new_client->addr = address;	
	i2c_set_clientdata(new_client, data);
        new_client->adapter = adapter;
        new_client->driver = &adcm2650_driver;
        new_client->flags = I2C_CLIENT_ALLOW_USE; //new_client->flags = 0;

    	g_client = new_client;

    	/* Now, we do the remaining detection. If no `force' parameter is used. */

	data->valid = 0; /* Only if you use this field */
	
	init_MUTEX(&data->update_lock); /* Only if you use this field */

    	/* Tell the i2c layer a new client has arrived */
    	if ((err = i2c_attach_client(new_client)))
      		goto ERROR3;
    	
	return 0;

    	/* OK, this is not exactly good programming practice, usually. But it is
          very code-efficient in this case. */

ERROR3:
//ERROR1:
      	kfree(new_client);
ERROR0:
	return err;
}


int i2c_adcm2650_probe(struct i2c_adapter *adap)
{
//        printk(KERN_INFO "In function %s.\n", __FUNCTION__);
        return i2c_probe(adap, &addr_data, i2c_adcm2650_detect_client);
}


int i2c_adcm2650_detach(struct i2c_client *client)
{
	int err;
    	
	/* Try to detach the client from i2c space */
    	if ((err = i2c_detach_client(client))) {
      		printk("adcm2650.o: Client deregistration failed, client not detached.\n");
	      	return err;
    	}

    	kfree(client); /* Frees client data too, if allocated at the same time */
    	g_client = NULL;
    	return 0;
}

/* 	Keep track of how far we got in the initialization process. If several
	things have to initialized, and we fail halfway, only those things
	have to be cleaned up! */
static int adcm2650_initialized = 0;

int i2c_adcm2650_init(void)
{
	int res;
	
	if (!adcm2650_initialized ){
		if ( (res = i2c_add_driver(&adcm2650_driver)) ) {
			printk("adcm2650: Driver registration failed, module not inserted.\n");
			i2c_adcm2650_cleanup();
			return res;
		}
	}
	adcm2650_initialized ++;
	return 0;
}

void i2c_adcm2650_cleanup(void)
{
	if (adcm2650_initialized == 1) {
		if ((i2c_del_driver(&adcm2650_driver))) {
			printk("adcm2650: Driver registration failed, module not removed.\n");
		}
	}
	adcm2650_initialized --;
}

MODULE_LICENSE("GPL");

EXPORT_SYMBOL(i2c_adcm2650_init);
EXPORT_SYMBOL(adcm2650_write);
EXPORT_SYMBOL(adcm2650_read);
EXPORT_SYMBOL(i2c_adcm2650_cleanup);

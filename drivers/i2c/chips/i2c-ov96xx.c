/*
 *  i2c-ov96xx.c - Omnivision 9640, 9650 CMOS sensor I2C adapter
 *
 *  Copyright (C) 2003 Intel Corporation
 *  Copyright (C) 2004 Motorola Inc.
 *  Copyright (C) 2007 Philipp Zabel <philipp.zabel@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <asm/hardware.h>
#include <asm/types.h>

#include "i2c-ov96xx.h"

#define DEBUG 1
#define DPRINTK(fmt,args...) \
	do { if (DEBUG) printk("in function %s "fmt,__FUNCTION__,##args);} while(0)

extern struct i2c_adapter *i2cdev_adaps[];
struct i2c_client *g_client = NULL;
static unsigned short normal_i2c[] = { OV96xx_SLAVE_ADDR, I2C_CLIENT_END };
I2C_CLIENT_INSMOD;

int i2c_ov96xx_read(u8 addr, u8 *pvalue)
{
	int	res = 0;
	char 	buf = 0;
	struct i2c_msg msgs[2];
	struct ov96xx_data * p;
	
	if( g_client == NULL )	
		return -1;
	p = i2c_get_clientdata(g_client);

	msgs[0].addr = g_client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &addr;

	msgs[1].addr = g_client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 1;
	msgs[1].buf = &buf;
	
	down(&p->update_lock);
	res=i2c_transfer(g_client->adapter,&msgs[0],1);
	if (res<=0) 
		goto out;
	res=i2c_transfer(g_client->adapter,&msgs[1],1);
	if (res<=0) 
		goto out;
	*pvalue = buf;
	up(&p->update_lock);
out:
//	DPRINTK(KERN_INFO "In funtion %s addr:%x,value=%x\n", __FUNCTION__, addr,*pvalue);
//	if (res<=0) DPRINTK("res = %d \n",res);
	return res;
}	

int i2c_ov96xx_write(u8 addr, u8 value)
{
	int res = 0;
	char buf[2] = {addr, value};

	struct ov96xx_data * p;
	if( g_client == NULL )
		return -1;
	p = i2c_get_clientdata(g_client);
	if(!p)
	    return -1;
	down(&p->update_lock);
	res = i2c_master_send(g_client, buf, 2);
	up(&p->update_lock);
	if (res >0) res =0;
	else res =-1;
	DPRINTK("addr:%x value:%xreturn %d \n", addr,value,res);
	return res;
}	

static struct i2c_driver ov96xx_i2c_driver;

static int i2c_ov96xx_detect_client(struct i2c_adapter *adapter, int address,  int kind)
{
	struct i2c_client *new_client;
	int err = 0;
	char res = -1;
	struct ov96xx_data *data;
    
	if (g_client != NULL) {
		err = -ENXIO;
		goto ERROR0;
	}

	DPRINTK("address=0X%X\n", address);
    /* Let's see whether this adapter can support what we need.
       Please substitute the things you need here!  */
	if ( !i2c_check_functionality(adapter,I2C_FUNC_SMBUS_WORD_DATA) ) {
		DPRINTK("Word op is not permited.\n");
		goto ERROR0;
	}

    /* OK. For now, we presume we have a valid client. We now create the
       client structure, even though we cannot fill it completely yet.
       But it allows us to access several i2c functions safely */
    
    /* Note that we reserve some space for ov9640_data too. If you don't
       need it, remove it. We do it here to help to lessen memory
       fragmentation. */

	new_client=kzalloc(sizeof(struct i2c_client), GFP_KERNEL);

	if (!new_client) {
		err = -ENOMEM;
		goto ERROR0;
	}
	data = kzalloc(sizeof(struct ov96xx_data), GFP_KERNEL);

	if (!new_client) {
		err = -ENOMEM;
		goto ERROR0;
	}

	new_client->addr = address;	
	new_client->adapter = adapter;
	new_client->driver = &ov96xx_i2c_driver;
	new_client->flags = 0;
	init_MUTEX(&data->update_lock); 
	i2c_set_clientdata(new_client, data);

	g_client = new_client;

	/* no probing */
	/* no detection here */

	strcpy(new_client->name, "i2c-ov96xx");

    /* Only if you use this field */
	data->valid = 0; 

	printk("trying to attach the client\n");
	/* Tell the i2c layer a new client has arrived */
	if (err = i2c_attach_client(new_client))
		goto ERROR3;

    /* This function can write default values to the client registers, if
       needed. */

	/* no initialization of default registers here */
	return 0;

ERROR3:
      kfree(new_client);
      g_client = NULL;
ERROR0:
      return err;
}

/* 	Keep track of how far we got in the initialization process. If several
	things have to initialized, and we fail halfway, only those things
	have to be cleaned up! */
static int ov96xx_initialized = 0;

int i2c_ov96xx_detach_client(struct i2c_client *client)
{
	int err;

	/* Try to detach the client from i2c space */
	if (err = i2c_detach_client(client)) {
		DPRINTK("i2c-ov96xx.o: Client deregistration failed, client not detached.\n");
		return err;
	}

	kfree(client); /* Frees client data too, if allocated at the same time */
	g_client = NULL;
	return 0;
}

int i2c_ov96xx_attach_adapter(struct i2c_adapter *adap)
{
	printk("i2c_ov96xx_attach_adapter\n");
	return i2c_probe(adap, &addr_data, i2c_ov96xx_detect_client);
}

static struct i2c_driver ov96xx_i2c_driver  = 
{
	.driver = {
		.name   = "i2c-ov96xx",
		.owner = THIS_MODULE,
	},
	.id		= I2C_DRIVERID_OV96xx,
	.attach_adapter = &i2c_ov96xx_attach_adapter,
	.detach_client  = &i2c_ov96xx_detach_client,
};

int i2c_ov96xx_init(void)
{
	int res;

	if (ov96xx_initialized) 
		return 0;

	DPRINTK("I2C: driver for device ov96xx.\n");

	if (res = i2c_add_driver(&ov96xx_i2c_driver)) {
		DPRINTK("i2c-ov96xx: Driver registration failed, module not inserted.\n");
		i2c_ov96xx_cleanup();
		return res;
	}
	ov96xx_initialized++;
        if(g_client != NULL) {
   	   DPRINTK("I2C: driver for device %s registed!.\n", g_client->name);
	} else {
           DPRINTK("I2C: driver for device unregisted!.\n");
	}
	return 0;
}

void i2c_ov96xx_cleanup(void)
{
	if (ov96xx_initialized == 1) {
		if (i2c_del_driver(&ov96xx_i2c_driver)) {
			DPRINTK("i2c-ov96xx: Driver registration failed, module not removed.\n");
			return;
		}
		ov96xx_initialized --;
	}
}

EXPORT_SYMBOL(i2c_ov96xx_read);
EXPORT_SYMBOL(i2c_ov96xx_write);
EXPORT_SYMBOL(i2c_ov96xx_init);
EXPORT_SYMBOL(i2c_ov96xx_cleanup);

MODULE_LICENSE("GPL");


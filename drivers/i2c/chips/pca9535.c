/*
    Driver for Philips PCA9535 16-bit low power I/O port with interrupt

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
    
    Copyright (C) 2005-2007 Pawel Kolodziejski
    Framework based on pcf8574 driver
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <asm/mach-types.h>
#include <asm/irq.h>
#include <asm/mach/arch.h>
#include <asm/hardware.h>

static struct i2c_client *pca9535_i2c_client = 0;
static struct i2c_client pca9535_client;

static unsigned short normal_i2c[] = { 0x20, I2C_CLIENT_END };
static unsigned short ignore[] = { I2C_CLIENT_END };

static struct i2c_client_address_data addr_data = {
	.normal_i2c = normal_i2c,
	.probe = ignore,
	.ignore = ignore,
};

static u32 pca9535_read_reg(struct i2c_client *client, u8 regaddr)
{
	char buffer[3];
	int r;
	u32 data;

	buffer[0] = regaddr;
	buffer[1] = 0;
	buffer[2] = 0;

	r = i2c_master_send(client, buffer, 1);
	if (r != 1) {
		printk(KERN_ERR "pca9535: write failed, status %d\n", r);
		return (u32)-1;
	}

	r = i2c_master_recv(client, buffer, 3);
	if (r != 3) {
		printk(KERN_ERR "pca9535: write failed, status %d\n", r);
		return (u32)-1;
	}

	data = buffer[1];
	data |= buffer[2] << 8;

	//printk(KERN_ERR "%s: reading %x in %x\n", __FUNCTION__, data, regaddr);

	return data;
}

static void pca9535_write_reg(struct i2c_client *client, u8 regaddr, u16 data)
{
	char buffer[3];
	int r;

	//printk(KERN_ERR "%s: writing %x in %x\n", __FUNCTION__, data, regaddr);

	buffer[0] = regaddr;
	buffer[1] = data >> 8;
	buffer[2] = data & 0xff;

	r = i2c_master_send(client, buffer, 3);
	if (r != 3) {
		printk(KERN_ERR "pca9535: write failed, status %d\n", r);
	}
}

static void pca9535_init_chip(void)
{
	if (machine_is_a716()) {
		pca9535_write_reg(pca9535_i2c_client, 6, 0xffef); // set pin 4 as output, rest as input
		pca9535_write_reg(pca9535_i2c_client, 2, 0xffff); // set output
	} else if (machine_is_a730()) {
		pca9535_write_reg(pca9535_i2c_client, 6, 0x7f16);
		pca9535_write_reg(pca9535_i2c_client, 4, 0x0);
		pca9535_write_reg(pca9535_i2c_client, 2, 0x29);
	} else
		pca9535_write_reg(pca9535_i2c_client, 6, 0xffff); // set pins as input
}

static int pca9535_attach_adapter(struct i2c_adapter *adapter);
static int pca9535_detach_client(struct i2c_client *client);

static struct i2c_driver pca9535_driver = {
	.driver = {
		.name   = "pca9535",
	},
	.attach_adapter	= pca9535_attach_adapter,
	.detach_client	= pca9535_detach_client,
};

static int pca9535_attach(struct i2c_adapter *adapter, int address, int zero_or_minus_one)
{
	struct i2c_client *new_client;
	int err = 0;

	new_client = &pca9535_client;
	i2c_set_clientdata(new_client, 0);
	new_client->addr = address;
	new_client->adapter = adapter;
	new_client->driver = &pca9535_driver;
	new_client->flags = 0;
	strcpy(new_client->name, "pca9535");

	if ((err = i2c_attach_client(new_client)))
		goto exit_free;

	pca9535_i2c_client = new_client;
	pca9535_init_chip();

	return 0;

exit_free:
	return err;
}

static int pca9535_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_probe(adapter, &addr_data, pca9535_attach);
}

static int pca9535_detach_client(struct i2c_client *client)
{
	int err;

	if ((err = i2c_detach_client(client))) {
		dev_err(&client->dev, "Client deregistration failed, client not detached.\n");
		return err;
	}

	kfree(i2c_get_clientdata(client));

	pca9535_i2c_client = 0;

	return 0;
}

static int __init pca9535_init(void)
{
	return i2c_add_driver(&pca9535_driver);
}
static void __exit pca9535_exit(void)
{
        i2c_del_driver(&pca9535_driver);
}

u32 pca9535_read_input(void)
{
	return pca9535_read_reg(pca9535_i2c_client, 0);
}
EXPORT_SYMBOL(pca9535_read_input);

void pca9535_write_output(u16 param)
{
	pca9535_write_reg(pca9535_i2c_client, 2, param);
}
EXPORT_SYMBOL(pca9535_write_output);

void pca9535_set_dir(u16 param)
{
	pca9535_write_reg(pca9535_i2c_client, 6, param);
}
EXPORT_SYMBOL(pca9535_set_dir);

MODULE_AUTHOR("Pawel Kolodziejski");
MODULE_DESCRIPTION("PCA9535 driver");
MODULE_LICENSE("GPL");

module_init(pca9535_init);
module_exit(pca9535_exit);

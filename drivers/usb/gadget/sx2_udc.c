/*
 * linux/drivers/usb/gadget/sx2_udc.c
 * Cypress EZUSB SX2 high speed USB device controller
 *
 * Copyright (C) 2007 Marek Vasut
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/mm.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>

#include <asm/byteorder.h>
#include <asm/dma.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/mach-types.h>
#include <asm/unaligned.h>
#include <asm/hardware.h>

#include <asm/arch/gpio.h>

#include <linux/usb/ch9.h>
#include <linux/usb_gadget.h>

#include <asm/arch/sx2.h>

#include "sx2_udc.h"

struct sx2_udc_mach_info sx2_mach;

/*
 TESTING STUFF
 */
int sx2_cmd_set_reg(int reg);
int sx2_cmd_write(u8 data);

int sx2_enum(void)
{
	/* ENUMERATE WITH BUILTIN DESCRIPTOR */
	/* select register 0x30 - descriptor RAM*/
	sx2_cmd_set_reg(0x30);
	/* write descriptor length */
	sx2_cmd_write(0xa0);
	sx2_cmd_write(0x00);

	/* write descriptor as described in SX2 docs */
	/* Device Descriptor */
	sx2_cmd_write(18);
	sx2_cmd_write(01);
	sx2_cmd_write(00);
	sx2_cmd_write(02);
	sx2_cmd_write(00);
	sx2_cmd_write(00);
	sx2_cmd_write(00);
	sx2_cmd_write(64);
	sx2_cmd_write(0x25);
	sx2_cmd_write(0x05);
	sx2_cmd_write(0xab);
	sx2_cmd_write(0xcd);
	sx2_cmd_write(0x12);
	sx2_cmd_write(0x34);
	sx2_cmd_write(01);
	sx2_cmd_write(02);
	sx2_cmd_write(00);
	sx2_cmd_write(01);
	/* Device qualifier */
	sx2_cmd_write(10);
	sx2_cmd_write(06);
	sx2_cmd_write(00);
	sx2_cmd_write(02);
	sx2_cmd_write(00);
	sx2_cmd_write(00);
	sx2_cmd_write(00);
	sx2_cmd_write(64);
	sx2_cmd_write(01);
	sx2_cmd_write(00);

	/* High Speed Config */
	sx2_cmd_write(9);
	sx2_cmd_write(02);
	sx2_cmd_write(46);
	sx2_cmd_write(00);
	sx2_cmd_write(01);
	sx2_cmd_write(01);
	sx2_cmd_write(00);
	sx2_cmd_write(0x0A);
	sx2_cmd_write(50);
	/* Interface descr */
	sx2_cmd_write(9);
	sx2_cmd_write(04);
	sx2_cmd_write(00);
	sx2_cmd_write(00);
	sx2_cmd_write(04);
	sx2_cmd_write(0xFF);
	sx2_cmd_write(0x00);
	sx2_cmd_write(0x00);
	sx2_cmd_write(00);
	/* Endpoint descr */
	sx2_cmd_write(07);
	sx2_cmd_write(05);
	sx2_cmd_write(0x02);
	sx2_cmd_write(02);
	sx2_cmd_write(0x00);
	sx2_cmd_write(0x02);
	sx2_cmd_write(0x00);
	/* Endpoint descr */
	sx2_cmd_write(07);
	sx2_cmd_write(05);
	sx2_cmd_write(0x04);
	sx2_cmd_write(02);
	sx2_cmd_write(0x00);
	sx2_cmd_write(0x02);
	sx2_cmd_write(0x00);
	/* Endpoint descr */
	sx2_cmd_write(07);
	sx2_cmd_write(05);
	sx2_cmd_write(0x86);
	sx2_cmd_write(02);
	sx2_cmd_write(0x00);
	sx2_cmd_write(0x02);
	sx2_cmd_write(0x00);
	/* Endpoint descr */
	sx2_cmd_write(07);
	sx2_cmd_write(05);
	sx2_cmd_write(0x88);
	sx2_cmd_write(02);
	sx2_cmd_write(0x00);
	sx2_cmd_write(0x02);
	sx2_cmd_write(0x00);
		
	/* Full Speed Config */
	sx2_cmd_write(9);
	sx2_cmd_write(02);
	sx2_cmd_write(46);
	sx2_cmd_write(00);
	sx2_cmd_write(01);
	sx2_cmd_write(01);
	sx2_cmd_write(00);
	sx2_cmd_write(0x0A);
	sx2_cmd_write(50);
	/* Interface descr */
	sx2_cmd_write(9);
	sx2_cmd_write(04);
	sx2_cmd_write(00);
	sx2_cmd_write(00);
	sx2_cmd_write(04);
	sx2_cmd_write(0xFF);
	sx2_cmd_write(0x00);
	sx2_cmd_write(0x00);
	sx2_cmd_write(00);
	/* Endpoint descr */
	sx2_cmd_write(07);
	sx2_cmd_write(05);
	sx2_cmd_write(0x02);
	sx2_cmd_write(02);
	sx2_cmd_write(0x40);
	sx2_cmd_write(0x00);
	sx2_cmd_write(0x00);
	/* Endpoint descr */
	sx2_cmd_write(07);
	sx2_cmd_write(05);
	sx2_cmd_write(0x04);
	sx2_cmd_write(02);
	sx2_cmd_write(0x40);
	sx2_cmd_write(0x00);
	sx2_cmd_write(0x00);
	/* Endpoint descr */
	sx2_cmd_write(07);
	sx2_cmd_write(05);
	sx2_cmd_write(0x86);
	sx2_cmd_write(02);
	sx2_cmd_write(0x40);
	sx2_cmd_write(0x00);
	sx2_cmd_write(0x00);
	/* Endpoint descr */
	sx2_cmd_write(07);
	sx2_cmd_write(05);
	sx2_cmd_write(0x88);
	sx2_cmd_write(02);
	sx2_cmd_write(0x40);
	sx2_cmd_write(0x00);
	sx2_cmd_write(0x00);

	/* String descr 0 */
	sx2_cmd_write(04);
	sx2_cmd_write(03);
	sx2_cmd_write(0x09);
	sx2_cmd_write(0x04);

	/* String descr 1 */
	sx2_cmd_write(16);
	sx2_cmd_write(03);
	sx2_cmd_write('C');
	sx2_cmd_write(00);
	sx2_cmd_write('y');
	sx2_cmd_write(00);
	sx2_cmd_write('p');
	sx2_cmd_write(00);
	sx2_cmd_write('r');
	sx2_cmd_write(00);
	sx2_cmd_write('e');
	sx2_cmd_write(00);
	sx2_cmd_write('s');
	sx2_cmd_write(00);
	sx2_cmd_write('s');
	sx2_cmd_write(00);

	/* String descr 2 */
	sx2_cmd_write(20);
	sx2_cmd_write(03);
	sx2_cmd_write('C');
	sx2_cmd_write(00);
	sx2_cmd_write('Y');
	sx2_cmd_write(00);
	sx2_cmd_write('7');
	sx2_cmd_write(00);
	sx2_cmd_write('C');
	sx2_cmd_write(00);
	sx2_cmd_write('6');
	sx2_cmd_write(00);
	sx2_cmd_write('8');
	sx2_cmd_write(00);
	sx2_cmd_write('0');
	sx2_cmd_write(00);
	sx2_cmd_write('0');
    	sx2_cmd_write(00);
	sx2_cmd_write('1');
	sx2_cmd_write(00);
	return 0;
}

u16 sx2_cmd_read(int reg)
{
	u8 data = SX2_CMD_ADDR | SX2_CMD_READ
		    | (reg & SX2_ADDR_MASK);

	printk("READING 0x%02x\n",reg);
	writeb(data,SX2_ADDR_CMD);
/*	while((gpio_get_value(sx2_mach.ready_pin)?1:0)==0) {
	    msleep(1);
	};
*/
	printk("REG 0x%02x F2 0x%02x F4 0x%02x\n",reg,SX2_ADDR_FIFO2,SX2_ADDR_FIFO4);
	printk("REG 0x%02x F6 0x%02x F8 0x%02x\n",reg,SX2_ADDR_FIFO6,SX2_ADDR_FIFO8);
	printk("REG 0x%02x CD 0x%02x\n",reg,SX2_ADDR_CMD);
	return 0;
}

int sx2_cmd_set_reg(int reg)
{
	/* initiate write to addr */
	printk("SETTING 0x%02x\n",(reg | SX2_CMD_ADDR));
	writeb((reg | SX2_CMD_ADDR),(SX2_ADDR_CMD));
	/* wait for READY line */	
	while((gpio_get_value(sx2_mach.ready_pin)?1:0)==0) {
	};

	return 0;
}

int sx2_cmd_write(u8 data)
{
	/* prepare data */
	u8 data1 = ((data & 0xf0)>>4);  /* Upper nibble */
	u8 data2 = (data & 0x0f);	/* Lower nibble */

	/* initiate write to reg */
	printk("UPPER NIBBLE %02x\n",(data1 | SX2_CMD_WRITE));
	writeb((data1 | SX2_CMD_WRITE),(SX2_ADDR_CMD));
	/* wait for READY line */	
	while((gpio_get_value(sx2_mach.ready_pin)?1:0)==0) {
	};

	printk("LOWER NIBBLE %02x\n",(data2 | SX2_CMD_WRITE));
	writeb((data2 | SX2_CMD_WRITE),(SX2_ADDR_CMD));
	/* wait for READY line */	
	while((gpio_get_value(sx2_mach.ready_pin)?1:0)==0) {
	};

	return 0;
}

void sx2_hwtest(void)
{
	sx2_cmd_set_reg(0x2E);
	sx2_cmd_write(0xff);
	for (;;) {
	writeb(0x05,SX2_ADDR_CMD);
	printk("%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s\n",
	(gpio_get_value(0)?"X":"-"),	//  1   1   1
	(gpio_get_value(9)?"X":"-"),	//  0   0   0
	(gpio_get_value(11)?"X":"-"),	//  0   0   0
	(gpio_get_value(18)?"X":"-"),	//  1   1   1
	(gpio_get_value(20)?"X":"-"),	//  0   1   1
	(gpio_get_value(23)?"X":"-"),	//  1   1   1
	(gpio_get_value(24)?"X":"-"),	//  1   1   1
	(gpio_get_value(34)?"X":"-"),	//  1   1   1
	(gpio_get_value(37)?"X":"-"),	//  1   1   1
	(gpio_get_value(39)?"X":"-"),	//  1   1   1
	(gpio_get_value(53)?"X":"-"),	//  1   0   0
	(gpio_get_value(82)?"X":"-"),	//  1   1   1
	(gpio_get_value(86)?"X":"-"),	//  1   1   X
	(gpio_get_value(87)?"X":"-"),	//  0   0   0
	(gpio_get_value(88)?"X":"-"),	//  1   1   1
	(gpio_get_value(90)?"X":"-"),	//  0   0   0
	(gpio_get_value(93)?"X":"-"),	//  1   1   1
	(gpio_get_value(99)?"X":"-"),	//  0   0   0
	(gpio_get_value(106)?"X":"-"),	//  0   0   0
	(gpio_get_value(107)?"X":"-"),	//  0   0   0
	(gpio_get_value(113)?"X":"-"),	//  1   1   1
	(gpio_get_value(114)?"X":"-"),	//  0   0   0
	(gpio_get_value(116)?"X":"-"),	//  1   1   1
	(gpio_get_value(117)?"X":"-"),	//  1   1   1
	(gpio_get_value(118)?"X":"-"),	//  1   1   1
	(gpio_get_value(119)?"X":"-"),	//  1   1   1
	(gpio_get_value(120)?"X":"-")	//  1   1   1
	);				// def rst wrt
	}
/*	printk("REG 0x%02x %02x %02x %02x %02x %02x\n",0x05,readb(SX2_ADDR_FIFO2),readb(SX2_ADDR_FIFO4),
	readb(SX2_ADDR_FIFO6),readb(SX2_ADDR_FIFO8),readb(SX2_ADDR_CMD));
*/
	for (;;);
}


/*
 GADGET LEVEL HANDLING STUFF
 */

static const char driver_name [] = "sx2_udc";

int usb_gadget_register_driver(struct usb_gadget_driver *driver)
{
printk("SX2: usb_gadget_register_driver\n");
return 0;
}
EXPORT_SYMBOL(usb_gadget_register_driver);

int usb_gadget_unregister_driver(struct usb_gadget_driver *driver)
{
printk("SX2: usb_gadget_unregister_driver\n");
return 0;
}
EXPORT_SYMBOL(usb_gadget_unregister_driver);









/*
 HARDWARE LEVEL HANDLING STUFF
 */
static int sx2_udc_probe(struct platform_device *pdev)
{
	static struct sx2_dev *usb;
/*	
INIT THE CONTROLLER - POWER UP ETC
*/
	/* FIXME - this cant be static :-E */
	if(!request_mem_region(SX2_ADDR_BASE, 0x10000, "sx2-udc"))
		return -EBUSY;

	usb = kmalloc(sizeof(struct sx2_dev), GFP_KERNEL);
	if(!usb)
	        return -ENOMEM;
	
	memset(usb, 0,sizeof(struct sx2_dev));
	platform_set_drvdata(pdev, usb);
	
	usb->dev = &(pdev->dev);
	usb->mach = pdev->dev.platform_data;
	sx2_mach = *(usb->mach);
	
/* Now power down and up the chip */
	gpio_set_value(usb->mach->power_pin,0);
	udelay(100);
	gpio_set_value(usb->mach->power_pin,1);
	udelay(500);
/*	sx2_hwtest(); */
	sx2_enum();
	printk("SX2: sx2_udc_probe DONE\n");
	return 0;
}

static int sx2_udc_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, 0);

printk("SX2: sx2_udc_remove\n");
	return 0;
}

/*-------------------------------------------------------------------------*/

static struct platform_driver udc_driver = {
	.probe		= sx2_udc_probe,
	.remove		= __exit_p(sx2_udc_remove),
	.suspend	= NULL,
	.resume		= NULL,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "sx2-udc",
	},
};

static int __init udc_init(void)
{
	return platform_driver_register(&udc_driver);
}
module_init(udc_init);

static void __exit udc_exit(void)
{
	platform_driver_unregister(&udc_driver);
}
module_exit(udc_exit);

MODULE_DESCRIPTION("Cypress EZUSB SX2 Peripheral Controller");
MODULE_AUTHOR("Marek Vasut");
MODULE_LICENSE("GPL");

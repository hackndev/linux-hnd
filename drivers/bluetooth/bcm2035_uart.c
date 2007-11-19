/*  Bluetooth support driver for Broadcom Blutonium BCM2035 chip via UART
 * 
 *  Author: Jan Herman <2hp@seznam.cz>
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
 

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/fs.h>

#include <asm/hardware.h>
#include <asm/arch/serial.h>

//#define BCM2035_DEBUG

extern void bcm2035_bt_power(int on);
extern void bcm2035_bt_reset(int on);

struct bcm2035_bt_funcs {
	void (*configure) ( int state );
};

static void bcm2035_bt_configure( int state )
{
	printk( KERN_NOTICE "bcm2035: Configure bluetooth: %d\n", state );
	switch (state) {
		
	case PXA_UART_CFG_POST_STARTUP:
#ifdef BCM2035_DEBUG
		printk( KERN_NOTICE "DEBUG bcm2035: UART_PRE_STARTUP\n");
#endif
		//bcm2035_bt_reset(0);
		//bcm2035_bt_wakeup(0);
		//bcm2035_bt_reset(1);
		
		
	break;
	
	case PXA_UART_CFG_POST_SHUTDOWN:
		
#ifdef BCM2035_DEBUG
		printk( KERN_NOTICE "DEBUG bcm2035: UART_POST_SHUTDOWN");
#endif
		//bcm2035_bt_reset(0);
	break;
		
	default:
		break;
	}
}


static int bcm2035_bt_probe( struct platform_device *pdev )
{	
	struct bcm2035_bt_funcs *funcs = pdev->dev.platform_data;
	
	/* power ON chip  */
	bcm2035_bt_power(1);
	bcm2035_bt_reset(1);
	
	funcs->configure = bcm2035_bt_configure;

	return 0;
}

static int bcm2035_bt_remove( struct platform_device *pdev )
{
	struct bcm2035_bt_funcs *funcs = pdev->dev.platform_data;

	funcs->configure = NULL;
	
	/* power OFF chip */
	bcm2035_bt_power(0);
	bcm2035_bt_reset(0);
	
	printk( KERN_NOTICE "bcm2035: Bluetooth driver removed\n");

	return 0;
}

static struct platform_driver bt_driver = {
	.driver = {
		.name     = "bcm2035-bt",
	},
	.probe    = bcm2035_bt_probe,
	.remove   = bcm2035_bt_remove,
};

static int bcm2035_bt_init( void )
{
	printk(KERN_NOTICE "bcm2035: Bluetooth driver registered\n");
	return platform_driver_register( &bt_driver );
}

static void bcm2035_bt_exit( void )
{
	platform_driver_unregister( &bt_driver );
}

module_init( bcm2035_bt_init );
module_exit( bcm2035_bt_exit );

MODULE_AUTHOR("Jan Herman <2hp@seznam.cz>");
MODULE_DESCRIPTION("BCM2035 Bluetooth power support driver");
MODULE_LICENSE("GPL");

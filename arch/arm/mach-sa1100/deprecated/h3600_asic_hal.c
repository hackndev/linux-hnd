/*
* Driver interface to the ASIC Complasion chip on the iPAQ H3800
*
* Copyright 2001 Compaq Computer Corporation.
* Copyright 2003 Hewlett-Packard Company.
*
* Use consistent with the GNU GPL is permitted,
* provided that this copyright notice is
* preserved in its entirety in all copies and derived works.
*
* COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
* AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
* FITNESS FOR ANY PARTICULAR PURPOSE.
*
* Author:  Andrew Christian
*          <Andrew.Christian@compaq.com>
*          October 2001
*          Jamey Hicks <Jamey.Hicks@hp.com>
*          September 2003
*/

#warning "HAL interface is deprecated."

#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/mtd/mtd.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/serial.h>  /* For bluetooth */

#include <asm/irq.h>
#include <asm/uaccess.h>   /* for copy to/from user space */
#include <asm/hardware.h>
#include <asm/mach-types.h>
#include <asm/arch/h3600_hal.h>
#include <asm/arch/h3600_asic.h>
#include <asm/arch/serial_h3800.h>

#include "h3600_asic_io.h"
#include "../common/ipaq/h3600_asic_battery.h"
#include "h3600_asic_mmc.h"
#include "h3600_asic_core.h"

/***********************************************************************************/
/*      Standard entry points for HAL requests                                     */
/***********************************************************************************/

static int h3600_asic_eeprom_read( unsigned short address, unsigned char *data, unsigned short len )
{
	if (0) printk("%s: address=%d len=%d\n", __FUNCTION__, address, len);
        return -EINVAL;
}

static int h3600_asic_eeprom_write( unsigned short address, unsigned char *data, unsigned short len )
{
        return -EINVAL;
}

static int h3600_asic_spi_write(unsigned short address, unsigned char *data, unsigned short len)
{
	return -EINVAL;
}

static int h3600_asic_read_light_sensor( unsigned char *result )
{
	int value = h3600_asic_adc_read_channel( ASIC2_ADMUX_0_LIGHTSENSOR );
	if ( value >= 0 ) {
		*result = value >> 2;
		return 0;
	}
		
	return -EIO;
}

static int h3600_asic_option_pack( int *result )
{
        int opt_ndet = (GPLR & GPIO_H3800_NOPT_IND);

	if (0) printk("%s: opt_ndet=%d\n", __FUNCTION__, opt_ndet);
	*result = opt_ndet ? 0 : 1;
	return 0;
}

static struct h3600_hal_ops h3800_asic_ops = {
	get_version         : h3600_asic_get_version,
	eeprom_read         : h3600_asic_eeprom_read,
	eeprom_write        : h3600_asic_eeprom_write,
	get_thermal_sensor  : h3600_asic_thermal_sensor,
	set_notify_led      : h3600_asic_notify_led,
	read_light_sensor   : h3600_asic_read_light_sensor,
	get_battery         : h3600_asic_battery_read,
	spi_read            : h3600_asic_spi_read,
	spi_write           : h3600_asic_spi_write,
	get_option_detect   : h3600_asic_option_pack,
	audio_clock         : h3600_asic_audio_clock,
	audio_power         : h3600_asic_audio_power,
	audio_mute          : h3600_asic_audio_mute,
	backlight_control   : h3600_asic_backlight_control,
	asset_read          : ipaq_mtd_asset_read,
	set_ebat            : h3600_asic_spi_set_ebat,
        owner               : THIS_MODULE,
};

static void __init h3600_asic_hal_exit( void )
{
	h3600_hal_unregister_interface( &h3800_asic_ops );
}

static int __init h3600_asic_hal_init( void )
{
	return h3600_hal_register_interface( &h3800_asic_ops );
}

module_init(h3600_asic_hal_init)
module_exit(h3600_asic_hal_exit)


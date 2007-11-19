/*
* Driver interface to the ASIC Complasion chip on the iPAQ IPAQ
*
* Copyright 2001 Compaq Computer Corporation.
*
* This program is free software; you can redistribute it and/or modify 
* it under the terms of the GNU General Public License as published by 
* the Free Software Foundation; either version 2 of the License, or 
* (at your option) any later version. 
* 
* COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
* AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
* FITNESS FOR ANY PARTICULAR PURPOSE.
*
* Author:  Andrew Christian
*          <Andrew.Christian@compaq.com>
*          October 2001
*
* Restrutured June 2002
*/

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
#include <linux/device.h>

#include <asm/irq.h>
#include <asm/arch/hardware.h>
#include <asm/mach-types.h>

#include <asm/hardware/ipaq-ops.h>
#include <asm/hardware/ipaq-asic2.h>
#include <asm/ipaq-sleeve.h>

#ifdef CONFIG_ARCH_PXA
#include <asm/arch/pxa-regs.h>
#include <asm/arch/h3900-gpio.h>
#endif

#include <linux/mfd/asic2_base.h>

#define PDEBUG(format,arg...) printk(KERN_DEBUG __FILE__ ":%s - " format "\n", __FUNCTION__ , ## arg )
#define PALERT(format,arg...) printk(KERN_ALERT __FILE__ ":%s - " format "\n", __FUNCTION__ , ## arg )
#define PERROR(format,arg...) printk(KERN_ERR __FILE__ ":%s - " format "\n", __FUNCTION__ , ## arg )

/***********************************************************************************
 *      SPI interface
 *
 *   Resources used:     SPI interface on ASIC2
 *                       SPI Clock on CLOCK (CX5)
 *                       SPI KPIO interrupt line
 *   Shared resource:    EX1 (24.576 MHz crystal) on CLOCK
 ***********************************************************************************/

struct spi_data {
	int irq;
	struct device *asic;
	struct semaphore lock;
	wait_queue_head_t waitq;
	spinlock_t reg_lock;
};

static irqreturn_t 
asic2_spi_isr (int irq, void *dev_id, struct pt_regs *regs)
{
	struct device *dev = dev_id;
	struct spi_data *spi = dev->driver_data;
	unsigned long val, flags;

	wake_up_interruptible (&spi->waitq);
	
	spin_lock_irqsave (&spi->reg_lock, flags);
	val = asic2_read_register (spi->asic, IPAQ_ASIC2_SPI_Control);
	val &= ~SPI_CONTROL_SPIE;   /* Clear the interrupt */
	asic2_write_register (spi->asic, IPAQ_ASIC2_SPI_Control, val);
	spin_unlock_irqrestore (&spi->reg_lock, flags);

	return IRQ_HANDLED;
}

/* 
 * This routine both transmits and receives data from the SPI bus.
 * It returns the byte read in the result code, or a negative number
 * if there was an error.
 */

static int 
asic2_spi_process_byte (struct spi_data *spi, unsigned char data)
{
	wait_queue_t  wait;
	signed long   timeout;
	int           result = 0;
	unsigned long val, flags;

	spin_lock_irqsave (&spi->reg_lock, flags);
	val = asic2_read_register (spi->asic, IPAQ_ASIC2_SPI_Control);
	val |= SPI_CONTROL_SPIE;     /* Enable interrupts */
	asic2_write_register (spi->asic, IPAQ_ASIC2_SPI_Control, val);
	asic2_write_register (spi->asic, IPAQ_ASIC2_SPI_Data, data);                     /* Send data */
	val |= SPI_CONTROL_SPE;      /* Start the transfer */
	asic2_write_register (spi->asic, IPAQ_ASIC2_SPI_Control, val);
	spin_unlock_irqrestore (&spi->reg_lock, flags);

	/* We're basically using interruptible_sleep_on_timeout */
	/* and waiting for the transfer to finish */
	init_waitqueue_entry(&wait,current);
	add_wait_queue(&spi->waitq, &wait);
	timeout = 100 * HZ / 1000;    /* 100 milliseconds (empirically derived) */

	while ( timeout > 0 ) {
		set_current_state (TASK_INTERRUPTIBLE);
		val = asic2_read_register (spi->asic, IPAQ_ASIC2_SPI_Control);
		if ( !(val & SPI_CONTROL_SPE )) {
			result = asic2_read_register (spi->asic, IPAQ_ASIC2_SPI_Data);
			break;
		}
		if ( signal_pending(current) ) {
			result = -ERESTARTSYS;
			break;
		}
		timeout = schedule_timeout( timeout );
		if ( timeout <= 0 ) {
			result = -ETIMEDOUT;       /* is this right? */
		}
	}
	set_current_state( TASK_RUNNING );
	remove_wait_queue(&spi->waitq, &wait);

	spin_lock_irqsave (&spi->reg_lock, flags);
	val = asic2_read_register (spi->asic, IPAQ_ASIC2_SPI_Control);
	val &= ~SPI_CONTROL_SPIE;    /* Disable interrupts (may be timeout) */
	asic2_write_register (spi->asic, IPAQ_ASIC2_SPI_Control, val);
	spin_unlock_irqrestore (&spi->reg_lock, flags);

	if (0) PDEBUG("result=0x%02x", result);
	return result;
}

/* 
   Read from a Microchip 25LC040 EEPROM tied to CS1
   This is the standard EEPROM part on all option packs
*/

#define SPI_25LC040_RDSR        0x05     /* Read status register */
#define SPI_25LC040_RDSR_WIP  (1<<0)     /* Write-in-process (1=true) */
#define SPI_25LC040_RDSR_WEL  (1<<1)     /* Write enable latch */
#define SPI_25LC040_RDSR_BP0  (1<<2)     /* Block protection bit */
#define SPI_25LC040_RDSR_BP1  (1<<3)     /* Block protection bit */

#define SPI_25LC040_READ_HIGH(addr)   (0x03 |((addr & 0x100)>>5))  /* Read command (put A8 in bit 3) */
#define SPI_25LC040_READ_LOW(addr)    (addr&0xff)                  /* Low 8 bits */

/* Wait until the EEPROM is finished writing */
static int 
asic2_spi_eeprom_ready (struct spi_data *spi)
{
	int result;
	int i;

	asic2_write_register (spi->asic, IPAQ_ASIC2_SPI_ChipSelectDisabled, 0); /* Disable all chip selects */

	for ( i = 0 ; i < 20 ; i++ ) {
		if ( (result = asic2_spi_process_byte (spi, SPI_25LC040_RDSR)) < 0 )
			return result;

		if ( (result = asic2_spi_process_byte (spi, 0)) < 0 )
			return result;

		/* Really should wait a bit before giving up */
		if (!(result & SPI_25LC040_RDSR_WIP) )
			return 0;
	}
	return -ETIMEDOUT;
}

static int 
asic2_spi_eeprom_read (struct spi_data *spi, unsigned short address, unsigned char *data, unsigned short len)
{
	int result = 0;
	int i;

	asic2_write_register (spi->asic, IPAQ_ASIC2_SPI_ChipSelectDisabled, 0); /* Disable all chip selects */

	if ( (result = asic2_spi_process_byte (spi, SPI_25LC040_READ_HIGH(address))) < 0 )
		return result;

	if ( (result = asic2_spi_process_byte (spi, SPI_25LC040_READ_LOW(address))) < 0 )
		return result;

	for ( i = 0 ; i < len ; i++ ) {
		if ( (result = asic2_spi_process_byte (spi, 0)) < 0 )
			return result;
		data[i] = result;
	}

	return 0;
}

static int 
asic2_spi_read (void *dev, unsigned short address, unsigned char *data, unsigned short len)
{
	struct spi_data *spi = dev;
	int result;
	unsigned long shared = 0;

	if (down_interruptible (&spi->lock))
		return -ERESTARTSYS;

	asic2_shared_add (spi->asic, &shared, ASIC_SHARED_CLOCK_EX1);

	asic2_write_register (spi->asic, IPAQ_ASIC2_SPI_Control, SPI_CONTROL_SPR(3) | SPI_CONTROL_SEL_CS0);
	asic2_clock_enable (spi->asic, ASIC2_CLOCK_SPI, 1);

	if ((result = asic2_spi_eeprom_ready (spi)) < 0 )
		goto read_optionpaq_exit;

	if ((result = asic2_spi_eeprom_read (spi, address, data, len)) < 0 )
		goto read_optionpaq_exit;

	result = 0;    /* Good return code */

read_optionpaq_exit:
	asic2_write_register (spi->asic, IPAQ_ASIC2_SPI_ChipSelectDisabled, 0); /* Disable all chip selects */
	asic2_clock_enable (spi->asic, ASIC2_CLOCK_SPI, 0);
	asic2_shared_release (spi->asic, &shared, ASIC_SHARED_CLOCK_EX1);

	up (&spi->lock);
	return result;
}

/* 
   Read from the PCMCIA option jacket microcontroller
*/

#define SPI_PCMCIA_HEADER          0xA1     /* STX */
#define	SPI_PCMCIA_ID              0x10
#define SPI_PCMCIA_ID_RESULT       0x13     /* 3 bytes : x.xx */
#define SPI_PCMCIA_BATTERY         0x20
#define SPI_PCMCIA_BATTERY_RESULT  0x24     /* 4 bytes : chem percent flag voltage */
#define SPI_PCMCIA_EBAT_ON         0x30     /* No result */

#define SPI_WRITE(_x) do { int _result = asic2_spi_process_byte(spi, _x); \
                           if (_result < 0) return _result; } while (0)
#define SPI_READ(_x)  do { _x = asic2_spi_process_byte(spi, 0);	\
                           if (_x < 0) return _x; } while (0)

static int 
asic2_spi_pcmcia_read (struct spi_data *spi, unsigned char cmd, unsigned char reply, unsigned char *data)
{
	unsigned char checksum;
	int result;
	int i;

	if (0) PDEBUG("cmd=%d reply=%d", cmd, reply);
	/* Send message */
	SPI_WRITE( SPI_PCMCIA_HEADER );
	SPI_WRITE( cmd );
	SPI_WRITE( cmd );  /* The checksum */

	if ( !data )
		return 0;
	
	/* Pause for a jiffie to give the micro time to respond */
	set_current_state( TASK_INTERRUPTIBLE );
	schedule_timeout(1);
	set_current_state( TASK_RUNNING );

	/* Read message here */
	SPI_READ( result );
	if ( result != SPI_PCMCIA_HEADER )
		return -EIO;

	SPI_READ( result );
	if ( result != reply )
		return -EIO;
	checksum = result;

	for ( i = 0 ; i < ( reply & 0x0f ) ; i++ ) {
		SPI_READ( result );
		data[i] = result;
		checksum += result;
	}
	
	SPI_READ( result );
	if ( checksum != result )
		return -EIO;

	return 0;
}

static int 
asic2_sleeve_get_version (void *dev, struct ipaq_sleeve_version *version)
{
	struct spi_data *spi = dev;
	int result;
	unsigned long shared = 0;
	unsigned char data[3];

	/* Fill in some defaults...*/
	if ( down_interruptible(&spi->lock) )
		return -ERESTARTSYS;

	asic2_shared_add (spi->asic, &shared, ASIC_SHARED_CLOCK_EX1);
	asic2_write_register (spi->asic, IPAQ_ASIC2_SPI_Control, SPI_CONTROL_SPR(2) | SPI_CONTROL_SEL_CS1);
	asic2_clock_enable (spi->asic, ASIC2_CLOCK_SPI, 1);

	if ((result = asic2_spi_pcmcia_read (spi, SPI_PCMCIA_ID, SPI_PCMCIA_ID_RESULT, data)) < 0 )
		goto read_version_exit;

	version->d[0] = data[0];
	version->d[1] = data[1];
	version->d[2] = data[2];

read_version_exit:
	asic2_write_register (spi->asic, IPAQ_ASIC2_SPI_ChipSelectDisabled, 0); /* Disable all chip selects */
	asic2_clock_enable (spi->asic, ASIC2_CLOCK_SPI, 0);
	asic2_shared_release (spi->asic, &shared, ASIC_SHARED_CLOCK_EX1);

	up (&spi->lock);
	return 0;
}

static int 
asic2_sleeve_read_pcmcia_battery (void *dev, unsigned char *chem, unsigned char *percent, unsigned char *flag)
{
	int result;
	unsigned long shared = 0;
	unsigned char data[4];
	struct spi_data *spi = dev;

	if (down_interruptible(&spi->lock))
		return -ERESTARTSYS;

	asic2_shared_add (spi->asic, &shared, ASIC_SHARED_CLOCK_EX1);
	asic2_write_register (spi->asic, IPAQ_ASIC2_SPI_Control, SPI_CONTROL_SPR(2) | SPI_CONTROL_SEL_CS1);
	asic2_clock_enable (spi->asic, ASIC2_CLOCK_SPI, 1);

	if ( (result = asic2_spi_pcmcia_read (spi, SPI_PCMCIA_BATTERY, SPI_PCMCIA_BATTERY_RESULT, data)) < 0 )
		goto read_battery_exit;

	result = 0;    /* Good return code */
	*chem    = data[0];
	*percent = data[1];
	*flag    = data[2];

read_battery_exit:
	asic2_write_register (spi->asic, IPAQ_ASIC2_SPI_ChipSelectDisabled, 0); /* Disable all chip selects */
	asic2_clock_enable (spi->asic, ASIC2_CLOCK_SPI, 0);
	asic2_shared_release (spi->asic, &shared, ASIC_SHARED_CLOCK_EX1);

	up (&spi->lock);
	return result;
}

static int 
asic2_spi_set_ebat (void *dev)
{
	int result;
	unsigned long shared = 0;
	struct spi_data *spi = dev;

	if (down_interruptible (&spi->lock))
		return -ERESTARTSYS;

	asic2_shared_add (spi->asic, &shared, ASIC_SHARED_CLOCK_EX1);
	asic2_write_register (spi->asic, IPAQ_ASIC2_SPI_Control, SPI_CONTROL_SPR(2) | SPI_CONTROL_SEL_CS1);
	asic2_clock_enable (spi->asic, ASIC2_CLOCK_SPI, 1);

	result = asic2_spi_pcmcia_read (spi, SPI_PCMCIA_EBAT_ON, 0, NULL);

	asic2_write_register (spi->asic, IPAQ_ASIC2_SPI_ChipSelectDisabled, 0); /* Disable all chip selects */
	asic2_clock_enable (spi->asic, ASIC2_CLOCK_SPI, 0);
	asic2_shared_release (spi->asic, &shared, ASIC_SHARED_CLOCK_EX1);

	up (&spi->lock);
	return result;
}

static int
asic2_sleeve_get_option_detect (void *dev, int *result)
{
#ifdef CONFIG_ARCH_PXA
	if (machine_is_h3900 ()) {
		int opt_ndet = (GPLR0 & GPIO_H3900_OPT_IND_N);
		*result = opt_ndet ? 0 : 1;
		return 0;
	}
#endif
	return -ENODEV;
}

static int
asic2_sleeve_control_egpio (void *dev, int egpio, int state)
{
	struct spi_data *spi = dev;

	switch (egpio) {
	case IPAQ_EGPIO_OPT_NVRAM_ON:
		asic2_set_gpiod (spi->asic, GPIO2_OPT_ON_NVRAM, state ? GPIO2_OPT_ON_NVRAM : 0);
		break;
	case IPAQ_EGPIO_OPT_ON:
		asic2_set_gpiod (spi->asic, GPIO2_OPT_ON, state ? GPIO2_OPT_ON : 0);
		break;
	case IPAQ_EGPIO_CARD_RESET:
		asic2_set_gpiod (spi->asic, GPIO2_OPT_PCM_RESET, state ? GPIO2_OPT_PCM_RESET : 0);
		break;
	case IPAQ_EGPIO_OPT_RESET:
		asic2_set_gpiod (spi->asic, GPIO2_OPT_RESET, state ? GPIO2_OPT_RESET : 0);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int
asic2_sleeve_read_egpio (void *dev, int nr)
{
#ifdef CONFIG_ARCH_PXA
	if (machine_is_h3900 ()) {
		switch (nr) {
		case IPAQ_EGPIO_PCMCIA_CD0_N:
			return(GPLR(GPIO_NR_H3900_PCMCIA_CD0_N) & GPIO_bit(GPIO_NR_H3900_PCMCIA_CD0_N));
		case IPAQ_EGPIO_PCMCIA_CD1_N:
			return(GPLR(GPIO_NR_H3900_PCMCIA_CD1_N) & GPIO_bit(GPIO_NR_H3900_PCMCIA_CD1_N));
		case IPAQ_EGPIO_PCMCIA_IRQ0:
			return(GPLR(GPIO_NR_H3900_PCMCIA_IRQ0_N) & GPIO_bit(GPIO_NR_H3900_PCMCIA_IRQ0_N));
		case IPAQ_EGPIO_PCMCIA_IRQ1:
			return(GPLR(GPIO_NR_H3900_PCMCIA_IRQ1_N) & GPIO_bit(GPIO_NR_H3900_PCMCIA_IRQ1_N));
		}
	}
#endif

	printk("%s: unknown ipaq_egpio_type=%d\n", __FUNCTION__, nr);
	return 0;
}

struct egpio_irq_info {
	int egpio_nr;
	int gpio_nr; /* GPIO_GPIO(n) */
	int irq;  /* IRQ_GPIOn */
};

#ifdef CONFIG_MACH_H3900
static const struct egpio_irq_info h3900_egpio_irq_info[] = {
	{ IPAQ_EGPIO_PCMCIA_CD0_N, GPIO_NR_H3900_PCMCIA_CD0_N, IRQ_GPIO_H3900_PCMCIA_CD0 }, 
	{ IPAQ_EGPIO_PCMCIA_CD1_N, GPIO_NR_H3900_PCMCIA_CD1_N, IRQ_GPIO_H3900_PCMCIA_CD1 },
	{ IPAQ_EGPIO_PCMCIA_IRQ0,  GPIO_NR_H3900_PCMCIA_IRQ0_N, IRQ_GPIO_H3900_PCMCIA_IRQ0 },
	{ IPAQ_EGPIO_PCMCIA_IRQ1,  GPIO_NR_H3900_PCMCIA_IRQ1_N, IRQ_GPIO_H3900_PCMCIA_IRQ1 },
	{ 0, 0 }
};
#endif

static int 
asic2_sleeve_egpio_irq(void *dev, int egpio_nr)
{
	const struct egpio_irq_info *info = NULL;

#ifdef CONFIG_MACH_H3900
	if (machine_is_h3900())
		info = h3900_egpio_irq_info;
#endif

	if (info) {
		while (info->irq != 0) {
			if (info->egpio_nr == egpio_nr) {
				if (0) printk("%s: egpio_nr=%d irq=%d\n", __FUNCTION__, egpio_nr, info->irq);
				return info->irq;
			}
			info++;
		}
	}

	printk("%s: unhandled egpio_nr=%d\n", __FUNCTION__, egpio_nr); 
	return -EINVAL;
}

static struct ipaq_sleeve_ops asic2_sleeve_ops = {
	.set_ebat = asic2_spi_set_ebat,
	.get_option_detect = asic2_sleeve_get_option_detect,
	.spi_read = asic2_spi_read,
	.spi_write = NULL,
	.get_version = asic2_sleeve_get_version,
	.control_egpio = asic2_sleeve_control_egpio,
	.read_egpio = asic2_sleeve_read_egpio,
	.egpio_irq = asic2_sleeve_egpio_irq,
	.read_battery = asic2_sleeve_read_pcmcia_battery,
};

static int
asic2_spi_suspend (struct device *dev, u32 state, u32 level)
{
	struct spi_data *spi = dev->driver_data;

	down (&spi->lock);   // Grab the lock, no interruptions
	disable_irq (spi->irq);

	return 0;
}

static int
asic2_spi_resume (struct device *dev, u32 level)
{
	struct spi_data *spi = dev->driver_data;

	enable_irq (spi->irq);
	up (&spi->lock);

	return 0;
}

static int
asic2_spi_probe (struct device *dev)
{
	int result;
	struct spi_data *spi;

	spi = kmalloc (sizeof (*spi), GFP_KERNEL);
	if (!spi)
		return -ENOMEM;

	init_waitqueue_head (&spi->waitq);
	init_MUTEX (&spi->lock);
	spin_lock_init (&spi->reg_lock);

	spi->asic = dev->parent;
	spi->irq = asic2_irq_base (spi->asic) + IRQ_IPAQ_ASIC2_SPI;

	dev->driver_data = spi;

	result = request_irq (spi->irq, asic2_spi_isr, SA_INTERRUPT, "asic2_spi", dev);
	if (result) {
		printk("asic2_spi: unable to claim irq %d, error %d\n", spi->irq, result);
		kfree (spi);
		return result;
	}

	result = ipaq_sleeve_register (&asic2_sleeve_ops, spi);
	if (result) {
		printk("asic2_sleeve: unable to start up sleeve driver, error %d\n", result);
		free_irq (spi->irq, spi);
		kfree (spi);
		return result;		
	}

	return result;
}

static int
asic2_spi_cleanup (struct device *dev)
{
	struct spi_data *spi = dev->driver_data;

	ipaq_sleeve_unregister ();
	
	down (&spi->lock);   // Grab the lock, no interruptions
	free_irq (spi->irq, dev);
	kfree (spi);

	return 0;
}

//static platform_device_id asic2_spi_device_ids[] = { { IPAQ_ASIC2_SPI_DEVICE_ID }, { 0 } };

static struct device_driver 
asic2_spi_device_driver = {
		.name = "asic2 spi-controller",
		.probe    = asic2_spi_probe,
		.remove   = asic2_spi_cleanup,
		.suspend  = asic2_spi_suspend,
		.resume   = asic2_spi_resume
};

static int __init
asic2_spi_init (void)
{
	return driver_register (&asic2_spi_device_driver);
}

static void __exit
asic2_spi_exit (void)
{
	driver_unregister (&asic2_spi_device_driver);
}

module_init (asic2_spi_init);
module_exit (asic2_spi_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Phil Blundell <pb@handhelds.org> and others");
MODULE_DESCRIPTION("SPI driver for iPAQ ASIC2");
MODULE_SUPPORTED_DEVICE("asic2 spi");
//MODULE_DEVICE_TABLE(soc, asic2_spi_device_ids);

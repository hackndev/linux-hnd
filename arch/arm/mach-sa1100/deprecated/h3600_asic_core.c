/*
* Driver interface to the ASIC Complasion chip on the iPAQ H3800
*
* Copyright 2001 Compaq Computer Corporation.
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
#include <linux/serial.h>  /* For bluetooth */

#include <linux/device.h>
#include <linux/soc-old.h>
#include <asm/arch-sa1100/ipaq_asic_device.h>

#include <asm/irq.h>
#include <asm/uaccess.h>   /* for copy to/from user space */
#include <asm/hardware.h>
#include <asm/mach-types.h>
#include <asm/arch/h3600_hal.h>
#include <asm/arch/h3600_asic.h>
#include <asm/arch/serial_h3800.h>

#include "h3600_asic_io.h"
#include "../common/ipaq/h3600_asic_battery.h"
#include "../common/ipaq/asic2_adc.h"
#include "h3600_asic_mmc.h"
#include "h3600_asic_core.h"

#define IPAQ_ASIC_PROC_DIR     "asic"
#define REG_DIRNAME "registers"

MODULE_AUTHOR("Andrew Christian");
MODULE_DESCRIPTION("Hardware abstraction layer for the iPAQ H3800");
MODULE_LICENSE("Dual BSD/GPL");

/* Statistics */
struct asic_statistics g_ipaq_asic_statistics;

extern struct asic2_battery_ops h3800_battery_ops;

/***********************************************************************************
 *      Power button handling
 ***********************************************************************************/

static irqreturn_t ipaq_asic_power_isr(int irq, void *dev_id, struct pt_regs *regs)
{
        int down = (GPLR & GPIO_H3600_NPOWER_BUTTON) ? 0 : 1;
	h3600_hal_keypress( H3600_MAKEKEY( H3600_KEYCODE_SUSPEND, down ) );
	return IRQ_HANDLED;
}

/***********************************************************************************
 *      Generic IRQ handling
 ***********************************************************************************/

static int ipaq_asic_init_isr( void )
{
	int result;
	DEBUG_INIT();

	/* Request IRQs */
	result = request_irq(IRQ_GPIO_H3600_NPOWER_BUTTON, 
			     ipaq_asic_power_isr, 
			     SA_INTERRUPT | SA_SAMPLE_RANDOM,
			     "h3600_suspend", NULL);
        set_irq_type( IRQ_GPIO_H3600_NPOWER_BUTTON, IRQT_BOTHEDGE );

	if ( result ) {
		printk(KERN_CRIT "%s: unable to grab power button IRQ\n", __FUNCTION__);
		return result;
	}

	result = request_irq(IRQ_GPIO_H3800_AC_IN,
			     ipaq_asic_ac_in_isr,
			     SA_INTERRUPT | SA_SAMPLE_RANDOM,
			     "h3800_ac_in", NULL );
	set_irq_type( IRQ_GPIO_H3800_AC_IN, IRQT_BOTHEDGE );
	if ( result ) {
		printk(KERN_CRIT "%s: unable to grab AC in IRQ\n", __FUNCTION__);
		free_irq(IRQ_GPIO_H3600_NPOWER_BUTTON, NULL );
		return result;
	}

	result = request_irq(IRQ_GPIO_H3800_NOPT_IND,
			     ipaq_asic_sleeve_isr,
			     SA_INTERRUPT | SA_SAMPLE_RANDOM,
			     "h3800_sleeve", NULL );
	set_irq_type( IRQ_GPIO_H3800_NOPT_IND, IRQT_BOTHEDGE );
	if ( result ) {
		printk(KERN_CRIT "%s: unable to grab sleeve insertion IRQ\n", __FUNCTION__);
		free_irq(IRQ_GPIO_H3600_NPOWER_BUTTON, NULL );
		free_irq(IRQ_GPIO_H3800_AC_IN, NULL );
		return result;
	}

	return 0;
}

static void ipaq_asic_release_isr( void )
{
	DEBUG_INIT();

//	H3800_ASIC2_KPIINTSTAT  = 0;    
//	H3800_ASIC2_GPIINTSTAT  = 0;

	free_irq(IRQ_GPIO_H3600_NPOWER_BUTTON, NULL);
	free_irq(IRQ_GPIO_H3800_AC_IN, NULL);
	free_irq(IRQ_GPIO_H3800_NOPT_IND, NULL);
}

/***********************************************************************************
 *   Proc filesystem interface
 ***********************************************************************************/

static int ipaq_asic_reset_handler(ctl_table *ctl, int write, struct file * filp,
			     void *buffer, size_t *lenp);

static struct ctl_table ipaq_asic_table[] =
{
	{ 1, "reset", NULL, 0, 0600, NULL, (proc_handler *) &ipaq_asic_reset_handler },
	{ 2, "battery", NULL, 0, 055, ipaq_asic_battery_table },
	{0}
};

static struct ctl_table ipaq_asic_dir_table[] =
{
	{1, "asic", NULL, 0, 0555, ipaq_asic_table},
	{0}
};

static struct ctl_table_header *ipaq_asic_sysctl_header = NULL;

static struct proc_dir_entry   *asic_proc_dir;
static struct proc_dir_entry   *reg_proc_dir;
static struct proc_dir_entry   *adc_proc_dir;

#define PRINT_DATA(x,s) \
	p += sprintf (p, "%-28s : %d\n", s, g_ipaq_asic_statistics.x)

#define PRINT_OWM_DATA(x,s) \
	p += sprintf (p, "%-28s : %d\n", s, g_owm_statistics.x)

static int ipaq_asic_proc_stats_read(char *page, char **start, off_t off,
				      int count, int *eof, void *data)
{
	char *p = page;
	int len;
	int i;
	int ex1, ex2;

	PRINT_DATA(spi_bytes,   "SPI bytes sent/received");
	PRINT_DATA(spi_wip,     "SPI write-in-process delays");
	PRINT_DATA(spi_timeout, "SPI timeouts");
	PRINT_OWM_DATA(owm_timeout, "OWM timeouts");
	PRINT_OWM_DATA(owm_reset,   "OWM reset");
	PRINT_OWM_DATA(owm_written, "OWM written");
	PRINT_OWM_DATA(owm_read,    "OWM read");
	p += sprintf(p,         "OWM ISR received   Valid Invalid Post\n");
	for ( i = 0 ; i < 5 ; i++ )
		p += sprintf(p, "     %10s : %4d   %4d   %4d\n", 
			     owm_state_names[i],
			     g_owm_statistics.owm_valid_isr[i],
			     g_owm_statistics.owm_invalid_isr[i],
			     g_owm_statistics.owm_post_isr[i]);

	ipaq_asic_shared_debug (&ex1, &ex2);
	p += sprintf(p, "%-28s : %d\n", "EX1 usage", ex1 );
	p += sprintf(p, "%-28s : %d\n", "EX2 usage", ex2 );

	PRINT_DATA(mmc_insert,  "MMC insert events");
	PRINT_DATA(mmc_eject,   "MMC eject event");
	PRINT_DATA(mmc_reset,   "MMC reset commands");
	PRINT_DATA(mmc_command, "MMC commands issued");
	PRINT_DATA(mmc_read,    "MMC blocks read");
	PRINT_DATA(mmc_written, "MMC blocks written");
	PRINT_DATA(mmc_timeout, "MMC timeout interrupts");
	PRINT_DATA(mmc_error ,  "MMC response errors");

	len = (p - page) - off;
	if (len < 0)
		len = 0;

	*eof = (len <= count) ? 1 : 0;
	*start = page + off;

	return len;
}

static int ipaq_asic_proc_adc_read(char *page, char **start, off_t off,
				    int count, int *eof, void *data )
{
	int result;
	char *p = page;

	MOD_INC_USE_COUNT;
	result = ipaq_asic_adc_read_channel((int) data);
	if ( result < 0 )
		p += sprintf(p, "Error code %d\n", result );
	else
		p += sprintf(p, "0x%04x\n",(unsigned int) result );

	*eof = 1;
	MOD_DEC_USE_COUNT;
	return (p - page);
}

/* Coded lifted from "registers.c" */

static ssize_t proc_read_reg(struct file * file, char * buf,
		size_t nbytes, loff_t *ppos);
static ssize_t proc_write_reg(struct file * file, const char * buffer,
		size_t count, loff_t *ppos);

static struct file_operations proc_reg_operations = {
	read:	proc_read_reg,
	write:	proc_write_reg
};

typedef struct asic_reg_entry {
	u16   bytes;
	u16   phyaddr;
	char* name;
	char* description;
	unsigned short low_ino;
} asic_reg_entry_t;

static asic_reg_entry_t asic_regs[] =
{
/*	{ virt_addr,   name,     description } */
	{ 4, 0x0000, "GPIODIR",    "GPIO Input/Output direction register" },
	{ 4, 0x0004, "GPIINTTYPE", "GPI Interrupt Type (Edge/Level)"},
	{ 4, 0x0008, "GPIINTESEL", "GPI Interrupt active edge select"},
	{ 4, 0x000c, "GPIINTALSEL","GPI Interrupt active level select"},
	{ 4, 0x0010, "GPIINTFLAG", "GPI Interrupt active flag" },
	{ 4, 0x0014, "GPIOPIOD",   "GPIO Port input/output data" },
	{ 4, 0x0018, "GPOBFSTAT",  "GPO output data in batt_fault" },
	{ 4, 0x001c, "GPIINTSTAT", "GPI Interrupt status" },
	{ 4, 0x003c, "GPIOALT",    "GPIO ALTernate function" },

	{ 4, 0x0200, "KPIODIR",    "KPIO Input/output direction"},
	{ 4, 0x0204, "KPIINTTYP",  "KPI interrupt type (edge/level)" },
	{ 4, 0x0208, "KPIINTESEL", "KPI Interrupt active edge select" },
	{ 4, 0x020c, "KPIINTALSEL","KPI Interrupt active level select" },
	{ 4, 0x0210, "KPIINTFLAG", "KPI Interrupt active flag" },
	{ 4, 0x0214, "KPIOPIOD",   "KPIO Port input/output data" },
	{ 4, 0x0218, "KPOBFSTAT",  "KPO Ouput data in batt_fault status" },
	{ 4, 0x021c, "KPIINTSTAT", "KPI Interrupt status" },
	{ 4, 0x023c, "KALT",       "KIU alternate function" },

	{ 4, 0x0400, "SPICR",      "SPI control register" },
	{ 4, 0x0404, "SPIDR",      "SPI data register" },
	{ 4, 0x0408, "SPIDCS",     "SPI chip select disabled register" },

	{ 4, 0x0600, "PWM0_TBS",   "PWM Time base set register" },
	{ 4, 0x0604, "PWM0_PTS",   "PWM Period time set register" },
	{ 4, 0x0608, "PWM0_DTS",   "PWM Duty time set register" },

	{ 4, 0x0700, "PWM1_TBS",   "PWM Time base set register" },
	{ 4, 0x0704, "PWM1_PTS",   "PWM Period time set register" },
	{ 4, 0x0708, "PWM1_DTS",   "PWM Duty time set register" },

	{ 4, 0x0800, "LED0_TBS",   "LED time base set register" },
	{ 4, 0x0804, "LED0_PTS",   "LED period time set register" },
	{ 4, 0x0808, "LED0_DTS",   "LED duty time set register" },
	{ 4, 0x080c, "LED0_ASTC",  "LED auto stop counter register" },

	{ 4, 0x0880, "LED1_TBS",   "LED time base set register" },
	{ 4, 0x0884, "LED1_PTS",   "LED period time set register" },
	{ 4, 0x0888, "LED1_DTS",   "LED duty time set register" },
	{ 4, 0x088c, "LED1_ASTC",  "LED auto stop counter register" },

	{ 4, 0x0900, "LED2_TBS",   "LED time base set register" },
	{ 4, 0x0904, "LED2_PTS",   "LED period time set register" },
	{ 4, 0x0908, "LED2_DTS",   "LED duty time set register" },
	{ 4, 0x090c, "LED2_ASTC",  "LED auto stop counter register" },

	{ 4, 0x0a00, "UART0_BUF",  "Receive/transmit buffer"},
	{ 4, 0x0a04, "UART0_IER",  "Interrupt enable" },
	{ 4, 0x0a08, "UART0_IIR",  "Interrupt identify" },
	{ 4, 0x0a08, "UART0_FCR",  "Fifo control" },
	{ 4, 0x0a0c, "UART0_LCR",  "Line control" },
	{ 4, 0x0a10, "UART0_MCR",  "Modem control" },
	{ 4, 0x0a14, "UART0_LSR",  "Line status" },
	{ 4, 0x0a18, "UART0_MSR",  "Modem status" },
	{ 4, 0x0a1c, "UART0_SCR",  "Scratch pad" },

	{ 4, 0x0c00, "UART1_BUF",  "Receive/transmit buffer"},
	{ 4, 0x0c04, "UART1_IER",  "Interrupt enable" },
	{ 4, 0x0c08, "UART1_IIR",  "Interrupt identify" },
	{ 4, 0x0c08, "UART1_FCR",  "Fifo control" },
	{ 4, 0x0c0c, "UART1_LCR",  "Line control" },
	{ 4, 0x0c10, "UART1_MCR",  "Modem control" },
	{ 4, 0x0c14, "UART1_LSR",  "Line status" },
	{ 4, 0x0c18, "UART1_MSR",  "Modem status" },
	{ 4, 0x0c1c, "UART1_SCR",  "Scratch pad" },

	{ 4, 0x0e00, "TIMER_0",    "Timer counter 0 register" },
	{ 4, 0x0e04, "TIMER_1",    "Timer counter 1 register" },
	{ 4, 0x0e08, "TIMER_2",    "Timer counter 2 register" },
	{ 4, 0x0e0a, "TIMER_CNTL", "Timer control register (write only)" },
	{ 4, 0x0e10, "TIMER_CMD",  "Timer command register" },

	{ 4, 0x1000, "CDEX",       "Crystal source, control clock" },

	{ 4, 0x1200, "ADMUX",      "ADC multiplixer select register" },
	{ 4, 0x1204, "ADCSR",      "ADC control and status register" },
	{ 4, 0x1208, "ADCDR",      "ADC data register" },
	
	{ 4, 0x1600, "INTMASK",    "Interrupt mask control & cold boot flag" },
	{ 4, 0x1604, "INTCPS",     "Interrupt timer clock pre-scale" },
	{ 4, 0x1608, "INTTBS",     "Interrupt timer set" },

	{ 4, 0x1800, "OWM_CMD",    "OWM command register" },
	{ 4, 0x1804, "OWM_DATA",   "OWM transmit/receive buffer" },
	{ 4, 0x1808, "OWM_INT",    "OWM interrupt register" },
	{ 4, 0x180c, "OWM_INTEN",  "OWM interrupt enable register" },
	{ 4, 0x1810, "OWM_CLKDIV", "OWM clock divisor register" },

	{ 4, 0x1a00, "SSETR",      "Size of flash memory setting register" },

	{ 2, 0x1c00, "MMC_STR_STP_CLK",      "MMC Start/Stop Clock" },
	{ 2, 0x1c04, "MMC_STATUS",           "MMC Status" },
	{ 2, 0x1c08, "MMC_CLK_RATE",         "MMC Clock rate" },
	{ 2, 0x1c0c, "MMC_REVISION",         "MMC revision" },
	{ 2, 0x1c10, "MMC_SPI_REG",          "MMC SPI register" },
	{ 2, 0x1c14, "MMC_CMD_DATA_CONT",    "MMC command data control"},
	{ 2, 0x1c18, "MMC_RESPONSE_TO",      "MMC response timeout" }, 
	{ 2, 0x1c1c, "MMC_READ_TO",          "MMC read timeout" }, 
	{ 2, 0x1c20, "MMC_BLK_LEN",          "MMC block length" },
	{ 2, 0x1c24, "MMC_NOB",              "MMC number of blocks" },
	{ 2, 0x1c34, "MMC_INT_MASK",         "MMC interrupt mask" },
	{ 2, 0x1c38, "MMC_CMD",              "MMC command number" },
	{ 2, 0x1c3c, "MMC_ARGUMENT_H",       "MMC argument high word" },
	{ 2, 0x1c40, "MMC_ARGUMENT_L",       "MMC argument low word" },
	{ 2, 0x1c44, "MMC_RES_FIFO",         "MMC response fifo" },
	{ 2, 0x1c4c, "MMC_DATA_BUF",         "MMC data buffer" },
	{ 2, 0x1c50, "MMC_BUF_PART_FULL",    "MMC buffer part full" },

	{ 2, 0x1e60, "GPIO1_MASK",           "GPIO1 mask" },
	{ 2, 0x1e64, "GPIO1_DIR",            "GPIO1 direction" },
	{ 2, 0x1e68, "GPIO1_OUT",            "GPIO1 output level" },
	{ 2, 0x1e6c, "GPIO1_LEVELTRI",       "GPIO1 level trigger (0=edge trigger)" },
	{ 2, 0x1e70, "GPIO1_RISING",         "GPIO1 rising edge trigger" },
	{ 2, 0x1e74, "GPIO1_LEVEL",          "GPIO1 high level trigger" },
	{ 2, 0x1e78, "GPIO1_LEVEL_STATUS",   "GPIO1 level trigger sttaus" },
	{ 2, 0x1e7c, "GPIO1_EDGE_STATUS",    "GPIO1 edge trigger status" },
	{ 2, 0x1e80, "GPIO1_STATE",          "GPIO1 state (read only)" },
	{ 2, 0x1e84, "GPIO1_RESET",          "GPIO1 reset" },
	{ 2, 0x1e88, "GPIO1_SLEEP_MASK",     "GPIO1 sleep mask trigger" },
	{ 2, 0x1e8c, "GPIO1_SLEEP_DIR",      "GPIO1 sleep direction" },
	{ 2, 0x1e90, "GPIO1_SLEEP_OUT",      "GPIO1 sleep level" },
	{ 2, 0x1e94, "GPIO1_STATUS",         "GPIO1 status (read only)" },
	{ 2, 0x1e98, "GPIO1_BATT_FAULT_DIR", "GPIO1 battery fault direction" },
	{ 2, 0x1e9c, "GPIO1_BATT_FAULT_OUT", "GPIO1 battery fault level" },

	{ 4, 0x1f00, "FLASHWP",    "Flash write protect" },
};

#define NUM_OF_ASIC_REG_ENTRY	ARRAY_SIZE(asic_regs)

static int proc_read_reg(struct file * file, char * buf,
		size_t nbytes, loff_t *ppos)
{
	int i_ino = (file->f_dentry->d_inode)->i_ino;
	char outputbuf[15];
	int count;
	int i;
	asic_reg_entry_t* current_reg=NULL;
	if (*ppos>0) /* Assume reading completed in previous read*/
		return 0;
	for (i=0;i<NUM_OF_ASIC_REG_ENTRY;i++) {
		if (asic_regs[i].low_ino==i_ino) {
			current_reg = &asic_regs[i];
			break;
		}
	}
	if (current_reg==NULL)
		return -EINVAL;

	switch (current_reg->bytes) {
	case 1:
		count = sprintf(outputbuf, "0x%02X\n", *((volatile u8 *)(_H3800_ASIC2_Base + current_reg->phyaddr)));
		break;
	case 2:
		count = sprintf(outputbuf, "0x%04X\n", *((volatile u16 *)(_H3800_ASIC2_Base + current_reg->phyaddr)));
		break;
	case 4:
	default:
		count = sprintf(outputbuf, "0x%08X\n", *((volatile u32 *)(_H3800_ASIC2_Base + current_reg->phyaddr)));
		break;
	}
	*ppos+=count;
	if (count>nbytes)  /* Assume output can be read at one time */
		return -EINVAL;
	if (copy_to_user(buf, outputbuf, count))
		return -EFAULT;
	return count;
}

static ssize_t proc_write_reg(struct file * file, const char * buffer,
		size_t count, loff_t *ppos)
{
	int i_ino = (file->f_dentry->d_inode)->i_ino;
	asic_reg_entry_t* current_reg=NULL;
	int i;
	unsigned long newRegValue;
	char *endp;

	for (i=0;i<NUM_OF_ASIC_REG_ENTRY;i++) {
		if (asic_regs[i].low_ino==i_ino) {
			current_reg = &asic_regs[i];
			break;
		}
	}
	if (current_reg==NULL)
		return -EINVAL;

	newRegValue = simple_strtoul(buffer,&endp,0);
	switch (current_reg->phyaddr) {
	case 1:
		*((volatile u8 *)(_H3800_ASIC2_Base + current_reg->phyaddr))=newRegValue;
		break;
	case 2:
		*((volatile u16 *)(_H3800_ASIC2_Base + current_reg->phyaddr))=newRegValue;
		break;
	case 4:
	default:
		*((volatile u32 *)(_H3800_ASIC2_Base + current_reg->phyaddr))=newRegValue;
		break;
	}
	return (count+endp-buffer);
}

struct simple_proc_entry {
	char *        name;
	read_proc_t * read_proc;
};

const struct simple_proc_entry sproc_list[] = {
	{ "stats",      ipaq_asic_proc_stats_read },
	{ "battery",    ipaq_asic_proc_battery },
	{ "ds2760",     ipaq_asic_proc_battery_ds2760 },
	{ "ds2760_raw", ipaq_asic_proc_battery_raw },
	{ "wakeup",     ipaq_asic_proc_wakeup_read },
};

static int ipaq_asic_register_procfs( void )
{
	int i;

	asic_proc_dir = proc_mkdir(IPAQ_ASIC_PROC_DIR, NULL);
	if ( !asic_proc_dir ) {
		printk(KERN_ALERT  
		       "%s: unable to create proc entry %s\n", __FUNCTION__, IPAQ_ASIC_PROC_DIR);
		return -ENOMEM;
	}

	for ( i = 0 ; i < ARRAY_SIZE(sproc_list) ; i++ )
		create_proc_read_entry(sproc_list[i].name, 0, asic_proc_dir, 
				       sproc_list[i].read_proc, NULL);

	adc_proc_dir = proc_mkdir("adc", asic_proc_dir);
	if (adc_proc_dir == NULL) {
		printk(KERN_ERR "%s: can't create /proc/adc\n", __FUNCTION__);
		return(-ENOMEM);
	}
	for (i=0;i<5;i++) {
		unsigned char name[10];
		sprintf(name,"%d",i);
		create_proc_read_entry(name, S_IWUSR |S_IRUSR | S_IRGRP | S_IROTH, 
					       adc_proc_dir, ipaq_asic_proc_adc_read, (void *) i );
	}

	reg_proc_dir = proc_mkdir(REG_DIRNAME, asic_proc_dir);
	if (reg_proc_dir == NULL) {
		printk(KERN_ERR "%s: can't create /proc/%s\n", __FUNCTION__, REG_DIRNAME);
		return(-ENOMEM);
	}

	for(i=0;i<NUM_OF_ASIC_REG_ENTRY;i++) {
		struct proc_dir_entry *entry = create_proc_entry(asic_regs[i].name,
								 S_IWUSR |S_IRUSR | S_IRGRP | S_IROTH,
								 reg_proc_dir);
		
		if ( !entry ) {
			printk( KERN_ERR "%s: can't create /proc/%s/%s\n", __FUNCTION__, REG_DIRNAME,
				asic_regs[i].name);
			return(-ENOMEM);
		}

		asic_regs[i].low_ino = entry->low_ino;
		entry->proc_fops = &proc_reg_operations;
	}

	ipaq_asic_sysctl_header = register_sysctl_table(ipaq_asic_dir_table, 0 );
	return 0;
}

static void ipaq_asic_unregister_procfs( void )
{
	int i;
	if ( asic_proc_dir ) {
		unregister_sysctl_table(ipaq_asic_sysctl_header);
		for(i=0;i<NUM_OF_ASIC_REG_ENTRY;i++)
			remove_proc_entry(asic_regs[i].name,reg_proc_dir);
		remove_proc_entry(REG_DIRNAME, asic_proc_dir);
		for(i=0;i<5;i++) {
			char name[10];
			sprintf(name,"%d",i);
			remove_proc_entry(name,adc_proc_dir);
		}
		remove_proc_entry("adc",asic_proc_dir);

		for (i=0 ; i<ARRAY_SIZE(sproc_list) ; i++ )
			remove_proc_entry(sproc_list[i].name, asic_proc_dir);

		remove_proc_entry(IPAQ_ASIC_PROC_DIR, NULL );
		asic_proc_dir = NULL;
	}
}

/***********************************************************************************
 *   Utility routines
 ***********************************************************************************/

struct ipaq_asic_device_info {
	__u32 device_id;
	struct device_driver *driver;
};

/* 
   We initialize and resume from top to bottom,
   cleanup and suspend from bottom to top 
*/
   
struct ipaq_asic_device_info ipaq_asic_system_devices[] = {
	{ 
		.device_id = IPAQ_ASIC2_ADC_DEVICE_ID,
		.driver    = &ipaq_asic2_adc_device_driver
	},{ 
		.device_id = IPAQ_ASIC2_KEY_DEVICE_ID,
		.driver    = &ipaq_asic2_key_device_driver
	},{ 
		.device_id = IPAQ_ASIC2_SPI_DEVICE_ID,
		.driver    = &ipaq_asic2_spi_device_driver
	},{ 
		.device_id = IPAQ_ASIC2_BACKLIGHT_DEVICE_ID,
		.driver    = &ipaq_asic_backlight_platform_driver
	},{ 
		.device_id = IPAQ_ASIC2_TOUCHSCREEN_DEVICE_ID,
		.driver    = &ipaq_asic2_touchscreen_device_driver
	},{ 
		.device_id = IPAQ_ASIC2_ONEWIRE_DEVICE_ID,
		.driver    = &ipaq_asic2_onewire_device_driver
	},{ 
		.device_id = IPAQ_ASIC2_BATTERY_DEVICE_ID,
		.driver    = &ipaq_asic2_battery_device_driver
	},{ 
		.device_id = IPAQ_ASIC_AUDIO_DEVICE_ID,
		.driver    = &ipaq_asic_audio_device_driver
#ifdef CONFIG_BLUETOOTH
	},{
		.device_id = IPAQ_ASIC2_BLUETOOTH_DEVICE_ID,
		.driver    = &ipaq_asic2_bluetooth_device_driver
#endif
#ifdef CONFIG_MMC
	},{
		.device_id = IPAQ_ASIC1_MMC_DEVICE_ID,
		.driver    = &ipaq_asic1_mmc_device_driver
#endif
	}
};

static int ipaq_asic_register_drivers_and_devices( void )
{
	int i;
	int result;
	for (i = 0; i < ARRAY_SIZE(ipaq_asic_system_devices); i++) {
		struct ipaq_asic_device_info *info = &ipaq_asic_system_devices[i];
		info->driver->driver.bus = &asic2_bus_type;
		result = driver_register(info->driver);
		if (result)
			return result;
	}
	for (i = 0; i < ARRAY_SIZE(ipaq_asic_system_devices); i++) {
		struct ipaq_asic_device_info *info = &ipaq_asic_system_devices[i];
		struct platform_device *platform_dev = (struct platform_device *)kmalloc(sizeof(struct platform_device), GFP_KERNEL);

		strcpy(platform_dev->device.name, info->driver->driver.name);
		platform_dev->device.parent = &asic2_device;
		platform_dev->device.bus_id = i;
		platform_dev->device.bus = &asic2_bus_type;

		result = platform_platform_device_register(platform_dev);
	}
	return 0;
}

/***********************************************************************************
 *   Power management
 *
 *   On sleep, if we return anything other than "0", we will cancel sleeping.
 *
 *   On resume, if we return anything other than "0", we will put the iPAQ
 *     back to sleep immediately.
 ***********************************************************************************/

static int ipaq_asic_suspend(struct device *dev)
{
	int result;
	H3800_ASIC2_INTR_MaskAndFlag &= ~ASIC2_INTMASK_GLOBAL;
	result = ipaq_asic_suspend_handlers();
	H3800_ASIC2_KPIINTSTAT  = 0;    
	H3800_ASIC2_GPIINTSTAT  = 0;
	ipaq_asic_initiate_sleep ();
	return result;
}

static int ipaq_asic_resume(struct device *dev)
{
	int result;
	MSC2 = (MSC2 & 0x0000ffff) | 0xE4510000;  /* Set MSC2 correctly */
	result = ipaq_asic_check_wakeup ();
	if ( !result ) {
		/* These probably aren't necessary */
		H3800_ASIC2_KPIINTSTAT     =  0;     /* Disable all interrupts */
		H3800_ASIC2_GPIINTSTAT     =  0;

		H3800_ASIC2_KPIINTCLR      =  0xffff;     /* Clear all KPIO interrupts */
		H3800_ASIC2_GPIINTCLR      =  0xffff;     /* Clear all GPIO interrupts */

		H3800_ASIC2_CLOCK_Enable       |= ASIC2_CLOCK_EX0;   /* 32 kHZ crystal on */
		H3800_ASIC2_INTR_ClockPrescale |= ASIC2_INTCPS_SET;
		H3800_ASIC2_INTR_ClockPrescale  = ASIC2_INTCPS_CPS(0x0e) | ASIC2_INTCPS_SET;
		H3800_ASIC2_INTR_TimerSet       = 1;

		ipaq_asic_resume_handlers();
		H3800_ASIC2_INTR_MaskAndFlag  |= ASIC2_INTMASK_GLOBAL; /* Turn on ASIC interrupts */
	}
	return result;
}

static int ipaq_asic_reset_handler(ctl_table *ctl, int write, struct file * filp,
				    void *buffer, size_t *lenp)
{
	MOD_INC_USE_COUNT;
	ipaq_asic_suspend( NULL );
	ipaq_asic_resume( NULL );
	MOD_DEC_USE_COUNT;

	return 0;
}


/***********************************************************************************
 *   Initialization code
 ***********************************************************************************/

static void ipaq_asic_cleanup( void )
{
	h3600_unregister_pm_callback( ipaq_asic_pm_callback );
	ipaq_asic_unregister_procfs();
	ipaq_asic_cleanup_handlers();
	ipaq_asic_shared_cleanup(); 
	ipaq_asic_release_isr();
}

int __init ipaq_asic_init( void )
{
	int result;

	if ( !machine_is_h3800() ) {
		printk("%s: unknown iPAQ model %s\n", __FUNCTION__, h3600_generic_name() );
		return -ENODEV;
	}

	/* We need to set nCS5 without changing nCS4 */
	/* Properly, this should be set in the bootldr */
	MSC2 = (MSC2 & 0x0000ffff) | 0xE4510000;

	result = ipaq_asic_init_isr();  
	if ( result ) return result;

	ipaq_asic_register_battery_ops (&h3800_battery_ops);

	result = ipaq_asic_shared_init();
	if ( result ) goto init_fail;
		
	result = ipaq_asic_register_drivers_and_devices();
	if ( result ) goto init_fail;

	result = ipaq_asic_register_procfs();
	if ( result ) goto init_fail;

	h3600_register_pm_callback( ipaq_asic_pm_callback );

	H3800_ASIC2_INTR_MaskAndFlag  |= ASIC2_INTMASK_GLOBAL; /* Turn on ASIC interrupts */
	return 0;

init_fail:
	printk("%s: FAILURE!  Exiting...\n", __FUNCTION__);
	ipaq_asic_cleanup();
	return result;
}

void __exit ipaq_asic_exit( void )
{
	if ( !machine_is_h3800() ) 
		return;
	ipaq_asic_cleanup();
	H3800_ASIC2_INTR_MaskAndFlag &= ~ASIC2_INTMASK_GLOBAL;
}

module_init(ipaq_asic_init)
module_exit(ipaq_asic_exit)

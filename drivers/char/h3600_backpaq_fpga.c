/*
*
* Driver for the HP iPAQ Mercury Backpaq FPGA programming interface
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
* 
* This character driver provides a programming interface to the Xilinx FPGA
* on the Mercury Backpaq.  To program the device, it suffices to execute:
*
*    cat fpga_file_name.bin > /dev/backpaq
*
* To check on the status of the FPGA, call:
*
*    cat /proc/backpaq
*
* 
* ToDo:
* 
*     1. Use the hardware_version bit to determine how many bytes should
*        be sent to the backpaq and return an appropriate error code.
*                   
* Author: Andrew Christian
*         <andyc@handhelds.org>
*/

#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>
#include <asm/uaccess.h>         /* get_user,copy_to_user*/
#include <linux/vmalloc.h>

#include <linux/proc_fs.h> 
#include <linux/pm.h>
#include <asm/arch-sa1100/backpaq.h>
#include <linux/delay.h>
#include <linux/devfs_fs_kernel.h>

struct h3600_backpaq_fpga_dev_struct {
	unsigned int  usage_count;     /* Number of people currently using this device */
	unsigned long busy_count;      /* Number of times we've had to wait for EMPTY bit */
	unsigned long bytes_written;   /* Bytes written in the most recent open/close */
};

#define MODULE_NAME      "h3600_backpaq_fpga"

#define FPGA_MINOR        0
#define FPGA_DEVICE_NAME "backpaq/fpga"

#define FPGA_PROC_DIR  "backpaq"
#define FPGA_PROC_NAME "backpaq/fpga"

/* Local variables */

static struct proc_dir_entry   *proc_backpaq_fpga;
static devfs_handle_t           devfs_fpga;

static struct h3600_backpaq_fpga_dev_struct  h3600_backpaq_fpga_data;
static int    h3600_backpaq_fpga_major_num = 0;

/**************************************************************/

enum {
	FPGA_READY      = 0,
	FPGA_NO_BACKPAQ,
	FPGA_CRC_ERROR,
	FPGA_NOT_DONE_PROGRAMMING,
	FPGA_NOT_PROGRAMMED,
	FPGA_BAD_DATA_FILE_SIZE
};

static char * h3600_backpaq_fpga_strings[] = {
	"Okay",
	"No backpaq present",
	"CRC error",
	"Not done programming",
	"Not programmed",
	"Bad data file size",
};

/* Refer to backpaq.h for list of EEPROM types */
static int valid_fpga_sizes[] = {
	97652,       /* BACKPAQ_EEPROM_FPGA_VERTEX_100  */
	107980,      /* BACKPAQ_EEPROM_FPGA_VERTEX_100E */
	0,           /* BACKPAQ_EEPROM_FPGA_VERTEX_200E (unknown size) */
        234456       /* BACKPAQ_EEPROM_FPGA_VERTEX_300E */
};

int h3600_backpaq_valid_fpga_size( int len )
{
	int version = h3600_backpaq_eeprom_shadow.fpga_version;

	if ( version >= 0 && version < BACKPAQ_EEPROM_FPGA_MAXIMUM 
	     && (valid_fpga_sizes[version] == 0 || valid_fpga_sizes[version] == len))
		return 1;

	return 0; /* Not legal program */
}



int h3600_backpaq_fpga_state( void  )
{
	if ( !h3600_backpaq_present() )
		return FPGA_NO_BACKPAQ;
	     
	if ( !(BackpaqSysctlFPGAStatus & BACKPAQ_FPGASTATUS_INITL) )
		return FPGA_CRC_ERROR;

	if ( h3600_backpaq_fpga_data.bytes_written == 0 )
		return FPGA_NOT_PROGRAMMED;

	if ( !(BackpaqSysctlFPGAStatus & BACKPAQ_FPGASTATUS_DONE))
		return FPGA_NOT_DONE_PROGRAMMING;

	if ( !h3600_backpaq_valid_fpga_size(h3600_backpaq_fpga_data.bytes_written) ) 
		return FPGA_BAD_DATA_FILE_SIZE;

	return FPGA_READY;
}

int h3600_backpaq_fpga_status( void )
{
	int result = -ENODEV;

	switch (h3600_backpaq_fpga_state()) {
	case FPGA_READY:
		return 0;
	case FPGA_NO_BACKPAQ:
		return -ENODEV;
	}
	return result;
}

EXPORT_SYMBOL(h3600_backpaq_fpga_status);

/**************************************************************/

static __inline__ int backpaq_get_18ven(void) 
{
        if (h3600_backpaq_eeprom_shadow.major_revision == 3)
                return BackpaqSysctlGenControl & BACKPAQ3_GENCTL_18VEN;
        else
                return BackpaqSysctlPCMCIAPower & BACKPAQ2_PWRCTL_18VEN;
}

static __inline__ void backpaq_assign_18ven(u8 value)
{
        int is_backpaq3 = (h3600_backpaq_eeprom_shadow.major_revision == 3);
        u16 curval = (is_backpaq3 ? BackpaqSysctlGenControl : BackpaqSysctlPCMCIAPower);
        u16 newbit = (is_backpaq3 ? BACKPAQ3_GENCTL_18VEN : BACKPAQ2_PWRCTL_18VEN);
        u16 newval;
        if (value)
                newval = curval | newbit;
        else
                newval = curval & ~newbit;
        if (is_backpaq3)
                BackpaqSysctlGenControl = newval;
        else
                BackpaqSysctlPCMCIAPower = newval;
}

static __inline__ void backpaq_reset_fpga(void)
{
        int is_backpaq3 = (h3600_backpaq_eeprom_shadow.major_revision == 3);
        if (is_backpaq3)
	    BackpaqSysctlFPGAControl |= BACKPAQ3_FPGACTL_RESET;
        else
	    BackpaqFPGAReset &= ~BACKPAQ2_FPGA_RESET;

}
static __inline__ void backpaq_unreset_fpga(void)
{
        int is_backpaq3 = (h3600_backpaq_eeprom_shadow.major_revision == 3);
        if (is_backpaq3)
	    BackpaqSysctlFPGAControl &= ~BACKPAQ3_FPGACTL_RESET;
        else	
	    BackpaqFPGAReset |= BACKPAQ2_FPGA_RESET;
}


/**************************************************************/

/*
 * Write function.
 * 
 * To prevent CPU deadlock, we write a limited number of bytes and then
 * return to the caller.
 *
 * The potential difficulty with this approach is that we perform a
 * kernel-to-user mode context switch each time this is called, which
 * is not time-efficient.
 *
 * A potential work around is to allow the function to block until 
 * all data is written, but insert "schedule()" calls periodically 
 * to allow other processes to get time. 
 */

#define MY_MAX_WRITE  512

ssize_t h3600_backpaq_fpga_write( struct file *filp, const char *buf, 
				  size_t count, loff_t *f_pos )
{
	unsigned char mybuf[MY_MAX_WRITE];
	unsigned char *pbuf;
	int bytes_to_write;
	int result;
	int i;

	if ( !h3600_backpaq_present())
		return -ENODEV;

	if ( count <= 0 )
		return 0;  /* Not an error to write nothing? */

	bytes_to_write = ( count < MY_MAX_WRITE ? count : MY_MAX_WRITE );

	/* Copy over from user space */
	result = copy_from_user( mybuf, buf, bytes_to_write );
	if ( result ) 
		return result;

	/* Write out the bytes */
	pbuf = mybuf;
	for ( i = 0 ; i < bytes_to_write ; pbuf++, i++ ) {
		/* Wait for the CPLD to signal it's ready for the next byte */
		while (!(BackpaqSysctlFPGAStatus & BACKPAQ_FPGASTATUS_EMPTY))
			h3600_backpaq_fpga_data.busy_count++;
		
		/* Write *pbuf to the FPGA */
		BackpaqSysctlFPGAProgram = *pbuf;
		udelay(1); // the CPLD can't always keep up :( adds at most .25 seconds)
	    
	}

	h3600_backpaq_fpga_data.bytes_written += i;
	return i;
}

/* 
 * On open we place the FPGA in a known programming state
 */

int setup_fpga( void )
{
	if (h3600_backpaq_eeprom_shadow.major_revision == 3)
		BackpaqSysctlGenControl |= BACKPAQ3_GENCTL_18VEN;
	else
		BackpaqSysctlPCMCIAPower |= BACKPAQ2_PWRCTL_18VEN;

	/* Turn on audio and camera clocks */
	BackpaqSysctlGenControl |= (BACKPAQ3_GENCTL_CAM_CLKEN | BACKPAQ3_GENCTL_AUDIO_CLKEN
                                    | BACKPAQ2_GENCTL_CAM_CLKEN | BACKPAQ2_GENCTL_AUDIO_CLKEN);
        printk("-3- BackpaqSysctlGenControl=%x\n", BackpaqSysctlGenControl);
	/* Clear the control registers to wipe out memory */
	BackpaqSysctlFPGAControl = BACKPAQ_FPGACTL_M0 | BACKPAQ_FPGACTL_M1 | BACKPAQ_FPGACTL_M2;
	/* Wait 100 ns */
	udelay( 1 );       /* Wait for 1 microsecond */
	/* Put the FPGA into program mode */
	BackpaqSysctlFPGAControl = (BACKPAQ_FPGACTL_M0 | BACKPAQ_FPGACTL_M1 | BACKPAQ_FPGACTL_M2 
				    | BACKPAQ_FPGACTL_PROGRAM);
	/* We could run a few sanity checks here */
	return 0;
}


/*
 * On close we need to verify that the correct number of bytes were
 * written and that the FPGA has asserted its "I've been programmed"
 * flag. 
 *
 * If the FPGA hasn't beepn programmed, we return an error code.
 */

int shutdown_fpga( void )
{
	if ( !h3600_backpaq_present()) {
		h3600_backpaq_fpga_data.bytes_written = 0;
		return -ENODEV;
	}

	/* Make sure we've finished programming */
	while (!(BackpaqSysctlFPGAStatus & BACKPAQ_FPGASTATUS_EMPTY))
		h3600_backpaq_fpga_data.busy_count++;

	/* Reset the FPGA */
	
	BackpaqSysctlFPGAControl = (BACKPAQ_FPGACTL_M0 | BACKPAQ_FPGACTL_M2 
                                    | BACKPAQ_FPGACTL_PROGRAM);


	backpaq_reset_fpga();	
	udelay(2);   /* Wait for 1 microsecond */

	/* Unreset the FPGA */
	backpaq_unreset_fpga();

/*  	printk("-5- BackpaqSysctlFPGAStatus=%x\n", BackpaqSysctlFPGAStatus); */
/*  	printk("-5- BackpaqSysctlGenControl=%x\n", BackpaqSysctlGenControl); */
/*  	printk("-5- byteswritten=%x\n", h3600_backpaq_fpga_data.bytes_written); */
/*  	printk("-5- busycount=%x\n", h3600_backpaq_fpga_data.busy_count); */
/*  	printk("-5- BackpaqFPGAFirmwareVersion=%x\n", BackpaqFPGAFirmwareVersion); */

	/* Check for illegal states */
  	if ( !(BackpaqSysctlFPGAStatus & BACKPAQ_FPGASTATUS_INITL) ) { 
 		h3600_backpaq_fpga_data.bytes_written = 0; 
                  printk(__FUNCTION__ ": CRC error\n"); 
  		return -EIO;      /* CRC error */ 
  	} 

	if ( !(BackpaqSysctlFPGAStatus & BACKPAQ_FPGASTATUS_DONE) ) {
		h3600_backpaq_fpga_data.bytes_written = 0;
                printk(__FUNCTION__ ": incomplete file\n");
		return -EIO;        /* Incomplete file */
	}

	/* This should be updated to reflect the type of FPGA we're programming */
	if ( !h3600_backpaq_valid_fpga_size( h3600_backpaq_fpga_data.bytes_written)) {
                printk(__FUNCTION__ ": invalid size\n");
		return -EIO;
        }

	return 0;
}
  
int h3600_backpaq_fpga_open( struct inode *inode, struct file *filp )
{
	int result = 0;
/*
	if ( BackpaqSysctl->hardware_version != BACKPAQ_HARDWARE_VERSION_1 )
		return -ENXIO;
*/
	if ( h3600_backpaq_fpga_data.usage_count > 0 )
		return -EBUSY;

	if ( !h3600_backpaq_present() ) {
		h3600_backpaq_fpga_data.bytes_written = 0;
		return -ENODEV;
	}

	if ( (result = setup_fpga()) != 0 )
		return result;

	h3600_backpaq_fpga_data.usage_count++;
	h3600_backpaq_fpga_data.busy_count = 0;
	h3600_backpaq_fpga_data.bytes_written = 0;

	MOD_INC_USE_COUNT;
	return 0;    /* Success */
}

int h3600_backpaq_fpga_release( struct inode *inode, struct file *filp )
{
	int result;

	result = shutdown_fpga();
	h3600_backpaq_fpga_data.usage_count--;

	MOD_DEC_USE_COUNT;
	return result;
}


/***************************************************************
 *  /proc/backpaq
 ***************************************************************/

#define PRINT_GENERAL_REG(x,s) \
	p += sprintf (p, "%lx %-24s : %04x\n", (unsigned long) &x, s, x)
#define PRINT_SYSCTL_REG(x,s)   PRINT_GENERAL_REG(BackpaqSysctl ## x,s)
#define PRINT_FPGA_REG(x,s)   PRINT_GENERAL_REG(BackpaqFPGA ## x,s)

static int proc_h3600_backpaq_read(char *page, char **start, off_t off,
			  int count, int *eof, void *data)
{
	char *p = page;
	int len;

	if (!h3600_backpaq_present()) {
		p += sprintf(p,"No backpaq present\n");
	}
	else {
		PRINT_SYSCTL_REG(MajorVersion,   "Major version");
		PRINT_SYSCTL_REG(FirmwareVersion,"Firmware version");
		PRINT_SYSCTL_REG(FPGAControl,    "FPGA control");
		PRINT_SYSCTL_REG(FPGAStatus,     "FPGA status");
		PRINT_SYSCTL_REG(FPGAProgram,    "FPGA program");
		PRINT_SYSCTL_REG(PCMCIAPower,    "PCMCIA power");
		PRINT_SYSCTL_REG(FlashControl,   "Flash control");
		PRINT_SYSCTL_REG(GenControl,     "General control");

		PRINT_FPGA_REG(FirmwareVersion,  "FPGA firmware rev");
		PRINT_FPGA_REG(CameraLiveMode,       "Camera live mode");
		PRINT_FPGA_REG(CameraIntegrationTime,"Camera integration time");
		PRINT_FPGA_REG(CameraClockDivisor,   "Camera clock divisor");
		PRINT_FPGA_REG(CameraFifoInfo,       "Camera fifo information");
		PRINT_FPGA_REG(CameraFifoDataAvail,  "Camera fifo data available");
		PRINT_FPGA_REG(CameraFifoStatus,     "Camera fifo status");
		PRINT_FPGA_REG(CameraRowCount,       "Camera row count");
		PRINT_FPGA_REG(CameraIntegrationCmd, "Camera integration command");
		PRINT_FPGA_REG(CameraInterruptFifo,  "Camera interrupt fifo");
		PRINT_FPGA_REG(InterruptMask,        "Interrupt mask");
		PRINT_FPGA_REG(InterruptStatus,      "Interrupt status");

		p += sprintf(p, "         Busy count        : %ld\n", 
			     h3600_backpaq_fpga_data.busy_count );
		p += sprintf(p, "         Bytes written     : %ld\n", 
			     h3600_backpaq_fpga_data.bytes_written );
		p += sprintf(p, "         Usage count       : %d\n", 
			     h3600_backpaq_fpga_data.usage_count );

		p += sprintf(p, "FPGA program status        : ");
		p += sprintf(p, h3600_backpaq_fpga_strings[ h3600_backpaq_fpga_state() ]);
		p += sprintf(p, "\n");
	}

	len = (p - page) - off;
	if (len < 0)
		len = 0;

	*eof = (len <= count) ? 1 : 0;
	*start = page + off;

	return len;
}

/***********************************************************************************/

/* power management skeleton */

#ifdef CONFIG_PM
static struct h3600_backpaq_device *fpga_backpaq_dev;

static int h3600_backpaq_fpga_callback(struct h3600_backpaq_device *device, 
				       h3600_backpaq_request_t req )
{
	printk(__FUNCTION__ ": fpga backpaq callback %d\n", req);
	switch (req) {
	case H3600_BACKPAQ_EJECT:
		h3600_backpaq_fpga_data.bytes_written = 0;
		break;
	case H3600_BACKPAQ_INSERT:
		/* We let the hotplug routine handle this */
		break;
	case H3600_BACKPAQ_SUSPEND: 
		h3600_backpaq_fpga_data.bytes_written = 0;
                break;
	case H3600_BACKPAQ_RESUME:
		break;
        }
        return 0;
}
#endif /* CONFIG_PM */

/***********************************************************************************/


struct file_operations h3600_backpaq_fpga_fops = {
	write:   h3600_backpaq_fpga_write,
	open:    h3600_backpaq_fpga_open,
	release: h3600_backpaq_fpga_release,
};

/***********************************************************************************/

int __init h3600_backpaq_fpga_init_module(void)    
{    
	int result;    
	printk(KERN_ALERT __FILE__ ": registering char device");    

	/* Register my device driver */
	result = devfs_register_chrdev(0,MODULE_NAME, &h3600_backpaq_fpga_fops);    
	if ( result <= 0 ) {
		printk(" can't get major number\n");
		return result;    
	}    
	if ( h3600_backpaq_fpga_major_num == 0 )
		h3600_backpaq_fpga_major_num = result;
	printk(" %d\n", h3600_backpaq_fpga_major_num);

	/* Clear the default structure */
 	memset(&h3600_backpaq_fpga_data, 0, sizeof(struct h3600_backpaq_fpga_dev_struct));

	/* Create a devfs entry */
#ifdef CONFIG_DEVFS_FS
	devfs_fpga = devfs_register( NULL, FPGA_DEVICE_NAME, DEVFS_FL_DEFAULT,
				       h3600_backpaq_fpga_major_num, FPGA_MINOR,
				       S_IFCHR | S_IRUSR | S_IWUSR, 
				       &h3600_backpaq_fpga_fops, NULL );
#endif

#ifdef CONFIG_PROC_FS
	/* Set up the PROC file system entry */
	proc_backpaq_fpga = create_proc_entry(FPGA_PROC_NAME, 0, NULL);
	if ( !proc_backpaq_fpga ) {
		/* We probably need to create the "backpaq" directory first */
		proc_mkdir(FPGA_PROC_DIR,0);
		proc_backpaq_fpga = create_proc_entry(FPGA_PROC_NAME, 0, NULL);
	}
	
	if ( proc_backpaq_fpga )
		proc_backpaq_fpga->read_proc = proc_h3600_backpaq_read;    
	else {
		printk(KERN_ALERT __FILE__ ": unable to create proc entry %s\n", FPGA_PROC_NAME);
		devfs_unregister( devfs_fpga );
		devfs_unregister_chrdev( h3600_backpaq_fpga_major_num, MODULE_NAME );
		return -ENODEV;
	}
#endif

#ifdef CONFIG_PM
	fpga_backpaq_dev = h3600_backpaq_register_device( H3600_BACKPAQ_FPGA_DEV, 0, 
							  h3600_backpaq_fpga_callback );
	printk(KERN_ALERT __FILE__ ": registered backpaq callback=%p\n", h3600_backpaq_fpga_callback);
#endif

	return 0;    
} 

void __exit h3600_backpaq_fpga_exit_module(void)
{
	printk(KERN_ALERT __FILE__ ": exit\n");

#ifdef CONFIG_PM
        h3600_backpaq_unregister_device(fpga_backpaq_dev);
#endif
#ifdef CONFIG_PROC_FS
	if (proc_backpaq_fpga) {
		remove_proc_entry(FPGA_PROC_NAME, 0);
		proc_backpaq_fpga = NULL;
	}
#endif
#ifdef CONFIG_DEVFS_FS
	devfs_unregister( devfs_fpga );
#endif
	devfs_unregister_chrdev( h3600_backpaq_fpga_major_num, MODULE_NAME );
}

MODULE_AUTHOR("Andrew Christian <andyc@handhelds.org>");
MODULE_DESCRIPTION("FPGA driver for Compaq Mercury BackPAQ camera");
MODULE_LICENSE("Dual BSD/GPL");

module_init(h3600_backpaq_fpga_init_module);
module_exit(h3600_backpaq_fpga_exit_module);

